#ifndef BSP_DELAY_H
#define BSP_DELAY_H /**< BSP_DELAY_H 底层驱动配置/状态宏。 */
#include "ti_msp_dl_config.h"

#ifndef u8
#define u8 uint8_t /**< u8 基础整数类型别名。 */
#endif

#ifndef u16
#define u16 uint16_t /**< u16 基础整数类型别名。 */
#endif

#ifndef u32
#define u32 uint32_t /**< u32 基础整数类型别名。 */
#endif

#ifndef u64
#define u64 uint64_t /**< u64 基础整数类型别名。 */
#endif
/**
 * @brief 毫秒级阻塞延时。
 * @param ms 需要延时的毫秒数。
 * @note 依赖 SysTick 中断周期递减内部计数。
 * @retval 无。
 */
void delay_ms(unsigned int ms);
/**
 * @brief 微秒级短延时。
 * @param __us 需要延时的微秒数。
 * @note 基于 CPUCLK_FREQ 计算空转周期，适合短延时。
 * @retval 无。
 */
void delay_us(int __us);
/**
 * @brief 微秒级短延时别名。
 * @param __us 需要延时的微秒数。
 * @note 与 delay_us() 使用相同实现。
 * @retval 无。
 */
void delay_1us(int __us);
/**
 * @brief 毫秒级短延时。
 * @param __ms 需要延时的毫秒数。
 * @note 基于 CPUCLK_FREQ 直接空转，不依赖 SysTick。
 * @retval 无。
 */
void delay_1ms(int __ms);
/**
 * @brief 获取系统毫秒节拍。
 * @param 无。
 * @note 节拍值在 SysTick_Handler() 中累加。
 * @retval 返回启动后的毫秒计数。
 */
uint32_t BSP_Delay_GetTick(void);

#endif
