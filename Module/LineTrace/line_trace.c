#include "line_trace.h"

/*
 * 与参考工程一致，D1(bit0) 到 D8(bit7) 使用 -7...+7 权重，
 * 并放大 10 倍保留整数精度。正误差表示黑线位于车体左侧。
 */
static const int16_t lineWeights[8] = {
    -70, -50, -30, -10, 10, 30, 50, 70
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

void LineTrace_ControllerReset(LineTrace_Controller *controller)
{
    if (controller == 0) {
        return;
    }
    controller->filteredError = 0;
    controller->lastSteeringError = 0;
}

void LineTrace_ControllerStep(LineTrace_Controller *controller,
                              const LineTrace_ControlConfig *config,
                              uint8_t hasLine, int16_t rawError,
                              float cruiseSpeed,
                              LineTrace_ControlOutput *output)
{
    int16_t rawDelta;
    int16_t steeringError;
    int16_t errorDiff;
    float minimumSpeed;
    float correctionLimit;

    if ((controller == 0) || (config == 0) || (output == 0)) {
        return;
    }

    output->valid = 0U;
    output->filteredError = controller->filteredError;
    output->baseTarget = 0.0f;
    output->correction = 0.0f;
    output->leftTarget = 0.0f;
    output->rightTarget = 0.0f;

    /* 参考工程的丢线行为：不产生新目标，车辆保持上一拍控制量。 */
    if (hasLine == 0U) {
        return;
    }

    rawDelta = LineTrace_AbsInt16(
        (int16_t)(rawError - controller->filteredError));

    /* 大偏差快速跟随，小偏差强滤波，避免直线抖动。 */
    if ((LineTrace_AbsInt16(rawError) >= config->fastErrorThreshold) ||
        (rawDelta >= config->fastDeltaThreshold)) {
        controller->filteredError = (int16_t)(
            (controller->filteredError + rawError * 2) / 3);
    } else {
        controller->filteredError = (int16_t)(
            (controller->filteredError * 3 + rawError) / 4);
    }

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

    output->correction = (float)steeringError * config->steeringKp +
                         (float)errorDiff * config->steeringKd;

    /* 不允许单轮目标反转，弯道最强时允许内轮降到 0。 */
    correctionLimit = output->baseTarget;
    output->correction = LineTrace_ClampFloat(output->correction,
                                               -correctionLimit,
                                               correctionLimit);

    output->leftTarget = output->baseTarget - output->correction;
    output->rightTarget = output->baseTarget + output->correction;
    output->filteredError = controller->filteredError;
    output->valid = 1U;
}
