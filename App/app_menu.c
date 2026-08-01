#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>

#include "Encoder.h"
#include "delay.h"
#include "menu.h"
#include "uart.h"

volatile uint8_t g_active_task; /**< 当前菜单选择的任务编号。 */

static Menu_State menuState; /**< menuState 全局状态或配置变量。 */
static Menu_ViewData menuData; /**< menuData 全局状态或配置变量。 */
static uint32_t lastDisplayTick; /**< lastDisplayTick 全局状态或配置变量。 */

/**
 * @brief 将 float 速度转换为菜单使用的 0.1 单位，并做 int16_t 饱和保护。
 * @param speed 速度值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回有符号 16 位结果。
 */
static int16_t App_SpeedToTenths(float speed)
{
    float tenths = speed * 10.0f;

    if (tenths > (float)INT16_MAX)
    {
        return INT16_MAX;
    }
    if (tenths < (float)INT16_MIN)
    {
        return INT16_MIN;
    }

    if (tenths >= 0.0f)
    {
        tenths += 0.5f;
    }
    else
    {
        tenths -= 0.5f;
    }

    return (int16_t)tenths;
}

/**
 * @brief 刷新 Speed 页的小车实际速度：目标速度保留车辆任务最近发布的值。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_UpdateCarSpeedDisplay(void)
{
    const float actualSpeed = (Motor1_Speed + Motor2_Speed) * 0.5f;
    const float targetSpeed = App_VehicleClosedLoopGetAverageTarget();

    App_MenuSetCarSpeedData(App_SpeedToTenths(targetSpeed),
                            App_SpeedToTenths(actualSpeed));
    App_MenuSetSpeedData(App_SpeedToTenths(Motor1_Speed),
                         App_SpeedToTenths(Motor2_Speed));
}

/**
 * @brief 初始化菜单状态，并填入模板默认参数。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuInitData(void)
{
    Menu_Init(&menuState);

    menuData.encoderLines = ENCODE_13X;
    menuData.gearRatio = JIANSUBI;
    menuData.encoderMultiplier = BEIPIN;
    menuData.samplePeriodMs = (uint16_t)(SAMPLE_TIME * 1000.0f);
    menuData.pwmPeriod = 2000U;
    App_UpdateCarSpeedDisplay();
}

/**
 * @brief 将最近一次按键 ADC 原始值同步给菜单状态页显示。
 * @param rawAdc ADC 原始值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuSetKeyAdc(uint16_t rawAdc)
{
    menuData.keyAdc = rawAdc;
}

/**
 * @brief 处理菜单按键事件，并在任务编号变化时通过 UART0 输出当前任务。
 * @param key 按键方向。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_HandleMenuKeyPress(Key5D_Key key)
{
    App_LogKeyPress(key);
    Menu_HandleInput(&menuState, App_InputToMenu(key));
    g_active_task = Menu_GetActiveTask(&menuState);
}

static void App_MenuPollInput(uint32_t now)
{
    Key5D_Event event = KEY5D_EVENT_NONE;

    if (App_InputPoll(now, &event))
    {
        App_MenuSetKeyAdc(App_InputGetLastAdc());
        if (event == KEY5D_EVENT_PRESSED)
        {
            App_HandleMenuKeyPress(App_InputGetStableKey());
        }
    }
}

void App_MenuInputRun(void)
{
    App_MenuPollInput(BSP_Delay_GetTick());
}

void App_MenuReturnToTaskList(void)
{
    Menu_ReturnToTaskList(&menuState);
    g_active_task = 0U;
}

/**
 * @brief 运行 OLED 菜单：读取按键、处理菜单输入，并按需刷新动态页面。
 * @param 无。
 * @note 非阻塞周期任务，由 App_Run 或对应调度函数重复调用。
 * @retval 无。
 */
void App_MenuRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    const bool shouldRender = Menu_IsDirty(&menuState) ||
        (Menu_IsDynamicPage(&menuState) &&
         ((uint32_t)(now - lastDisplayTick) >=
          APP_MENU_DYNAMIC_DISPLAY_PERIOD_MS));

    App_MenuPollInput(now);

    if (!shouldRender && !Menu_IsDirty(&menuState)) {
        return;
    }

    /* 只在即将重绘时换算显示数据，避免主循环空转时反复做软件浮点运算。 */
    App_UpdateCarSpeedDisplay();
    menuData.encoderLeft = Encoder_CumulativeL;
    menuData.encoderRight = Encoder_CumulativeR;
    lastDisplayTick = now;
    Menu_Render(&menuState, &menuData);
}

/* 更新参数查询页显示的 PID 参数，单位为 0.01。 */
void App_MenuSetPidData(int16_t kpHundredths, int16_t kiHundredths,
                        int16_t kdHundredths)
{
    menuData.pidKpHundredths = kpHundredths;
    menuData.pidKiHundredths = kiHundredths;
    menuData.pidKdHundredths = kdHundredths;
    menuData.pidValid = true;
}

/**
 * @brief 更新参数查询页显示的小车目标速度和实际速度，单位为 0.1。
 * @param targetTenths targetTenths 参数。
 * @param actualTenths actualTenths 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuSetCarSpeedData(int16_t targetTenths, int16_t actualTenths)
{
    menuData.targetSpeedTenths = targetTenths;
    menuData.actualSpeedTenths = actualTenths;
    menuData.carSpeedValid = true;
}

/* 更新小车状态页显示的 IMU 姿态角，单位为 0.1 度。 */
void App_MenuSetImuData(int16_t yawTenths, int16_t pitchTenths,
                        int16_t rollTenths)
{
    menuData.yawTenths = yawTenths;
    menuData.pitchTenths = pitchTenths;
    menuData.rollTenths = rollTenths;
    menuData.imuValid = true;
}

/**
 * @brief 更新小车状态页显示的左右轮速度，单位为 0.1。
 * @param leftTenths leftTenths 参数。
 * @param rightTenths rightTenths 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuSetSpeedData(int16_t leftTenths, int16_t rightTenths)
{
    menuData.leftSpeedTenths = leftTenths;
    menuData.rightSpeedTenths = rightTenths;
    menuData.speedValid = true;
}

void App_MenuSetTaskTime(uint16_t seconds)
{
    menuData.taskTimeSeconds = seconds;
}

void App_MenuForceTimerPage(void)
{
    Menu_ForcePage(&menuState, MENU_PAGE_TIMER);
}

/**
 * @brief 更新小车状态页显示的 ToF 距离，单位为 mm。
 * @param distanceMm 输出距离，单位 mm。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuSetTofData(uint16_t distanceMm)
{
    menuData.tofDistanceMm = distanceMm;
    menuData.tofValid = true;
}

/**
 * @brief 更新一级菜单底部显示的电池电压。
 * @param batteryMv 电池端电压，单位 mV。
 * @param batteryLow true 表示电池电压低于提醒阈值。
 * @note 主菜单第四行显示电压；低电量时改为充电提醒。
 * @retval 无。
 */
void App_MenuSetBatteryData(uint16_t batteryMv, bool batteryLow)
{
    menuData.batteryMv = batteryMv;
    menuData.batteryLow = batteryLow;
    menuData.batteryValid = true;
}

/**
 * @brief 标记 IMU 数据当前不可用，菜单状态页会显示 OFF。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuInvalidateImu(void)
{
    menuData.imuValid = false;
}

/**
 * @brief 标记速度数据当前不可用，菜单状态页会显示 OFF。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuInvalidateSpeed(void)
{
    menuData.speedValid = false;
}

/**
 * @brief 标记 Speed 页小车速度数据当前不可用，Speed 页会显示 --。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuInvalidateCarSpeed(void)
{
    menuData.carSpeedValid = false;
}

/**
 * @brief 标记 ToF 数据当前不可用，菜单状态页会显示 OFF。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_MenuInvalidateTof(void)
{
    menuData.tofValid = false;
}
