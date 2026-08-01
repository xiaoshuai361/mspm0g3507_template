#include "app.h"
#include "app_internal.h"

#include <stdint.h>

#include "Encoder.h"
#include "Grayscale_Sensor.h"
#include "delay.h"
#include "line_trace.h"
#include "oled.h"
#include "uart.h"

typedef enum {
    TASK1_RAMP_TO_SPEED = 0,
    TASK1_HOLD_SPEED,
    TASK1_RAMP_TO_ZERO,
    TASK1_ZERO_DWELL
} App_Task1Phase;

typedef struct {
    App_Task1Phase phase;
    int8_t direction;
    float currentTarget;
    uint32_t lastStepTick;
    uint32_t phaseStartTick;
} App_Task1Context;

static App_Task1Context task1Context;
static LineTrace_Controller task2Controller;
static uint32_t task2LastTick;

static float App_MoveToward(float current, float target, float step)
{
    if (current < target) {
        current += step;
        return (current > target) ? target : current;
    }
    if (current > target) {
        current -= step;
        return (current < target) ? target : current;
    }
    return current;
}

static void App_Task1Reset(uint32_t now)
{
    task1Context.phase = TASK1_RAMP_TO_SPEED;
    task1Context.direction = 1;
    task1Context.currentTarget = 0.0f;
    task1Context.lastStepTick = now;
    task1Context.phaseStartTick = now;
}

static void App_Task2Reset(uint32_t now)
{
    LineTrace_ControllerReset(&task2Controller);
    task2LastTick = now;
    App_VehicleSetLineTelemetry(0xFFU, 0, 0.0f);
}

void App_Task1Run(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    const float configuredSpeed = App_VehicleGetCommandSpeed();
    float desiredTarget;

    if (App_InputGetStableKey() == KEY5D_KEY_LEFT) {
        App_VehicleClosedLoopStop();
        App_MenuReturnToTaskList();
        return;
    }
    if ((uint32_t)(now - task1Context.lastStepTick) <
        APP_CONTROL_PERIOD_MS) {
        return;
    }
    task1Context.lastStepTick += APP_CONTROL_PERIOD_MS;

    switch (task1Context.phase) {
    case TASK1_RAMP_TO_SPEED:
        desiredTarget = (float)task1Context.direction * configuredSpeed;
        task1Context.currentTarget = App_MoveToward(
            task1Context.currentTarget, desiredTarget, APP_TASK1_RAMP_STEP);
        App_VehicleClosedLoopSetTarget(task1Context.currentTarget,
                                       task1Context.currentTarget);
        if (task1Context.currentTarget == desiredTarget) {
            task1Context.phase = TASK1_HOLD_SPEED;
            task1Context.phaseStartTick = now;
        }
        break;

    case TASK1_HOLD_SPEED:
        desiredTarget = (float)task1Context.direction * configuredSpeed;
        task1Context.currentTarget = App_MoveToward(
            task1Context.currentTarget, desiredTarget, APP_TASK1_RAMP_STEP);
        App_VehicleClosedLoopSetTarget(task1Context.currentTarget,
                                       task1Context.currentTarget);
        if ((uint32_t)(now - task1Context.phaseStartTick) >=
            APP_TASK1_HOLD_TIME_MS) {
            task1Context.phase = TASK1_RAMP_TO_ZERO;
        }
        break;

    case TASK1_RAMP_TO_ZERO:
        task1Context.currentTarget = App_MoveToward(
            task1Context.currentTarget, 0.0f, APP_TASK1_RAMP_STEP);
        App_VehicleClosedLoopSetTarget(task1Context.currentTarget,
                                       task1Context.currentTarget);
        if (task1Context.currentTarget == 0.0f) {
            task1Context.phase = TASK1_ZERO_DWELL;
            task1Context.phaseStartTick = now;
        }
        break;

    case TASK1_ZERO_DWELL:
        /* 保持零速闭环一小段时间，让车辆真正停稳后再换向。 */
        App_VehicleClosedLoopSetTarget(0.0f, 0.0f);
        if ((uint32_t)(now - task1Context.phaseStartTick) >=
            APP_TASK1_ZERO_DWELL_MS) {
            App_VehicleClosedLoopStop();
            task1Context.direction = (int8_t)-task1Context.direction;
            task1Context.phase = TASK1_RAMP_TO_SPEED;
        }
        break;

    default:
        App_Task1Reset(now);
        break;
    }
}

void App_Task2Run(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    LineTrace_ControlConfig config = {
        .fastErrorThreshold = APP_LINE_FAST_ERROR,
        .fastDeltaThreshold = APP_LINE_FAST_DELTA,
        .centerDeadband = APP_LINE_CENTER_DEADBAND,
        .steeringKp = App_VehicleGetLineKp(),
        .steeringKd = APP_LINE_FIXED_KD,
        .curveSlowdownGain = APP_LINE_CURVE_SLOWDOWN_GAIN,
        .minimumSpeedRatio = APP_LINE_MINIMUM_SPEED_RATIO,
    };
    LineTrace_ControlOutput output;
    int16_t error = 0;
    uint8_t activeCount = 0U;
    uint8_t hasLine;
    uint8_t raw;

    if (App_InputGetStableKey() == KEY5D_KEY_LEFT) {
        App_VehicleClosedLoopStop();
        App_MenuReturnToTaskList();
        return;
    }
    if ((uint32_t)(now - task2LastTick) < APP_LINE_CONTROL_PERIOD_MS) {
        return;
    }
    task2LastTick += APP_LINE_CONTROL_PERIOD_MS;

    Grayscale_Read();
    raw = Grayscale_GetRaw();
    hasLine = LineTrace_CalcActiveLowWeightedError(raw, &error,
                                                   &activeCount);
    (void)activeCount;
    LineTrace_ControllerStep(&task2Controller, &config, hasLine, error,
                             App_VehicleGetCommandSpeed(), &output);

    App_VehicleSetLineTelemetry(raw,
        (output.valid != 0U) ? output.filteredError : error,
        (output.valid != 0U) ? output.correction : 0.0f);

    /* 无停车线、无丢线停车；丢线时完全保持上一拍左右轮目标。 */
    if (output.valid != 0U) {
        App_VehicleClosedLoopSetTarget(output.leftTarget,
                                       output.rightTarget);
    }
}

void App_TasksRun(void)
{
    static uint8_t previousTask;
    const uint32_t now = BSP_Delay_GetTick();

    if (g_active_task != previousTask) {
        App_VehicleClosedLoopStop();
        App_Task1Reset(now);
        App_Task2Reset(now);
        previousTask = g_active_task;
    }

    switch (g_active_task) {
    case 1U:
        App_Task1Run();
        break;
    case 2U:
        App_Task2Run();
        break;
    default:
        break;
    }
}

void App_Init(void)
{
    App_InputInit();
    App_MenuInitData();
    uart0_init();
    OLED_Init();
    App_VehicleInit();
    App_Task1Reset(BSP_Delay_GetTick());
    App_Task2Reset(BSP_Delay_GetTick());
}

void App_Run(void)
{
    if (g_active_task == 0U) {
        App_MenuRun();
    } else {
        /* 任务运行时只扫按键，不做 OLED 全屏刷新，保证 20 ms 控制周期。 */
        App_MenuInputRun();
    }

    App_TasksRun();
    App_VehicleRun();
}
