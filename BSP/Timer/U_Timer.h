#ifndef __U_Timer_H
#define __U_Timer_H /**< __U_Timer_H 头文件重复包含保护宏。 */
#include "ti_msp_dl_config.h"
/**
 * @brief 初始化并启动 TIMER_1 定时器。
 * @param 无。
 * @note 中断号和定时器实例由 SysConfig 生成宏提供。
 * @retval 无。
 */
void TimeA1_Init(void);

#endif /* __MAIN_H */
