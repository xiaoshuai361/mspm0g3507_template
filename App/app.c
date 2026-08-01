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
#include "motor.h"
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
    APP_LINE_LAP_BRAKING,
    APP_LINE_LAP_STOPPED
};

enum {
    APP_LINE_CONTROL_PERIOD_MS = 10U,
    APP_LINE_CROSS_LOCKOUT_FRAMES = 80U
};

typedef struct {
    uint8_t state;
    uint8_t startKeyArmed;
    uint8_t stopOnDist;        /* 1=编码器距离停车(OLED Task 4) */
    uint8_t brakePending;      /* 1=已检测停车线，等待到达设定制动位置 */
    uint32_t lastTick;
    uint32_t startTick;
    uint32_t brakeStartTick;
    uint16_t crossLockout;
    uint8_t crossConfirm;
    int32_t startEncL;         /* 启动时左编码器累计值 */
    int32_t startEncR;         /* 启动时右编码器累计值 */
    int32_t brakeDetectEncL;   /* 检测到停车线时的左编码器累计值 */
    int32_t brakeDetectEncR;   /* 检测到停车线时的右编码器累计值 */
    float lastLeftTarget;
    float lastRightTarget;
    float brakeStartLeftTarget;
    float brakeStartRightTarget;
    LineTrace_OuterFilter outerFilter;
    LineTrace_Controller controller;
} App_LineLapContext;

#define TASK4_STOP_DIST_AB 12600  /* OLED Task 4：A→B 1.5m直线脉冲阈值 */

/*
 * OLED Task 2F/2L使用单一线性纠偏增益，避免偏差跨阈值时差速突变。
 * 半圆转向不足先增 steeringKp，最后才增 steeringMax。
 * 所有“帧”均为10ms，误差单位为0.1个灰度探头间距。
 */
static const LineTrace_ControlConfig task1FastControlConfig = {
    .cruisePwm = APP_TASK1_LINE_CRUISE_PWM, /* 直线巡航PWM，在app_config.h中修改。 */
    .lostPwm = 700,              /* 弯道重度降速/丢线搜索下限；增大可补偿配重，但更难找回线。 */
    .rampUpStep = 20,            /* 每帧基准PWM上升量；增大则起步和出弯加速更快。 */
    .rampDownStep = 20,          /* 限制每帧降速，避免短时抖动造成骤降。 */
    .curveSlowdownGain = 8,      /* 每单位绝对偏差扣除的PWM；增大可降低弯速。 */
    .slowdownEntryError = 10,    /* 小偏差只纠偏，不降低巡航基准。 */
    .steeringKp = 16,            /* 全偏差范围线性增益；避免外侧差速跨阈值突变。 */
    .steeringMax = 480,          /* 放宽高速档差速修正硬上限。 */
    .steeringSlewStep = 60,      /* 每10ms差速最多变化60，避免偏差突变时瞬间反打。 */
    .leftPwmBias = 50,           /* 高速直行稳定输出：左1150、右1100。 */
    .rightTurnBoost = 115,       /* 两个右半圆额外提升左侧重载外轮扭矩。 */
    .centerDeadband = 5,         /* 中心死区；增大更稳但纠偏变迟，减小更灵敏但易抖。 */
    .fastErrorThreshold = 15,    /* 原始偏差达到此值使用3/4新值快滤波；减小响应更快。 */
    .fastDeltaThreshold = 8,    /* 相邻帧偏差跳变量阈值；减小可更快响应突然换边。 */
    .lostSearchStartError = 18,  /* 丢线搜索的初始等效偏差；增大则首次找线转向更强。 */
    .lostSearchStepFrames = 2U,  /* 搜索偏差每隔多少帧加1；减小会更快增强搜索。 */
    .lostHoldFrames = 5U,        /* 丢线后保持最后纠偏的帧数；当前1帧即10ms。 */
    .slowdownConfirmFrames = 3U, /* 偏差持续30ms才开始降速，滤除瞬时抖动。 */
    .lostStopFrames = 1500U,     /* 连续丢线停车时间；当前150帧即1.5s。 */
};

static const LineTrace_ControlConfig task2StableControlConfig = {
    .cruisePwm = APP_STABLE_LINE_CRUISE_PWM,
    .lostPwm = 584,
    .rampUpStep = 19,
    .rampDownStep = 11,
    .curveSlowdownGain = 1,
    .slowdownEntryError = 8,
    .steeringKp = 12,
    .steeringMax = 240,
    .steeringSlewStep = 45,
    .leftPwmBias = 39,
    .rightTurnBoost = 70,
    .centerDeadband = 5,
    .fastErrorThreshold = 15,
    .fastDeltaThreshold = 12,
    .lostSearchStartError = 16,
    .lostSearchStepFrames = 3U,
    .lostHoldFrames = 2U,
    .slowdownConfirmFrames = 2U,
    .lostStopFrames = 150U,
};

/* Task 5/6 钢球稳定任务：纠偏参数向快速版靠拢，保留缓慢起步防止球滚落。 */
static const LineTrace_ControlConfig task56ControlConfig = {
    .cruisePwm = APP_STABLE_LINE_CRUISE_PWM,
    .lostPwm = 650,
    .rampUpStep = 20,            /* 恢复缓慢起步，钢球任务避免急加速球滚落。 */
    .rampDownStep = 20,
    .curveSlowdownGain = 3,
    .slowdownEntryError = 9,
    .steeringKp = 15,            /* 逼近快速版16，提升小偏差响应。 */
    .steeringMax = 400,          /* 逼近快速版480，弯道差速不再受限。 */
    .steeringSlewStep = 55,      /* 逼近快速版60，差速变化更快。 */
    .leftPwmBias = 45,
    .rightTurnBoost = 100,       /* 逼近快速版115，右弯外轮扭矩补偿。 */
    .centerDeadband = 5,
    .fastErrorThreshold = 15,
    .fastDeltaThreshold = 10,    /* 逼近快速版8，突变响应更快。 */
    .lostSearchStartError = 17,
    .lostSearchStepFrames = 2U,  /* 同快速版，丢线搜索加速。 */
    .lostHoldFrames = 3U,
    .slowdownConfirmFrames = 2U,
    .lostStopFrames = 150U,
};

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
           (context->state == APP_LINE_LAP_BRAKING);
}

static void App_LineLapReset(App_LineLapContext *context)
{
    context->state = APP_LINE_LAP_IDLE;
    context->startKeyArmed = 1U;
    context->lastTick = 0U;
    context->startTick = 0U;
    context->brakeStartTick = 0U;
    context->brakePending = 0U;
    context->brakeDetectEncL = 0;
    context->brakeDetectEncR = 0;
    context->lastLeftTarget = 0.0f;
    context->lastRightTarget = 0.0f;
    context->brakeStartLeftTarget = 0.0f;
    context->brakeStartRightTarget = 0.0f;
    LineTrace_OuterFilterReset(&context->outerFilter);
    LineTrace_ControllerReset(&context->controller);
    LineTrace_ResetCrossDetect(&context->crossLockout,
                               &context->crossConfirm);
}

static float App_LinePwmToSpeedTarget(int16_t equivalentPwm,
                                      float referenceSpeed)
{
    return ((float)equivalentPwm * referenceSpeed) /
           (float)APP_STABLE_LINE_CRUISE_PWM;
}

static void App_LineApplyClosedLoopTarget(
    App_LineLapContext *context,
    const LineTrace_ControlOutput *controlOutput, float referenceSpeed)
{
    const int16_t leftEquivalentPwm = (int16_t)(
        controlOutput->basePwm + controlOutput->correction);
    const int16_t rightEquivalentPwm = (int16_t)(
        controlOutput->basePwm - controlOutput->correction);

    /* 固定 PWM 偏置由速度环自动补偿，只保留巡线产生的目标差速。 */
    context->lastLeftTarget = App_LinePwmToSpeedTarget(
        leftEquivalentPwm, referenceSpeed);
    context->lastRightTarget = App_LinePwmToSpeedTarget(
        rightEquivalentPwm, referenceSpeed);
    App_VehicleClosedLoopSetTarget(context->lastLeftTarget,
                                   context->lastRightTarget);
}

static void App_LineApplyBrake(int16_t brakePwm)
{
    App_VehicleClosedLoopDisable();
    Set_Speed((int)-brakePwm, (int)-brakePwm);
}

static void App_LineBeginStopping(App_LineLapContext *context,
                                  uint32_t now, int16_t brakePwm)
{
    context->state = APP_LINE_LAP_BRAKING;
    context->brakeStartTick = now;
    context->brakeStartLeftTarget = context->lastLeftTarget;
    context->brakeStartRightTarget = context->lastRightTarget;

    if (brakePwm > 0) {
        App_LineApplyBrake(brakePwm);
    } else {
        App_VehicleClosedLoopSetTarget(context->brakeStartLeftTarget,
                                       context->brakeStartRightTarget);
    }
}

static void App_LineApplySlowStop(const App_LineLapContext *context,
                                  uint32_t elapsedMs,
                                  uint16_t durationMs)
{
    const float remainingRatio =
        (float)((uint32_t)durationMs - elapsedMs) / (float)durationMs;

    App_VehicleClosedLoopSetTarget(
        context->brakeStartLeftTarget * remainingRatio,
        context->brakeStartRightTarget * remainingRatio);
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
        App_OPiLogStatus(code, 0U);
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

/*
 * 香橙派启动完成后发送 AA01。M0只接收并消费此帧，不发送握手回包；
 * 其它残留状态记录后丢弃，避免带入下一项任务。
 */
static void App_OPiDrainIdleStatus(void)
{
    uint8_t code;

    while (OPi_ReadFrame(&code) != 0U) {
        if (code == OPI_STATUS_BOOT_READY) {
            continue;
        } else {
            App_OPiLogStatus(code, 0U);
        }
    }
}

static void App_OPiStartTask(uint8_t taskCode)
{
    /* Consume the one-shot boot status and discard stale task statuses. */
    App_OPiDrainIdleStatus();
    OPi_FlushRx();
    OPi_SendCmd(taskCode);
    opiTaskActive = 1U;
}

static void App_OPiStartTask6(int8_t positionTenthsCm)
{
    App_OPiDrainIdleStatus();
    OPi_FlushRx();
    OPi_SendTask6(positionTenthsCm);
    opiTaskActive = 1U;
}

static void App_OPiAbortCurrentTask(void)
{
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
    LineTrace_ControllerReset(&context->controller);
    LineTrace_ResetCrossDetect(&context->crossLockout,
                               &context->crossConfirm);
    context->crossLockout = APP_LINE_CROSS_LOCKOUT_FRAMES;
    context->startEncL = Encoder_CumulativeL;
    context->startEncR = Encoder_CumulativeR;
    context->brakePending = 0U;
    context->brakeDetectEncL = 0;
    context->brakeDetectEncR = 0;
    LineTrace_OuterFilterReset(&context->outerFilter);
    App_VehicleClosedLoopStop();
    App_MenuForceTimerPage();
    Menu_SetTaskTime(0U);
    (void)snprintf(message, sizeof(message), "T%u: START\r\n",
                   (unsigned int)taskNumber);
    uart0_send_string(message);
}

static void App_LineLapRun(App_LineLapContext *context,
                           const LineTrace_ControlConfig *config,
                           uint8_t taskNumber, float referenceSpeed,
                           int16_t brakePwm,
                           uint16_t brakeDurationMs,
                           uint32_t brakeDelayPulses)
{
    const uint32_t now = BSP_Delay_GetTick();
    const Key5D_Key stableKey = App_InputGetStableKey();
    uint8_t raw;
    uint8_t steeringRaw;
    uint8_t activeCount;
    uint8_t hasLine;
    int16_t errorTenths;
    LineTrace_ControlOutput controlOutput;

    if (stableKey != KEY5D_KEY_RIGHT) {
        context->startKeyArmed = 1U;
    }
    if (context->state == APP_LINE_LAP_BRAKING) {
        const uint32_t elapsedMs = (uint32_t)(
            now - context->brakeStartTick);

        if (elapsedMs < brakeDurationMs) {
            if (brakePwm > 0) {
                App_LineApplyBrake(brakePwm);
            } else {
                /* 慢停+巡线：读取灰度偏差，在减速基础上保持纠偏。 */
                int16_t corr;
                float baseSpeed;
                float remainingRatio;

                if ((uint32_t)(now - context->lastTick) <
                    APP_LINE_CONTROL_PERIOD_MS) {
                    return;
                }
                context->lastTick = now;

                Grayscale_Read();
                raw = Grayscale_GetRaw();
                steeringRaw = LineTrace_FilterActiveLowOuterChannels(
                    &context->outerFilter, raw,
                    APP_LINE_OUTER_FILTER_FRAMES);
                hasLine = LineTrace_CalcActiveLowWeightedError(
                    steeringRaw, &errorTenths, 0);

                remainingRatio = (float)((uint32_t)brakeDurationMs -
                    elapsedMs) / (float)brakeDurationMs;
                baseSpeed = (context->brakeStartLeftTarget +
                    context->brakeStartRightTarget) * 0.5f *
                    remainingRatio;

                corr = (int16_t)((int32_t)errorTenths *
                    (int32_t)(APP_VEHICLE_LINE_DIFF_GAIN * 10.0f) / 10);
                if (corr > (int16_t)APP_VEHICLE_LINE_DIFF_MAX) {
                    corr = (int16_t)APP_VEHICLE_LINE_DIFF_MAX;
                } else if (corr < -(int16_t)APP_VEHICLE_LINE_DIFF_MAX) {
                    corr = -(int16_t)APP_VEHICLE_LINE_DIFF_MAX;
                }

                App_VehicleClosedLoopSetTarget(
                    baseSpeed + (float)corr,
                    baseSpeed - (float)corr);
            }
        } else {
            App_VehicleClosedLoopStop();
            context->state = APP_LINE_LAP_STOPPED;
        }
        return;
    }
    if ((context->state != APP_LINE_LAP_TRACKING) &&
        (context->startKeyArmed != 0U) &&
        (stableKey == KEY5D_KEY_RIGHT)) {
        App_LineLapStart(context, taskNumber);
        return;
    }

    if (context->state != APP_LINE_LAP_TRACKING) {
        App_VehicleClosedLoopStop();
        return;
    }
    if ((uint32_t)(now - context->lastTick) <
        APP_LINE_CONTROL_PERIOD_MS) {
        return;
    }
    context->lastTick = now;

    if (context->brakePending != 0U) {
        int32_t dL = Encoder_CumulativeL - context->brakeDetectEncL;
        int32_t dR = Encoder_CumulativeR - context->brakeDetectEncR;
        uint32_t distance = (uint32_t)((dL >= 0 ? dL : -dL) +
                                       (dR >= 0 ? dR : -dR));

        if (distance >= brakeDelayPulses) {
            context->brakePending = 0U;
            App_LineBeginStopping(context, now, brakePwm);
            return;
        }
    }

    /* 每秒更新计时显示 */
    {   static uint16_t lastReportedSec;
        uint16_t sec = (uint16_t)((now - context->startTick) / 1000U);
        if (sec != lastReportedSec) {
            lastReportedSec = sec;
            Menu_SetTaskTime(sec);
        }
    }

    Grayscale_Read();
    raw = Grayscale_GetRaw();
    steeringRaw = LineTrace_FilterActiveLowOuterChannels(
        &context->outerFilter, raw, APP_LINE_OUTER_FILTER_FRAMES);
    hasLine = LineTrace_CalcActiveLowWeightedError(
        steeringRaw, &errorTenths, 0);
    activeCount = LineTrace_CountActiveLow(raw);

    if ((context->brakePending == 0U) &&
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

        if (brakeDurationMs > 0U) {
            if (brakeDelayPulses > 0U) {
                context->brakePending = 1U;
                context->brakeDetectEncL = Encoder_CumulativeL;
                context->brakeDetectEncR = Encoder_CumulativeR;
            } else {
                App_LineBeginStopping(context, now, brakePwm);
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
        if (context->brakePending == 0U) {
            return;
        }
    }
    skip_cross: (void)0;

    /* 编码器距离停车(OLED Task 4)：|ΔL|+|ΔR| >= 阈值 → 停车 */
    if (context->stopOnDist != 0U) {
        int32_t dL = Encoder_CumulativeL - context->startEncL;
        int32_t dR = Encoder_CumulativeR - context->startEncR;
        uint32_t dist = (uint32_t)((dL >= 0 ? dL : -dL) + (dR >= 0 ? dR : -dR));
        if (dist >= TASK4_STOP_DIST_AB) {
            App_VehicleClosedLoopStop();
            context->state = APP_LINE_LAP_STOPPED;
            uart0_send_string("T4: DIST STOP\r\n");
            return;
        }
    }

    LineTrace_ControllerStep(&context->controller, config,
                             hasLine, errorTenths, &controlOutput);
    if (controlOutput.shouldStop != 0U) {
        char message[20];

        App_VehicleClosedLoopStop();
        context->state = APP_LINE_LAP_STOPPED;
        (void)snprintf(message, sizeof(message), "T%u: LOST\r\n",
                       (unsigned int)taskNumber);
        uart0_send_string(message);
        return;
    }

    App_LineApplyClosedLoopTarget(context, &controlOutput,
                                  referenceSpeed);
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
    OLED_ShowString(0U, 54U, "RIGHT OK", 8U, 1U);
    OLED_Refresh();
}

static void App_Task5RenderStatus(const char *status)
{
    OLED_ClearBuffer();
    OLED_ShowString(0U, 0U, "TASK6", 16U, 1U);
    OLED_ShowString(0U, 24U, status, 8U, 1U);
    OLED_Refresh();
}

static void App_Task5ReturnToTaskList(void)
{
    Encoder_XZ_Disable();
    App_VehicleClosedLoopStop();
    task5OwnsInterface = 0U;
    App_LineLapReset(&task5LineContext);
    App_MenuReturnToTaskList();
}

/* OLED Task 2F/2L: the selection key starts OPI Task2.  After the
 * operator releases it, the next RIGHT press starts driving and timing. */
void App_Task1Run(void)
{
    enum { TASK2_WAIT_RIGHT_RELEASE, TASK2_READY_TO_RUN };
    static uint32_t activationGeneration;
    static uint8_t state;
    const float referenceSpeed = (g_active_task == 6U)
                                     ? APP_TASK1_LOW_SPEED
                                     : APP_VEHICLE_DEFAULT_SPEED;
    const Key5D_Key stableKey = App_InputGetStableKey();

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        state = TASK2_WAIT_RIGHT_RELEASE;
        App_OPiStartTask(OPI_CMD_TASK2);
        uart0_send_string("OPI T2: START, WAIT OPERATOR\r\n");
    }

    App_OPiDrainRuntimeStatus();

    if (stableKey == KEY5D_KEY_LEFT) {
        App_OPiAbortCurrentTask();
        App_MenuReturnToTaskList();
        return;
    }

    if (state == TASK2_WAIT_RIGHT_RELEASE) {
        App_VehicleClosedLoopStop();
        if (stableKey != KEY5D_KEY_RIGHT) {
            state = TASK2_READY_TO_RUN;
        }
        return;
    }

    App_LineLapRun(&task1LineContext, &task1FastControlConfig, 2U,
                   referenceSpeed, 0,
                   APP_TASK2_SLOW_STOP_DURATION_MS,
                   APP_TASK1_BRAKE_DELAY_PULSES);
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
    static uint8_t state;
    const Key5D_Key stableKey = App_InputGetStableKey();
    uint8_t code;

    App_VehicleClosedLoopStop();

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        state = TASK3_WAIT_READY;
        App_OPiStartTask(OPI_CMD_TASK3);
        uart0_send_string("OPI T3: START\r\n");
    }

    if (stableKey == KEY5D_KEY_LEFT) {
        App_OPiAbortCurrentTask();
        App_MenuReturnToTaskList();
        return;
    }

    while (OPi_ReadFrame(&code) != 0U) {
        if ((state == TASK3_WAIT_READY) &&
            (code == OPI_STATUS_CONTROL_READY)) {
            state = TASK3_WAIT_RIGHT_RELEASE;
            uart0_send_string("OPI T3: READY\r\n");
        } else if ((state == TASK3_WAIT_DONE) &&
                   (code == OPI_STATUS_TASK3_DONE)) {
            state = TASK3_DONE;
            uart0_send_string("OPI T3: DONE\r\n");
        } else {
            App_OPiLogStatus(code,
                (state == TASK3_WAIT_READY) ? OPI_STATUS_CONTROL_READY :
                (state == TASK3_WAIT_DONE) ? OPI_STATUS_TASK3_DONE : 0U);
        }
    }

    if ((state == TASK3_WAIT_RIGHT_RELEASE) &&
        (stableKey != KEY5D_KEY_RIGHT)) {
        state = TASK3_ACTION_ARMED;
    } else if ((state == TASK3_ACTION_ARMED) &&
               (stableKey == KEY5D_KEY_RIGHT)) {
        OPi_SendCmd(OPI_CMD_TASK3_ACTION);
        state = TASK3_WAIT_DONE;
        uart0_send_string("OPI T3: ACTION\r\n");
    }
}

/* OLED Task 4: send AA 04 once, then run the existing A-to-B control. */
void App_Task3Run(void)
{
    static uint32_t activationGeneration;

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        App_OPiStartTask(OPI_CMD_TASK4);
    }
    App_OPiDrainRuntimeStatus();
    App_LineLapRun(&task3LineContext, &task2StableControlConfig,
                   4U, APP_VEHICLE_DEFAULT_SPEED, 0, 0U, 0U);
}

/* OLED Task 5: send AA 05 once, then run the existing line control. */
void App_Task4Run(void)
{
    static uint32_t activationGeneration;

    if (activationGeneration != taskActivationGeneration) {
        activationGeneration = taskActivationGeneration;
        App_OPiStartTask(OPI_CMD_TASK5);
    }
    App_OPiDrainRuntimeStatus();
    App_LineLapRun(&task4LineContext, &task56ControlConfig,
                   5U, APP_VEHICLE_TASK56_SPEED, 0,
                   APP_TASK56_SLOW_STOP_DURATION_MS, 0U);
}

/* OLED Task 6: confirm the encoder position with AA 06 POS. */
void App_Task5Run(void)
{
    enum { T6_SELECT_POSITION, T6_TRACK };
    static uint8_t state = T6_SELECT_POSITION;
    static uint8_t rightKeyArmed;
    static int16_t psTenths;
    static int16_t lastDisplayPsTenths = 32767;
    static uint32_t lastDisplayTick;
    Key5D_Event keyEvent = KEY5D_EVENT_NONE;

    if (task5ResetPending != 0U) {
        task5ResetPending = 0U;
        state = T6_SELECT_POSITION;
        rightKeyArmed = 0U;
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
            App_OPiStartTask6((int8_t)psTenths);
            App_LineLapStart(&task5LineContext, 6U);
            task5OwnsInterface = 0U;
            state = T6_TRACK;
        }
        break; }
    case T6_TRACK:
        App_OPiDrainRuntimeStatus();
        App_LineLapRun(&task5LineContext, &task56ControlConfig,
                       6U, APP_VEHICLE_TASK56_SPEED, 0,
                       APP_TASK56_SLOW_STOP_DURATION_MS, 0U);
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

/*
 * (旧测试函数已删除，循迹逻辑移至 App_Task1Run)
 */
static void App_GrayscaleDisplayRun_removed(void)
{
    enum { T_IDLE, T_TRACKING, T_BRAKE, T_STOPPED };

    const uint32_t now = BSP_Delay_GetTick();
    uint8_t raw;
    char line[17];

    static uint8_t  state = T_IDLE;
    static uint32_t lastTick;
    static uint16_t crossLockout;
    static uint8_t  crossConfirm;
    static uint32_t detectCount;
    static int16_t  sLeft, sRight;  /* 整形后的 PWM */
    static uint32_t startTick;
    static float    prevErr;    /* 滤波状态 */
    static int16_t  trimBias;   /* 自校准偏置 */
    static int32_t  calibSum;   /* 校准累积 */
    static uint16_t calibCnt;   /* 校准采样数 */
    static uint16_t calFrame;   /* 帧计数 */

    uint8_t  activeCount;
    int16_t  errorTenths;
    uint8_t  crossNow;

    if ((uint32_t)(now - lastTick) < 20U) return;
    lastTick = now;

    /* ---- 读灰度 ---- */
    Grayscale_Read();
    raw = Grayscale_GetRaw();
    (void)LineTrace_CalcActiveLowWeightedError(raw, &errorTenths, &activeCount);

    /* ---- 横切线实时判定：任意连续3路全黑 ---- */
    {
        uint8_t b0 = (((raw >> 0) & 1U) == 0U) ? 1U : 0U;
        uint8_t b1 = (((raw >> 1) & 1U) == 0U) ? 1U : 0U;
        uint8_t b2 = (((raw >> 2) & 1U) == 0U) ? 1U : 0U;
        uint8_t b3 = (((raw >> 3) & 1U) == 0U) ? 1U : 0U;
        uint8_t b4 = (((raw >> 4) & 1U) == 0U) ? 1U : 0U;
        uint8_t b5 = (((raw >> 5) & 1U) == 0U) ? 1U : 0U;
        uint8_t b6 = (((raw >> 6) & 1U) == 0U) ? 1U : 0U;
        uint8_t b7 = (((raw >> 7) & 1U) == 0U) ? 1U : 0U;
        crossNow = ((b0&&b1&&b2)||(b1&&b2&&b3)||(b2&&b3&&b4)
                 ||(b3&&b4&&b5)||(b4&&b5&&b6)||(b5&&b6&&b7)) ? 1U : 0U;
    }

    /* ---- 状态机 ---- */
    switch (state) {

    case T_IDLE:
        Set_Speed(0, 0);
        /* 等右键按下 */
        {
            Key5D_Event ev = KEY5D_EVENT_NONE;
            if (App_InputPoll(now, &ev) && ev == KEY5D_EVENT_PRESSED
                && App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
                state = T_TRACKING;
                Encoder_Init();
                LineTrace_ResetCrossDetect(&crossLockout, &crossConfirm);
                detectCount = 0U;
                sLeft = sRight = 600;
                prevErr = 0.0f;
                calibSum = 0;
                calibCnt = 0;
                calFrame = 0;
                trimBias = 0;
                startTick = now;
                uart0_send_string("TRACK START\r\n");
            }
        }
        break;

    case T_TRACKING:
        /* 横切线检测 + 计数 */
        if (LineTrace_DetectCrossLine(raw, activeCount, &crossLockout, &crossConfirm)
            == CROSS_LINE_DETECTED) {
            detectCount++;
            state = T_BRAKE;
            uart0_send_string("STOP LINE!\r\n");
            break;
        }

        /* 循迹修正：P 控制 + 固定偏置补偿机械不对称 */
        {
            int16_t err = errorTenths;
            int16_t corr;
            static uint8_t lostCnt;
            float fe;
            #define CALIB_FRAMES 200           /* 前200帧(~4s)校准 */
            #define CALIB_SKIP    20           /* 跳过前20帧(车未稳) */

            /* 丢线处理 */
            if (activeCount == 0U) {
                lostCnt++;
                err = (prevErr > 0) ? (int16_t)(prevErr * 1.5f) : (int16_t)(prevErr * 1.5f);
                if (lostCnt > 60U) { Set_Speed(0, 0); state = T_STOPPED; break; }
            } else {
                lostCnt = 0U;
                if (err >= -3 && err <= 3) err = 0;
                fe = (float)err;
                fe = 0.7f * prevErr + 0.3f * fe;
                prevErr = fe;
                err = (int16_t)fe;
            }

            /* 校准：跳过起步不稳，|err|<5 时采样，×1.5 补偿 */
            calFrame++;
            if (calibCnt < CALIB_FRAMES && calFrame > CALIB_SKIP) {
                if (err >= -5 && err <= 5) {
                    calibSum += err;
                    calibCnt++;
                }
            }
            if (calibCnt > 0) {
                trimBias = (int16_t)((-calibSum * 3) / ((int32_t)calibCnt * 2));
            }

            /* P 修正 + 自校准偏置 */
            corr = (int16_t)(((int32_t)err * 12) / 10) + trimBias;
            if (corr > 150) corr = 150;
            if (corr < -150) corr = -150;

            /* 目标 PWM */
            int16_t tl = (int16_t)(600 + corr);
            int16_t tr = (int16_t)(600 - corr);
            int16_t d;
            if (tl < 0) tl = 0; if (tl > 1800) tl = 1800;
            if (tr < 0) tr = 0; if (tr > 1800) tr = 1800;

            /* 变化率限制（更平缓） */
            int16_t rate = (sLeft < 200) ? 80 : 30;
            d = (int16_t)(tl - sLeft);
            if (d > rate) sLeft += rate; else if (d < -rate) sLeft -= rate; else sLeft = tl;
            d = (int16_t)(tr - sRight);
            if (d > rate) sRight += rate; else if (d < -rate) sRight -= rate; else sRight = tr;

            Set_Speed((int)sLeft, (int)sRight);
        }
        break;

    case T_BRAKE:
        /* 平缓减速：直接停转，靠惯性滑行 */
        Set_Speed(0, 0);
        state = T_STOPPED;
        uart0_send_string("STOPPED\r\n");
        break;

    case T_STOPPED:
        /* 保持停车 */
        /* 按右键重新开始 */
        {
            Key5D_Event ev = KEY5D_EVENT_NONE;
            if (App_InputPoll(now, &ev) && ev == KEY5D_EVENT_PRESSED
                && App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
                state = T_TRACKING;
                LineTrace_ResetCrossDetect(&crossLockout, &crossConfirm);
                detectCount = 0U;
                sLeft = sRight = 600;
                prevErr = 0.0f;
                calibSum = 0;
                calibCnt = 0;
                calFrame = 0;
                trimBias = 0;
                startTick = now;
                uart0_send_string("TRACK START\r\n");
            }
        }
        break;
    }

    /* ---- OLED ---- */
    OLED_ClearBuffer();

    (void)snprintf(line, sizeof(line), "%u%u%u%u%u%u%u%u",
                   (unsigned)((raw >> 7) & 1), (unsigned)((raw >> 6) & 1),
                   (unsigned)((raw >> 5) & 1), (unsigned)((raw >> 4) & 1),
                   (unsigned)((raw >> 3) & 1), (unsigned)((raw >> 2) & 1),
                   (unsigned)((raw >> 1) & 1), (unsigned)(raw & 1));
    OLED_ShowString(0U, 0U, line, 16U, 1U);

    (void)snprintf(line, sizeof(line), "STOP:%s %s",
                   crossNow ? "YES" : "no ",
                   (state == T_IDLE) ? "RIGHT->go" :
                   (state == T_STOPPED) ? "DONE" : "");
    OLED_ShowString(0U, 16U, line, 16U, 1U);

    (void)snprintf(line, sizeof(line), "C:%lu T:%d sL=%d",
                   (unsigned long)detectCount, (int)trimBias, (int)sLeft);
    OLED_ShowString(0U, 32U, line, 16U, 1U);

    (void)snprintf(line, sizeof(line), "0x%02X t=%lus",
                   (unsigned)raw,
                   (unsigned long)((now - startTick) / 1000UL));
    OLED_ShowString(0U, 48U, line, 16U, 1U);

    OLED_Refresh();
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
