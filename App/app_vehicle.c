#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bluetooth.h"
#include "Encoder.h"
#include "delay.h"
#include "motor.h"
#include "pid.h"
#include "uart.h"

volatile uint8_t g_vehicle_last_bt_command; /**< 最近一次蓝牙命令。 */
volatile uint8_t g_vehicle_line_raw;
volatile int16_t g_vehicle_line_error_tenths;

static bool vehicleInitialized;                         /**< 车辆外设和控制状态已初始化标志。 */
static bool speedLoopEnabled;                           /**< 左右轮速度闭环输出使能标志。 */
static bool activeBrakeEnabled;
static float standardSpeed = APP_VEHICLE_DEFAULT_SPEED; /**< 蓝牙手动控制和循迹控制使用的基准速度。 */
static float lineKp = APP_LINE_DEFAULT_KP;
static uint32_t lastSpeedTick;                          /**< 上一次编码器测速时间戳。 */
static uint32_t lastControlTick;                        /**< 上一次 PID 闭环输出时间戳。 */

static PID_t leftSpeedPid = {
    /**< 左轮速度闭环 PID 参数和运行状态。 */
    .Target = 0.0f,
    .Kp = 11.5f,
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
    .Kp = 11.5f,
    .Ki = 0.3f,
    .Kd = 0.1f,
    .OutMax = APP_VEHICLE_PID_OUT_MAX,
    .OutMin = APP_VEHICLE_PID_OUT_MIN,
    .ErrMax = APP_VEHICLE_PID_ERR_MAX,
    .DeltaMax = APP_VEHICLE_PID_DELTA_MAX,
};

enum {
    APP_VOFA_COMMAND_SIZE = 32U
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

static float App_VehicleAbsFloat(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static void App_VehicleResetPid(PID_t *pid)
{
    pid->Target = 0.0f;
    pid->Actual = 0.0f;
    pid->Out = 0.0f;
    pid->Error0 = 0.0f;
    pid->Error1 = 0.0f;
    pid->ErrorInt = 0.0f;
    pid->Deriv = 0.0f;
}

static uint8_t App_VehicleAsciiUpper(uint8_t ch)
{
    if ((ch >= (uint8_t)'a') && (ch <= (uint8_t)'z'))
    {
        ch = (uint8_t)(ch - ((uint8_t)'a' - (uint8_t)'A'));
    }
    return ch;
}

static bool App_VehicleParseFloat(const char *text, float *value)
{
    float parsed = 0.0f;
    float fractionScale = 0.1f;
    bool negative = false;
    bool hasDigit = false;

    if ((text == NULL) || (value == NULL))
    {
        return false;
    }

    while ((*text == ' ') || (*text == '\t'))
    {
        text++;
    }
    if ((*text == '+') || (*text == '-'))
    {
        negative = (*text == '-');
        text++;
    }
    while ((*text >= '0') && (*text <= '9'))
    {
        parsed = (parsed * 10.0f) + (float)(*text - '0');
        hasDigit = true;
        text++;
    }
    if (*text == '.')
    {
        text++;
        while ((*text >= '0') && (*text <= '9'))
        {
            parsed += (float)(*text - '0') * fractionScale;
            fractionScale *= 0.1f;
            hasDigit = true;
            text++;
        }
    }
    while ((*text == ' ') || (*text == '\t'))
    {
        text++;
    }
    if (!hasDigit || (*text != '\0'))
    {
        return false;
    }

    *value = negative ? -parsed : parsed;
    return true;
}

static int16_t App_VehicleGainToHundredths(float gain)
{
    return (int16_t)((gain * 100.0f) + 0.5f);
}

static void App_VehiclePublishPidParameters(void)
{
    App_MenuSetPidData(App_VehicleGainToHundredths(leftSpeedPid.Kp),
                       App_VehicleGainToHundredths(leftSpeedPid.Ki),
                       App_VehicleGainToHundredths(leftSpeedPid.Kd));
}

static void App_VehicleApplyVofaCommand(const char *command)
{
    const char *valueText;
    float value;
    uint8_t first;
    uint8_t second;

    if (command == NULL)
    {
        return;
    }

    /* 指定巡线版本使用小写 kp/ki 在线整定灰度外环比例增益。 */
    if ((command[0] == 'k') &&
        ((command[1] == 'p') || (command[1] == 'i'))) {
        if ((command[2] != '=') && (command[2] != ':')) {
            return;
        }
        valueText = &command[3];
        if (App_VehicleParseFloat(valueText, &value) &&
            (value >= 0.0f) && (value <= APP_VOFA_LINE_KP_MAX)) {
            lineKp = value;
        }
        return;
    }

    first = App_VehicleAsciiUpper((uint8_t)command[0]);
    second = App_VehicleAsciiUpper((uint8_t)command[1]);

    if (first == (uint8_t)'S')
    {
        if ((command[1] != '=') && (command[1] != ':'))
        {
            return;
        }
        valueText = &command[2];
    }
    else if ((first == (uint8_t)'K') &&
             ((second == (uint8_t)'P') || (second == (uint8_t)'I') ||
              (second == (uint8_t)'D')))
    {
        if ((command[2] != '=') && (command[2] != ':'))
        {
            return;
        }
        valueText = &command[3];
    }
    else
    {
        return;
    }

    if (!App_VehicleParseFloat(valueText, &value))
    {
        return;
    }

    if ((first == (uint8_t)'K') &&
        ((second == (uint8_t)'P') || (second == (uint8_t)'I') ||
         (second == (uint8_t)'D')) &&
        (value >= 0.0f) && (value <= APP_VOFA_PID_GAIN_MAX))
    {
        if (second == (uint8_t)'P')
        {
            leftSpeedPid.Kp = value;
            rightSpeedPid.Kp = value;
        }
        else if (second == (uint8_t)'I')
        {
            leftSpeedPid.Ki = value;
            rightSpeedPid.Ki = value;
        }
        else
        {
            leftSpeedPid.Kd = value;
            rightSpeedPid.Kd = value;
        }
        App_VehiclePublishPidParameters();
    }
    else if ((first == (uint8_t)'S') &&
             (value >= 0.0f) && (value <= APP_VOFA_SPEED_MAX))
    {
        standardSpeed = value;
        if (value == 0.0f)
        {
            App_VehicleClosedLoopStop();
        }
        else
        {
            App_VehicleClosedLoopSetTarget(value, value);
        }
    }
}

static void App_VehicleVofaReceiveRun(void)
{
    char command[APP_VOFA_COMMAND_SIZE];

    if (vofa_read_command(command, sizeof(command)) != 0U)
    {
        App_VehicleApplyVofaCommand(command);
    }
}

/**
 * @brief 设置左右轮速度闭环目标值。
 * @param leftTarget 左轮目标速度。
 * @param rightTarget 右轮目标速度。
 * @note 仅更新 PID 目标，不直接写 PWM；实际输出由 App_VehicleControlRun() 周期完成。
 * @retval 无。
 */
void App_VehicleClosedLoopSetTarget(float leftTarget, float rightTarget)
{
    leftSpeedPid.Target = App_VehicleClampFloat(
        leftTarget, -APP_VOFA_SPEED_MAX, APP_VOFA_SPEED_MAX);
    rightSpeedPid.Target = App_VehicleClampFloat(
        rightTarget, -APP_VOFA_SPEED_MAX, APP_VOFA_SPEED_MAX);
    speedLoopEnabled = true;
}

void App_VehicleClosedLoopSetActiveBrake(bool enabled)
{
    activeBrakeEnabled = enabled;
}

void App_VehicleClosedLoopDisable(void)
{
    speedLoopEnabled = false;
    activeBrakeEnabled = false;
    App_VehicleResetPid(&leftSpeedPid);
    App_VehicleResetPid(&rightSpeedPid);
}

void App_VehicleClosedLoopStop(void)
{
    App_VehicleClosedLoopDisable();
    Motor_Stop();
}

float App_VehicleClosedLoopGetAverageTarget(void)
{
    return (leftSpeedPid.Target + rightSpeedPid.Target) * 0.5f;
}

float App_VehicleGetLineKp(void)
{
    return lineKp;
}

void App_VehicleSetLineTelemetry(uint8_t raw, int16_t error,
                                 float correction)
{
    g_vehicle_line_raw = raw;
    g_vehicle_line_error_tenths = error;
    (void)correction;
}

/**
 * @brief 切换 Task 5/6 专用速度 PID 参数或恢复默认参数。
 * @param enable true 切换到钢球任务参数，false 恢复默认竞速参数。
 * @note 仅在 App_VehicleInit() 初始化默认参数后调用有效。切换后不重置积分累加项，
 *       由 App_TasksRun() 中 App_VehicleClosedLoopStop() 统一清零。
 * @retval 无。
 */
void App_VehicleSetSpeedPidForTask56(bool enable)
{
    if (enable)
    {
        leftSpeedPid.Kp  = APP_VEHICLE_TASK56_KP_L;
        leftSpeedPid.Ki  = APP_VEHICLE_TASK56_KI_L;
        leftSpeedPid.Kd  = APP_VEHICLE_TASK56_KD_L;
        rightSpeedPid.Kp = APP_VEHICLE_TASK56_KP_R;
        rightSpeedPid.Ki = APP_VEHICLE_TASK56_KI_R;
        rightSpeedPid.Kd = APP_VEHICLE_TASK56_KD_R;
    }
    else
    {
        leftSpeedPid.Kp  = 11.5f;
        leftSpeedPid.Ki  = 0.3f;
        leftSpeedPid.Kd  = 0.1f;
        rightSpeedPid.Kp = 11.5f;
        rightSpeedPid.Ki = 0.3f;
        rightSpeedPid.Kd = 0.1f;
    }
    App_VehiclePublishPidParameters();
}

/**
 * @brief 初始化编码器和车辆控制状态。
 * @param 无。
 * @note UART2由香橙派协议独占；这里只初始化编码器和PID状态。
 * @retval 无。
 */
void App_VehicleInit(void)
{
    if (vehicleInitialized)
    {
        return;
    }

    uart0_send_string("Vehicle init begin\r\n");
    Encoder_Init();
    uart0_send_string("Vehicle encoder OK\r\n");
    App_VehicleClosedLoopStop();
    App_VehiclePublishPidParameters();

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
 * @note 命令 1 前进、2 后退、3 停止、4 右转、5 左转。
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

    switch (command)
    {
    case 1U:
        App_VehicleClosedLoopSetTarget(standardSpeed, standardSpeed);
        break;
    case 2U:
        App_VehicleClosedLoopSetTarget(-standardSpeed, -standardSpeed);
        break;
    case 3U:
        App_VehicleClosedLoopStop();
        break;
    case 4U:
        App_VehicleClosedLoopSetTarget(standardSpeed, 0.0f);
        break;
    case 5U:
        App_VehicleClosedLoopSetTarget(0.0f, standardSpeed);
        break;
    default:
        break;
    }

    (void)snprintf(message, sizeof(message), "BT RAW=%u CMD=%u\r\n",
                   (unsigned int)rawCommand,
                   (unsigned int)command);
    uart0_send_string(message);
}

/**
 * @brief 兼容旧工程的蓝牙任务函数名。
 * @param 无。
 * @note UART2现由香橙派协议独占，仅保留本函数避免旧模板链接失败。
 * @retval 无。
 */
void bluetooth_work(void)
{
    App_BluetoothRun();
}

/**
 * @brief 运行编码器测速和速度 PID 闭环输出。
 * @param 无。
 * @note 编码器始终周期测速；闭环启用后按控制周期读取最新反馈并输出 PWM。
 * @retval 无。
 */
void App_VehicleControlRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();

    App_VehicleVofaReceiveRun();

    if ((uint32_t)(now - lastSpeedTick) >= APP_VEHICLE_SPEED_PERIOD_MS)
    {
        lastSpeedTick = now;
        MEASURE_MOTORS_SPEED();
    }

    if (speedLoopEnabled &&
        ((uint32_t)(now - lastControlTick) >= APP_VEHICLE_CONTROL_PERIOD_MS))
    {
        lastControlTick = now;

        /* PID 输入来自编码器测速，目标来自蓝牙手动命令或循迹状态机。 */
        leftSpeedPid.Actual = Motor1_Speed;
        rightSpeedPid.Actual = Motor2_Speed;
        PID_Update(&leftSpeedPid);
        PID_Update(&rightSpeedPid);
        if (activeBrakeEnabled) {
            const float brakePwmMax = (g_active_task == 6U)
                                          ? APP_TASK2L_ACTIVE_BRAKE_PWM_MAX
                                          : APP_TASK2F_ACTIVE_BRAKE_PWM_MAX;
            const float brakeSteerMax = (g_active_task == 6U)
                                            ? APP_TASK2L_ACTIVE_BRAKE_STEER_MAX
                                            : APP_TASK2F_ACTIVE_BRAKE_STEER_MAX;
            float brakeCommon =
                (App_VehicleAbsFloat(leftSpeedPid.Out) +
                 App_VehicleAbsFloat(rightSpeedPid.Out)) * 0.5f;
            float steering =
                (rightSpeedPid.Out - leftSpeedPid.Out) * 0.5f;
            float steeringLimit;

            brakeCommon = App_VehicleClampFloat(
                brakeCommon, 0.0f, brakePwmMax);
            steeringLimit = App_VehicleClampFloat(
                brakeCommon, 0.0f, brakeSteerMax);
            steering = App_VehicleClampFloat(
                steering, -steeringLimit, steeringLimit);

            Motor_ApplySpeedLoopOutput(
                (int)(-brakeCommon - steering),
                (int)(-brakeCommon + steering));
        } else {
            Motor_ApplySpeedLoopOutput((int)leftSpeedPid.Out,
                                       (int)rightSpeedPid.Out);
        }
    }
}

/**
 * @brief 车辆总任务调度入口。
 * @param 无。
 * @note UART2由香橙派协议独占，此入口不再轮询蓝牙命令。
 * @retval 无。
 */
void App_VehicleRun(void)
{
    App_VehicleInit();
    App_VehicleControlRun();
}
