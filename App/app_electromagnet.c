#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "delay.h"
#include "ti_msp_dl_config.h"
#include "uart.h"

volatile uint8_t g_electromagnet_enabled;      /**< 电磁铁当前输出状态，1 表示 PA26 高电平打开。 */
volatile uint32_t g_electromagnet_toggle_count;/**< PA28 按键触发切换次数。 */
volatile uint8_t g_electromagnet_button_raw;   /**< PA28 独立按键原始按下状态，1 表示按下。 */

static bool electromagnetInitialized;          /**< 电磁铁 App 任务初始化标志。 */
static uint32_t lastElectromagnetKeyTick;      /**< 上一次 PA28 按键扫描时间戳。 */
static uint8_t stablePressed;                  /**< 消抖后的 PA28 稳定按下状态。 */
static uint8_t candidatePressed;               /**< PA28 候选按下状态。 */
static uint8_t consecutiveSamples;             /**< PA28 候选状态连续采样次数。 */

#define APP_ELECTROMAGNET_DEBOUNCE_SAMPLES (3U) /**< PA28 按键消抖采样次数。 */

/**
 * @brief 将当前状态输出到 PA26。
 */
static void App_ElectromagnetApplyOutput(void)
{
    if (g_electromagnet_enabled != 0U)
    {
        DL_GPIO_setPins(Electromagnet_PORT, Electromagnet_MOS_PIN);
    }
    else
    {
        DL_GPIO_clearPins(Electromagnet_PORT, Electromagnet_MOS_PIN);
    }
}

/**
 * @brief 读取 PA28 独立按键是否按下。
 * @note 当前按低有效处理：PA28=0 表示按下，PA28=1 表示松开。
 */
static uint8_t App_ElectromagnetReadButtonPressed(void)
{
    return (DL_GPIO_readPins(Key_PORT, Key_F1_PIN) == 0U) ? 1U : 0U;
}

/**
 * @brief 初始化电磁铁控制任务。
 * @note 初始输出低电平，确保上电默认关闭电磁铁。
 */
void App_ElectromagnetInit(void)
{
    if (electromagnetInitialized)
    {
        return;
    }

    g_electromagnet_enabled = 0U;
    g_electromagnet_toggle_count = 0U;
    g_electromagnet_button_raw = 0U;
    stablePressed = 0U;
    candidatePressed = 0U;
    consecutiveSamples = 0U;
    App_ElectromagnetApplyOutput();
    lastElectromagnetKeyTick = BSP_Delay_GetTick();
    electromagnetInitialized = true;
}

/**
 * @brief 设置电磁铁输出状态。
 * @param enabled true 打开电磁铁，false 关闭电磁铁。
 */
void App_ElectromagnetSetEnabled(bool enabled)
{
    App_ElectromagnetInit();
    g_electromagnet_enabled = enabled ? 1U : 0U;
    App_ElectromagnetApplyOutput();
}

/**
 * @brief 翻转电磁铁输出状态。
 */
void App_ElectromagnetToggle(void)
{
    App_ElectromagnetInit();
    g_electromagnet_enabled = (g_electromagnet_enabled == 0U) ? 1U : 0U;
    App_ElectromagnetApplyOutput();
}

/**
 * @brief 读取电磁铁输出状态。
 * @retval true 表示 PA26 输出高电平打开，false 表示关闭。
 */
bool App_ElectromagnetIsEnabled(void)
{
    App_ElectromagnetInit();
    return (g_electromagnet_enabled != 0U);
}

/**
 * @brief 周期扫描 PA28 独立按键，稳定按下一次就切换一次电磁铁。
 * @note 非阻塞函数，应在 App_Run() 中周期调用。
 */
void App_ElectromagnetRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    char message[48];
    uint8_t pressed;
    bool toggled = false;

    App_ElectromagnetInit();

    if ((uint32_t)(now - lastElectromagnetKeyTick) < APP_ELECTROMAGNET_KEY_PERIOD_MS)
    {
        return;
    }
    lastElectromagnetKeyTick = now;

    pressed = App_ElectromagnetReadButtonPressed();
    g_electromagnet_button_raw = pressed;

    if (pressed != candidatePressed)
    {
        candidatePressed = pressed;
        consecutiveSamples = 1U;
        return;
    }

    if (consecutiveSamples < APP_ELECTROMAGNET_DEBOUNCE_SAMPLES)
    {
        consecutiveSamples++;
    }

    if ((consecutiveSamples >= APP_ELECTROMAGNET_DEBOUNCE_SAMPLES) &&
        (stablePressed != candidatePressed))
    {
        stablePressed = candidatePressed;
        if (stablePressed != 0U)
        {
            App_ElectromagnetToggle();
            g_electromagnet_toggle_count++;
            toggled = true;
        }
    }

    if (toggled)
    {
        (void)snprintf(message, sizeof(message), "MAG=%s CNT=%lu\r\n",
                       g_electromagnet_enabled ? "ON" : "OFF",
                       (unsigned long)g_electromagnet_toggle_count);
        uart0_send_string(message);
    }
}
