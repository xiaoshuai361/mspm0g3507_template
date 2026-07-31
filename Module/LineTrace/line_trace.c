#include "line_trace.h"

#define LINE_TRACE_BIT_TURN_RIGHT (0U) /**< LINE_TRACE_BIT_TURN_RIGHT 模块配置或状态宏。 */
#define LINE_TRACE_BIT_GO_RIGHT_1 (1U) /**< LINE_TRACE_BIT_GO_RIGHT_1 模块配置或状态宏。 */
#define LINE_TRACE_BIT_GO_RIGHT_2 (2U) /**< LINE_TRACE_BIT_GO_RIGHT_2 模块配置或状态宏。 */
#define LINE_TRACE_BIT_FORWARD_1  (3U) /**< LINE_TRACE_BIT_FORWARD_1 模块配置或状态宏。 */
#define LINE_TRACE_BIT_FORWARD_2  (4U) /**< LINE_TRACE_BIT_FORWARD_2 模块配置或状态宏。 */
#define LINE_TRACE_BIT_GO_LEFT_1  (5U) /**< LINE_TRACE_BIT_GO_LEFT_1 模块配置或状态宏。 */
#define LINE_TRACE_BIT_GO_LEFT_2  (6U) /**< LINE_TRACE_BIT_GO_LEFT_2 模块配置或状态宏。 */
#define LINE_TRACE_BIT_TURN_LEFT  (7U) /**< LINE_TRACE_BIT_TURN_LEFT 模块配置或状态宏。 */

static const int16_t lineTraceWeightsTenths[8] = {
    35, 25, 15, 5, -5, -15, -25, -35
}; /**< bit0~bit7 的位置权重，单位 0.1 路间距；右正左负。 */

/**
 * @brief 读取 raw 中指定 bit，返回 0/1。
 * @param raw 原始传感器数据。
 * @param bit bit 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
static uint8_t LineTrace_Bit(uint8_t raw, uint8_t bit)
{
    return (uint8_t)((raw >> bit) & 0x01U);
}

/**
 * @brief 将高有效循迹数据解码为运动状态。
 * @param raw 原始传感器数据。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回循迹状态枚举。
 */
LineTrace_State LineTrace_DecodeState(uint8_t raw)
{
    /*
     * 优先级保持旧工程逻辑：
     * 边缘丢线时先原地转；其次小角度修正；最后中间线前进。
     */
    if (LineTrace_Bit(raw, LINE_TRACE_BIT_TURN_LEFT) != 0U) {
        return LINE_TRACE_TURN_LEFT;
    }
    if ((LineTrace_Bit(raw, LINE_TRACE_BIT_GO_LEFT_1) != 0U) ||
        (LineTrace_Bit(raw, LINE_TRACE_BIT_GO_LEFT_2) != 0U)) {
        return LINE_TRACE_GO_LEFT;
    }
    if (LineTrace_Bit(raw, LINE_TRACE_BIT_TURN_RIGHT) != 0U) {
        return LINE_TRACE_TURN_RIGHT;
    }
    if ((LineTrace_Bit(raw, LINE_TRACE_BIT_GO_RIGHT_1) != 0U) ||
        (LineTrace_Bit(raw, LINE_TRACE_BIT_GO_RIGHT_2) != 0U)) {
        return LINE_TRACE_GO_RIGHT;
    }
    if ((LineTrace_Bit(raw, LINE_TRACE_BIT_FORWARD_1) != 0U) ||
        (LineTrace_Bit(raw, LINE_TRACE_BIT_FORWARD_2) != 0U)) {
        return LINE_TRACE_FORWARD;
    }

    return LINE_TRACE_STOP;
}

/**
 * @brief 将低有效灰度数据解码为运动状态。
 * @param raw 原始传感器数据。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回循迹状态枚举。
 */
LineTrace_State LineTrace_DecodeActiveLowRaw(uint8_t raw)
{
    return LineTrace_DecodeState((uint8_t)~raw);
}

uint8_t LineTrace_CalcWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount)
{
    int16_t weightedSum = 0;
    uint8_t count = 0U;
    uint8_t bit;

    if (errorTenths == 0)
    {
        if (activeCount != 0) {
            *activeCount = 0U;
        }
        return 0U;
    }

    for (bit = 0U; bit < 8U; bit++)
    {
        if (LineTrace_Bit(raw, bit) != 0U)
        {
            weightedSum = (int16_t)(weightedSum + lineTraceWeightsTenths[bit]);
            count++;
        }
    }

    if (count == 0U)
    {
        *errorTenths = 0;
        if (activeCount != 0) {
            *activeCount = 0U;
        }
        return 0U;
    }

    *errorTenths = (int16_t)(weightedSum / (int16_t)count);
    if (activeCount != 0) {
        *activeCount = count;
    }

    return 1U;
}

uint8_t LineTrace_CalcActiveLowWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount)
{
    return LineTrace_CalcWeightedError((uint8_t)~raw, errorTenths, activeCount);
}

static int16_t LineTrace_AbsInt16(int16_t value)
{
    return (value >= 0) ? value : (int16_t)(-value);
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

static int16_t LineTrace_MoveToward(int16_t current, int16_t target,
                                    int16_t upStep, int16_t downStep)
{
    if (current < target) {
        int16_t next = (int16_t)(current + upStep);
        return (next > target) ? target : next;
    }
    if (current > target) {
        int16_t next = (int16_t)(current - downStep);
        return (next < target) ? target : next;
    }
    return current;
}

void LineTrace_ControllerReset(LineTrace_Controller *controller)
{
    controller->filteredError = 0;
    controller->previousRawError = 0;
    controller->basePwm = 0;
    controller->lastDirection = 0;
    controller->slowdownFrames = 0U;
    controller->lostFrames = 0U;
}

void LineTrace_ControllerStep(LineTrace_Controller *controller,
                              const LineTrace_ControlConfig *config,
                              uint8_t hasLine, int16_t rawError,
                              LineTrace_ControlOutput *output)
{
    int16_t steeringError;
    int16_t desiredBase;
    int16_t correction;
    int16_t correctionLimit;
    int16_t steeringGain;
    int16_t leftBias;
    int16_t rightTurnBoost;

    output->lineLost = (hasLine == 0U) ? 1U : 0U;
    output->shouldStop = 0U;

    if (hasLine != 0U) {
        int16_t absRaw = LineTrace_AbsInt16(rawError);
        int16_t rawDelta = LineTrace_AbsInt16(
            (int16_t)(rawError - controller->previousRawError));

        /* Small corrections favor response; large corrections favor stability. */
        if ((absRaw >= config->fastErrorThreshold) ||
            (rawDelta >= config->fastDeltaThreshold)) {
            controller->filteredError = (int16_t)(
                (controller->filteredError + rawError) / 2);
        } else {
            controller->filteredError = (int16_t)(
                (controller->filteredError + rawError * 3) / 4);
        }

        controller->previousRawError = rawError;
        controller->lostFrames = 0U;
        steeringError = LineTrace_ApplyDeadband(
            controller->filteredError, config->centerDeadband);

        if (rawError > config->centerDeadband) {
            controller->lastDirection = 1;
        } else if (rawError < -config->centerDeadband) {
            controller->lastDirection = -1;
        }

        absRaw = LineTrace_AbsInt16(controller->filteredError);
        if (absRaw >= config->slowdownEntryError) {
            if (controller->slowdownFrames < config->slowdownConfirmFrames) {
                controller->slowdownFrames++;
            }
        } else {
            controller->slowdownFrames = 0U;
        }

        desiredBase = config->cruisePwm;
        if ((config->slowdownConfirmFrames == 0U) ||
            (controller->slowdownFrames >= config->slowdownConfirmFrames)) {
            int16_t slowdownError = (int16_t)(
                absRaw - config->slowdownEntryError);

            if (slowdownError < 0) {
                slowdownError = 0;
            }
            desiredBase = (int16_t)(config->cruisePwm -
                slowdownError * config->curveSlowdownGain);
        }
        if (desiredBase < config->lostPwm) {
            desiredBase = config->lostPwm;
        }
    } else {
        int16_t searchMagnitude;

        if (controller->lostFrames < UINT16_MAX) {
            controller->lostFrames++;
        }
        controller->slowdownFrames = config->slowdownConfirmFrames;

        if (controller->lostFrames >= config->lostStopFrames) {
            output->shouldStop = 1U;
        }

        if (controller->lostFrames <= config->lostHoldFrames) {
            steeringError = LineTrace_ApplyDeadband(
                controller->filteredError, config->centerDeadband);
        } else if (controller->lastDirection != 0) {
            uint16_t searchFrames = (uint16_t)(
                controller->lostFrames - config->lostHoldFrames - 1U);
            uint8_t stepFrames = (config->lostSearchStepFrames == 0U)
                ? 1U : config->lostSearchStepFrames;

            searchMagnitude = (int16_t)(config->lostSearchStartError +
                (int16_t)(searchFrames / stepFrames));
            if (searchMagnitude > 35) {
                searchMagnitude = 35;
            }
            steeringError = (int16_t)(
                searchMagnitude * controller->lastDirection);
        } else {
            steeringError = 0;
        }

        desiredBase = config->lostPwm;
    }

    controller->basePwm = LineTrace_MoveToward(
        controller->basePwm, desiredBase,
        config->rampUpStep, config->rampDownStep);

    steeringGain = config->steeringKp;
    if (LineTrace_AbsInt16(steeringError) >=
        config->edgeSteeringThreshold) {
        steeringGain = config->edgeSteeringKp;
    }
    correction = (int16_t)(steeringError * steeringGain);
    correctionLimit = config->steeringMax;

    /* 两轮保持正转，同时允许配重增加后使用更强的40%差速纠偏。 */
    if (((controller->basePwm * 2) / 5) < correctionLimit) {
        correctionLimit = (int16_t)((controller->basePwm * 2) / 5);
    }
    if (correction > correctionLimit) {
        correction = correctionLimit;
    } else if (correction < -correctionLimit) {
        correction = (int16_t)(-correctionLimit);
    }

    leftBias = 0;
    if (config->cruisePwm > 0) {
        leftBias = (int16_t)(((int32_t)config->leftPwmBias *
            controller->basePwm) / config->cruisePwm);
    }
    rightTurnBoost = 0;
    if ((correction > 0) && (correctionLimit > 0)) {
        rightTurnBoost = (int16_t)(((int32_t)config->rightTurnBoost *
            correction) / correctionLimit);
    }

    output->leftPwm = (int16_t)(controller->basePwm + correction +
        leftBias + rightTurnBoost);
    output->rightPwm = (int16_t)(controller->basePwm - correction);
    output->filteredError = controller->filteredError;
    output->basePwm = controller->basePwm;
    output->correction = correction;
}

const char *LineTrace_StateName(LineTrace_State state)
{
    switch (state) {
        case LINE_TRACE_FORWARD:
            return "FORWARD";
        case LINE_TRACE_BACKWARD:
            return "BACKWARD";
        case LINE_TRACE_STOP:
            return "STOP";
        case LINE_TRACE_TURN_RIGHT:
            return "TURN_R";
        case LINE_TRACE_TURN_LEFT:
            return "TURN_L";
        case LINE_TRACE_GO_RIGHT:
            return "GO_R";
        case LINE_TRACE_GO_LEFT:
            return "GO_L";
        default:
            return "UNKNOWN";
    }
}

/* ================================================================
 * A点停车线检测（灰度板原始输入低有效）
 * 条件：任意位置恰好3路有效时立即停车，不要求位置连续或跨帧确认。
 * 四路及以上同时有效不触发停车。
 * ================================================================ */

CrossLine_Type LineTrace_DetectCrossLine(uint8_t raw, uint8_t activeCount,
                                          uint16_t *lockoutFrames, uint8_t *confirmCount)
{
    /* 锁定期内递减 */
    if ((lockoutFrames != 0) && (*lockoutFrames > 0U)) {
        (*lockoutFrames)--;
        return CROSS_LINE_NONE;
    }

    /* Count-only detection keeps steering responsive and removes frame delay. */
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

void LineTrace_ResetCrossDetect(uint16_t *lockoutFrames, uint8_t *confirmCount)
{
    if (lockoutFrames != 0) *lockoutFrames = 0U;
    if (confirmCount != 0) *confirmCount = 0U;
}
