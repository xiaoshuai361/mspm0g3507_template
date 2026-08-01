#ifndef MODULE_MOTOR_MOTOR_H
#define MODULE_MOTOR_MOTOR_H

#include "ti_msp_dl_config.h"

/** 将左右速度 PID 输出施加到电机驱动。 */
void Motor_ApplySpeedLoopOutput(int leftPidOutput, int rightPidOutput);

/** 立即把两路电机输出置零。 */
void Motor_Stop(void);

#endif
