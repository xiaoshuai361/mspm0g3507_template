#ifndef BSP_DL1B_H
#define BSP_DL1B_H /**< BSP_DL1B_H 底层驱动配置/状态宏。 */

#include <stdint.h>

#define BSP_DL1B_OK              (0U) /**< DL1B BSP 访问成功状态码。 */
#define BSP_DL1B_ERR_I2C_WRITE   (1U) /**< DL1B BSP I2C 写失败状态码。 */
#define BSP_DL1B_ERR_I2C_READ    (2U) /**< DL1B BSP I2C 读失败状态码。 */

/**
 * @brief 初始化 DL1B 相关控制引脚。
 * @param 无。
 * @note 当前仅配置 XS 引脚为输出并默认拉高。
 * @retval 无。
 */
void BSP_DL1B_InitPins(void);
/**
 * @brief 设置 DL1B 的 XS 控制脚电平。
 * @param high 非 0 拉高 XS，0 拉低 XS。
 * @note XS 引脚已在 BSP_DL1B_InitPins() 中配置为输出。
 * @retval 无。
 */
void BSP_DL1B_SetXs(uint8_t high);
/**
 * @brief 向 DL1B 的 16 位寄存器地址连续写入数据。
 * @param reg 16 位寄存器地址。
 * @param data 待写入数据缓冲区。
 * @param len 待写入字节数。
 * @note 通过软件 I2C 访问固定的 DL1B 器件地址。
 * @retval 0 表示成功，其他值表示 I2C 写入阶段错误码。
 */
uint8_t BSP_DL1B_WriteRegister(uint16_t reg, const uint8_t *data, uint16_t len);
/**
 * @brief 从 DL1B 的 16 位寄存器地址连续读取数据。
 * @param reg 16 位寄存器地址。
 * @param data 读取数据保存缓冲区。
 * @param len 待读取字节数。
 * @note 通过软件 I2C 访问固定的 DL1B 器件地址。
 * @retval 0 表示成功，其他值表示 I2C 读取阶段错误码。
 */
uint8_t BSP_DL1B_ReadRegister(uint16_t reg, uint8_t *data, uint16_t len);

#endif
