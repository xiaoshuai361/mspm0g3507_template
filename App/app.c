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
#include "line_trace_test.h"
#include "menu_test.h"
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
static uint32_t lastGrayscaleDisplayTick; /**< 上一次灰度 OLED 刷新时间戳。 */

#define APP_GRAYSCALE_DISPLAY_PERIOD_MS (100U) /**< 灰度 OLED 刷新周期，单位 ms。 */

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

/**
 * @brief 读取 8 路灰度传感器并在 OLED 上显示各通道值。
 * @param 无。
 * @note 每 APP_GRAYSCALE_DISPLAY_PERIOD_MS 刷新一次；仅显示不输出电机控制。
 * @retval 无。
 */
static void App_GrayscaleDisplayRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    uint8_t raw;
    char line[17];

    if ((uint32_t)(now - lastGrayscaleDisplayTick) < APP_GRAYSCALE_DISPLAY_PERIOD_MS)
    {
        return;
    }
    lastGrayscaleDisplayTick = now;

    Grayscale_Read();
    raw = Grayscale_GetRaw();

    OLED_ClearBuffer();

    /* 第 1 行：通道编号 D8(MSB) → D1(LSB) */
    OLED_ShowString(0U, 0U, "GS D8..D1", 16U, 1U);

    /* 第 2 行：每通道 0/1 状态 */
    (void)snprintf(line, sizeof(line), "%u%u%u%u%u%u%u%u",
                   (unsigned int)((raw >> 7) & 0x01U),
                   (unsigned int)((raw >> 6) & 0x01U),
                   (unsigned int)((raw >> 5) & 0x01U),
                   (unsigned int)((raw >> 4) & 0x01U),
                   (unsigned int)((raw >> 3) & 0x01U),
                   (unsigned int)((raw >> 2) & 0x01U),
                   (unsigned int)((raw >> 1) & 0x01U),
                   (unsigned int)(raw & 0x01U));
    OLED_ShowString(0U, 16U, line, 16U, 1U);

    /* 第 3 行：原始 hex 值 */
    (void)snprintf(line, sizeof(line), "hex=0x%02X", (unsigned int)raw);
    OLED_ShowString(0U, 32U, line, 16U, 1U);

    /* 第 4 行：传感器读数时间戳（取低 4 位 hex 简化显示） */
    (void)snprintf(line, sizeof(line), "tick=%04lX", (unsigned long)(now & 0xFFFFUL));
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
    lastGrayscaleDisplayTick = BSP_Delay_GetTick() - APP_GRAYSCALE_DISPLAY_PERIOD_MS;

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
