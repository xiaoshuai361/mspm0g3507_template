#ifndef __motor_H__
#define __motor_H__ /**< __motor_H__ 头文件重复包含保护宏。 */

#include "ti_msp_dl_config.h"
//#define AIN1(x)   x?DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FL1_PIN):DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FL1_PIN)
//#define AIN2(x)   x?DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FL2_PIN):DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FL2_PIN)
//#define BIN1(x)   x?DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FR1_PIN):DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FR1_PIN)
//#define BIN2(x)   x?DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FR2_PIN):DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_FR2_PIN)
/**
 * @brief 设置左右电机 PWM 和方向。
 * @param PWMA 左电机 PWM，正负表示方向。
 * @param PWMB 右电机 PWM，正负表示方向。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Set_Speed(int PWMA,int PWMB);

#endif
