#ifndef BSP_ADC_H
#define BSP_ADC_H /**< BSP_ADC_H 底层驱动配置/状态宏。 */

#include <stdint.h>

#define BSP_ADC_BATTERY_FULL_SCALE_MV (33297U) /**< 电池 ADC 满量程对应输入电池电压，单位 mV；由 3.3V 和 9.09k/1k 分压计算。 */
#define BSP_ADC_MAX_RAW               (4095U)  /**< ADC 12 位最大原始值。 */

/**
 * @brief 读取按键 ADC 的原始转换值。
 * @param 无。
 * @note 启动一次 ADC_KEY 转换，读取配置的 ADCMEM0 结果。
 * @retval 返回 12/16 位 ADC 原始采样值，位宽取决于 SysConfig 配置。
 */
uint16_t BSP_ADC_KeyReadRaw(void);

/**
 * @brief 将电池 ADC 原始值换算为电池端电压。
 * @param raw ADC 原始采样值。
 * @note 按 R12=9.09k、R13=1k 分压计算，使用整数 mV，避免 MSPM0G3507 软件浮点开销。
 * @retval 电池端电压，单位 mV。
 */
uint16_t BSP_ADC_BatteryRawToMv(uint16_t raw);

/**
 * @brief 读取电池电压 ADC 原始值。
 * @param 无。
 * @note PA27 配置为 ADC0 Channel 0，对应 ADC_KEY sequence 的 ADCMEM1。
 * @retval 电池 ADC 原始采样值。
 */
uint16_t BSP_ADC_BatteryReadRaw(void);

/**
 * @brief 读取电池端电压。
 * @param 无。
 * @note 先读取 PA27 ADC 原始值，再按分压比例换算为 mV。
 * @retval 电池端电压，单位 mV。
 */
uint16_t BSP_ADC_BatteryReadMv(void);

#endif
