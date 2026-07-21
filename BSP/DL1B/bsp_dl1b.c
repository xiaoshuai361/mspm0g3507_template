#include "bsp_dl1b.h"

#include "bsp_iic.h"
#include "ti_msp_dl_config.h"

#define BSP_DL1B_I2C_ADDRESS (0x29U) /**< DL1B 默认 I2C 7 位地址。 */

/**
 * @brief 初始化 DL1B 相关控制引脚。
 * @param 无。
 * @note 当前将 XS 引脚设置为输出并拉高。
 * @retval 无。
 */
void BSP_DL1B_InitPins(void)
{
    DL_GPIO_setPins(DL1B_PORT, DL1B_XS_PIN);
    DL_GPIO_enableOutput(DL1B_PORT, DL1B_XS_PIN);
}

/**
 * @brief 设置 DL1B 的 XS 控制脚电平。
 * @param high 非 0 拉高 XS，0 拉低 XS。
 * @note 调用前需已完成 BSP_DL1B_InitPins()。
 * @retval 无。
 */
void BSP_DL1B_SetXs(uint8_t high)
{
    if (high != 0U) {
        /* 非零参数视为有效高电平。 */
        DL_GPIO_setPins(DL1B_PORT, DL1B_XS_PIN);
    } else {
        DL_GPIO_clearPins(DL1B_PORT, DL1B_XS_PIN);
    }
}

/**
 * @brief 向 DL1B 的 16 位寄存器地址连续写入数据。
 * @param reg 16 位寄存器地址。
 * @param data 待写入数据缓冲区。
 * @param len 待写入字节数。
 * @note 底层使用软件 I2C 的 16 位寄存器写接口。
 * @retval 0 表示成功，其他值表示 I2C 写入阶段错误码。
 */
uint8_t BSP_DL1B_WriteRegister(uint16_t reg, const uint8_t *data, uint16_t len)
{
    return IICwriteBytes16(BSP_DL1B_I2C_ADDRESS, reg, len, data);
}

/**
 * @brief 从 DL1B 的 16 位寄存器地址连续读取数据。
 * @param reg 16 位寄存器地址。
 * @param data 读取数据保存缓冲区。
 * @param len 待读取字节数。
 * @note 底层使用软件 I2C 的 16 位寄存器读接口。
 * @retval 0 表示成功，其他值表示 I2C 读取阶段错误码。
 */
uint8_t BSP_DL1B_ReadRegister(uint16_t reg, uint8_t *data, uint16_t len)
{
    return IICreadBytes16(BSP_DL1B_I2C_ADDRESS, reg, len, data);
}
