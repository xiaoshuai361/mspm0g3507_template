#include "U_Timer.h"


/**
 * @brief 初始化并启动 TIMER_1 定时器。
 * @param 无。
 * @note 清除挂起中断后使能 NVIC，并启动 TimerA 计数器。
 * @retval 无。
 */
void TimeA1_Init(void)
{
	/* 先清除可能残留的挂起位，再打开中断。 */
	NVIC_ClearPendingIRQ(TIMER_1_INST_INT_IRQN);
	NVIC_EnableIRQ(TIMER_1_INST_INT_IRQN);
	DL_TimerA_startCounter(TIMER_1_INST);
	
}

