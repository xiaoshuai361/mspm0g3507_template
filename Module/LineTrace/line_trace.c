#include "line_trace.h"

/*
 * 指定版本使用的对称非线性权重。中心附近保持灵敏，
 * 从中心向外相邻增量逐步减小。正误差表示黑线位于车体左侧。
 */
static const int16_t lineWeights[8] = {
    -50, -42, -28, -10, 10, 28, 42, 50
};

static int16_t LineTrace_AbsInt16(int16_t value)
{
    return (value >= 0) ? value : (int16_t)-value;
}

static float LineTrace_ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static int16_t LineTrace_ApplyDeadband(int16_t error, int16_t deadband)
{
    if (error > deadband) {
        return (int16_t)(error - deadband);
    }
    if (error < -deadband) {
        return (int16_t)(error + deadband);
    }
    return 0;
}

uint8_t LineTrace_CalcActiveLowWeightedError(uint8_t raw,
                                             int16_t *errorTenths,
                                             uint8_t *activeCount)
{
    int16_t weightedSum = 0;
    uint8_t count = 0U;
    uint8_t bit;
    const uint8_t active = (uint8_t)~raw;

    if (errorTenths == 0) {
        return 0U;
    }

    for (bit = 0U; bit < 8U; bit++) {
        if ((active & (uint8_t)(1U << bit)) != 0U) {
            weightedSum = (int16_t)(weightedSum + lineWeights[bit]);
            count++;
        }
    }

    if (activeCount != 0) {
        *activeCount = count;
    }
    if (count == 0U) {
        *errorTenths = 0;
        return 0U;
    }

    *errorTenths = (int16_t)(weightedSum / (int16_t)count);
    return 1U;
}

uint8_t LineTrace_CountActiveLow(uint8_t raw)
{
    uint8_t active = (uint8_t)~raw;
    uint8_t count = 0U;

    while (active != 0U) {
        count = (uint8_t)(count + (active & 0x01U));
        active >>= 1U;
    }
    return count;
}

CrossLine_Type LineTrace_DetectCrossLine(uint8_t raw, uint8_t activeCount,
                                         uint16_t *lockoutFrames,
                                         uint8_t *confirmCount)
{
    if ((lockoutFrames != 0) && (*lockoutFrames > 0U)) {
        (*lockoutFrames)--;
        return CROSS_LINE_NONE;
    }

    if (activeCount != 3U) {
        if (confirmCount != 0) {
            *confirmCount = 0U;
        }
        return CROSS_LINE_NONE;
    }

    (void)raw;
    if (lockoutFrames != 0) {
        *lockoutFrames = 80U;
    }
    if (confirmCount != 0) {
        *confirmCount = 0U;
    }
    return CROSS_LINE_DETECTED;
}

void LineTrace_ResetCrossDetect(uint16_t *lockoutFrames,
                                uint8_t *confirmCount)
{
    if (lockoutFrames != 0) {
        *lockoutFrames = 0U;
    }
    if (confirmCount != 0) {
        *confirmCount = 0U;
    }
}

void LineTrace_ControllerReset(LineTrace_Controller *controller)
{
    if (controller == 0) {
        return;
    }
    controller->filteredError = 0;
    controller->lastSteeringError = 0;
    controller->appliedCorrection = 0.0f;
}

void LineTrace_ControllerStep(LineTrace_Controller *controller,
                              const LineTrace_ControlConfig *config,
                              uint8_t hasLine, int16_t rawError,
                              float cruiseSpeed,
                              LineTrace_ControlOutput *output)
{
    int16_t steeringError;
    int16_t errorDiff;
    float minimumSpeed;
    float correctionLimit;
    float desiredCorrection;
    float correctionDelta;

    if ((controller == 0) || (config == 0) || (output == 0)) {
        return;
    }

    output->valid = 0U;
    output->filteredError = controller->filteredError;
    output->baseTarget = 0.0f;
    output->correction = 0.0f;
    output->leftTarget = 0.0f;
    output->rightTarget = 0.0f;

    /* 丢线时不产生新目标，车辆保持上一拍控制量。 */
    if (hasLine == 0U) {
        return;
    }

    controller->filteredError = (int16_t)(
        (controller->filteredError + rawError * 2) / 3);

    steeringError = LineTrace_ApplyDeadband(controller->filteredError,
                                            config->centerDeadband);
    errorDiff = (int16_t)(steeringError - controller->lastSteeringError);
    controller->lastSteeringError = steeringError;

    cruiseSpeed = (cruiseSpeed > 0.0f) ? cruiseSpeed : 0.0f;
    minimumSpeed = cruiseSpeed * config->minimumSpeedRatio;
    output->baseTarget = cruiseSpeed -
        (float)LineTrace_AbsInt16(controller->filteredError) *
        config->curveSlowdownGain;
    output->baseTarget = LineTrace_ClampFloat(output->baseTarget,
                                               minimumSpeed, cruiseSpeed);

    desiredCorrection = (float)steeringError * config->steeringKp +
                        (float)errorDiff * config->steeringKd;

    correctionLimit = output->baseTarget;
    if ((config->correctionMax > 0.0f) &&
        (correctionLimit > config->correctionMax)) {
        correctionLimit = config->correctionMax;
    }
    desiredCorrection = LineTrace_ClampFloat(desiredCorrection,
                                              -correctionLimit,
                                              correctionLimit);
    correctionDelta = desiredCorrection - controller->appliedCorrection;
    if (config->correctionSlewStep > 0.0f) {
        correctionDelta = LineTrace_ClampFloat(
            correctionDelta, -config->correctionSlewStep,
            config->correctionSlewStep);
    }
    controller->appliedCorrection = LineTrace_ClampFloat(
        controller->appliedCorrection + correctionDelta,
        -correctionLimit, correctionLimit);
    output->correction = controller->appliedCorrection;

    output->leftTarget = output->baseTarget - output->correction;
    output->rightTarget = output->baseTarget + output->correction;
    output->filteredError = controller->filteredError;
    output->valid = 1U;
}
