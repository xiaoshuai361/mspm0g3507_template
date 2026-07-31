#ifndef __ENCODER_XZ_H__
#define __ENCODER_XZ_H__ /**< __ENCODER_XZ_H__ 头文件重复包含保护宏。 */

#include "stdio.h"
#include "ti_msp_dl_config.h"

extern volatile int32_t Encoder_XZ_Value; /**< Encoder_XZ_Value 全局状态或配置变量。 */

/**
 * @brief 执行 Encoder  X Z  Disable 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Encoder_XZ_Disable();
void Encoder_XZ_Enable();

void Encoder_XZ_Set(bool state);
int32_t Encoder_XZ_GetValue(void);
void Encoder_XZ_SetValue(int32_t value);
int32_t Encoder_XZ_GetClampedValue(int32_t minValue, int32_t maxValue);
/**
 * @brief 执行 Encoder  X Z toggle 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Encoder_XZ_toggle();
#endif
