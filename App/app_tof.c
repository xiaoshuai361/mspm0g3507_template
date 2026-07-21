#include "app.h"
#include "app_internal.h"

#include <stdio.h>

#include "../Module/ToF/dl1a.h"
#include "delay.h"
#include "uart.h"

volatile uint8_t g_tof_init_complete; /**< ToF 初始化流程完成标志。 */
volatile uint8_t g_tof_init_ok; /**< ToF 初始化成功标志。 */
volatile uint16_t g_tof_distance_mm; /**< ToF 最近距离，单位 mm。 */
volatile uint32_t g_tof_update_count; /**< ToF 成功更新次数。 */
volatile uint8_t g_tof_last_status; /**< ToF 最近一次测距状态。 */
volatile uint8_t g_tof_last_bus_status; /**< ToF 最近一次 I2C 状态。 */

static uint32_t lastTofTick; /**< lastTofTick 全局状态或配置变量。 */
static uint32_t lastTofLogTick; /**< lastTofLogTick 全局状态或配置变量。 */

/**
 * @brief DL1A 首次启用流程：初始化传感器并通过 UART0 输出成功或失败原因。
 * @param now 当前系统毫秒 tick。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_InitTofOnce(uint32_t now)
{
    char message[80];

    g_tof_init_complete = 1U;
    g_tof_init_ok = DL1A_Init() ? 1U : 0U;
    g_tof_last_status = (uint8_t)DL1A_GetLastStatus();
    g_tof_last_bus_status = DL1A_GetLastBusStatus();
    lastTofTick = now;
    lastTofLogTick = now;

    if (g_tof_init_ok != 0U)
    {
        uart0_send_string("DL1A INIT OK\r\n");
    }
    else
    {
        App_MenuInvalidateTof();
        (void)snprintf(message, sizeof(message),
                       "DL1A INIT FAIL STATUS=%u BUS=%u\r\n",
                       (unsigned int)g_tof_last_status,
                       (unsigned int)g_tof_last_bus_status);
        uart0_send_string(message);
    }
}

/**
 * @brief 读取一次 DL1A 距离数据；有效时更新菜单，无效时记录状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_UpdateTofData(void)
{
    if (DL1A_Update() && DL1A_HasNewData())
    {
        g_tof_distance_mm = DL1A_GetDistanceMm();
        g_tof_update_count++;
        App_MenuSetTofData(g_tof_distance_mm);
        DL1A_ClearNewDataFlag();
        return;
    }

    g_tof_last_status = (uint8_t)DL1A_GetLastStatus();
    g_tof_last_bus_status = DL1A_GetLastBusStatus();
    if (g_tof_last_status != (uint8_t)DL1A_STATUS_NOT_READY)
    {
        App_MenuInvalidateTof();
    }
}

/**
 * @brief 周期输出 DL1A 距离、状态码和成功读取次数，便于串口调试。
 * @param now 当前系统毫秒 tick。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_LogTofData(uint32_t now)
{
    char message[80];

    if ((uint32_t)(now - lastTofLogTick) < APP_TOF_LOG_PERIOD_MS)
    {
        return;
    }

    lastTofLogTick = now;
    (void)snprintf(message, sizeof(message),
                   "DL1A DIST=%umm STATUS=%u BUS=%u CNT=%lu\r\n",
                   (unsigned int)g_tof_distance_mm,
                   (unsigned int)DL1A_GetLastStatus(),
                   (unsigned int)DL1A_GetLastBusStatus(),
                   (unsigned long)g_tof_update_count);
    uart0_send_string(message);
}

/**
 * @brief DL1A 周期任务：首次初始化，之后每 20 ms 轮询一次距离。
 * @param 无。
 * @note 非阻塞周期任务，由 App_Run 或对应调度函数重复调用。
 * @retval 无。
 */
void App_ToFRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();

    if (g_tof_init_complete == 0U)
    {
        App_InitTofOnce(now);
        return;
    }

    if (g_tof_init_ok == 0U)
    {
        return;
    }

    if ((uint32_t)(now - lastTofTick) < APP_TOF_POLL_PERIOD_MS)
    {
        return;
    }

    lastTofTick = now;
    App_UpdateTofData();
    App_LogTofData(now);
}
