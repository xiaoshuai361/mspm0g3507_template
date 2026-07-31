#include "line_trace_test.h"

#include "line_trace.h"

static uint32_t LineTrace_ExpectState(uint8_t raw, LineTrace_State expected)
{
    return (LineTrace_DecodeState(raw) == expected) ? 0U : 1U;
}

static uint32_t LineTrace_ExpectActiveLowState(uint8_t raw,
                                               LineTrace_State expected)
{
    return (LineTrace_DecodeActiveLowRaw(raw) == expected) ? 0U : 1U;
}

static uint32_t LineTrace_ExpectWeightedError(uint8_t raw,
                                              int16_t expectedError,
                                              uint8_t expectedCount)
{
    int16_t error = 0;
    uint8_t count = 0U;

    if (LineTrace_CalcWeightedError(raw, &error, &count) == 0U) {
        return 1U;
    }

    return ((error == expectedError) && (count == expectedCount)) ? 0U : 1U;
}

static uint32_t LineTrace_ExpectActiveLowWeightedError(uint8_t raw,
                                                       int16_t expectedError,
                                                       uint8_t expectedCount)
{
    int16_t error = 0;
    uint8_t count = 0U;

    if (LineTrace_CalcActiveLowWeightedError(raw, &error, &count) == 0U) {
        return 1U;
    }

    return ((error == expectedError) && (count == expectedCount)) ? 0U : 1U;
}

static uint32_t LineTrace_ExpectLostLine(uint8_t raw)
{
    int16_t error = 123;
    uint8_t count = 99U;

    if (LineTrace_CalcWeightedError(raw, &error, &count) != 0U) {
        return 1U;
    }

    return ((error == 0) && (count == 0U)) ? 0U : 1U;
}

static const LineTrace_ControlConfig lineControlTestConfig = {
    .cruisePwm = 1100,
    .lostPwm = 700,
    .rampUpStep = 20,
    .rampDownStep = 20,
    .curveSlowdownGain = 8,
    .slowdownEntryError = 10,
    .steeringKp = 4,
    .edgeSteeringKp = 22,
    .edgeSteeringThreshold = 6,
    .steeringMax = 480,
    .leftPwmBias = 50,
    .rightTurnBoost = 120,
    .centerDeadband = 5,
    .fastErrorThreshold = 15,
    .fastDeltaThreshold = 12,
    .lostSearchStartError = 18,
    .lostSearchStepFrames = 2U,
    .lostHoldFrames = 1U,
    .slowdownConfirmFrames = 3U,
    .lostStopFrames = 150U,
};

static const LineTrace_ControlConfig lineControlStableTestConfig = {
    .cruisePwm = 702,
    .lostPwm = 584,
    .rampUpStep = 19,
    .rampDownStep = 11,
    .curveSlowdownGain = 1,
    .slowdownEntryError = 8,
    .steeringKp = 4,
    .edgeSteeringKp = 14,
    .edgeSteeringThreshold = 6,
    .steeringMax = 240,
    .leftPwmBias = 39,
    .rightTurnBoost = 60,
    .centerDeadband = 5,
    .fastErrorThreshold = 15,
    .fastDeltaThreshold = 12,
    .lostSearchStartError = 16,
    .lostSearchStepFrames = 3U,
    .lostHoldFrames = 2U,
    .slowdownConfirmFrames = 2U,
    .lostStopFrames = 150U,
};

static uint32_t LineTrace_TestCrossLine(void)
{
    uint32_t failures = 0U;
    uint16_t lockoutFrames = 0U;
    uint8_t confirmCount = 0U;

    /* Four active sensors are a normal track shape, not a stop line. */
    if (LineTrace_DetectCrossLine(0xF0U, 4U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_NONE) {
        failures++;
    }

    /* Any three active sensors stop immediately; adjacency is irrelevant. */
    if (LineTrace_DetectCrossLine(0x76U, 3U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_DETECTED) {
        failures++;
    }

    lockoutFrames = 0U;
    confirmCount = 0U;
    /* A regular contiguous three-sensor stop line is also immediate. */
    if (LineTrace_DetectCrossLine(0xE3U, 3U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_DETECTED) {
        failures++;
    }

    lockoutFrames = 0U;
    confirmCount = 0U;
    /* Two active sensors keep tracking. */
    if (LineTrace_DetectCrossLine(0xE7U, 2U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_NONE) {
        failures++;
    }

    /* Startup lockout still suppresses a three-sensor reading. */
    lockoutFrames = 2U;
    confirmCount = 0U;
    if (LineTrace_DetectCrossLine(0xC7U, 3U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_NONE) {
        failures++;
    }
    if (lockoutFrames != 1U) {
        failures++;
    }

    return failures;
}

static uint32_t LineTrace_TestController(void)
{
    uint32_t failures = 0U;
    uint16_t frame;
    LineTrace_Controller controller;
    LineTrace_Controller curveController;
    LineTrace_ControlOutput output;

    LineTrace_ControllerReset(&controller);
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             1U, 0, &output);
    if ((output.leftPwm != 20) || (output.rightPwm != 20)) {
        failures++;
    }

    /* 55个10ms周期平滑升到1100 PWM，起步时间约550ms。 */
    for (frame = 1U; frame < 55U; frame++) {
        LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                                 1U, 0, &output);
    }
    if ((output.leftPwm != 1150) || (output.rightPwm != 1100)) {
        failures++;
    }

    /* D3/D6 中等偏差需在两帧内进入半圆弯道增强差速。 */
    LineTrace_ControllerReset(&curveController);
    curveController.basePwm = 1100;
    LineTrace_ControllerStep(&curveController, &lineControlTestConfig,
                             1U, 15, &output);
    if ((output.filteredError != 7) ||
        (output.correction != 8) ||
        (output.basePwm != 1100) ||
        (output.leftPwm != 1160) || (output.rightPwm != 1092)) {
        failures++;
    }
    LineTrace_ControllerStep(&curveController, &lineControlTestConfig,
                             1U, 15, &output);
    if ((output.filteredError != 11) ||
        (output.correction != 132) ||
        (output.basePwm != 1100) ||
        (output.leftPwm != 1318) || (output.rightPwm != 968)) {
        failures++;
    }
    for (frame = 0U; frame < 3U; frame++) {
        LineTrace_ControllerStep(&curveController, &lineControlTestConfig,
                                 1U, 15, &output);
    }
    if ((output.basePwm != 1068) ||
        (output.correction != 198) ||
        (output.leftPwm != 1369) || (output.rightPwm != 870)) {
        failures++;
    }

    /* 最外侧偏差首帧必须立即产生差速，且两轮保持正向。 */
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             1U, 35, &output);
    if ((output.filteredError != 17) ||
        (output.correction != 264) ||
        (output.leftPwm != 1486) || (output.rightPwm != 836)) {
        failures++;
    }

    /* A large side change is damped to avoid an abrupt full-scale reversal. */
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             1U, -35, &output);
    if ((output.filteredError != -9) ||
        (output.correction != -16) ||
        (output.leftPwm != 1134) || (output.rightPwm != 1116)) {
        failures++;
    }

    /* 首帧短暂保持，第二帧立即进入强力定向搜索。 */
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             0U, 0, &output);
    if ((output.shouldStop != 0U) ||
        (output.leftPwm != 1113) || (output.rightPwm != 1096)) {
        failures++;
    }
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             0U, 0, &output);
    if ((output.shouldStop != 0U) ||
        (output.leftPwm != 712) || (output.rightPwm != 1456)) {
        failures++;
    }

    /* 149帧内保持正转搜索，第150帧才报告持续丢线停车。 */
    for (frame = 2U; frame < 149U; frame++) {
        LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                                 0U, 0, &output);
        if ((output.shouldStop != 0U) ||
            (output.leftPwm <= 0) || (output.rightPwm <= 0)) {
            failures++;
            break;
        }
    }
    if ((output.basePwm != 700) ||
        (output.correction != -280) ||
        (output.leftPwm != 451) || (output.rightPwm != 980)) {
        failures++;
    }
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             0U, 0, &output);
    if (output.shouldStop == 0U) {
        failures++;
    }

    return failures;
}

static uint32_t LineTrace_TestStableController(void)
{
    uint32_t failures = 0U;
    uint16_t frame;
    LineTrace_Controller controller;
    LineTrace_ControlOutput output;

    LineTrace_ControllerReset(&controller);
    for (frame = 0U; frame < 40U; frame++) {
        LineTrace_ControllerStep(&controller, &lineControlStableTestConfig,
                                 1U, 0, &output);
    }
    if ((output.basePwm != 702) ||
        (output.leftPwm != 741) || (output.rightPwm != 702)) {
        failures++;
    }

    LineTrace_ControllerStep(&controller, &lineControlStableTestConfig,
                             1U, 15, &output);
    LineTrace_ControllerStep(&controller, &lineControlStableTestConfig,
                             1U, 15, &output);
    if ((output.filteredError != 11) ||
        (output.basePwm != 702) ||
        (output.correction != 84) ||
        (output.leftPwm != 846) || (output.rightPwm != 618)) {
        failures++;
    }

    return failures;
}

uint32_t LineTrace_RunSelfTest(void)
{
    uint32_t failures = 0U;

    failures += LineTrace_ExpectState(0x00U, LINE_TRACE_STOP);
    failures += LineTrace_ExpectState(0x08U, LINE_TRACE_FORWARD);
    failures += LineTrace_ExpectState(0x10U, LINE_TRACE_FORWARD);
    failures += LineTrace_ExpectState(0x20U, LINE_TRACE_GO_LEFT);
    failures += LineTrace_ExpectState(0x40U, LINE_TRACE_GO_LEFT);
    failures += LineTrace_ExpectState(0x02U, LINE_TRACE_GO_RIGHT);
    failures += LineTrace_ExpectState(0x04U, LINE_TRACE_GO_RIGHT);
    failures += LineTrace_ExpectState(0x80U, LINE_TRACE_TURN_LEFT);
    failures += LineTrace_ExpectState(0x01U, LINE_TRACE_TURN_RIGHT);

    /* 与旧工程一致：最左/最右边缘的原地转弯优先级高于普通修正。 */
    failures += LineTrace_ExpectState(0xA0U, LINE_TRACE_TURN_LEFT);
    failures += LineTrace_ExpectState(0x03U, LINE_TRACE_TURN_RIGHT);

    /*
     * 实测灰度板为主动低电平：白底=1，黑线=0。
     * 例如 raw=0xE7(11100111) 表示中间两路压黑线，应该前进。
     */
    failures += LineTrace_ExpectActiveLowState(0xFFU, LINE_TRACE_STOP);
    failures += LineTrace_ExpectActiveLowState(0xE7U, LINE_TRACE_FORWARD);
    failures += LineTrace_ExpectActiveLowState(0xE6U, LINE_TRACE_FORWARD);
    failures += LineTrace_ExpectActiveLowState(0x7FU, LINE_TRACE_TURN_LEFT);
    failures += LineTrace_ExpectActiveLowState(0xFEU, LINE_TRACE_TURN_RIGHT);

    /*
     * 加权连续偏差：D8(bit7) 为最左，D1(bit0) 为最右。
     * 位置权重为 -35, -25, -15, -5, +5, +15, +25, +35。
     */
    failures += LineTrace_ExpectLostLine(0x00U);
    failures += LineTrace_ExpectWeightedError(0x18U, 0, 2U);
    failures += LineTrace_ExpectWeightedError(0x08U, 5, 1U);
    failures += LineTrace_ExpectWeightedError(0x10U, -5, 1U);
    failures += LineTrace_ExpectWeightedError(0x03U, 30, 2U);
    failures += LineTrace_ExpectWeightedError(0xC0U, -30, 2U);
    failures += LineTrace_ExpectActiveLowWeightedError(0xE7U, 0, 2U);
    failures += LineTrace_ExpectActiveLowWeightedError(0xFCU, 30, 2U);
    failures += LineTrace_ExpectActiveLowWeightedError(0x3FU, -30, 2U);
    failures += LineTrace_TestCrossLine();
    failures += LineTrace_TestController();
    failures += LineTrace_TestStableController();

    return failures;
}
