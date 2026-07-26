#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bluetooth.h"
#include "Encoder.h"
#include "Grayscale_Sensor.h"
#include "delay.h"
#include "line_trace.h"
#include "motor.h"
#include "pid.h"
#include "uart.h"

volatile uint8_t g_vehicle_follow_enabled;  /**< 循迹模式使能标志。 */
volatile uint8_t g_vehicle_last_bt_command; /**< 最近一次蓝牙命令。 */
volatile uint8_t g_vehicle_line_raw;        /**< 灰度传感器原始 8 位值。 */
volatile uint8_t g_vehicle_line_state;      /**< 循迹状态枚举值。 */
volatile int16_t g_vehicle_line_error_tenths; /**< 加权循迹偏差，单位 0.1 路间距，左负右正。 */
volatile uint8_t g_vehicle_line_active_count; /**< 当前检测到黑线的灰度通道数量。 */

static bool vehicleInitialized;                         /**< 车辆外设和控制状态已初始化标志。 */
static float standardSpeed = APP_VEHICLE_DEFAULT_SPEED; /**< 蓝牙手动控制和循迹控制使用的基准速度。 */
static uint32_t lastLineTick;                           /**< 上一次灰度循迹采样时间戳。 */
static uint32_t lastSpeedTick;                          /**< 上一次编码器测速时间戳。 */
static uint32_t lastControlTick;                        /**< 上一次 PID 闭环输出时间戳。 */
static uint32_t lastDebugTick;                          /**< 上一次车辆调试信息输出时间戳。 */

static PID_t leftSpeedPid = {
    /**< 左轮速度闭环 PID 参数和运行状态。 */
    .Target = 0.0f,
    .Kp = 23.0f,
    .Ki = 0.3f,
    .Kd = 0.1f,
    .OutMax = APP_VEHICLE_PID_OUT_MAX,
    .OutMin = APP_VEHICLE_PID_OUT_MIN,
    .ErrMax = APP_VEHICLE_PID_ERR_MAX,
    .DeltaMax = APP_VEHICLE_PID_DELTA_MAX,
};

static PID_t rightSpeedPid = {
    /**< 右轮速度闭环 PID 参数和运行状态。 */
    .Target = 0.0f,
    .Kp = 23.0f,
    .Ki = 0.3f,
    .Kd = 0.1f,
    .OutMax = APP_VEHICLE_PID_OUT_MAX,
    .OutMin = APP_VEHICLE_PID_OUT_MIN,
    .ErrMax = APP_VEHICLE_PID_ERR_MAX,
    .DeltaMax = APP_VEHICLE_PID_DELTA_MAX,
};

/**
 * @brief 对 float 数值做上下限裁剪。
 * @param value 待裁剪数值。
 * @param minValue 最小值。
 * @param maxValue 最大值。
 * @retval 裁剪后的数值。
 */
static float App_VehicleClampFloat(float value, float minValue, float maxValue)
{
    if (value > maxValue)
    {
        return maxValue;
    }
    if (value < minValue)
    {
        return minValue;
    }
    return value;
}

/**
 * @brief 将浮点速度转换为菜单使用的 0.1 单位整数。
 * @param speed 待显示的速度值。
 * @note 四舍五入到 0.1 单位，便于 OLED 菜单用整数显示。
 * @retval 转换后的 0.1 单位速度。
 */
static int16_t App_VehicleSpeedToTenths(float speed)
{
    float tenths = speed * 10.0f;

    /* 正负数分别补偿 0.5，避免强制转换时总是向 0 截断。 */
    tenths += (tenths >= 0.0f) ? 0.5f : -0.5f;
    return (int16_t)tenths;
}

/**
 * @brief 将当前目标速度和实测速度发布到菜单显示数据。
 * @param 无。
 * @note Speed 页显示车辆平均速度，状态页显示左右轮独立速度。
 * @retval 无。
 */
static void App_VehiclePublishSpeed(void)
{
    const float targetSpeed = (leftSpeedPid.Target + rightSpeedPid.Target) * 0.5f;
    const float actualSpeed = (Motor1_Speed + Motor2_Speed) * 0.5f;

    App_MenuSetCarSpeedData(App_VehicleSpeedToTenths(targetSpeed),
                            App_VehicleSpeedToTenths(actualSpeed));
    App_MenuSetSpeedData(App_VehicleSpeedToTenths(Motor1_Speed),
                         App_VehicleSpeedToTenths(Motor2_Speed));
}

/**
 * @brief 设置左右轮速度闭环目标值。
 * @param leftTarget 左轮目标速度。
 * @param rightTarget 右轮目标速度。
 * @note 仅更新 PID 目标，不直接写 PWM；实际输出由 App_VehicleControlRun() 周期完成。
 * @retval 无。
 */
static void App_VehicleSetTarget(float leftTarget, float rightTarget)
{
    leftSpeedPid.Target = leftTarget;
    rightSpeedPid.Target = rightTarget;
    App_VehiclePublishSpeed();
}

/**
 * @brief 清零 PID 状态并立即停车。
 * @param 无。
 * @note 蓝牙停止、循迹关闭和异常状态都通过这里清除积分和输出，避免重新启动时冲击。
 * @retval 无。
 */
static void App_VehicleStop(void)
{
    /* 停车时同步清目标、实际、输出和历史误差，避免积分残留。 */
    leftSpeedPid.Target = 0.0f;
    leftSpeedPid.Actual = 0.0f;
    leftSpeedPid.Out = 0.0f;
    leftSpeedPid.Error0 = 0.0f;
    leftSpeedPid.Error1 = 0.0f;
    leftSpeedPid.ErrorInt = 0.0f;
    leftSpeedPid.Deriv = 0.0f;

    rightSpeedPid.Target = 0.0f;
    rightSpeedPid.Actual = 0.0f;
    rightSpeedPid.Out = 0.0f;
    rightSpeedPid.Error0 = 0.0f;
    rightSpeedPid.Error1 = 0.0f;
    rightSpeedPid.ErrorInt = 0.0f;
    rightSpeedPid.Deriv = 0.0f;

    Set_Speed(0, 0);
    App_VehiclePublishSpeed();
}

/**
 * @brief 初始化蓝牙、编码器和车辆控制状态。
 * @param 无。
 * @note 首次调用完成 UART2、编码器和 PID 停车初始化；后续调用直接返回。
 * @retval 无。
 */
void App_VehicleInit(void)
{
    if (vehicleInitialized)
    {
        return;
    }

    uart0_send_string("Vehicle init begin\r\n");
    uart2_init();
    uart0_send_string("Vehicle uart2 OK\r\n");
    Encoder_Init();
    uart0_send_string("Vehicle encoder OK\r\n");
    App_VehicleStop();

    vehicleInitialized = true;
    uart0_send_string("Vehicle init OK\r\n");
}

/**
 * @brief 将蓝牙收到的字节转换为统一命令编号。
 * @param rawCommand UART2 收到的原始字节。
 * @note 兼容二进制 1~7 和手机蓝牙助手常见的 ASCII '1'~'7'。
 * @retval 1~7 为有效命令，0 表示无效命令。
 */
uint8_t App_VehicleNormalizeBluetoothCommand(uint8_t rawCommand)
{
    if ((rawCommand >= 1U) && (rawCommand <= 7U))
    {
        return rawCommand;
    }

    if ((rawCommand >= (uint8_t)'1') && (rawCommand <= (uint8_t)'7'))
    {
        return (uint8_t)(rawCommand - (uint8_t)'0');
    }

    return 0U;
}

/**
 * @brief 处理蓝牙运动命令。
 * @param 无。
 * @note 命令 1 前进、2 后退、3 停止、4 右转、5 左转、6 循迹开、7 循迹关。
 * @retval 无。
 */
void App_BluetoothRun(void)
{
    uint8_t rawCommand;
    uint8_t command;
    char message[48];

    if (Bluetooth_ReadByte(&rawCommand) == 0U)
    {
        return;
    }

    command = App_VehicleNormalizeBluetoothCommand(rawCommand);
    g_vehicle_last_bt_command = command;

    /* 手动运动命令会关闭循迹；只有命令 6 会把目标速度交给灰度循迹任务。 */
    switch (command)
    {
    case 1U:
        g_vehicle_follow_enabled = 0U;
        App_VehicleSetTarget(standardSpeed, standardSpeed);
        break;
    case 2U:
        g_vehicle_follow_enabled = 0U;
        App_VehicleSetTarget(-standardSpeed, -standardSpeed);
        break;
    case 3U:
        g_vehicle_follow_enabled = 0U;
        App_VehicleStop();
        break;
    case 4U:
        g_vehicle_follow_enabled = 0U;
        App_VehicleSetTarget(standardSpeed, 0.0f);
        break;
    case 5U:
        g_vehicle_follow_enabled = 0U;
        App_VehicleSetTarget(0.0f, standardSpeed);
        break;
    case 6U:
        g_vehicle_follow_enabled = 1U;
        break;
    case 7U:
        g_vehicle_follow_enabled = 0U;
        App_VehicleStop();
        break;
    default:
        break;
    }

    (void)snprintf(message, sizeof(message), "BT RAW=%u CMD=%u FOLLOW=%u\r\n",
                   (unsigned int)rawCommand,
                   (unsigned int)command,
                   (unsigned int)g_vehicle_follow_enabled);
    uart0_send_string(message);
}

/**
 * @brief 兼容旧工程的蓝牙任务函数名。
 * @param 无。
 * @note 新代码推荐直接调用 App_BluetoothRun()，保留本函数避免旧模板调用失效。
 * @retval 无。
 */
void bluetooth_work(void)
{
    App_BluetoothRun();
}

/**
 * @brief 根据加权循迹偏差更新左右轮目标速度。
 * @param errorTenths 加权偏差，单位 0.1 路间距；左负右正。
 * @note 偏差为正表示黑线在右侧，左轮加速、右轮减速，让车向右修正。
 * @retval 无。
 */
static void App_VehicleApplyLineError(int16_t errorTenths)
{
    float correction = (float)errorTenths * APP_VEHICLE_LINE_DIFF_GAIN;

    correction = App_VehicleClampFloat(correction,
                                       -APP_VEHICLE_LINE_DIFF_MAX,
                                       APP_VEHICLE_LINE_DIFF_MAX);

    /*
     * 这里是照片里的“normalize → 外环输出目标”的落地版本：
     * 灰度加权偏差只生成左右轮目标差速，实际 PWM 仍由后面的速度 PID 闭环计算。
     */
    App_VehicleSetTarget(standardSpeed + correction, standardSpeed - correction);
}

/**
 * @brief 运行灰度循迹采样和目标速度更新任务。
 * @param 无。
 * @note 按 APP_VEHICLE_LINE_PERIOD_MS 读取 74HC165 灰度值；循迹模式关闭时只刷新诊断状态，不接管目标速度。
 * @retval 无。
 */
void App_LineTraceRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    LineTrace_State state;
    int16_t errorTenths = 0;
    uint8_t activeCount = 0U;
    uint8_t hasLine;

    if ((uint32_t)(now - lastLineTick) < APP_VEHICLE_LINE_PERIOD_MS)
    {
        return;
    }
    lastLineTick = now;

    Grayscale_Read();
    g_vehicle_line_raw = Grayscale_GetRaw();
    state = LineTrace_DecodeActiveLowRaw(g_vehicle_line_raw);
    hasLine = LineTrace_CalcActiveLowWeightedError(g_vehicle_line_raw, &errorTenths, &activeCount);
    g_vehicle_line_state = (uint8_t)state;
    g_vehicle_line_error_tenths = errorTenths;
    g_vehicle_line_active_count = activeCount;

    /* 蓝牙命令 6 打开循迹后，加权偏差才会覆盖左右轮目标速度。 */
    if (g_vehicle_follow_enabled != 0U)
    {
        if (hasLine != 0U)
        {
            App_VehicleApplyLineError(errorTenths);
        }
        else
        {
            App_VehicleStop();
        }
    }
}

/**
 * @brief 运行编码器测速和速度 PID 闭环输出。
 * @param 无。
 * @note 速度采样和 PID 周期由 APP_VEHICLE_SPEED_PERIOD_MS、APP_VEHICLE_CONTROL_PERIOD_MS 配置。
 * @retval 无。
 */
void App_VehicleControlRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();

    if ((uint32_t)(now - lastSpeedTick) >= APP_VEHICLE_SPEED_PERIOD_MS)
    {
        lastSpeedTick = now;
        MEASURE_MOTORS_SPEED();
    }

    if ((uint32_t)(now - lastControlTick) >= APP_VEHICLE_CONTROL_PERIOD_MS)
    {
        lastControlTick = now;

        /* PID 输入来自编码器测速，目标来自蓝牙手动命令或循迹状态机。 */
        leftSpeedPid.Actual = Motor1_Speed;
        rightSpeedPid.Actual = Motor2_Speed;
        PID_Update(&leftSpeedPid);
        PID_Update(&rightSpeedPid);
        Set_Speed((int)(leftSpeedPid.Out), (int)(rightSpeedPid.Out));
        App_VehiclePublishSpeed();
    }
}

/**
 * @brief 周期输出车辆调试信息。
 * @param 无。
 * @note 通过 UART0 观察蓝牙命令、循迹原始值、目标速度、实际速度和按键采样计数。
 * @retval 无。
 */
static void App_VehicleDebugRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    char message[160];

    if ((uint32_t)(now - lastDebugTick) < APP_VEHICLE_DEBUG_PERIOD_MS)
    {
        return;
    }
    lastDebugTick = now;

    (void)snprintf(message, sizeof(message),
                   "VEH raw=0x%02X gs=%u%u%u%u%u%u%u%u state=%s err=%d cnt=%u follow=%u tgt=%d act=%d key=%u ks=%lu\r\n",
                   (unsigned int)g_vehicle_line_raw,
                   (unsigned int)((g_vehicle_line_raw >> 7) & 0x01U),
                   (unsigned int)((g_vehicle_line_raw >> 6) & 0x01U),
                   (unsigned int)((g_vehicle_line_raw >> 5) & 0x01U),
                   (unsigned int)((g_vehicle_line_raw >> 4) & 0x01U),
                   (unsigned int)((g_vehicle_line_raw >> 3) & 0x01U),
                   (unsigned int)((g_vehicle_line_raw >> 2) & 0x01U),
                   (unsigned int)((g_vehicle_line_raw >> 1) & 0x01U),
                   (unsigned int)(g_vehicle_line_raw & 0x01U),
                   LineTrace_StateName((LineTrace_State)g_vehicle_line_state),
                   (int)g_vehicle_line_error_tenths,
                   (unsigned int)g_vehicle_line_active_count,
                   (unsigned int)g_vehicle_follow_enabled,
                   (int)App_VehicleSpeedToTenths((leftSpeedPid.Target + rightSpeedPid.Target) * 0.5f),
                   (int)App_VehicleSpeedToTenths((Motor1_Speed + Motor2_Speed) * 0.5f),
                   (unsigned int)App_InputGetLastAdc(),
                   (unsigned long)g_app_sample_count);
    uart0_send_string(message);
}

/**
 * @brief 车辆总任务调度入口。
 * @param 无。
 * @note 在 App_Run 中保留本调用即可启用蓝牙命令、灰度循迹、编码器测速和 PID 闭环。
 * @retval 无。
 */
void App_VehicleRun(void)
{
    App_VehicleInit();
    App_BluetoothRun();
    App_LineTraceRun();
    App_VehicleControlRun();
    App_VehicleDebugRun();
}
