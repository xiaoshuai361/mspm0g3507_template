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

typedef enum {
    TASK2_TRACKING = 0,
    TASK2_STOPPING,
    TASK2_STOPPED
} App_Task2Phase;

typedef struct {
    App_Task2Phase phase;
    LineTrace_Controller controller;
    float stopTargetSpeed;
    uint32_t lastTick;
    uint32_t stopStartTick;
    uint16_t crossLockout;
    uint8_t crossConfirm;
    int32_t startEncL;
    int32_t startEncR;
} App_Task2Context;

static App_Task1Context task1Context;
static App_Task2Context task2Context;

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

static float App_AbsFloat(float value)
{
    return (value >= 0.0f) ? value : -value;
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
    task2Context.phase = TASK2_TRACKING;
    LineTrace_ControllerReset(&task2Context.controller);
    LineTrace_ResetCrossDetect(&task2Context.crossLockout,
                               &task2Context.crossConfirm);
    task2Context.crossLockout = APP_TASK2_CROSS_LOCKOUT_FRAMES;
    task2Context.stopTargetSpeed = 0.0f;
    task2Context.lastTick = now;
    task2Context.stopStartTick = now;
    task2Context.startEncL = Encoder_CumulativeL;
    task2Context.startEncR = Encoder_CumulativeR;
    App_VehicleSetLineTelemetry(0xFFU, 0, 0.0f);
}

static uint32_t App_Task2AverageEncoderDelta(void)
{
    int32_t leftDelta = Encoder_CumulativeL - task2Context.startEncL;
    int32_t rightDelta = Encoder_CumulativeR - task2Context.startEncR;

    if (leftDelta < 0) {
        leftDelta = -leftDelta;
    }
    if (rightDelta < 0) {
        rightDelta = -rightDelta;
    }
    return ((uint32_t)leftDelta + (uint32_t)rightDelta) / 2U;
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
    float controlSpeed = App_VehicleGetCommandSpeed();
    float measuredSpeed;
    LineTrace_ControlConfig config = {
        .centerDeadband = APP_LINE_CENTER_DEADBAND,
        .steeringKp = App_VehicleGetLineKp(),
        .steeringKd = APP_LINE_FIXED_KD,
        .curveSlowdownGain = APP_LINE_CURVE_SLOWDOWN_GAIN,
        .minimumSpeedRatio = APP_LINE_MINIMUM_SPEED_RATIO,
        .correctionMax = APP_LINE_CORRECTION_MAX,
        .correctionSlewStep = APP_LINE_CORRECTION_SLEW_STEP,
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
    if ((uint32_t)(now - task2Context.lastTick) <
        APP_LINE_CONTROL_PERIOD_MS) {
        return;
    }
    task2Context.lastTick += APP_LINE_CONTROL_PERIOD_MS;

    if (task2Context.phase == TASK2_STOPPED) {
        App_VehicleClosedLoopStop();
        return;
    }

    Grayscale_Read();
    raw = Grayscale_GetRaw();
    hasLine = LineTrace_CalcActiveLowWeightedError(raw, &error,
                                                   &activeCount);

    if ((task2Context.phase == TASK2_TRACKING) &&
        (LineTrace_DetectCrossLine(raw, activeCount,
                                   &task2Context.crossLockout,
                                   &task2Context.crossConfirm) ==
         CROSS_LINE_DETECTED) &&
        (App_Task2AverageEncoderDelta() >=
         APP_TASK2_CROSS_MIN_ENC_AVG)) {
        task2Context.phase = TASK2_STOPPING;
        task2Context.stopStartTick = now;
        task2Context.stopTargetSpeed =
            App_VehicleClosedLoopGetAverageTarget();
        if (task2Context.stopTargetSpeed <= 0.0f) {
            task2Context.stopTargetSpeed = App_VehicleGetCommandSpeed();
        }
    }

    if (task2Context.phase == TASK2_STOPPING) {
        task2Context.stopTargetSpeed = App_MoveToward(
            task2Context.stopTargetSpeed, 0.0f,
            APP_TASK2_STOP_RAMP_STEP);
        controlSpeed = task2Context.stopTargetSpeed;
        measuredSpeed = (App_AbsFloat(Motor1_Speed) +
                         App_AbsFloat(Motor2_Speed)) * 0.5f;

        if (((controlSpeed == 0.0f) &&
             (measuredSpeed <= APP_TASK2_STOP_SPEED)) ||
            ((uint32_t)(now - task2Context.stopStartTick) >=
             APP_TASK2_STOP_TIMEOUT_MS)) {
            App_VehicleClosedLoopStop();
            task2Context.phase = TASK2_STOPPED;
            return;
        }
    }

    LineTrace_ControllerStep(&task2Context.controller, &config, hasLine,
                             error, controlSpeed, &output);

    App_VehicleSetLineTelemetry(raw,
        (output.valid != 0U) ? output.filteredError : error,
        (output.valid != 0U) ? output.correction : 0.0f);

    /* 停车阶段仍按灰度差速巡线，只逐拍降低平均目标速度。 */
    if (output.valid != 0U) {
        App_VehicleClosedLoopSetTarget(output.leftTarget,
                                       output.rightTarget);
    } else if (task2Context.phase == TASK2_STOPPING) {
        /* 丢线时不再加速，仍按停车斜坡降到0。 */
        App_VehicleClosedLoopSetTarget(controlSpeed, controlSpeed);
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
