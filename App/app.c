#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdio.h>

#include "bsp_adc.h"
#include "bluetooth_command_test.h"
#include "delay.h"
#include "dl1a_test.h"
#include "Grayscale_Sensor.h"
#include "key5d_test.h"
#include "line_trace.h"
#include "line_trace_test.h"
#include "menu_test.h"
#include "Encoder.h"
#include "Encoder_XZ.h"
#include "oled.h"
#include "../Module/OrangePi/opipi.h"
#include "uart.h"

volatile uint32_t g_key5d_self_test_failures; /**< 五向按键自检失败项数量。 */
volatile uint32_t g_key5d_self_test_complete; /**< 五向按键自检已执行标志。 */
volatile uint32_t g_menu_self_test_failures; /**< 菜单模块自检失败项数量。 */
volatile uint32_t g_menu_self_test_complete; /**< 菜单模块自检已执行标志。 */
volatile uint32_t g_dl1a_self_test_failures; /**< DL1A 测距模块自检失败项数量。 */
volatile uint32_t g_dl1a_self_test_complete; /**< DL1A 测距模块自检已执行标志。 */
volatile uint32_t g_line_trace_self_test_failures; /**< 灰度循迹解码自检失败项数量。 */
volatile uint32_t g_line_trace_self_test_complete; /**< 灰度循迹解码自检已执行标志。 */
volatile uint32_t g_bt_command_self_test_failures; /**< 蓝牙车辆命令解析自检失败项数量。 */
volatile uint32_t g_bt_command_self_test_complete; /**< 蓝牙车辆命令解析自检已执行标志。 */

static uint32_t lastBatteryTick; /**< 上一次电池电压采样时间戳。 */

/**
 * @brief 输出上电软件自检结果。
 * @param 无。
 * @note 用于确认 Key5D、Menu、DL1A、循迹和蓝牙命令解析逻辑是否异常。
 * @retval 无。
 */
static void App_LogSelfTests(void)
{
    char message[96];

    /* 汇总各模块自检结果，方便模板工程上电后直接从 UART0 判断状态。 */
    (void)snprintf(message, sizeof(message),
                   "Self-test Key5D=%lu Menu=%lu DL1A=%lu Line=%lu BT=%lu\r\n",
                   (unsigned long)g_key5d_self_test_failures,
                   (unsigned long)g_menu_self_test_failures,
                   (unsigned long)g_dl1a_self_test_failures,
                   (unsigned long)g_line_trace_self_test_failures,
                   (unsigned long)g_bt_command_self_test_failures);
    uart0_send_string(message);
}

enum {
    APP_LINE_LAP_IDLE,
    APP_LINE_LAP_TRACKING,
    APP_LINE_LAP_STOPPING,
    APP_LINE_LAP_STOPPED
};

typedef enum {
    APP_LINE_STOP_IMMEDIATE,
    APP_LINE_STOP_ACTIVE_BRAKE,
    APP_LINE_STOP_SLOW_RAMP
} App_LineStopMode;

typedef struct {
    uint8_t state;
    uint8_t startKeyArmed;
    uint8_t stopOnDist;        /* 1=编码器距离停车(OLED Task 4) */
    uint32_t lastTick;
    uint32_t startTick;
    uint32_t stopStartTick;
    uint16_t lastReportedSec;
    uint16_t crossLockout;
    uint8_t crossConfirm;
    int32_t startEncL;         /* 启动时左编码器累计值 */
    int32_t startEncR;         /* 启动时右编码器累计值 */
    float stopTargetSpeed;
    LineTrace_Controller controller;
} App_LineLapContext;

#define TASK4_STOP_DIST_AB 12600  /* OLED Task 4：A→B 1.5m直线脉冲阈值 */

static App_LineLapContext task1LineContext = { .startKeyArmed = 1U };
static App_LineLapContext task3LineContext = {
    .startKeyArmed = 1U, .stopOnDist = 1U };
static App_LineLapContext task4LineContext = { .startKeyArmed = 1U };
static App_LineLapContext task5LineContext = { .startKeyArmed = 1U };
static uint8_t task5ResetPending = 1U;
static uint8_t task5OwnsInterface;
static uint8_t opiTaskActive;
static uint32_t taskActivationGeneration;

enum {
    TASK6_POSITION_MIN_TENTHS = -125,
    TASK6_POSITION_MAX_TENTHS = 125
};

static bool App_LineControlIsTimingCritical(void)
{
    const App_LineLapContext *context = NULL;

    switch (g_active_task) {
    case 1U: context = &task1LineContext; break;
    case 6U: context = &task1LineContext; break;
    case 3U: context = &task3LineContext; break;
    case 4U: context = &task4LineContext; break;
    case 5U: context = &task5LineContext; break;
    default: return false;
    }

    return (context->state == APP_LINE_LAP_TRACKING) ||
           (context->state == APP_LINE_LAP_STOPPING);
}

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

static void App_LineLapReset(App_LineLapContext *context)
{
    context->state = APP_LINE_LAP_IDLE;
    context->startKeyArmed = 1U;
    context->lastTick = 0U;
    context->startTick = 0U;
    context->stopStartTick = 0U;
    context->lastReportedSec = 0U;
    context->stopTargetSpeed = 0.0f;
    LineTrace_ControllerReset(&context->controller);
    LineTrace_ResetCrossDetect(&context->crossLockout,
                               &context->crossConfirm);
}

static void App_OPiLogStatus(uint8_t code, uint8_t expected)
{
    char message[32];

    (void)snprintf(message, sizeof(message),
                   "OPI RX=%02X WAIT=%02X\r\n",
                   (unsigned int)code, (unsigned int)expected);
    uart0_send_string(message);
}

static void App_OPiDrainRuntimeStatus(void)
{
    uint8_t code;

    while (OPi_ReadFrame(&code) != 0U) {
        /* AA01 已不参与启动门控，仅消费以免污染后续任务状态。 */
        if (code != OPI_STATUS_BOOT_READY) {
            App_OPiLogStatus(code, 0U);
        }
    }
}

static void App_OPiPollControlReady(uint8_t *controlReady)
{
    uint8_t code;

    while (OPi_ReadFrame(&code) != 0U) {
        if (code == OPI_STATUS_CONTROL_READY) {
            if (*controlReady == 0U) {
                *controlReady = 1U;
                App_MenuSetTaskControlReady(true);
                uart0_send_string("OPI: CONTROL READY\r\n");
            }
        } else if (code != OPI_STATUS_BOOT_READY) {
            App_OPiLogStatus(code, OPI_STATUS_CONTROL_READY);
        }
    }
}

static void App_OPiForwardRxToUart0(void)
{
    enum { OPI_FORWARD_BYTES_PER_RUN = 16U };
    static uint8_t pendingByte;
    static uint8_t pendingValid;
    uint8_t count;

    for (count = 0U; count < OPI_FORWARD_BYTES_PER_RUN; count++) {
        if (pendingValid == 0U) {
            if (OPi_ReadForwardByte(&pendingByte) == 0U) {
                break;
            }
            pendingValid = 1U;
        }

        if (uart0_bridge_write_nonblocking(&pendingByte, 1U) == 0U) {
            break;
        }
        pendingValid = 0U;
    }
}

/* AA01 不再作为开机条件，仅消费；其它残留状态记录后丢弃。 */
static void App_OPiDrainIdleStatus(void)
{
    uint8_t code;

    while (OPi_ReadFrame(&code) != 0U) {
        if (code != OPI_STATUS_BOOT_READY) {
            App_OPiLogStatus(code, 0U);
        }
    }
}

static void App_OPiStartTask(uint8_t taskCode)
{
    /* 丢弃 AA01 和陈旧任务状态后发送新任务。 */
    App_OPiDrainIdleStatus();
    OPi_FlushRx();
    App_MenuSetTaskControlReady(false);
    OPi_SendCmd(taskCode);
    opiTaskActive = 1U;
}

static void App_OPiStartTask6(int8_t positionTenthsCm)
{
    App_OPiDrainIdleStatus();
    OPi_FlushRx();
    App_MenuSetTaskControlReady(false);
    OPi_SendTask6(positionTenthsCm);
    opiTaskActive = 1U;
}

static void App_OPiAbortCurrentTask(void)
{
    App_MenuSetTaskControlReady(false);
    if (opiTaskActive != 0U) {
        OPi_SendCmd(OPI_CMD_ABORT);
        opiTaskActive = 0U;
    }
}

static void App_LineLapStart(App_LineLapContext *context,
                             uint8_t taskNumber)
{
    const uint32_t now = BSP_Delay_GetTick();
    char message[20];

    context->startKeyArmed = 0U;
    context->state = APP_LINE_LAP_TRACKING;
    context->lastTick = now;
    context->startTick = now;
    context->lastReportedSec = 0U;
    LineTrace_ControllerReset(&context->controller);
    LineTrace_ResetCrossDetect(&context->crossLockout,
                               &context->crossConfirm);
    context->crossLockout = APP_TASK2_CROSS_LOCKOUT_FRAMES;
    context->startEncL = Encoder_CumulativeL;
    context->startEncR = Encoder_CumulativeR;
    context->stopTargetSpeed = 0.0f;
    App_VehicleClosedLoopStop();
    App_MenuForceTimerPage();
    Menu_SetTaskTime(0U);
    (void)snprintf(message, sizeof(message), "T%u: START\r\n",
                   (unsigned int)taskNumber);
    uart0_send_string(message);
}

static void App_LineLapRun(App_LineLapContext *context,
                           uint8_t taskNumber, float cruiseSpeed,
                           App_LineStopMode stopMode,
                           uint16_t stopDurationMs,
                           float stopRampStep)
{
    const uint32_t now = BSP_Delay_GetTick();
    const Key5D_Key stableKey = App_InputGetStableKey();
    LineTrace_ControlConfig config = {
        .centerDeadband = APP_LINE_CENTER_DEADBAND,
        .steeringKp = App_VehicleGetLineKp(),
        .steeringKd = APP_LINE_FIXED_KD,
        .curveSlowdownGain = APP_LINE_CURVE_SLOWDOWN_GAIN,
        .minimumSpeedRatio = APP_LINE_MINIMUM_SPEED_RATIO,
        .correctionMax = APP_LINE_CORRECTION_MAX,
        .correctionSlewStep = APP_LINE_CORRECTION_SLEW_STEP,
    };
    float controlSpeed = cruiseSpeed;
    float measuredSpeed;
    uint8_t raw;
    uint8_t activeCount;
    uint8_t hasLine;
    int16_t errorTenths = 0;
    LineTrace_ControlOutput controlOutput;

    if (stableKey != KEY5D_KEY_RIGHT) {
        context->startKeyArmed = 1U;
    }
    if ((context->state == APP_LINE_LAP_STOPPING) &&
        (stopMode == APP_LINE_STOP_ACTIVE_BRAKE)) {
        const uint32_t elapsedMs = now - context->stopStartTick;

        measuredSpeed = (Motor1_Speed + Motor2_Speed) * 0.5f;
        if ((elapsedMs >= stopDurationMs) ||
            (measuredSpeed <= APP_TASK2_ACTIVE_BRAKE_STOP_SPEED)) {
            App_VehicleClosedLoopStop();
            context->state = APP_LINE_LAP_STOPPED;
            return;
        }
        controlSpeed = measuredSpeed;
    }

    if ((context->state != APP_LINE_LAP_TRACKING) &&
        (context->state != APP_LINE_LAP_STOPPING) &&
        (context->startKeyArmed != 0U) &&
        (stableKey == KEY5D_KEY_RIGHT)) {
        App_LineLapStart(context, taskNumber);
        return;
    }

    if ((context->state != APP_LINE_LAP_TRACKING) &&
        (context->state != APP_LINE_LAP_STOPPING)) {
        App_VehicleClosedLoopStop();
        return;
    }
    if ((uint32_t)(now - context->lastTick) <
        APP_LINE_CONTROL_PERIOD_MS) {
        return;
    }
    context->lastTick = now;

    /* 仅行驶阶段更新计时；进入停车状态的当拍即冻结成绩。 */
    if (context->state == APP_LINE_LAP_TRACKING) {
        uint16_t sec = (uint16_t)((now - context->startTick) / 1000U);
        if (sec != context->lastReportedSec) {
            context->lastReportedSec = sec;
            Menu_SetTaskTime(sec);
        }
    }

    Grayscale_Read();
    raw = Grayscale_GetRaw();
    hasLine = LineTrace_CalcActiveLowWeightedError(
        raw, &errorTenths, &activeCount);

    /* stopOnDist 任务（OLED Task 4）只由编码器阈值触发停车。 */
    if ((context->state == APP_LINE_LAP_TRACKING) &&
        (context->stopOnDist == 0U) &&
        (LineTrace_DetectCrossLine(raw, activeCount,
                                  &context->crossLockout,
                                  &context->crossConfirm)
        == CROSS_LINE_DETECTED)) {
        uint32_t elapsed = (uint32_t)(now - context->startTick);
        char message[40];

        /* Task 2：起步阶段编码器平均增量未达阈值前忽略横切线，防止起点误触发。 */
        if (taskNumber == 2U) {
            int32_t dL = Encoder_CumulativeL - context->startEncL;
            int32_t dR = Encoder_CumulativeR - context->startEncR;
            uint32_t avgEnc = (uint32_t)((dL >= 0 ? dL : -dL) +
                                         (dR >= 0 ? dR : -dR)) / 2U;
            if (avgEnc < (uint32_t)APP_TASK2_CROSS_MIN_ENC_AVG) {
                goto skip_cross;
            }
        }

        if (stopMode == APP_LINE_STOP_ACTIVE_BRAKE) {
            context->state = APP_LINE_LAP_STOPPING;
            context->stopStartTick = now;
            App_VehicleClosedLoopSetActiveBrake(true);
        } else if (stopMode == APP_LINE_STOP_SLOW_RAMP) {
            context->state = APP_LINE_LAP_STOPPING;
            context->stopStartTick = now;
            context->stopTargetSpeed =
                App_VehicleClosedLoopGetAverageTarget();
            if (context->stopTargetSpeed <= 0.0f) {
                context->stopTargetSpeed = cruiseSpeed;
            }
        } else {
            App_VehicleClosedLoopStop();
            context->state = APP_LINE_LAP_STOPPED;
        }
        (void)snprintf(message, sizeof(message),
                       "T%u: A %lu.%lus\r\n",
                       (unsigned int)taskNumber,
                       (unsigned long)(elapsed / 1000U),
                       (unsigned long)((elapsed % 1000U) / 100U));
        uart0_send_string(message);
        if (stopMode != APP_LINE_STOP_SLOW_RAMP) {
            return;
        }
    }
    skip_cross: (void)0;

    /* OLED Task 4 距离到达后进入与 Task 5/6 相同的缓停状态。 */
    if ((context->state == APP_LINE_LAP_TRACKING) &&
        (context->stopOnDist != 0U)) {
        int32_t dL = Encoder_CumulativeL - context->startEncL;
        int32_t dR = Encoder_CumulativeR - context->startEncR;
        uint32_t dist = (uint32_t)((dL >= 0 ? dL : -dL) + (dR >= 0 ? dR : -dR));
        if (dist >= TASK4_STOP_DIST_AB) {
            if (stopMode == APP_LINE_STOP_SLOW_RAMP) {
                context->state = APP_LINE_LAP_STOPPING;
                context->stopStartTick = now;
                context->stopTargetSpeed =
                    App_VehicleClosedLoopGetAverageTarget();
                if (context->stopTargetSpeed <= 0.0f) {
                    context->stopTargetSpeed = cruiseSpeed;
                }
            } else {
                App_VehicleClosedLoopStop();
                context->state = APP_LINE_LAP_STOPPED;
            }
            uart0_send_string("T4: DIST STOP\r\n");
            if (stopMode != APP_LINE_STOP_SLOW_RAMP) {
                return;
            }
        }
    }

    if ((context->state == APP_LINE_LAP_STOPPING) &&
        (stopMode == APP_LINE_STOP_SLOW_RAMP)) {
        context->stopTargetSpeed = App_MoveToward(
            context->stopTargetSpeed, 0.0f,
            stopRampStep);
        controlSpeed = context->stopTargetSpeed;
        measuredSpeed = (App_AbsFloat(Motor1_Speed) +
                         App_AbsFloat(Motor2_Speed)) * 0.5f;

        if (((controlSpeed == 0.0f) &&
             (measuredSpeed <= APP_TASK456_STOP_SPEED)) ||
            ((uint32_t)(now - context->stopStartTick) >=
             stopDurationMs)) {
            App_VehicleClosedLoopStop();
            context->state = APP_LINE_LAP_STOPPED;
            return;
        }
    }

    LineTrace_ControllerStep(&context->controller, &config, hasLine,
                             errorTenths, controlSpeed, &controlOutput);
    App_VehicleSetLineTelemetry(raw,
        (controlOutput.valid != 0U) ? controlOutput.filteredError :
                                     errorTenths,
        (controlOutput.valid != 0U) ? controlOutput.correction : 0.0f);

    if (controlOutput.valid != 0U) {
        App_VehicleClosedLoopSetTarget(controlOutput.leftTarget,
                                       controlOutput.rightTarget);
    } else if (context->state == APP_LINE_LAP_STOPPING) {
        App_VehicleClosedLoopSetTarget(controlSpeed, controlSpeed);
    }
}

static void App_Task5FormatTenths(char *buffer, uint32_t size,
                                  int16_t valueTenths)
{
    int16_t whole;
    int16_t fraction;
    char sign = '+';

    if (valueTenths < 0) {
        sign = '-';
        valueTenths = (int16_t)-valueTenths;
    }
    whole = (int16_t)(valueTenths / 10);
    fraction = (int16_t)(valueTenths % 10);

    (void)snprintf(buffer, size, "%c%d.%d", sign,
                   (int)whole, (int)fraction);
}

static void App_Task5RenderPsSelect(int16_t psTenths)
{
    char line[22];

    OLED_ClearBuffer();
    OLED_ShowString(0U, 0U, "TASK6 SET POS", 8U, 1U);
    App_Task5FormatTenths(line, sizeof(line), psTenths);
    OLED_ShowString(0U, 18U, "POS:", 16U, 1U);
    OLED_ShowString(32U, 18U, line, 16U, 1U);
    OLED_ShowString(0U, 42U, "ENC -12.0..+12.0", 8U, 1U);
    OLED_ShowString(0U, 54U, "R1 EN  R2 GO", 8U, 1U);
    OLED_Refresh();
}

static void App_Task5RenderStatus(const char *status)
{
    OLED_ClearBuffer();
    OLED_ShowString(0U, 0U, "TASK6", 16U, 1U);
    OLED_ShowString(0U, 24U, status, 8U, 1U);
    OLED_Refresh();
}

static void App_Task5UpdateReadyBlink(uint32_t now,
                                      uint32_t *lastTick,
                                      uint8_t *blinkVisible)
{
    enum { TASK6_READY_BLINK_HALF_PERIOD_MS = 400U };

    if ((uint32_t)(now - *lastTick) <
        TASK6_READY_BLINK_HALF_PERIOD_MS) {
        return;
    }

    *lastTick = now;
    *blinkVisible = (*blinkVisible == 0U) ? 1U : 0U;
    App_Task5RenderStatus((*blinkVisible != 0U)
                              ? "* READY  R=GO"
                              : "  READY  R=GO");
}

static void App_Task5ReturnToTaskList(void)
{
    Encoder_XZ_Disable();
    App_VehicleClosedLoopStop();
    task5OwnsInterface = 0U;
    App_LineLapReset(&task5LineContext);
    App_MenuReturnToTaskList();
}

/* OLED Task 2F/2L：第一次RIGHT确认任务，释放后第二次RIGHT直接发车。 */
void App_Task1Run(void)
{
    static uint32_t activationGeneration;
    const float referenceSpeed = (g_active_task == 6U)
                                     ? APP_TASK2L_LINE_SPEED
                                     : APP_TASK2F_LINE_SPEED;
    const uint16_t brakeDurationMs = (g_active_task == 6U)
                                         ? APP_TASK2L_ACTIVE_BRAKE_DURATION_MS
                                         : APP_TASK2F_ACTIVE_BRAKE_DURATION_MS;

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        task1LineContext.startKeyArmed = 0U;
        App_OPiStartTask(OPI_CMD_TASK2);
        uart0_send_string("OPI T2: START, NO WAIT\r\n");
    }

    if (App_InputGetStableKey() == KEY5D_KEY_LEFT) {
        App_OPiAbortCurrentTask();
        App_MenuReturnToTaskList();
        return;
    }

    /* 第一次RIGHT在菜单中确认任务；释放后第二次RIGHT直接发车，不等待AA11。 */
    App_OPiDrainRuntimeStatus();
    App_LineLapRun(&task1LineContext, 2U, referenceSpeed,
                   APP_LINE_STOP_ACTIVE_BRAKE, brakeDurationMs, 0.0f);
}

/* OLED Task 3: AA 03 -> AA 11 -> AA 07 -> AA 12. */
void App_Task2Run(void)
{
    enum {
        TASK3_WAIT_READY,
        TASK3_WAIT_RIGHT_RELEASE,
        TASK3_ACTION_ARMED,
        TASK3_WAIT_DONE,
        TASK3_DONE
    };
    static uint32_t activationGeneration;
    static uint32_t actionStartTick;
    static uint16_t lastTimerSeconds;
    static uint8_t state;
    const Key5D_Key stableKey = App_InputGetStableKey();
    const uint32_t now = BSP_Delay_GetTick();
    uint8_t code;

    App_VehicleClosedLoopStop();

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        state = TASK3_WAIT_READY;
        actionStartTick = 0U;
        lastTimerSeconds = 0U;
        App_OPiStartTask(OPI_CMD_TASK3);
        uart0_send_string("OPI T3: START\r\n");
    }

    if (stableKey == KEY5D_KEY_LEFT) {
        App_OPiAbortCurrentTask();
        App_MenuReturnToTaskList();
        return;
    }

    while (OPi_ReadFrame(&code) != 0U) {
        if (code == OPI_STATUS_BOOT_READY) {
            /* AA01 仅消费，不参与任务状态。 */
        } else if ((state == TASK3_WAIT_READY) &&
            (code == OPI_STATUS_CONTROL_READY)) {
            state = TASK3_WAIT_RIGHT_RELEASE;
            App_MenuSetTaskControlReady(true);
            uart0_send_string("OPI T3: READY\r\n");
        } else if ((state == TASK3_WAIT_DONE) &&
                   (code == OPI_STATUS_TASK3_DONE)) {
            lastTimerSeconds = (uint16_t)((now - actionStartTick) / 1000U);
            Menu_SetTaskTime(lastTimerSeconds);
            state = TASK3_DONE;
            uart0_send_string("OPI T3: DONE\r\n");
        } else {
            App_OPiLogStatus(code,
                (state == TASK3_WAIT_READY) ? OPI_STATUS_CONTROL_READY :
                (state == TASK3_WAIT_DONE) ? OPI_STATUS_TASK3_DONE : 0U);
        }
    }

    if (state == TASK3_WAIT_DONE) {
        const uint16_t seconds = (uint16_t)(
            (now - actionStartTick) / 1000U);

        if (seconds != lastTimerSeconds) {
            lastTimerSeconds = seconds;
            Menu_SetTaskTime(seconds);
        }
    }

    if ((state == TASK3_WAIT_RIGHT_RELEASE) &&
        (stableKey != KEY5D_KEY_RIGHT)) {
        state = TASK3_ACTION_ARMED;
    } else if ((state == TASK3_ACTION_ARMED) &&
               (stableKey == KEY5D_KEY_RIGHT)) {
        OPi_SendCmd(OPI_CMD_TASK3_ACTION);
        actionStartTick = now;
        lastTimerSeconds = 0U;
        Menu_SetTaskTime(0U);
        App_MenuForceTimerPage();
        state = TASK3_WAIT_DONE;
        uart0_send_string("OPI T3: ACTION\r\n");
    }
}

/* OLED Task 4: AA04发布后必须收到AA11；随后释放并再次按RIGHT发车。 */
void App_Task3Run(void)
{
    static uint32_t activationGeneration;
    static uint8_t controlReady;
    const Key5D_Key stableKey = App_InputGetStableKey();

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        controlReady = 0U;
        App_OPiStartTask(OPI_CMD_TASK4);
        task3LineContext.startKeyArmed = 0U;
        uart0_send_string("OPI T4: ENABLE, WAIT AA11\r\n");
    }
    if (stableKey == KEY5D_KEY_LEFT) {
        App_OPiAbortCurrentTask();
        App_MenuReturnToTaskList();
        return;
    }
    if (controlReady == 0U) {
        App_OPiPollControlReady(&controlReady);
    } else {
        App_OPiDrainRuntimeStatus();
    }
    if (controlReady == 0U) {
        App_VehicleClosedLoopStop();
        return;
    }
    App_LineLapRun(&task3LineContext, 4U, APP_TASK4_LINE_SPEED,
                   APP_LINE_STOP_SLOW_RAMP,
                   APP_TASK4_STOP_TIMEOUT_MS,
                   APP_TASK4_STOP_RAMP_STEP);
}

/* OLED Task 5: AA05发布后必须收到AA11；随后释放并再次按RIGHT发车。 */
void App_Task4Run(void)
{
    static uint32_t activationGeneration;
    static uint8_t controlReady;
    const Key5D_Key stableKey = App_InputGetStableKey();

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        controlReady = 0U;
        App_OPiStartTask(OPI_CMD_TASK5);
        task4LineContext.startKeyArmed = 0U;
        uart0_send_string("OPI T5: ENABLE, WAIT AA11\r\n");
    }
    if (stableKey == KEY5D_KEY_LEFT) {
        App_OPiAbortCurrentTask();
        App_MenuReturnToTaskList();
        return;
    }
    if (controlReady == 0U) {
        App_OPiPollControlReady(&controlReady);
    } else {
        App_OPiDrainRuntimeStatus();
    }
    if (controlReady == 0U) {
        App_VehicleClosedLoopStop();
        return;
    }
    App_LineLapRun(&task4LineContext, 5U, APP_TASK56_LINE_SPEED,
                   APP_LINE_STOP_SLOW_RAMP,
                   APP_TASK56_STOP_TIMEOUT_MS,
                   APP_TASK56_STOP_RAMP_STEP);
}

/* OLED Task 6: enter the position page with one menu press.  On that page,
 * the first RIGHT sends AA06+POS and the second RIGHT starts car motion. */
void App_Task5Run(void)
{
    enum {
        T6_SELECT_POSITION,
        T6_WAIT_CONTROL_READY,
        T6_WAIT_RUN_RELEASE,
        T6_READY_TO_RUN,
        T6_TRACK
    };
    static uint8_t state = T6_SELECT_POSITION;
    static uint8_t rightKeyArmed;
    static uint8_t controlReady;
    static uint8_t readyBlinkVisible;
    static int16_t psTenths;
    static int16_t lastDisplayPsTenths = 32767;
    static uint32_t lastDisplayTick;
    Key5D_Event keyEvent = KEY5D_EVENT_NONE;

    if (task5ResetPending != 0U) {
        task5ResetPending = 0U;
        state = T6_SELECT_POSITION;
        rightKeyArmed = 0U;
        controlReady = 0U;
        readyBlinkVisible = 0U;
        psTenths = 0;
        lastDisplayPsTenths = 32767;
        lastDisplayTick = 0U;
        Encoder_XZ_SetValue(0);
        Encoder_XZ_Enable();
        task5OwnsInterface = 1U;
        App_LineLapReset(&task5LineContext);
    }

    if ((task5OwnsInterface != 0U) &&
        App_InputPoll(BSP_Delay_GetTick(), &keyEvent) &&
        (keyEvent == KEY5D_EVENT_PRESSED)) {
        const Key5D_Key pressedKey = App_InputGetStableKey();

        App_LogKeyPress(pressedKey);
        if (pressedKey == KEY5D_KEY_LEFT) {
            App_Task5ReturnToTaskList();
            return;
        }
    }

    switch (state) {
    case T6_SELECT_POSITION: {
        const uint32_t now = BSP_Delay_GetTick();
        const Key5D_Key stableKey = App_InputGetStableKey();

        App_VehicleClosedLoopStop();
        psTenths = (int16_t)Encoder_XZ_GetClampedValue(
            TASK6_POSITION_MIN_TENTHS, TASK6_POSITION_MAX_TENTHS);

        if ((psTenths != lastDisplayPsTenths) ||
            ((uint32_t)(now - lastDisplayTick) >= 100U)) {
            lastDisplayPsTenths = psTenths;
            lastDisplayTick = now;
            App_Task5RenderPsSelect(psTenths);
        }

        if (stableKey != KEY5D_KEY_RIGHT) {
            rightKeyArmed = 1U;
        } else if (rightKeyArmed != 0U) {
            rightKeyArmed = 0U;
            Encoder_XZ_Disable();
            controlReady = 0U;
            App_OPiStartTask6((int8_t)psTenths);
            App_Task5RenderStatus("WAIT AA11");
            state = T6_WAIT_CONTROL_READY;
        }
        break; }
    case T6_WAIT_CONTROL_READY:
        App_VehicleClosedLoopStop();
        App_OPiPollControlReady(&controlReady);
        if (controlReady != 0U) {
            lastDisplayTick = BSP_Delay_GetTick() - 400U;
            readyBlinkVisible = 0U;
            App_Task5UpdateReadyBlink(BSP_Delay_GetTick(),
                                      &lastDisplayTick,
                                      &readyBlinkVisible);
            state = (App_InputGetStableKey() == KEY5D_KEY_RIGHT)
                        ? T6_WAIT_RUN_RELEASE
                        : T6_READY_TO_RUN;
        }
        break;
    case T6_WAIT_RUN_RELEASE:
        App_VehicleClosedLoopStop();
        App_OPiDrainRuntimeStatus();
        App_Task5UpdateReadyBlink(BSP_Delay_GetTick(),
                                  &lastDisplayTick,
                                  &readyBlinkVisible);
        if (App_InputGetStableKey() != KEY5D_KEY_RIGHT) {
            state = T6_READY_TO_RUN;
        }
        break;
    case T6_READY_TO_RUN:
        App_VehicleClosedLoopStop();
        App_OPiDrainRuntimeStatus();
        App_Task5UpdateReadyBlink(BSP_Delay_GetTick(),
                                  &lastDisplayTick,
                                  &readyBlinkVisible);
        if (App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
            App_LineLapStart(&task5LineContext, 6U);
            task5OwnsInterface = 0U;
            state = T6_TRACK;
        }
        break;
    case T6_TRACK:
        App_OPiDrainRuntimeStatus();
        App_LineLapRun(&task5LineContext, 6U, APP_TASK56_LINE_SPEED,
                       APP_LINE_STOP_SLOW_RAMP,
                       APP_TASK56_STOP_TIMEOUT_MS,
                       APP_TASK56_STOP_RAMP_STEP);
        break;
    default:
        state = T6_SELECT_POSITION;
        break;
    }
}

/*
 * ================================================================
 *  Task 分发
 * ================================================================
 */
void App_TasksRun(void)
{
    static uint8_t previousTask;

    if (g_active_task != previousTask) {
        taskActivationGeneration++;
        App_VehicleClosedLoopStop();
        if (previousTask == 5U) {
            Encoder_XZ_Disable();
            task5OwnsInterface = 0U;
        }
        /* Every OPI task remains enabled until the operator leaves it. */
        App_OPiAbortCurrentTask();
        App_LineLapReset(&task1LineContext);
        App_LineLapReset(&task3LineContext);
        App_LineLapReset(&task4LineContext);
        App_LineLapReset(&task5LineContext);
        if (g_active_task == 5U) {
            task5ResetPending = 1U;
        }

        /* Task 4/5(OLED Task 5/6)钢球任务使用独立速度PID参数。 */
        App_VehicleSetSpeedPidForTask56(
            (g_active_task == 4U) || (g_active_task == 5U));

        previousTask = g_active_task;
    }

    switch (g_active_task)
    {
    case 1U: App_Task1Run(); break;
    case 6U: App_Task1Run(); break;
    case 2U: App_Task2Run(); break;
    case 3U: App_Task3Run(); break;
    case 4U: App_Task4Run(); break;
    case 5U: App_Task5Run(); break;
    default: break;
    }
}

/**
 * @brief 运行电池电压检测任务。
 * @param 无。
 * @note 1Hz 读取 PA27/BAT_ADC，并把电压和低电量状态发布给一级菜单第四行。
 * @retval 无。
 */
static void App_BatteryRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    uint16_t batteryMv;
    bool batteryLow;

    if ((uint32_t)(now - lastBatteryTick) < APP_BATTERY_SAMPLE_PERIOD_MS)
    {
        return;
    }
    lastBatteryTick = now;

    batteryMv = BSP_ADC_BatteryReadMv();
    batteryLow = (batteryMv < APP_BATTERY_LOW_MV);
    App_MenuSetBatteryData(batteryMv, batteryLow);
}

/**
 * @brief 初始化 App 层公共资源。
 * @param 无。
 * @note 初始化输入、菜单、自检、UART0 和 OLED；车辆/IMU/ToF 在各自任务首次运行时初始化。
 * @retval 无。
 */
void App_Init(void)
{
    /* 先初始化不依赖外设输出的状态机，避免后续菜单显示读取到未定义状态。 */
    App_InputInit();
    App_MenuInitData();
    lastBatteryTick = BSP_Delay_GetTick() - APP_BATTERY_SAMPLE_PERIOD_MS;

    /* 软件自检只检查纯逻辑，硬件在线状态由各任务首次运行时再诊断。 */
    g_key5d_self_test_failures = Key5D_RunSelfTest();
    g_key5d_self_test_complete = 1U;
    g_menu_self_test_failures = Menu_RunSelfTest();
    g_menu_self_test_complete = 1U;
    g_dl1a_self_test_failures = DL1A_RunSelfTest();
    g_dl1a_self_test_complete = 1U;
    g_line_trace_self_test_failures = LineTrace_RunSelfTest();
    g_line_trace_self_test_complete = 1U;
    g_bt_command_self_test_failures = BluetoothCommand_RunSelfTest();
    g_bt_command_self_test_complete = 1U;
    App_ElectromagnetInit();
    Encoder_XZ_Disable();

    uart0_init();
    uart0_send_string("BOOT uart0 OK\r\n");
    OPi_Init();
    uart0_send_string("BOOT opi OK\r\n");
    uart0_send_string("BOOT oled init\r\n");
    OLED_Init();
    uart0_send_string("BOOT oled OK\r\n");
    App_LogSelfTests();
}

/**
 * @brief App 层主循环调度入口。
 * @param 无。
 * @note 在 main 的 while(1) 中重复调用；通过注释任务调用行控制模板功能开关。
 * @retval 无。
 */
void App_Run(void)
{
    static uint8_t runLoopLogged;
    const bool lineControlCritical = App_LineControlIsTimingCritical();
    const bool taskOwnsInterface = (task5OwnsInterface != 0U);

    if (runLoopLogged == 0U) {
        runLoopLogged = 1U;
        uart0_send_string("RUN loop OK\r\n");
    }

    App_OPiForwardRxToUart0();

    if (!lineControlCritical) {
        App_BatteryRun();
    }
    App_ElectromagnetRun();
    if (lineControlCritical) {
        /* 保留按键返回/急停；每1s刷新一次计时器 OLED（不刷新菜单其他页面）。 */
        App_MenuInputRun();
        {
            static uint16_t lastRenderedSec = 0xFFFFU;
            const uint16_t currentSec = Menu_GetTaskTime();
            if (currentSec != lastRenderedSec) {
                char line[17];
                lastRenderedSec = currentSec;
                OLED_ClearBuffer();
                (void)snprintf(line, sizeof(line), "Time:%us",
                               (unsigned int)currentSec);
                OLED_ShowString(0U, 0U, line, 16U, 1U);
                OLED_Refresh();
            }
        }
    } else if (!taskOwnsInterface) {
        App_MenuRun();
    }

    /* 编码器中断始终开启，停车时也继续累计脉冲。 */
    {
        static uint8_t encInited;
        if (encInited == 0U) { encInited = 1U; Encoder_Init(); }
    }

    /* 无任务时停车；有任务时由 Task 接管 */
    if (g_active_task == 0U) {
        App_OPiDrainIdleStatus();
        App_VehicleClosedLoopStop();
    }

    App_TasksRun();
    App_VehicleControlRun();
}
