#include "delay.h"


static volatile unsigned int delay_times; /**< 毫秒阻塞延时的剩余计数。 */
static volatile uint32_t millisecond_tick; /**< millisecond_tick 全局状态或配置变量。 */
/**
 * @brief 毫秒级阻塞延时。
 * @param ms 需要延时的毫秒数。
 * @note 依赖 SysTick_Handler() 周期递减 delay_times。
 * @retval 无。
 */
void delay_ms(unsigned int ms)
{
    delay_times = ms;
    /* 等待 SysTick 中断把计数递减到 0。 */
    while(delay_times!=0);
}

/**
 * @brief SysTick 中断服务函数。
 * @param 无。
 * @note 同时维护系统毫秒计数和阻塞延时计数。
 * @retval 无。
 */
void SysTick_Handler()
{
    millisecond_tick++;
    if(delay_times!=0)
    {
        delay_times--;
    }
}
/**
 * @brief 获取系统毫秒节拍。
 * @param 无。
 * @note 节拍值在 SysTick_Handler() 中累加。
 * @retval 返回启动后的毫秒计数。
 */
uint32_t BSP_Delay_GetTick(void)
{
    return millisecond_tick;
}
/**
 * @brief 微秒级短延时。
 * @param __us 需要延时的微秒数。
 * @note 基于 CPUCLK_FREQ 计算空转周期，适合短延时。
 * @retval 无。
 */
void delay_us(int __us) { delay_cycles( (CPUCLK_FREQ / 1000 / 1000)*__us); }


/**
 * @brief 微秒级短延时别名。
 * @param __us 需要延时的微秒数。
 * @note 与 delay_us() 使用相同的 cycle 延时实现。
 * @retval 无。
 */
void delay_1us(int __us) { delay_cycles( (CPUCLK_FREQ / 1000 / 1000)*__us); }
/**
 * @brief 毫秒级短延时。
 * @param __ms 需要延时的毫秒数。
 * @note 基于 CPUCLK_FREQ 直接空转，不依赖 SysTick。
 * @retval 无。
 */
void delay_1ms(int __ms) { delay_cycles( (CPUCLK_FREQ / 1000)*__ms); }
