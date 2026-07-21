#include "Beep.h"

/**
 * @brief 翻转蜂鸣器输出状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Beep_toggle(void)
{
	DL_GPIO_togglePins(Beep_PORT,Beep_PIN_PIN);
}

void Beep_set(bool state)
{
	state ? DL_GPIO_setPins(Beep_PORT,Beep_PIN_PIN)			//1
          : DL_GPIO_clearPins(Beep_PORT,Beep_PIN_PIN);		//0
}

