#ifndef _BSP_IIC_H__
#define _BSP_IIC_H__ /**< _BSP_IIC_H__ 头文件重复包含保护宏。 */

#include "ti_msp_dl_config.h"
#include "delay.h"

// 设置SDA输出模式
/** @brief SDA_OUT 函数式宏封装。 */
#define SDA_OUT()                                                      \
    {                                                                  \
        DL_GPIO_initDigitalOutput(IIC_Software_IIC_SDA_IOMUX);         \
        DL_GPIO_setPins(IIC_Software_PORT, IIC_Software_IIC_SDA_PIN);  \
        DL_GPIO_enableOutput(IIC_Software_PORT, IIC_Software_IIC_SDA_PIN); \
    }
// 设置SDA输入模式（启用内部上拉，无外部上拉电阻时保证线路不浮空）
/** @brief SDA_IN 函数式宏封装。 */
#define SDA_IN()                                                                              \
    {                                                                                         \
        /* DriverLib 的输入初始化不会清除 GPIO 输出使能，必须先真正释放 SDA。 */              \
        DL_GPIO_disableOutput(IIC_Software_PORT, IIC_Software_IIC_SDA_PIN);                   \
        DL_GPIO_initDigitalInputFeatures(IIC_Software_IIC_SDA_IOMUX,                          \
                                         DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP, \
                                         DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE); \
    }

// 获取SDA引脚的电平变化
/** @brief SDA_GET 函数式宏封装。 */
#define SDA_GET() (((DL_GPIO_readPins(IIC_Software_PORT, IIC_Software_IIC_SDA_PIN) & IIC_Software_IIC_SDA_PIN) > 0) ? 1 : 0)
// SDA与SCL输出
/** @brief SDA 函数式宏封装。 */
#define SDA(x) ((x) ? (DL_GPIO_setPins(IIC_Software_PORT, IIC_Software_IIC_SDA_PIN)) : (DL_GPIO_clearPins(IIC_Software_PORT, IIC_Software_IIC_SDA_PIN)))
/** @brief SCL 函数式宏封装。 */
#define SCL(x) ((x) ? (DL_GPIO_setPins(IIC_Software_PORT, IIC_Software_IIC_SCL_PIN)) : (DL_GPIO_clearPins(IIC_Software_PORT, IIC_Software_IIC_SCL_PIN)))

/**
 * @brief 向 8 位寄存器地址连续写入数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 8 位寄存器地址。
 * @param num 待写入字节数。
 * @param regdata 待写入数据缓冲区。
 * @note 软件模拟 I2C 时序，错误码表示失败阶段。
 * @retval 0 表示成功，其他值表示写入阶段错误码。
 */
uint8_t IICwriteBytes(uint8_t addr, uint8_t regaddr, uint8_t num, uint8_t *regdata);
/**
 * @brief 从 8 位寄存器地址连续读取数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 8 位寄存器地址。
 * @param num 待读取字节数。
 * @param Read 读取数据保存缓冲区。
 * @note 软件模拟 I2C 时序，最后 1 字节后发送 NACK。
 * @retval 0 表示成功，其他值表示读取阶段错误码。
 */
uint8_t IICreadBytes(uint8_t addr, uint8_t regaddr, uint8_t num, uint8_t *Read);
/**
 * @brief 向 16 位寄存器地址连续写入数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 16 位寄存器地址。
 * @param num 待写入字节数。
 * @param regdata 待写入数据缓冲区。
 * @note 写寄存器地址时先发送高 8 位，再发送低 8 位。
 * @retval 0 表示成功，其他值表示写入阶段错误码。
 */
uint8_t IICwriteBytes16(uint8_t addr, uint16_t regaddr, uint16_t num, const uint8_t *regdata);
/**
 * @brief 从 16 位寄存器地址连续读取数据。
 * @param addr I2C 7 位器件地址。
 * @param regaddr 16 位寄存器地址。
 * @param num 待读取字节数。
 * @param Read 读取数据保存缓冲区。
 * @note 写寄存器地址时先发送高 8 位，再发送低 8 位。
 * @retval 0 表示成功，其他值表示读取阶段错误码。
 */
uint8_t IICreadBytes16(uint8_t addr, uint16_t regaddr, uint16_t num, uint8_t *Read);
#endif
