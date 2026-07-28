#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdio.h>

#include "bsp_adc.h"
#include "bluetooth_command_test.h"
#include "delay.h"
#include "dl1a_test.h"
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
    App_MenuRun();

    /* 可选模块：这些任务都是非阻塞轮询，启停只需要保留或注释对应调用。 */
    App_ImuRun();
    App_ToFRun();
    App_VehicleRun();

    /* 菜单任务接口：进入 Task 1~4 后，自动分发到对应 App_TaskXRun()。 */
    App_TasksRun();
}
