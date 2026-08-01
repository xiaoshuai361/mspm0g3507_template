#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdio.h>

#include "bsp_adc.h"
#include "delay.h"
#include "key5d.h"
#include "oled.h"
#include "ti_msp_dl_config.h"
#include "uart.h"

volatile uint32_t g_app_sample_count; /**< 按键采样累计次数。 */

static Key5D_State keyState; /**< keyState 全局状态或配置变量。 */
static Key5D_Diagnostic keyDiagnostic; /**< keyDiagnostic 全局状态或配置变量。 */
static uint16_t lastAdcValue = 4095U; /**< lastAdcValue 全局状态或配置变量。 */
static uint32_t lastSampleTick; /**< lastSampleTick 全局状态或配置变量。 */
static uint32_t lastDisplayTick; /**< lastDisplayTick 全局状态或配置变量。 */
static uint32_t lastKeySerialTick; /**< lastKeySerialTick 全局状态或配置变量。 */
static bool keyTestScreenDirty = true; /**< keyTestScreenDirty 全局状态或配置变量。 */

/**
 * @brief 初始化五向按键状态机和 ADC 诊断数据。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_InputInit(void)
{
    Key5D_Init(&keyState);
    Key5D_DiagnosticInit(&keyDiagnostic, 4095U);
}

/**
 * @brief 每 10 ms 读取一次按键 ADC，更新消抖状态并返回是否完成采样。
 * @param now 当前系统毫秒 tick。
 * @param event 输出按键边沿事件。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval true 表示成功或满足条件，false 表示失败或未满足。
 */
bool App_InputPoll(uint32_t now, Key5D_Event *event)
{
    if ((uint32_t)(now - lastSampleTick) < APP_KEY_SAMPLE_PERIOD_MS)
    {
        return false;
    }

    lastSampleTick = now;
    lastAdcValue = BSP_ADC_KeyReadRaw();
    g_app_sample_count++;

    *event = Key5D_Update(&keyState, lastAdcValue);
    Key5D_DiagnosticUpdate(&keyDiagnostic, lastAdcValue, keyState.candidate,
                           keyState.stable, keyState.consecutiveSamples,
                           *event);
    return true;
}

/**
 * @brief 返回当前已经消抖确认的五向按键。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回五向按键枚举。
 */
Key5D_Key App_InputGetStableKey(void)
{
    return Key5D_GetKey(&keyState);
}

/**
 * @brief 返回最近一次采样到的按键 ADC 原始值。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 16 位数据。
 */
uint16_t App_InputGetLastAdc(void)
{
    return lastAdcValue;
}

/**
 * @brief 返回按键诊断信息，供 OLED 测试页显示 RAW/NOW/STABLE 等状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回诊断结构体指针。
 */
const Key5D_Diagnostic *App_InputGetDiagnostic(void)
{
    return &keyDiagnostic;
}

/**
 * @brief 把五向按键方向映射为菜单控制：左返回、右进入、上下选择。
 * @param key 按键方向。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回菜单输入事件。
 */
Menu_Input App_InputToMenu(Key5D_Key key)
{
    switch (key)
    {
    case KEY5D_KEY_UP:
        return MENU_INPUT_UP;
    case KEY5D_KEY_DOWN:
        return MENU_INPUT_DOWN;
    case KEY5D_KEY_LEFT:
        return MENU_INPUT_BACK;
    case KEY5D_KEY_RIGHT:
        return MENU_INPUT_ENTER;
    case KEY5D_KEY_CENTER:
    case KEY5D_KEY_NONE:
    default:
        return MENU_INPUT_NONE;
    }
}

/**
 * @brief 串口输出按键名和 ADC 值，并翻转 LED 作为按下反馈。
 * @param key 按键方向。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void App_LogKeyPress(Key5D_Key key)
{
    (void)key;
    DL_GPIO_togglePins(LED_PORT, LED_PIN_22_PIN);
}

/**
 * @brief 刷新五向按键 OLED 测试页，用于校准 ADC 窗口和确认消抖状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_RenderKey5DTest(void)
{
    const Key5D_Diagnostic *diagnostic = App_InputGetDiagnostic();
    char line[22];

    OLED_ClearBuffer();
    OLED_ShowString(0U, 0U, "KEY5D ADC TEST", 8U, 1U);

    (void)snprintf(line, sizeof(line), "RAW: %u",
                   (unsigned int)diagnostic->rawAdc);
    OLED_ShowString(0U, 8U, line, 8U, 1U);
    (void)snprintf(line, sizeof(line), "NOW: %s",
                   Key5D_GetName(diagnostic->instantKey));
    OLED_ShowString(0U, 16U, line, 8U, 1U);
    (void)snprintf(line, sizeof(line), "STABLE: %s",
                   Key5D_GetName(diagnostic->stableKey));
    OLED_ShowString(0U, 24U, line, 8U, 1U);
    (void)snprintf(line, sizeof(line), "CAND:%s N:%u",
                   Key5D_GetName(diagnostic->candidateKey),
                   (unsigned int)diagnostic->consecutiveSamples);
    OLED_ShowString(0U, 32U, line, 8U, 1U);
    (void)snprintf(line, sizeof(line), "LAST: %s",
                   Key5D_GetEventName(diagnostic->lastEvent));
    OLED_ShowString(0U, 40U, line, 8U, 1U);
    (void)snprintf(line, sizeof(line), "MIN: %u",
                   (unsigned int)diagnostic->minimumAdc);
    OLED_ShowString(0U, 48U, line, 8U, 1U);
    (void)snprintf(line, sizeof(line), "MAX: %u",
                   (unsigned int)diagnostic->maximumAdc);
    OLED_ShowString(0U, 56U, line, 8U, 1U);
    OLED_Refresh();
}

/**
 * @brief 运行五向按键测试模式：OLED 显示状态，UART0 周期输出诊断数据。
 * @param 无。
 * @note 非阻塞周期任务，由 App_Run 或对应调度函数重复调用。
 * @retval 无。
 */
void App_Key5DTestRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    Key5D_Event event = KEY5D_EVENT_NONE;
    char message[64];

    if (App_InputPoll(now, &event))
    {
        keyTestScreenDirty = true;
        if (event == KEY5D_EVENT_PRESSED)
        {
            App_LogKeyPress(App_InputGetStableKey());
        }
    }

    if ((uint32_t)(now - lastKeySerialTick) >= APP_KEY_TEST_SERIAL_PERIOD_MS)
    {
        const Key5D_Diagnostic *diagnostic = App_InputGetDiagnostic();

        lastKeySerialTick = now;
        (void)snprintf(message, sizeof(message),
                       "ADC=%u NOW=%s STABLE=%s\r\n",
                       (unsigned int)diagnostic->rawAdc,
                       Key5D_GetName(diagnostic->instantKey),
                       Key5D_GetName(diagnostic->stableKey));
        uart0_send_string(message);
    }

    if (keyTestScreenDirty &&
        ((uint32_t)(now - lastDisplayTick) >= APP_KEY_TEST_DISPLAY_PERIOD_MS))
    {
        lastDisplayTick = now;
        keyTestScreenDirty = false;
        App_RenderKey5DTest();
    }
}
