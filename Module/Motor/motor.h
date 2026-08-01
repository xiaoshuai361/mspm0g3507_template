#ifndef MODULE_MOTOR_MOTOR_H
#define MODULE_MOTOR_MOTOR_H

#include "ti_msp_dl_config.h"

/**
 * @brief 将左右速度 PID 的输出施加到电机驱动。
 * @note 该接口只允许由闭环速度控制器调用，不提供开环 PWM 控制入口。
 */
void Motor_ApplySpeedLoopOutput(int leftPidOutput, int rightPidOutput);

/** @brief 立即把两路电机输出置零，作为初始化和安全停机出口。 */
void Motor_Stop(void);

#endif
