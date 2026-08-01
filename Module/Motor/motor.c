#include "motor.h"

static uint8_t motorPwmStarted;
static int8_t motorLeftDirection;
static int8_t motorRightDirection;

static int Motor_ClampSpeedLoopOutput(int output)
{
    if (output > 1999)
    {
        return 1999;
    }
    if (output < -1999)
    {
        return -1999;
    }
    return output;
}

/* 唯一接触电机 PWM 寄存器的底层函数，不对模块外公开。 */
static void Motor_WriteDriverOutput(int leftOutput, int rightOutput)
{
    int8_t leftDirection;
    int8_t rightDirection;

    leftOutput = Motor_ClampSpeedLoopOutput(leftOutput);
    rightOutput = Motor_ClampSpeedLoopOutput(rightOutput);
    leftDirection = (leftOutput > 0) ? 1 : ((leftOutput < 0) ? -1 : 0);
    rightDirection = (rightOutput > 0) ? 1 : ((rightOutput < 0) ? -1 : 0);

    if (leftDirection != motorLeftDirection)
    {
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U,
                                         DL_TIMER_CC_0_INDEX);
        if (leftDirection < 0)
        {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);
        }
        else if (leftDirection > 0)
        {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN);
        }
        else
        {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT,
                              GPIO_MOTOR_PIN_L1_PIN | GPIO_MOTOR_PIN_L2_PIN);
        }
        motorLeftDirection = leftDirection;
    }

    if (rightDirection != motorRightDirection)
    {
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U,
                                         DL_TIMER_CC_1_INDEX);
        if (rightDirection < 0)
        {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
        }
        else if (rightDirection > 0)
        {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
        }
        else
        {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT,
                              GPIO_MOTOR_PIN_R1_PIN | GPIO_MOTOR_PIN_R2_PIN);
        }
        motorRightDirection = rightDirection;
    }

    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST,
                                     (uint32_t)((leftOutput < 0) ? -leftOutput : leftOutput),
                                     DL_TIMER_CC_0_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST,
                                     (uint32_t)((rightOutput < 0) ? -rightOutput : rightOutput),
                                     DL_TIMER_CC_1_INDEX);

    if (motorPwmStarted == 0U)
    {
        DL_TimerA_enableClock(PWM_MOTOR_INST);
        DL_TimerA_startCounter(PWM_MOTOR_INST);
        motorPwmStarted = 1U;
    }
}

void Motor_ApplySpeedLoopOutput(int leftPidOutput, int rightPidOutput)
{
    Motor_WriteDriverOutput(leftPidOutput, rightPidOutput);
}

void Motor_Stop(void)
{
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U,
                                     DL_TIMER_CC_0_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U,
                                     DL_TIMER_CC_1_INDEX);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT,
                      GPIO_MOTOR_PIN_L1_PIN | GPIO_MOTOR_PIN_L2_PIN |
                          GPIO_MOTOR_PIN_R1_PIN | GPIO_MOTOR_PIN_R2_PIN);
    motorLeftDirection = 0;
    motorRightDirection = 0;
}
