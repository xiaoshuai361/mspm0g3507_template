#include "bsp_dl1a.h"

#include "bsp_iic.h"
#include "ti_msp_dl_config.h"

#define BSP_DL1A_I2C_ADDRESS (0x29U) /**< DL1A 默认 I2C 7 位地址。 */

/**
 * @brief 初始化 DL1A 相关控制引脚。
 * @param 无。
 * @note 当前 SysConfig 的 ToF 控制脚仍命名为 DL1B_XS，硬件上接 DL1A 的 XS/XSHUT。
 * @retval 无。
 */
void BSP_DL1A_InitPins(void)
{
    DL_GPIO_setPins(DL1B_PORT, DL1B_XS_PIN);
    DL_GPIO_enableOutput(DL1B_PORT, DL1B_XS_PIN);
}

/**
 * @brief 设置 DL1A 的 XS/XSHUT 控制脚电平。
 * @param high 非 0 拉高 XS，0 拉低 XS。
 * @retval 无。
 */
void BSP_DL1A_SetXs(uint8_t high)
{
    if (high != 0U) {
        DL_GPIO_setPins(DL1B_PORT, DL1B_XS_PIN);
    } else {
        DL_GPIO_clearPins(DL1B_PORT, DL1B_XS_PIN);
    }
}

/**
 * @brief 向 DL1A 的 8 位寄存器地址连续写入数据。
 * @param reg 8 位寄存器地址。
 * @param data 待写入数据缓冲区。
 * @param len 待写入字节数。
 * @retval 0 表示成功，其他值表示 I2C 写入阶段错误码。
 */
uint8_t BSP_DL1A_WriteRegister(uint8_t reg, const uint8_t *data, uint8_t len)
{
    return IICwriteBytes(BSP_DL1A_I2C_ADDRESS, reg, len, (uint8_t *)data);
}

/**
 * @brief 从 DL1A 的 8 位寄存器地址连续读取数据。
 * @param reg 8 位寄存器地址。
 * @param data 读取数据保存缓冲区。
 * @param len 待读取字节数。
 * @retval 0 表示成功，其他值表示 I2C 读取阶段错误码。
 */
uint8_t BSP_DL1A_ReadRegister(uint8_t reg, uint8_t *data, uint8_t len)
{
    return IICreadBytes(BSP_DL1A_I2C_ADDRESS, reg, len, data);
}
