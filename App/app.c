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
    uint8_t stopOnDist;        /* 1=编码器距离停车(Task3) */
    uint32_t lastTick;
    uint32_t startTick;
    uint32_t brakeStartTick;
    uint16_t crossLockout;
    uint8_t crossConfirm;
    int32_t startEncL;         /* 启动时左编码器累计值 */
    int32_t startEncR;         /* 启动时右编码器累计值 */
    LineTrace_Controller controller;
} App_LineLapContext;

#define TASK3_STOP_DIST_AB 12600  /* A→B 1.5m直线脉冲阈值 */

/*
 * Task 1 调参顺序：半圆转不动先增 edgeSteeringKp，再减
 * edgeSteeringThreshold，最后才增 steeringMax；直线摆动则反向调整。
 * 所有“帧”均为10ms，误差单位为0.1个灰度探头间距。
 */
static const LineTrace_ControlConfig task1FastControlConfig = {
    .cruisePwm = APP_TASK1_LINE_CRUISE_PWM, /* 直线巡航PWM，在app_config.h中修改。 */
    .lostPwm = 700,              /* 弯道重度降速/丢线搜索下限；增大可补偿配重，但更难找回线。 */
    .rampUpStep = 20,            /* 每帧基准PWM上升量；增大则起步和出弯加速更快。 */
    .rampDownStep = 20,          /* 限制每帧降速，避免短时抖动造成骤降。 */
    .curveSlowdownGain = 8,      /* 每单位绝对偏差扣除的PWM；增大可降低弯速。 */
    .slowdownEntryError = 10,    /* 小偏差只纠偏，不降低巡航基准。 */
    .steeringKp = 4,             /* 小偏差比例增益；增大可加快微调，过大会造成直线摆动。 */
    .edgeSteeringKp = 20,        /* 提高右半圆持续转向力。 */
    .edgeSteeringThreshold = 6,  /* 死区后偏差达到此值启用强增益；减小会更早强纠偏。 */
    .steeringMax = 480,          /* 放宽高速档差速修正硬上限。 */
    .leftPwmBias = 50,           /* 高速直行稳定输出：左1150、右1100。 */
    .rightTurnBoost = 135,       /* 两个右半圆额外提升左侧重载外轮扭矩。 */
    .centerDeadband = 5,         /* 中心死区；增大更稳但纠偏变迟，减小更灵敏但易抖。 */
    .fastErrorThreshold = 15,    /* 原始偏差达到此值使用3/4新值快滤波；减小响应更快。 */
    .fastDeltaThreshold = 12,    /* 相邻帧偏差跳变量阈值；减小可更快响应突然换边。 */
    .lostSearchStartError = 18,  /* 丢线搜索的初始等效偏差；增大则首次找线转向更强。 */
    .lostSearchStepFrames = 2U,  /* 搜索偏差每隔多少帧加1；减小会更快增强搜索。 */
    .lostHoldFrames = 1U,        /* 丢线后保持最后纠偏的帧数；当前1帧即10ms。 */
    .slowdownConfirmFrames = 3U, /* 偏差持续30ms才开始降速，滤除瞬时抖动。 */
    .lostStopFrames = 150U,      /* 连续丢线停车时间；当前150帧即1.5s。 */
};

static const LineTrace_ControlConfig task2StableControlConfig = {
    .cruisePwm = APP_STABLE_LINE_CRUISE_PWM,
    .lostPwm = 584,
    .rampUpStep = 19,
    .rampDownStep = 11,
    .curveSlowdownGain = 1,
    .slowdownEntryError = 8,
    .steeringKp = 4,
    .edgeSteeringKp = 14,
    .edgeSteeringThreshold = 6,
    .steeringMax = 240,
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

static App_LineLapContext task1LineContext = { .startKeyArmed = 1U };
static App_LineLapContext task3LineContext = {
    .startKeyArmed = 1U, .stopOnDist = 1U };
static App_LineLapContext task4LineContext = { .startKeyArmed = 1U };
static App_LineLapContext task5LineContext = { .startKeyArmed = 1U };

static void App_LineLapReset(App_LineLapContext *context)
{
    context->state = APP_LINE_LAP_IDLE;
    context->startKeyArmed = 1U;
    context->lastTick = 0U;
    context->startTick = 0U;
    context->brakeStartTick = 0U;
    LineTrace_ControllerReset(&context->controller);
    LineTrace_ResetCrossDetect(&context->crossLockout,
                               &context->crossConfirm);
}

static void App_LineLapRun(App_LineLapContext *context,
                           const LineTrace_ControlConfig *config,
                           uint8_t taskNumber, int16_t brakePwm,
                           uint16_t brakeDurationMs)
{
    const uint32_t now = BSP_Delay_GetTick();
    const Key5D_Key stableKey = App_InputGetStableKey();
    uint8_t raw;
    uint8_t activeCount;
    uint8_t hasLine;
    int16_t errorTenths;
    LineTrace_ControlOutput controlOutput;

    if (stableKey != KEY5D_KEY_RIGHT) {
        context->startKeyArmed = 1U;
    }
    if (context->state == APP_LINE_LAP_BRAKING) {
        if ((uint32_t)(now - context->brakeStartTick) <
            brakeDurationMs) {
            Set_Speed((int)-brakePwm, (int)-brakePwm);
        } else {
            Set_Speed(0, 0);
            context->state = APP_LINE_LAP_STOPPED;
        }
        return;
    }
    if ((context->state != APP_LINE_LAP_TRACKING) &&
        (context->startKeyArmed != 0U) &&
        (stableKey == KEY5D_KEY_RIGHT)) {
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
        Set_Speed(0, 0);
        App_MenuForceTimerPage();
        Menu_SetTaskTime(0U);
        (void)snprintf(message, sizeof(message), "T%u: START\r\n",
                       (unsigned int)taskNumber);
        uart0_send_string(message);
        return;
    }

    if (context->state != APP_LINE_LAP_TRACKING) {
        Set_Speed(0, 0);
        return;
    }
    if ((uint32_t)(now - context->lastTick) <
        APP_LINE_CONTROL_PERIOD_MS) {
        return;
    }
    context->lastTick = now;

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
    hasLine = LineTrace_CalcActiveLowWeightedError(
        raw, &errorTenths, &activeCount);

    if (LineTrace_DetectCrossLine(raw, activeCount,
                                  &context->crossLockout,
                                  &context->crossConfirm)
        == CROSS_LINE_DETECTED) {
        uint32_t elapsed = (uint32_t)(now - context->startTick);
        char message[40];

        if ((brakePwm > 0) && (brakeDurationMs > 0U)) {
            context->state = APP_LINE_LAP_BRAKING;
            context->brakeStartTick = now;
            Set_Speed((int)-brakePwm, (int)-brakePwm);
        } else {
            Set_Speed(0, 0);
            context->state = APP_LINE_LAP_STOPPED;
        }
        (void)snprintf(message, sizeof(message),
                       "T%u: A %lu.%lus\r\n",
                       (unsigned int)taskNumber,
                       (unsigned long)(elapsed / 1000U),
                       (unsigned long)((elapsed % 1000U) / 100U));
        uart0_send_string(message);
        return;
    }

    /* 编码器距离停车(Task3)：|ΔL|+|ΔR| >= 阈值 → 停车 */
    if (context->stopOnDist != 0U) {
        int32_t dL = Encoder_CumulativeL - context->startEncL;
        int32_t dR = Encoder_CumulativeR - context->startEncR;
        uint32_t dist = (uint32_t)((dL >= 0 ? dL : -dL) + (dR >= 0 ? dR : -dR));
        if (dist >= TASK3_STOP_DIST_AB) {
            Set_Speed(0, 0);
            context->state = APP_LINE_LAP_STOPPED;
            uart0_send_string("T3: DIST STOP\r\n");
            return;
        }
    }

    LineTrace_ControllerStep(&context->controller, config,
                             hasLine, errorTenths, &controlOutput);
    if (controlOutput.shouldStop != 0U) {
        char message[20];

        Set_Speed(0, 0);
        context->state = APP_LINE_LAP_STOPPED;
        (void)snprintf(message, sizeof(message), "T%u: LOST\r\n",
                       (unsigned int)taskNumber);
        uart0_send_string(message);
        return;
    }

    Set_Speed((int)controlOutput.leftPwm, (int)controlOutput.rightPwm);
}

/* Task 1：1100 PWM 竞速档 + 遇A停车。 */
void App_Task1Run(void)
{
    App_LineLapRun(&task1LineContext, &task1FastControlConfig, 1U,
                   APP_TASK1_BRAKE_PWM, APP_TASK1_BRAKE_DURATION_MS);
}

/*
 * Task 2 —— 题3：摆杆球控制（香橙派协议）
 * 发送AA 03 → 等待AA 00 → 完成
 */
void App_Task2Run(void)
{
    enum { T2_SEND, T2_WAIT, T2_DONE };
    static uint8_t state = T2_SEND;
    static uint8_t lastActive;

    if (lastActive != 2U) { lastActive = 2U; state = T2_SEND; OPi_FlushRx(); }
    Set_Speed(0, 0);

    switch (state) {
    case T2_SEND:
        OPi_SendCmd(OPI_CMD_TASK3);
        state = T2_WAIT;
        break;
    case T2_WAIT: {
        uint8_t b;
        if (OPi_ReadByte(&b) && b == OPI_ACK_OK) state = T2_DONE;
        break; }
    case T2_DONE: {
        Key5D_Event ev = KEY5D_EVENT_NONE;
        if (App_InputPoll(BSP_Delay_GetTick(), &ev) && ev == KEY5D_EVENT_PRESSED
            && App_InputGetStableKey() == KEY5D_KEY_RIGHT) { state = T2_SEND; }
        break; }
    default: break;
    }
}

/*
 * Task 3 —— 题4：A→B（香橙派协议 + 编码器距离停车）
 * 发送AA 04 → 等待AA 00 → 循迹 → 停车发AA 0F → 等待AA 00
 */
void App_Task3Run(void)
{
    enum { T3_SEND, T3_WAIT, T3_READY, T3_TRACK, T3_FINISH, T3_DONE };
    static uint8_t state = T3_SEND;
    static uint8_t lastActive;

    if (lastActive != 3U) {
        lastActive = 3U; state = T3_SEND; OPi_FlushRx();
        App_LineLapReset(&task3LineContext);
    }

    switch (state) {
    case T3_SEND:
        OPi_SendCmd(OPI_CMD_TASK4);
        state = T3_WAIT;
        break;
    case T3_WAIT: {
        uint8_t b;
        if (OPi_ReadByte(&b) && b == OPI_ACK_OK &&
            App_InputGetStableKey() != KEY5D_KEY_RIGHT) state = T3_READY;
        Set_Speed(0, 0);
        break; }
    case T3_READY:
        /* 等待中键按下启动循迹 */
        App_LineLapRun(&task3LineContext, &task2StableControlConfig, 3U, 0, 0U);
        if (task3LineContext.state == APP_LINE_LAP_TRACKING) state = T3_TRACK;
        break;
    case T3_TRACK:
        App_LineLapRun(&task3LineContext, &task2StableControlConfig, 3U, 0, 0U);
        if (task3LineContext.state == APP_LINE_LAP_STOPPED) state = T3_FINISH;
        break;
    case T3_FINISH: {
        static uint8_t sent;
        if (sent == 0U) { OPi_SendCmd(OPI_CMD_FINISH); sent = 1U; }
        uint8_t b;
        if (OPi_ReadByte(&b) && b == OPI_ACK_OK) { sent = 0U; state = T3_DONE; }
        break; }
    case T3_DONE: {
        Key5D_Event ev = KEY5D_EVENT_NONE;
        if (App_InputPoll(BSP_Delay_GetTick(), &ev) && ev == KEY5D_EVENT_PRESSED
            && App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
            state = T3_SEND; OPi_FlushRx(); App_LineLapReset(&task3LineContext);
        }
        break; }
    default: break;
    }
}

/*
 * Task 4 —— 题5：单圈+经过A（香橙派协议，球稳定后发车）
 * 发送AA 05 → 等待AA 00 → 循迹 → 停车发AA 0F → 等待AA 00
 */
void App_Task4Run(void)
{
    enum { T4_SEND, T4_WAIT, T4_READY, T4_TRACK, T4_FINISH, T4_DONE };
    static uint8_t state = T4_SEND;
    static uint8_t lastActive;

    if (lastActive != 4U) {
        lastActive = 4U; state = T4_SEND; OPi_FlushRx();
        App_LineLapReset(&task4LineContext);
    }

    switch (state) {
    case T4_SEND:
        OPi_SendCmd(OPI_CMD_TASK5);
        state = T4_WAIT;
        break;
    case T4_WAIT: {
        uint8_t b;
        if (OPi_ReadByte(&b) && b == OPI_ACK_OK &&
            App_InputGetStableKey() != KEY5D_KEY_RIGHT) state = T4_READY;
        Set_Speed(0, 0);
        break; }
    case T4_READY:
        App_LineLapRun(&task4LineContext, &task2StableControlConfig, 4U, 0, 0U);
        if (task4LineContext.state == APP_LINE_LAP_TRACKING) state = T4_TRACK;
        break;
    case T4_TRACK:
        App_LineLapRun(&task4LineContext, &task2StableControlConfig, 4U, 0, 0U);
        if (task4LineContext.state == APP_LINE_LAP_STOPPED) state = T4_FINISH;
        break;
    case T4_FINISH: {
        static uint8_t sent;
        if (sent == 0U) { OPi_SendCmd(OPI_CMD_FINISH); sent = 1U; }
        uint8_t b;
        if (OPi_ReadByte(&b) && b == OPI_ACK_OK) { sent = 0U; state = T4_DONE; }
        break; }
    case T4_DONE: {
        Key5D_Event ev = KEY5D_EVENT_NONE;
        if (App_InputPoll(BSP_Delay_GetTick(), &ev) && ev == KEY5D_EVENT_PRESSED
            && App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
            state = T4_SEND; OPi_FlushRx(); App_LineLapReset(&task4LineContext);
        }
        break; }
    default: break;
    }
}

/*
 * Task 5 —— 题6：单圈+经过A（香橙派协议，不记录初始位置直接发车）
 * 发送AA 06 → 等待AA 00 → 循迹 → 停车发AA 0F → 等待AA 00
 */
void App_Task5Run(void)
{
    enum { T5_SEND, T5_WAIT, T5_READY, T5_TRACK, T5_FINISH, T5_DONE };
    static uint8_t state = T5_SEND;
    static uint8_t lastActive;

    if (lastActive != 5U) {
        lastActive = 5U; state = T5_SEND; OPi_FlushRx();
        App_LineLapReset(&task5LineContext);
    }

    switch (state) {
    case T5_SEND:
        OPi_SendCmd(OPI_CMD_TASK6);
        state = T5_WAIT;
        break;
    case T5_WAIT: {
        uint8_t b;
        if (OPi_ReadByte(&b) && b == OPI_ACK_OK &&
            App_InputGetStableKey() != KEY5D_KEY_RIGHT) state = T5_READY;
        Set_Speed(0, 0);
        break; }
    case T5_READY:
        App_LineLapRun(&task5LineContext, &task2StableControlConfig, 5U, 0, 0U);
        if (task5LineContext.state == APP_LINE_LAP_TRACKING) state = T5_TRACK;
        break;
    case T5_TRACK:
        App_LineLapRun(&task5LineContext, &task2StableControlConfig, 5U, 0, 0U);
        if (task5LineContext.state == APP_LINE_LAP_STOPPED) state = T5_FINISH;
        break;
    case T5_FINISH: {
        static uint8_t sent;
        if (sent == 0U) { OPi_SendCmd(OPI_CMD_FINISH); sent = 1U; }
        uint8_t b;
        if (OPi_ReadByte(&b) && b == OPI_ACK_OK) { sent = 0U; state = T5_DONE; }
        break; }
    case T5_DONE: {
        Key5D_Event ev = KEY5D_EVENT_NONE;
        if (App_InputPoll(BSP_Delay_GetTick(), &ev) && ev == KEY5D_EVENT_PRESSED
            && App_InputGetStableKey() == KEY5D_KEY_RIGHT) {
            state = T5_SEND; OPi_FlushRx(); App_LineLapReset(&task5LineContext);
        }
        break; }
    default: break;
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
        Set_Speed(0, 0);
        App_LineLapReset(&task1LineContext);
        App_LineLapReset(&task3LineContext);
        App_LineLapReset(&task4LineContext);
        App_LineLapReset(&task5LineContext);
        previousTask = g_active_task;
    }

    switch (g_active_task)
    {
    case 1U: App_Task1Run(); break;
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

    if (runLoopLogged == 0U) {
        runLoopLogged = 1U;
        uart0_send_string("RUN loop OK\r\n");
    }

    App_BatteryRun();
    App_ElectromagnetRun();
    App_MenuRun();

    /* 编码器中断始终开启，无任务时也周期测速（Car Status 需要累计值） */
    {
        static uint8_t encInited;
        if (encInited == 0U) { encInited = 1U; Encoder_Init(); }
    }
    {
        static uint32_t lastEncTick;
        uint32_t now = BSP_Delay_GetTick();
        if ((uint32_t)(now - lastEncTick) >= 20U) {
            lastEncTick = now;
            MEASURE_MOTORS_SPEED();
        }
    }

    /* 无任务时停车；有任务时由 Task 接管 */
    if (g_active_task == 0U) {
        Set_Speed(0, 0);
    }

    App_TasksRun();
}
