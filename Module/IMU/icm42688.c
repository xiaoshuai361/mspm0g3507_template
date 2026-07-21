#include <stdio.h>
#include "icm42688.h"
#include "board.h"

#define ICM_USE_I2C /**< 启用 ICM42688 的 I2C 通信路径。 */


#if defined(ICM_USE_HARD_SPI)
#include "bsp_spi.h"
#elif defined(ICM_USE_I2C)
#include "bsp_iic.h"
#endif

static float accSensitivity   = 0.244f;   //加速度的最小分辨率 mg/LSB
static float gyroSensitivity  = 32.8f;    //陀螺仪的最小分辨率
static uint8_t lastI2cStatus; /**< lastI2cStatus 全局状态或配置变量。 */
static uint8_t lastWhoAmI; /**< lastWhoAmI 全局状态或配置变量。 */


/*ICM42688使用的ms级延时函数，须由用户提供。*/
/** @brief ICM42688DelayMs 函数式宏封装。 */
#define ICM42688DelayMs(_nms)  delay_ms(_nms)

/**
 * @brief 获取 ICM42688 最近一次 I2C 状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t bsp_Icm42688GetLastI2cStatus(void)
{
    return lastI2cStatus;
}

uint8_t bsp_Icm42688GetWhoAmI(void)
{
    return lastWhoAmI;
}

/**
 * @brief 探测 ICM42688 当前 I2C 地址。
 * @param address address 参数。
 * @param whoAmI whoAmI 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t bsp_Icm42688ProbeAddress(uint8_t address, uint8_t *whoAmI)
{
    uint8_t value = 0U;
    uint8_t status;

    status = IICreadBytes(address, ICM42688_WHO_AM_I, 1U, &value);
    if (whoAmI != NULL)
    {
        *whoAmI = value;
    }
    return status;
}

#if defined(ICM_USE_HARD_SPI)


/*******************************************************************************
* 名    称： Icm_Spi_ReadWriteNbytes
* 功    能： 使用SPI读写n个字节
* 入口参数： pBuffer: 写入的数组  len:写入数组的长度
* 出口参数： 无
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注：
*******************************************************************************/
/**
 * @brief 兼容原 SPI 接口的多字节读写封装。
 * @param pBuffer pBuffer 参数。
 * @param len 数据长度。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void Icm_Spi_ReadWriteNbytes(uint8_t* pBuffer, uint8_t len)
{
    uint8_t i = 0;

    for(i = 0; i < len; i ++)
    {
		*pBuffer = spi_read_write_byte(*pBuffer);
        pBuffer++;
    }

}
#endif

/*******************************************************************************
* 名    称： icm42688_read_reg
* 功    能： 读取单个寄存器的值
* 入口参数： reg: 寄存器地址
* 出口参数： 当前寄存器地址的值
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注： 使用SPI读取寄存器时要注意:最高位为读写位，详见datasheet page51.
*******************************************************************************/
/**
 * @brief 读取 ICM42688 单个寄存器。
 * @param reg 寄存器地址。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
static uint8_t icm42688_read_reg(uint8_t reg)
{
    uint8_t regval = 0;

#if defined(ICM_USE_HARD_SPI)
    SPI_CS(0);
    reg |= 0x80;
    /* 写入要读的寄存器地址 */
    spi_read_write_byte(reg);
    /* 读取寄存器数据 */
    regval = spi_read_write_byte(0xFF);
    SPI_CS(1);
#elif defined(ICM_USE_I2C)
	lastI2cStatus = IICreadBytes(ICM42688_ADDRESS, reg, 1, &regval);
#endif

    return regval;
}

/*******************************************************************************
* 名    称： icm42688_read_regs
* 功    能： 连续读取多个寄存器的值
* 入口参数： reg: 起始寄存器地址 *buf数据指针,uint16_t len长度
* 出口参数： 无
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注： 使用SPI读取寄存器时要注意:最高位为读写位，详见datasheet page50.
*******************************************************************************/
/**
 * @brief 连续读取 ICM42688 寄存器。
 * @param reg 寄存器地址。
 * @param buf buf 参数。
 * @param len 数据长度。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
static uint8_t icm42688_read_regs(uint8_t reg, uint8_t* buf, uint16_t len)
{
#if defined(ICM_USE_HARD_SPI)
    reg |= 0x80;
    SPI_CS(0);
    /* 写入要读的寄存器地址 */
    spi_read_write_byte(reg);
    /* 读取寄存器数据 */
    while(len)
	{
		*buf = spi_read_write_byte(0x00);
		len--;
		buf++;
	}
    SPI_CS(1);
    lastI2cStatus = 0U;
#elif defined(ICM_USE_I2C)
	lastI2cStatus = IICreadBytes(ICM42688_ADDRESS, reg, len, buf);
#endif
    return lastI2cStatus;
}


/*******************************************************************************
* 名    称： icm42688_write_reg
* 功    能： 向单个寄存器写数据
* 入口参数： reg: 寄存器地址 value:数据
* 出口参数： 0
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注： 使用SPI读取寄存器时要注意:最高位为读写位，详见datasheet page50.
*******************************************************************************/
/**
 * @brief 写入 ICM42688 单个寄存器。
 * @param reg 寄存器地址。
 * @param value 寄存器值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
static uint8_t icm42688_write_reg(uint8_t reg, uint8_t value)
{
#if defined(ICM_USE_HARD_SPI)
    SPI_CS(0);
    /* 写入要读的寄存器地址 */
    /* 写入要读的寄存器地址 */
    spi_read_write_byte(reg);
    /* 读取寄存器数据 */
    spi_read_write_byte(value);
    SPI_CS(1);
#elif defined(ICM_USE_I2C)
	lastI2cStatus = IICwriteBytes(ICM42688_ADDRESS, reg, 1, &value);
#endif
    return lastI2cStatus;
}



/**
 * @brief 获取加速度量程分辨率。
 * @param Ascale Ascale 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回浮点结果。
 */
float bsp_Icm42688GetAres(uint8_t Ascale)
{
    switch(Ascale)
    {
    // Possible accelerometer scales (and their register bit settings) are:
    // 2 Gs (11), 4 Gs (10), 8 Gs (01), and 16 Gs  (00).
    case AFS_2G:
        accSensitivity = 2000 / 32768.0f;
        break;
    case AFS_4G:
        accSensitivity = 4000 / 32768.0f;
        break;
    case AFS_8G:
        accSensitivity = 8000 / 32768.0f;
        break;
    case AFS_16G:
        accSensitivity = 16000 / 32768.0f;
        break;
    }

    return accSensitivity;
}

/**
 * @brief 获取陀螺仪量程分辨率。
 * @param Gscale Gscale 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回浮点结果。
 */
float bsp_Icm42688GetGres(uint8_t Gscale)
{
    switch(Gscale)
    {
    case GFS_15_125DPS:
        gyroSensitivity = 15.125f / 32768.0f;
        break;
    case GFS_31_25DPS:
        gyroSensitivity = 31.25f / 32768.0f;
        break;
    case GFS_62_5DPS:
        gyroSensitivity = 62.5f / 32768.0f;
        break;
    case GFS_125DPS:
        gyroSensitivity = 125.0f / 32768.0f;
        break;
    case GFS_250DPS:
        gyroSensitivity = 250.0f / 32768.0f;
        break;
    case GFS_500DPS:
        gyroSensitivity = 500.0f / 32768.0f;
        break;
    case GFS_1000DPS:
        gyroSensitivity = 1000.0f / 32768.0f;
        break;
    case GFS_2000DPS:
        gyroSensitivity = 2000.0f / 32768.0f;
        break;
    }
    return gyroSensitivity;
}

/*******************************************************************************
* 名    称： bsp_Icm42688RegCfg
* 功    能： Icm42688 寄存器配置
* 入口参数： 无
* 出口参数： 无
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注：
*******************************************************************************/
/**
 * @brief 配置 ICM42688 常用寄存器。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_Icm42688RegCfg(void)
{
    uint8_t reg_val = 0;
    /* 读取 who am i 寄存器 */
    reg_val = icm42688_read_reg(ICM42688_WHO_AM_I);
    lastWhoAmI = reg_val;
	printf("IMU WHO=0x%02X BUS=%u\r\n", reg_val, lastI2cStatus);
    if ((lastI2cStatus != 0U) || (reg_val != ICM42688_ID))
    {
        return -1;
    }

    if (icm42688_write_reg(ICM42688_REG_BANK_SEL, 0) != 0U)
        return -2;
    if (icm42688_write_reg(ICM42688_REG_BANK_SEL, 0x01) != 0U)
        return -2;
    ICM42688DelayMs(100);

    {

        bsp_Icm42688GetAres(AFS_4G);
        if (icm42688_write_reg(ICM42688_REG_BANK_SEL, 0x00) != 0U)
            return -2;
        //reg_val = icm42688_read_reg(ICM42688_ACCEL_CONFIG0);//page74
        reg_val = (AFS_4G << 5);   //量程 ±2g
        reg_val |= (AODR_100Hz);     //输出速率 100HZ
        if (icm42688_write_reg(ICM42688_ACCEL_CONFIG0, reg_val) != 0U)
            return -2;

        bsp_Icm42688GetGres(GFS_1000DPS);
        if (icm42688_write_reg(ICM42688_REG_BANK_SEL, 0x00) != 0U)
            return -2;
        //reg_val = icm42688_read_reg(ICM42688_GYRO_CONFIG0);//page73
        reg_val = (GFS_1000DPS << 5);   //量程 ±1000dps
        reg_val |= (GODR_100Hz);     //输出速率 100HZ
        if (icm42688_write_reg(ICM42688_GYRO_CONFIG0, reg_val) != 0U)
            return -2;

        if (icm42688_write_reg(ICM42688_REG_BANK_SEL, 0x00) != 0U)
            return -2;
        reg_val = icm42688_read_reg(ICM42688_PWR_MGMT0); //读取PWR—MGMT0当前寄存器的值(page72)
        if (lastI2cStatus != 0U)
            return -2;
        reg_val &= ~(1 << 5);//使能温度测量
        reg_val |= ((3) << 2);//设置GYRO_MODE  0:关闭 1:待机 2:预留 3:低噪声
        reg_val |= (3);//设置ACCEL_MODE 0:关闭 1:关闭 2:低功耗 3:低噪声
        if (icm42688_write_reg(ICM42688_PWR_MGMT0, reg_val) != 0U)
            return -2;
        ICM42688DelayMs(1); //操作完PWR—MGMT0寄存器后 200us内不能有任何读写寄存器的操作

        return 0;
    }
}
/*******************************************************************************
* 名    称： bsp_Icm42688Init
* 功    能： Icm42688 传感器初始化
* 入口参数： 无
* 出口参数： 0: 初始化成功  其他值: 初始化失败
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注：
*******************************************************************************/
/**
 * @brief 初始化 ICM42688 传感器。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_Icm42688Init(void)
{
    return(bsp_Icm42688RegCfg());

}

/*******************************************************************************
* 名    称： bsp_IcmGetTemperature
* 功    能： 读取Icm42688 内部传感器温度
* 入口参数： 无
* 出口参数： 无
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注： datasheet page62
*******************************************************************************/
/**
 * @brief 读取 ICM42688 温度。
 * @param pTemp pTemp 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_IcmGetTemperature(int16_t* pTemp)
{
    uint8_t buffer[2] = {0};

    if (icm42688_read_regs(ICM42688_TEMP_DATA1, buffer, 2) != 0U)
        return (int8_t)lastI2cStatus;

    *pTemp = (int16_t)(((int16_t)((buffer[0] << 8) | buffer[1])) / 132.48 + 25);
    return 0;
}

/*******************************************************************************
* 名    称： bsp_IcmGetAccelerometer
* 功    能： 读取Icm42688 加速度的值
* 入口参数： 三轴加速度的值
* 出口参数： 无
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注： datasheet page62
*******************************************************************************/
/**
 * @brief 读取 ICM42688 加速度。
 * @param accData accData 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_IcmGetAccelerometer(icm42688RawData_t* accData)
{
    uint8_t buffer[6] = {0};

    if (icm42688_read_regs(ICM42688_ACCEL_DATA_X1, buffer, 6) != 0U)
        return (int8_t)lastI2cStatus;

    accData->x = ((uint16_t)buffer[0] << 8) | buffer[1];
    accData->y = ((uint16_t)buffer[2] << 8) | buffer[3];
    accData->z = ((uint16_t)buffer[4] << 8) | buffer[5];

    accData->x = (int16_t)(accData->x * accSensitivity);
    accData->y = (int16_t)(accData->y * accSensitivity);
    accData->z = (int16_t)(accData->z * accSensitivity);

    return 0;
}

/*******************************************************************************
* 名    称： bsp_IcmGetGyroscope
* 功    能： 读取Icm42688 陀螺仪的值
* 入口参数： 三轴陀螺仪的值
* 出口参数： 无
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注： datasheet page63
*******************************************************************************/
/**
 * @brief 读取 ICM42688 陀螺仪。
 * @param GyroData GyroData 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_IcmGetGyroscope(icm42688RawData_t* GyroData)
{
    uint8_t buffer[6] = {0};

    if (icm42688_read_regs(ICM42688_GYRO_DATA_X1, buffer, 6) != 0U)
        return (int8_t)lastI2cStatus;

    GyroData->x = ((uint16_t)buffer[0] << 8) | buffer[1];
    GyroData->y = ((uint16_t)buffer[2] << 8) | buffer[3];
    GyroData->z = ((uint16_t)buffer[4] << 8) | buffer[5];

    GyroData->x = (int16_t)(GyroData->x * gyroSensitivity);
    GyroData->y = (int16_t)(GyroData->y * gyroSensitivity);
    GyroData->z = (int16_t)(GyroData->z * gyroSensitivity);
    return 0;
}

/*******************************************************************************
* 名    称： bsp_IcmGetRawData
* 功    能： 读取Icm42688加速度陀螺仪数据
* 入口参数： 六轴
* 出口参数： 无
* 作　　者： Baxiange
* 创建日期： 2024-07-25
* 修    改：
* 修改日期：
* 备    注： datasheet page62,63
*******************************************************************************/
/**
 * @brief 读取 ICM42688 原始三轴数据。
 * @param accData accData 参数。
 * @param GyroData GyroData 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回执行结果或状态。
 */
int8_t bsp_IcmGetRawData(icm42688RealData_t* accData, icm42688RealData_t* GyroData)
{
    uint8_t buffer[12] = {0};
	icm42688RawData_t accRaw;
	icm42688RawData_t gyroRaw;

    if (icm42688_read_regs(ICM42688_ACCEL_DATA_X1, buffer, 12) != 0U)
    {
        accData->x = 0.0f;
        accData->y = 0.0f;
        accData->z = 0.0f;
        GyroData->x = 0.0f;
        GyroData->y = 0.0f;
        GyroData->z = 0.0f;
        return (int8_t)lastI2cStatus;
    }

    accRaw.x  = ((uint16_t)buffer[0] << 8)  | buffer[1];
    accRaw.y  = ((uint16_t)buffer[2] << 8)  | buffer[3];
    accRaw.z  = ((uint16_t)buffer[4] << 8)  | buffer[5];
    gyroRaw.x = ((uint16_t)buffer[6] << 8)  | buffer[7];
    gyroRaw.y = ((uint16_t)buffer[8] << 8)  | buffer[9];
    gyroRaw.z = ((uint16_t)buffer[10] << 8) | buffer[11];


    accData->x = (float)(accRaw.x * accSensitivity);
    accData->y = (float)(accRaw.y * accSensitivity);
    accData->z = (float)(accRaw.z * accSensitivity);

    GyroData->x = (float)(gyroRaw.x * gyroSensitivity);
    GyroData->y = (float)(gyroRaw.y * gyroSensitivity);
    GyroData->z = (float)(gyroRaw.z * gyroSensitivity);

    return 0;
}

