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
    .lostPwm = 650,
    .rampUpStep = 20,
    .rampDownStep = 35,
    .curveSlowdownGain = 12,
    .steeringKp = 3,
    .edgeSteeringKp = 14,
    .edgeSteeringThreshold = 8,
    .steeringMax = 300,
    .centerDeadband = 5,
    .fastErrorThreshold = 15,
    .fastDeltaThreshold = 12,
    .lostSearchStartError = 12,
    .lostSearchStepFrames = 4U,
    .lostHoldFrames = 3U,
    .lostStopFrames = 120U,
};

static const LineTrace_ControlConfig lineControlStableTestConfig = {
    .cruisePwm = 600,
    .lostPwm = 480,
    .rampUpStep = 15,
    .rampDownStep = 8,
    .curveSlowdownGain = 1,
    .steeringKp = 3,
    .edgeSteeringKp = 8,
    .edgeSteeringThreshold = 8,
    .steeringMax = 160,
    .centerDeadband = 5,
    .fastErrorThreshold = 15,
    .fastDeltaThreshold = 12,
    .lostSearchStartError = 12,
    .lostSearchStepFrames = 4U,
    .lostHoldFrames = 3U,
    .lostStopFrames = 120U,
};

static uint32_t LineTrace_TestCrossLine(void)
{
    uint32_t failures = 0U;
    uint16_t lockoutFrames = 0U;
    uint8_t confirmCount = 0U;

    /* 外侧 D1~D4 同时扫到赛道是弯道结束标志，不应触发停车。 */
    if (LineTrace_DetectCrossLine(0xF0U, 4U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_NONE) {
        failures++;
    }

    /* 另一侧 D5~D8 同时有效也不能触发停车。 */
    if (LineTrace_DetectCrossLine(0x0FU, 4U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_NONE) {
        failures++;
    }

    /* 中间 D3~D5 三路为黑，第一帧只进入确认。 */
    if (LineTrace_DetectCrossLine(0xE3U, 3U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_NONE) {
        failures++;
    }
    if (confirmCount != 1U) {
        failures++;
    }
    /* 下一帧左偏到 D4~D7 四路，仍应连续确认并停车。 */
    if (LineTrace_DetectCrossLine(0x87U, 4U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_DETECTED) {
        failures++;
    }

    lockoutFrames = 0U;
    confirmCount = 0U;
    /* 非连续的中部三路不能误判为停车线。 */
    if (LineTrace_DetectCrossLine(0xD3U, 3U,
                                  &lockoutFrames, &confirmCount)
        != CROSS_LINE_NONE) {
        failures++;
    }

    /* 锁定期必须压制起步停车线，且按控制帧递减。 */
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
    if ((output.leftPwm != 1100) || (output.rightPwm != 1100)) {
        failures++;
    }

    /* D3/D6 中等偏差需在两帧内进入半圆弯道增强差速。 */
    LineTrace_ControllerReset(&curveController);
    curveController.basePwm = 1100;
    LineTrace_ControllerStep(&curveController, &lineControlTestConfig,
                             1U, 15, &output);
    if ((output.filteredError != 11) ||
        (output.correction != 18) ||
        (output.basePwm != 1065) ||
        (output.leftPwm != 1083) || (output.rightPwm != 1047)) {
        failures++;
    }
    LineTrace_ControllerStep(&curveController, &lineControlTestConfig,
                             1U, 15, &output);
    if ((output.filteredError != 14) ||
        (output.correction != 126) ||
        (output.basePwm != 1030) ||
        (output.leftPwm != 1156) || (output.rightPwm != 904)) {
        failures++;
    }
    for (frame = 0U; frame < 3U; frame++) {
        LineTrace_ControllerStep(&curveController, &lineControlTestConfig,
                                 1U, 15, &output);
    }
    if ((output.basePwm != 932) ||
        (output.correction != 126) ||
        (output.leftPwm != 1058) || (output.rightPwm != 806)) {
        failures++;
    }

    /* 最外侧偏差首帧必须立即产生差速，且两轮保持正向。 */
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             1U, 35, &output);
    if ((output.filteredError != 26) ||
        (output.correction != 294) ||
        (output.leftPwm != 1359) || (output.rightPwm != 771)) {
        failures++;
    }

    /* 从右边缘跳到左边缘时，首帧纠偏方向必须同步翻转。 */
    LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                             1U, -35, &output);
    if ((output.filteredError != -19) ||
        (output.correction != -196) ||
        (output.leftPwm != 834) || (output.rightPwm != 1226)) {
        failures++;
    }

    /* 119帧内保持有界搜索，第120帧才报告持续丢线停车。 */
    for (frame = 0U; frame < 119U; frame++) {
        LineTrace_ControllerStep(&controller, &lineControlTestConfig,
                                 0U, 0, &output);
        if ((output.shouldStop != 0U) ||
            (output.leftPwm <= 0) || (output.rightPwm <= 0)) {
            failures++;
            break;
        }
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
    if ((output.basePwm != 600) ||
        (output.leftPwm != 600) || (output.rightPwm != 600)) {
        failures++;
    }

    LineTrace_ControllerStep(&controller, &lineControlStableTestConfig,
                             1U, 15, &output);
    LineTrace_ControllerStep(&controller, &lineControlStableTestConfig,
                             1U, 15, &output);
    if ((output.filteredError != 14) ||
        (output.basePwm != 586) ||
        (output.correction != 72) ||
        (output.leftPwm != 658) || (output.rightPwm != 514)) {
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
