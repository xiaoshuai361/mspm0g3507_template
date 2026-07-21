#include "motor.h"
int pwmA,pwmB;


//正转的时候：L ： L1=0 L2=1  R1=1 R2=0
void Set_Speed(int PWMA,int PWMB)//-2000 ~ 2000   
{
    pwmA = PWMA;
    pwmB = PWMB;
    DL_TimerA_disableClock(PWM_MOTOR_INST);
    uint32_t compareValue = 0;
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN);
	DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
    DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
    if(PWMA < 0)
    {
        compareValue = -PWMA;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, compareValue, DL_TIMER_CC_0_INDEX);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN); 

    }
    else if(PWMA > 0)
    {
        compareValue = PWMA;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, compareValue, DL_TIMER_CC_0_INDEX);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);

    }
    else 
    {
		compareValue = 0;
		DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, compareValue, DL_TIMER_CC_0_INDEX);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_L2_PIN);
    }

    if(PWMB < 0)
    {
        compareValue = -PWMB;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, compareValue, DL_TIMER_CC_1_INDEX);  
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
    }
    else if(PWMB > 0)
    {
        compareValue = PWMB;
        DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, compareValue, DL_TIMER_CC_1_INDEX);
        DL_GPIO_setPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
    }
    else 
    {
		compareValue = 0;
		DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, compareValue, DL_TIMER_CC_1_INDEX);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R1_PIN);
        DL_GPIO_clearPins(GPIO_MOTOR_PORT, GPIO_MOTOR_PIN_R2_PIN);
    }
	DL_TimerA_enableClock(PWM_MOTOR_INST);
	DL_TimerA_startCounter(PWM_MOTOR_INST);
}
