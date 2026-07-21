#include "bsp_iic.h"

/**
 * @brief 产生软件 I2C 起始信号。
 * @param 无。
 * @note 在 SCL 为高电平时拉低 SDA，形成 START 条件。
 * @retval 无。
 */
void IIC_Start(void)
{
        SDA_OUT();
        /* 确保 SDA、SCL 都为高（空闲状态），再拉低 SDA 产生合法 START 条件 */
        SDA(1);
        delay_us(2);
        SCL(1);
        delay_us(5);
        SDA(0); /* SDA 下降沿（SCL=1 时）→ START 条件 */
        delay_us(5);
        SCL(0); /* 拉低 SCL，保持 START，准备发送数据 */
}
/**
 * @brief 产生软件 I2C 停止信号。
 * @param 无。
 * @note 在 SCL 为高电平时释放 SDA，形成 STOP 条件。
 * @retval 无。
 */
void IIC_Stop(void)
{
        SDA_OUT();
        SCL(0);
        SDA(0);

        SCL(1);
        delay_us(5);
        SDA(1);
        delay_us(5);
}

/**
 * @brief 主机发送 ACK 或 NACK 信号。
 * @param ack 0 发送 ACK，非 0 发送 NACK。
 * @note 在第 9 个时钟周期输出应答电平。
 * @retval 无。
 */
void IIC_Send_Ack(unsigned char ack)
{
        SDA_OUT();
        SCL(0);
        SDA(0);
        delay_us(5);
        if (!ack)
                SDA(0);
        else
                SDA(1);
        SCL(1);
        delay_us(5);
        SCL(0);
        SDA(1);
}

/**
 * @brief 等待从机 ACK 应答。
 * @param 无。
 * @note 释放 SDA 后在 SCL 高电平期间采样，超时会主动发送 STOP。
 * @retval 0 表示收到 ACK，1 表示超时或无 ACK。
 */
unsigned char I2C_WaitAck(void)
{

        char ack = 0;
        unsigned char ack_flag = 10;
        SCL(0);
        SDA(1);
        SDA_IN();
        /* 释放 SDA 后等待线路上升，避免弱上拉/大电容把残留低电平误判为 ACK。 */
        delay_us(5);

        SCL(1);
        /* ACK 在 SCL 高电平的稳定区采样，不能刚产生上升沿就读取。 */
        delay_us(5);
        while ((SDA_GET() == 1) && (ack_flag))
        {
                ack_flag--;
                delay_us(5);
        }

        if (ack_flag <= 0)
        {
                IIC_Stop();
                return 1;
        }
        else
        {
                SCL(0);
                SDA_OUT();
        }
        return ack;
}

/**
 * @brief 通过软件 I2C 发送 1 个字节。
 * @param dat 待发送字节。
 * @note 按高位在前的顺序输出 8 个数据位。
 * @retval 无。
 */
void Send_Byte(uint8_t dat)
{
        int i = 0;
        SDA_OUT();
        SCL(0); // 拉低时钟开始数据传输

        for (i = 0; i < 8; i++)
        {
                SDA((dat & 0x80) >> 7);
                delay_us(1);
                SCL(1);
                delay_us(5);
                SCL(0);
                delay_us(5);
                dat <<= 1;
        }
}

/**
 * @brief 通过软件 I2C 读取 1 个字节。
 * @param 无。
 * @note 按高位在前的顺序采样 8 个数据位。
 * @retval 返回读取到的字节数据。
 */
unsigned char Read_Byte(void)
{
        unsigned char i, receive = 0;
        SDA_IN(); // SDA设置为输入
        for (i = 0; i < 8; i++)
        {
                SCL(0);
                delay_us(5);
                SCL(1);
                delay_us(5);
                receive <<= 1;
                if (SDA_GET())
                {
                        receive |= 1;
                }
                delay_us(5);
        }

        SCL(0);

        return receive;
}

/**
 * @brief 向 8 位寄存器地址连续写入数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 8 位寄存器地址。
 * @param num 待写入字节数。
 * @param regdata 待写入数据缓冲区。
 * @note 依次发送器件写地址、寄存器地址和数据字节。
 * @retval 0 表示成功，其他值表示写入阶段错误码。
 */
uint8_t IICwriteBytes(uint8_t addr, uint8_t regaddr, uint8_t num, uint8_t *regdata)
{
        uint16_t i = 0;
        IIC_Start();
        Send_Byte((addr << 1) | 0);
        if (I2C_WaitAck() == 1)
        {
                /* 地址阶段无应答，停止总线后返回。 */
                IIC_Stop();
                return 1;
        }
        Send_Byte(regaddr);
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 2;
        }

        for (i = 0; i < num; i++)
        {
                Send_Byte(regdata[i]);
                if (I2C_WaitAck() == 1)
                {
                        IIC_Stop();
                        return (3 + i);
                }
        }

        IIC_Stop();
        return 0;
}

/**
 * @brief 从 8 位寄存器地址连续读取数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 8 位寄存器地址。
 * @param num 待读取字节数。
 * @param Read 读取数据保存缓冲区。
 * @note 先写入寄存器地址，再重新起始切换到读方向。
 * @retval 0 表示成功，其他值表示读取阶段错误码。
 */
uint8_t IICreadBytes(uint8_t addr, uint8_t regaddr, uint8_t num, uint8_t *Read)
{
        uint8_t i;
        IIC_Start();
        Send_Byte((addr << 1) | 0);
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 1;
        }
        Send_Byte(regaddr);
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 2;
        }

        IIC_Start();
        Send_Byte((addr << 1) | 1);
        if (I2C_WaitAck() == 1)
        {
                /* 读地址阶段失败，释放总线。 */
                IIC_Stop();
                return 3;
        }

        for (i = 0; i < (num - 1); i++)
        {
                Read[i] = Read_Byte();
                /* 中间字节发送 ACK，通知从机继续发送。 */
                IIC_Send_Ack(0);
        }
        Read[i] = Read_Byte();
        /* 最后 1 字节发送 NACK，结束连续读取。 */
        IIC_Send_Ack(1);
        IIC_Stop();
        return 0;
}

/**
 * @brief 向 16 位寄存器地址连续写入数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 16 位寄存器地址。
 * @param num 待写入字节数。
 * @param regdata 待写入数据缓冲区。
 * @note 寄存器地址按高字节、低字节顺序发送。
 * @retval 0 表示成功，其他值表示写入阶段错误码。
 */
uint8_t IICwriteBytes16(uint8_t addr, uint16_t regaddr, uint16_t num, const uint8_t *regdata)
{
        uint16_t i = 0;

        if ((num > 0U) && (regdata == 0))
        {
                /* 有数据长度时必须提供有效缓冲区。 */
                return 1;
        }

        IIC_Start();
        Send_Byte((addr << 1) | 0);
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 2;
        }
        Send_Byte((uint8_t)(regaddr >> 8U));
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 3;
        }
        Send_Byte((uint8_t)(regaddr & 0xFFU));
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 4;
        }

        for (i = 0U; i < num; i++)
        {
                Send_Byte(regdata[i]);
                if (I2C_WaitAck() == 1)
                {
                        IIC_Stop();
                        return (uint8_t)(5U + i);
                }
        }

        IIC_Stop();
        return 0;
}

/**
 * @brief 从 16 位寄存器地址连续读取数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 16 位寄存器地址。
 * @param num 待读取字节数。
 * @param Read 读取数据保存缓冲区。
 * @note 寄存器地址按高字节、低字节顺序发送。
 * @retval 0 表示成功，其他值表示读取阶段错误码。
 */
uint8_t IICreadBytes16(uint8_t addr, uint16_t regaddr, uint16_t num, uint8_t *Read)
{
        uint16_t i;

        if ((num == 0U) || (Read == 0))
        {
                /* 连续读取至少需要 1 字节长度和有效缓冲区。 */
                return 1;
        }

        IIC_Start();
        Send_Byte((addr << 1) | 0);
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 2;
        }
        Send_Byte((uint8_t)(regaddr >> 8U));
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 3;
        }
        Send_Byte((uint8_t)(regaddr & 0xFFU));
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 4;
        }

        IIC_Start();
        Send_Byte((addr << 1) | 1);
        if (I2C_WaitAck() == 1)
        {
                IIC_Stop();
                return 5;
        }

        for (i = 0U; i < (num - 1U); i++)
        {
                Read[i] = Read_Byte();
                /* 非末尾字节继续 ACK。 */
                IIC_Send_Ack(0);
        }
        Read[i] = Read_Byte();
        /* 末尾字节 NACK 后停止总线。 */
        IIC_Send_Ack(1);
        IIC_Stop();
        return 0;
}
