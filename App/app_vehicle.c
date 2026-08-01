#include "app.h"
#include "app_internal.h"

#include <stdbool.h>
#include <stdint.h>

#include "Encoder.h"
#include "delay.h"
#include "motor.h"
#include "pid.h"
#include "uart.h"

volatile uint8_t g_vehicle_line_raw;
volatile int16_t g_vehicle_line_error_tenths;

static bool vehicleInitialized;
static bool speedLoopEnabled;
static bool activeBrakeEnabled;
static float commandSpeed = APP_VEHICLE_DEFAULT_SPEED;
static float lineKp = APP_LINE_DEFAULT_KP;
static float lineCorrection;
static uint32_t lastSpeedTick;
static uint32_t lastControlTick;
static uint32_t lastTelemetryTick;

static PID_t leftSpeedPid = {
    .Kp = 11.5f,
    .Ki = 0.3f,
    .Kd = 0.1f,
    .OutMax = APP_VEHICLE_PID_OUT_MAX,
    .OutMin = APP_VEHICLE_PID_OUT_MIN,
    .ErrMax = APP_VEHICLE_PID_ERR_MAX,
    .DeltaMax = APP_VEHICLE_PID_DELTA_MAX,
};

static PID_t rightSpeedPid = {
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

static float App_VehicleClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
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

static bool App_VehicleParseFloat(const char *text, float *value)
{
    float parsed = 0.0f;
    float fractionScale = 0.1f;
    bool negative = false;
    bool hasDigit = false;

    if ((text == 0) || (value == 0)) {
        return false;
    }
    while ((*text == ' ') || (*text == '\t')) {
        text++;
    }
    if ((*text == '+') || (*text == '-')) {
        negative = (*text == '-');
        text++;
    }
    while ((*text >= '0') && (*text <= '9')) {
        parsed = parsed * 10.0f + (float)(*text - '0');
        hasDigit = true;
        text++;
    }
    if (*text == '.') {
        text++;
        while ((*text >= '0') && (*text <= '9')) {
            parsed += (float)(*text - '0') * fractionScale;
            fractionScale *= 0.1f;
            hasDigit = true;
            text++;
        }
    }
    while ((*text == ' ') || (*text == '\t')) {
        text++;
    }
    if ((!hasDigit) || (*text != '\0')) {
        return false;
    }

    *value = negative ? -parsed : parsed;
    return true;
}

static const char *App_VehicleCommandValue(const char *command,
                                            uint8_t prefixLength)
{
    const char *value = &command[prefixLength];

    while ((*value == ' ') || (*value == '\t')) {
        value++;
    }
    if ((*value == ',') || (*value == '=') || (*value == ':')) {
        value++;
    }
    return value;
}

static void App_VehicleApplyVofaCommand(const char *command)
{
    const char *valueText = 0;
    float value;

    if ((command == 0) || (command[0] == '\0')) {
        return;
    }

    /* 小写 ki/kp 专用于灰度方向环，必须先于大写速度环判断。 */
    if ((command[0] == 'k') &&
        ((command[1] == 'i') || (command[1] == 'p'))) {
        valueText = App_VehicleCommandValue(command, 2U);
        if (App_VehicleParseFloat(valueText, &value) &&
            (value >= 0.0f) && (value <= APP_VOFA_LINE_KP_MAX)) {
            lineKp = value;
        }
        return;
    }

    if ((command[0] == 'K') &&
        ((command[1] == 'P') || (command[1] == 'I') ||
         (command[1] == 'D'))) {
        valueText = App_VehicleCommandValue(command, 2U);
        if (!App_VehicleParseFloat(valueText, &value) ||
            (value < 0.0f) || (value > APP_VOFA_SPEED_PID_GAIN_MAX)) {
            return;
        }

        if (command[1] == 'P') {
            leftSpeedPid.Kp = value;
            rightSpeedPid.Kp = value;
        } else if (command[1] == 'I') {
            leftSpeedPid.Ki = value;
            rightSpeedPid.Ki = value;
        } else {
            leftSpeedPid.Kd = value;
            rightSpeedPid.Kd = value;
        }
        return;
    }

    if ((command[0] == 'S') || (command[0] == 's')) {
        valueText = App_VehicleCommandValue(command, 1U);
        if (App_VehicleParseFloat(valueText, &value) &&
            (value >= 0.0f) && (value <= APP_VOFA_SPEED_MAX)) {
            commandSpeed = value;
        }
    }
}

static void App_VehicleVofaReceiveRun(void)
{
    char command[APP_VOFA_COMMAND_SIZE];

    if (vofa_read_command(command, sizeof(command)) != 0U) {
        App_VehicleApplyVofaCommand(command);
    }
}

static void App_VehicleVofaTelemetryRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();
    float channels[11];

    if ((uint32_t)(now - lastTelemetryTick) <
        APP_VOFA_TELEMETRY_PERIOD_MS) {
        return;
    }
    lastTelemetryTick = now;

    channels[0] = leftSpeedPid.Target;
    channels[1] = Motor1_Speed;
    channels[2] = rightSpeedPid.Target;
    channels[3] = Motor2_Speed;
    channels[4] = leftSpeedPid.Kp;
    channels[5] = leftSpeedPid.Ki;
    channels[6] = leftSpeedPid.Kd;
    channels[7] = lineKp;
    channels[8] = commandSpeed;
    channels[9] = (float)g_vehicle_line_error_tenths;
    channels[10] = lineCorrection;
    (void)vofa_justfloat_send(channels, 11U);
}

void App_VehicleInit(void)
{
    const uint32_t now = BSP_Delay_GetTick();

    if (vehicleInitialized) {
        return;
    }
    Encoder_Init();
    App_VehicleResetPid(&leftSpeedPid);
    App_VehicleResetPid(&rightSpeedPid);
    Motor_Stop();
    lastSpeedTick = now;
    lastControlTick = now;
    lastTelemetryTick = now;
    vehicleInitialized = true;
}

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

float App_VehicleGetCommandSpeed(void)
{
    return commandSpeed;
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
    lineCorrection = correction;
}

void App_VehicleControlRun(void)
{
    const uint32_t now = BSP_Delay_GetTick();

    if ((uint32_t)(now - lastSpeedTick) >= APP_VEHICLE_SPEED_PERIOD_MS) {
        lastSpeedTick += APP_VEHICLE_SPEED_PERIOD_MS;
        MEASURE_MOTORS_SPEED();
    }

    if (speedLoopEnabled &&
        ((uint32_t)(now - lastControlTick) >=
         APP_VEHICLE_CONTROL_PERIOD_MS)) {
        lastControlTick += APP_VEHICLE_CONTROL_PERIOD_MS;
        leftSpeedPid.Actual = Motor1_Speed;
        rightSpeedPid.Actual = Motor2_Speed;
        PID_Update(&leftSpeedPid);
        PID_Update(&rightSpeedPid);
        if (activeBrakeEnabled) {
            float brakeCommon =
                (App_VehicleAbsFloat(leftSpeedPid.Out) +
                 App_VehicleAbsFloat(rightSpeedPid.Out)) * 0.5f;
            float steering =
                (rightSpeedPid.Out - leftSpeedPid.Out) * 0.5f;
            float steeringLimit;

            brakeCommon = App_VehicleClampFloat(
                brakeCommon, 0.0f, APP_TASK2_ACTIVE_BRAKE_PWM_MAX);
            steeringLimit = App_VehicleClampFloat(
                brakeCommon, 0.0f, APP_TASK2_ACTIVE_BRAKE_STEER_MAX);
            steering = App_VehicleClampFloat(
                steering, -steeringLimit, steeringLimit);

            /* 公共量反向制动；差速方向保持，使滑行阶段继续贴线。 */
            Motor_ApplySpeedLoopOutput(
                (int)(-brakeCommon - steering),
                (int)(-brakeCommon + steering));
        } else {
            Motor_ApplySpeedLoopOutput((int)leftSpeedPid.Out,
                                       (int)rightSpeedPid.Out);
        }
    } else if (!speedLoopEnabled) {
        lastControlTick = now;
    }
}

void App_VehicleRun(void)
{
    App_VehicleInit();
    App_VehicleVofaReceiveRun();
    App_VehicleControlRun();
    App_VehicleVofaTelemetryRun();
}
