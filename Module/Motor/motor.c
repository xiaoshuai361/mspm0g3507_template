#include "motor.h"
int pwmA,pwmB;
static uint8_t motorPwmStarted;
static int8_t motorLeftDirection;
static int8_t motorRightDirection;

/* 兼容旧调用，当前不需要额外初始化 */
void Motor_Init(void) {}

static int Motor_ClampPwm(int pwm)
{
    if (pwm > 1999) {
        return 1999;
    }
    if (pwm < -1999) {
        return -1999;
    }
    return pwm;
}

// 正转：L1=0 L2=1，R1=1 R2=0。
void Set_Speed(int PWMA, int PWMB)
{
    int8_t leftDirection;
    int8_t rightDirection;

    PWMA = Motor_ClampPwm(PWMA);
    PWMB = Motor_ClampPwm(PWMB);
    pwmA = PWMA;
    pwmB = PWMB;
    leftDirection = (PWMA > 0) ? 1 : ((PWMA < 0) ? -1 : 0);
    rightDirection = (PWMB > 0) ? 1 : ((PWMB < 0) ? -1 : 0);

    if (leftDirection != motorLeftDirection) {
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U,
                                         DL_TIMER_CC_0_INDEX);
        if (leftDirection < 0) {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);
        } else if (leftDirection > 0) {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN);
        } else {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT,
                GPIO_MOTOR_PIN_L1_PIN | GPIO_MOTOR_PIN_L2_PIN);
        }
        motorLeftDirection = leftDirection;
    }

    if (rightDirection != motorRightDirection) {
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0U,
                                         DL_TIMER_CC_1_INDEX);
        if (rightDirection < 0) {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
        } else if (rightDirection > 0) {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
            DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
        } else {
            DL_GPIO_clearPins(GPIO_MOTOR_PORT,
                GPIO_MOTOR_PIN_R1_PIN | GPIO_MOTOR_PIN_R2_PIN);
        }
        motorRightDirection = rightDirection;
    }

    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST,
        (uint32_t)((PWMA < 0) ? -PWMA : PWMA), DL_TIMER_CC_0_INDEX);
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST,
        (uint32_t)((PWMB < 0) ? -PWMB : PWMB), DL_TIMER_CC_1_INDEX);

    if (motorPwmStarted == 0U) {
        DL_TimerA_enableClock(PWM_MOTOR_INST);
        DL_TimerA_startCounter(PWM_MOTOR_INST);
        motorPwmStarted = 1U;
    }
}
