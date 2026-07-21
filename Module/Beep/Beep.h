#ifndef  __Beep_h
#define  __Beep_h /**< __Beep_h 头文件重复包含保护宏。 */

#include "ti_msp_dl_config.h"

/**
 * @brief 执行 Beep toggle 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Beep_toggle(void);
void Beep_set(bool state);

#endif

