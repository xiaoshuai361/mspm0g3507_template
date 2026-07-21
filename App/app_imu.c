#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdio.h>

#include "IMU.h"
#include "delay.h"
#include "uart.h"

volatile uint8_t g_imu_init_complete; /**< IMU 初始化流程完成标志。 */
volatile uint8_t g_imu_init_ok; /**< IMU 初始化成功标志。 */
volatile uint32_t g_imu_update_count; /**< IMU 有效采样帧计数。 */
volatile int16_t g_imu_yaw_tenths; /**< yaw 角，单位 0.1 度。 */
volatile int16_t g_imu_pitch_tenths; /**< pitch 角，单位 0.1 度。 */
volatile int16_t g_imu_roll_tenths; /**< roll 角，单位 0.1 度。 */
volatile uint8_t g_imu_last_bus_status; /**< IMU 最近一次 I2C 状态。 */
volatile uint8_t g_imu_who_am_i; /**< IMU WHO_AM_I 寄存器值。 */
volatile uint32_t g_imu_read_error_count; /**< g_imu_read_error_count 全局状态或配置变量。 */
volatile uint32_t g_imu_zero_frame_count; /**< g_imu_zero_frame_count 全局状态或配置变量。 */
volatile uint32_t g_imu_diagnostic_frame_count; /**< g_imu_diagnostic_frame_count 全局状态或配置变量。 */
volatile uint8_t g_imu_probe68_status; /**< g_imu_probe68_status 全局状态或配置变量。 */
volatile uint8_t g_imu_probe68_who; /**< g_imu_probe68_who 全局状态或配置变量。 */
volatile uint8_t g_imu_probe69_status; /**< g_imu_probe69_status 全局状态或配置变量。 */
volatile uint8_t g_imu_probe69_who; /**< g_imu_probe69_who 全局状态或配置变量。 */

static uint32_t lastImuTick; /**< lastImuTick 全局状态或配置变量。 */
static uint32_t lastImuDiagnosticTick; /**< lastImuDiagnosticTick 全局状态或配置变量。 */

/**
 * @brief 组织并输出 IMU 初始化结果，包括探测地址、WHO_AM_I 和总线状态。
 * @param message message 参数。
 * @param messageSize messageSize 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_LogImuInitResult(char *message, size_t messageSize)
{
    if (g_imu_init_ok != 0U)
    {
        (void)snprintf(message, messageSize,
                       "IMU INIT OK WHO=0x%02X BUS=%u A68:S%u/W%02X A69:S%u/W%02X\r\n",
                       (unsigned int)g_imu_who_am_i,
                       (unsigned int)g_imu_last_bus_status,
                       (unsigned int)g_imu_probe68_status,
                       (unsigned int)g_imu_probe68_who,
                       (unsigned int)g_imu_probe69_status,
                       (unsigned int)g_imu_probe69_who);
    }
    else
    {
        App_MenuInvalidateImu();
        (void)snprintf(message, messageSize,
                       "IMU INIT FAIL WHO=0x%02X BUS=%u A68:S%u/W%02X A69:S%u/W%02X\r\n",
                       (unsigned int)g_imu_who_am_i,
                       (unsigned int)g_imu_last_bus_status,
                       (unsigned int)g_imu_probe68_status,
                       (unsigned int)g_imu_probe68_who,
                       (unsigned int)g_imu_probe69_status,
                       (unsigned int)g_imu_probe69_who);
    }
    uart0_send_string(message);
}

/**
 * @brief IMU 首次启用流程：探测 0x68/0x69，初始化模块并记录诊断状态。
 * @param now 当前系统毫秒 tick。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_InitImuOnce(uint32_t now)
{
    uint8_t probeWho = 0U;
    char message[160];

    g_imu_init_complete = 1U;
    g_imu_probe68_status = IMU_ProbeAddress(0x68U, &probeWho);
    g_imu_probe68_who = probeWho;
    probeWho = 0U;
    g_imu_probe69_status = IMU_ProbeAddress(0x69U, &probeWho);
    g_imu_probe69_who = probeWho;
    g_imu_init_ok = IMU_init() ? 1U : 0U;
    g_imu_last_bus_status = IMU_GetLastBusStatus();
    g_imu_who_am_i = IMU_GetWhoAmI();
    lastImuTick = now;
    lastImuDiagnosticTick = now;

    App_LogImuInitResult(message, sizeof(message));
}

/**
 * @brief 初始化失败后有限次数输出诊断信息，避免串口被错误日志刷屏。
 * @param now 当前系统毫秒 tick。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_LogImuInitFailure(uint32_t now)
{
    char message[160];

    if ((g_imu_diagnostic_frame_count >= APP_IMU_DIAGNOSTIC_FRAME_COUNT) ||
        ((uint32_t)(now - lastImuDiagnosticTick) < APP_IMU_DIAGNOSTIC_PERIOD_MS))
    {
        return;
    }

    lastImuDiagnosticTick = now;
    g_imu_diagnostic_frame_count++;
    (void)snprintf(message, sizeof(message),
                   "IMU INIT FAIL %lu/%u A68:S%u/W%02X A69:S%u/W%02X FINAL:S%u/W%02X\r\n",
                   (unsigned long)g_imu_diagnostic_frame_count,
                   (unsigned int)APP_IMU_DIAGNOSTIC_FRAME_COUNT,
                   (unsigned int)g_imu_probe68_status,
                   (unsigned int)g_imu_probe68_who,
                   (unsigned int)g_imu_probe69_status,
                   (unsigned int)g_imu_probe69_who,
                   (unsigned int)g_imu_last_bus_status,
                   (unsigned int)g_imu_who_am_i);
    uart0_send_string(message);
}

/**
 * @brief 根据 IMU 读取结果更新菜单数据；失败时统计 I2C 错误或全零帧。
 * @param frameValid frameValid 参数。
 * @param angles angles 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void App_UpdateImuMenu(bool frameValid, const float *angles)
{
    if (frameValid)
    {
        g_imu_yaw_tenths = IMU_DegreesToTenths(angles[0]);
        g_imu_pitch_tenths = IMU_DegreesToTenths(angles[1]);
        g_imu_roll_tenths = IMU_DegreesToTenths(angles[2]);
        g_imu_update_count++;

        App_MenuSetImuData(g_imu_yaw_tenths, g_imu_pitch_tenths,
                           g_imu_roll_tenths);
        return;
    }

    App_MenuInvalidateImu();
    if (g_imu_last_bus_status != 0U)
    {
        g_imu_read_error_count++;
    }
    else
    {
        g_imu_zero_frame_count++;
    }
}

/* 有限次数输出 IMU 帧诊断，帮助判断是 I2C 错误还是传感器数据异常。 */
static void App_LogImuFrame(uint32_t now, bool frameValid,
                            const float *sensorData)
{
    char message[160];

    if ((g_imu_diagnostic_frame_count >= APP_IMU_DIAGNOSTIC_FRAME_COUNT) ||
        ((uint32_t)(now - lastImuDiagnosticTick) < APP_IMU_DIAGNOSTIC_PERIOD_MS))
    {
        return;
    }

    lastImuDiagnosticTick = now;
    g_imu_diagnostic_frame_count++;

    if (g_imu_last_bus_status != 0U)
    {
        (void)snprintf(message, sizeof(message),
                       "IMU READ FAIL %lu/%u BUS=%u ERR=%lu\r\n",
                       (unsigned long)g_imu_diagnostic_frame_count,
                       (unsigned int)APP_IMU_DIAGNOSTIC_FRAME_COUNT,
                       (unsigned int)g_imu_last_bus_status,
                       (unsigned long)g_imu_read_error_count);
    }
    else if (!frameValid)
    {
        (void)snprintf(message, sizeof(message),
                       "IMU ALL ZERO %lu/%u BUS=0 ZERO=%lu\r\n",
                       (unsigned long)g_imu_diagnostic_frame_count,
                       (unsigned int)APP_IMU_DIAGNOSTIC_FRAME_COUNT,
                       (unsigned long)g_imu_zero_frame_count);
    }
    else
    {
        (void)snprintf(message, sizeof(message),
                       "IMU FRAME %lu/%u A(mg)=%ld,%ld,%ld G(0.1dps)=%d,%d,%d YPR(0.1deg)=%d,%d,%d\r\n",
                       (unsigned long)g_imu_diagnostic_frame_count,
                       (unsigned int)APP_IMU_DIAGNOSTIC_FRAME_COUNT,
                       (long)sensorData[0], (long)sensorData[1],
                       (long)sensorData[2],
                       (int)IMU_DegreesToTenths(sensorData[3]),
                       (int)IMU_DegreesToTenths(sensorData[4]),
                       (int)IMU_DegreesToTenths(sensorData[5]),
                       (int)g_imu_yaw_tenths,
                       (int)g_imu_pitch_tenths,
                       (int)g_imu_roll_tenths);
    }
    uart0_send_string(message);
}

/**
 * @brief IMU 周期任务：首次初始化，之后每 10 ms 读取姿态并更新菜单。
 * @param 无。
 * @note 非阻塞周期任务，由 App_Run 或对应调度函数重复调用。
 * @retval 无。
 */
void App_ImuRun(void)
{
    float angles[3];
    float sensorData[6];
    bool frameValid = false;
    const uint32_t now = BSP_Delay_GetTick();

    if (g_imu_init_complete == 0U)
    {
        App_InitImuOnce(now);
        return;
    }

    if (g_imu_init_ok == 0U)
    {
        App_LogImuInitFailure(now);
        return;
    }

    if ((uint32_t)(now - lastImuTick) < APP_IMU_SAMPLE_PERIOD_MS)
    {
        return;
    }

    lastImuTick = now;
    frameValid = IMU_getYawPitchRoll(angles);
    IMU_GetLastSensorData(sensorData);
    g_imu_last_bus_status = IMU_GetLastBusStatus();

    App_UpdateImuMenu(frameValid, angles);
    App_LogImuFrame(now, frameValid, sensorData);
}
