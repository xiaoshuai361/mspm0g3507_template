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

/**
 * @brief 执行菜单选择的 Task 1 逻辑。
 * @param 无。
 * @note 用户在这里填写 Task 1 的非阻塞逻辑；不要写 while(1) 或长时间 delay。
 * @retval 无。
 */
void App_Task1Run(void)
{
    /* TODO(user): 在这里填写 Task 1 的业务逻辑，函数会在 Task 1 被选中后周期执行。 */
}

/**
 * @brief 执行菜单选择的 Task 2 逻辑。
 * @param 无。
 * @note 用户在这里填写 Task 2 的非阻塞逻辑；不要写 while(1) 或长时间 delay。
 * @retval 无。
 */
void App_Task2Run(void)
{
    /* TODO(user): 在这里填写 Task 2 的业务逻辑，函数会在 Task 2 被选中后周期执行。 */
}

/**
 * @brief 执行菜单选择的 Task 3 逻辑。
 * @param 无。
 * @note 用户在这里填写 Task 3 的非阻塞逻辑；不要写 while(1) 或长时间 delay。
 * @retval 无。
 */
void App_Task3Run(void)
{
    /* TODO(user): 在这里填写 Task 3 的业务逻辑，函数会在 Task 3 被选中后周期执行。 */
}

/**
 * @brief 执行菜单选择的 Task 4 逻辑。
 * @param 无。
 * @note 用户在这里填写 Task 4 的非阻塞逻辑；不要写 while(1) 或长时间 delay。
 * @retval 无。
 */
void App_Task4Run(void)
{
    /* TODO(user): 在这里填写 Task 4 的业务逻辑，函数会在 Task 4 被选中后周期执行。 */
}

/**
 * @brief 根据菜单当前任务编号分发执行 Task 1~4。
 * @param 无。
 * @note g_active_task 由菜单模块更新；任务函数必须非阻塞，避免卡住按键/OLED/PID。
 * @retval 无。
 */
void App_TasksRun(void)
{
    switch (g_active_task)
    {
    case 1U:
        App_Task1Run();
        break;
    case 2U:
        App_Task2Run();
        break;
    case 3U:
        App_Task3Run();
        break;
    case 4U:
        App_Task4Run();
        break;
    default:
        /* 未选择任务时不执行用户任务。 */
        break;
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
 * 直线循迹 + 启停线停车测试
 * 上电后显示传感器值，按右键启动循迹，遇横切线急刹停车。
 */
static void App_GrayscaleDisplayRun(void)
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

    if (runLoopLogged == 0U)
    {
        runLoopLogged = 1U;
        uart0_send_string("RUN loop OK\r\n");
    }

    /* OLED 显示任务：按键测试页和正式菜单二选一，避免两个页面抢屏。 */
    // App_Key5DTestRun();
    App_BatteryRun();
    App_ElectromagnetRun();
    // App_MenuRun();        /* 注释：原 OLED 多级菜单系统 */

    /* 灰度传感器 8 路 OLED 直显（不执行循迹/电机控制） */
    App_GrayscaleDisplayRun();

    /* 可选模块：这些任务都是非阻塞轮询，启停只需要保留或注释对应调用。 */
    // App_ImuRun();
    // App_ToFRun();
    // App_VehicleRun();     /* 注释：原车辆循迹+电机 PID 闭环 */

    /* 菜单任务接口：进入 Task 1~4 后，自动分发到对应 App_TaskXRun()。 */
    // App_TasksRun();       /* 注释：原 Task1~4 分发 */
}
