#ifndef BSP_DL1A_H
#define BSP_DL1A_H /**< BSP_DL1A_H 头文件重复包含保护宏。 */

#include <stdint.h>

#define BSP_DL1A_OK              (0U) /**< DL1A BSP 访问成功状态码。 */
#define BSP_DL1A_ERR_I2C_WRITE   (1U) /**< DL1A BSP I2C 写失败状态码。 */
#define BSP_DL1A_ERR_I2C_READ    (2U) /**< DL1A BSP I2C 读失败状态码。 */

/**
 * @brief 初始化 DL1A 相关控制引脚。
 * @param 无。
 * @note 当前使用 PB14 作为 XS/XSHUT，I2C 使用 BSP/SoftI2C 的 PA1/PA0。
 * @retval 无。
 */
void BSP_DL1A_InitPins(void);

/**
 * @brief 设置 DL1A 的 XS/XSHUT 控制脚电平。
 * @param high 非 0 拉高 XS，0 拉低 XS。
 * @retval 无。
 */
void BSP_DL1A_SetXs(uint8_t high);

/**
 * @brief 向 DL1A 的 8 位寄存器地址连续写入数据。
 * @param reg 8 位寄存器地址。
 * @param data 待写入数据缓冲区。
 * @param len 待写入字节数。
 * @note DL1A/VL53L0X 使用 8 位寄存器地址，I2C 7 位地址为 0x29。
 * @retval 0 表示成功，其他值表示 I2C 写入阶段错误码。
 */
uint8_t BSP_DL1A_WriteRegister(uint8_t reg, const uint8_t *data, uint8_t len);

/**
 * @brief 从 DL1A 的 8 位寄存器地址连续读取数据。
 * @param reg 8 位寄存器地址。
 * @param data 读取数据保存缓冲区。
 * @param len 待读取字节数。
 * @note DL1A/VL53L0X 使用 8 位寄存器地址，I2C 7 位地址为 0x29。
 * @retval 0 表示成功，其他值表示 I2C 读取阶段错误码。
 */
uint8_t BSP_DL1A_ReadRegister(uint8_t reg, uint8_t *data, uint8_t len);

#endif
