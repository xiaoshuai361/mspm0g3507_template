#include "line_trace_test.h"

#include "line_trace.h"

#define CHECK(condition)         \
    do {                         \
        if (!(condition)) {      \
            failures++;          \
        }                        \
    } while (0)

static uint32_t LineTrace_TestWeightedError(void)
{
    uint32_t failures = 0U;
    int16_t error;
    uint8_t count;

    CHECK(LineTrace_CalcActiveLowWeightedError(0xFFU, &error, &count) == 0U);
    CHECK((error == 0) && (count == 0U));
    CHECK(LineTrace_CalcActiveLowWeightedError(0xE7U, &error, &count) != 0U);
    CHECK((error == 0) && (count == 2U));
    CHECK(LineTrace_CalcActiveLowWeightedError(0xFEU, &error, &count) != 0U);
    CHECK((error == -70) && (count == 1U));
    CHECK(LineTrace_CalcActiveLowWeightedError(0x7FU, &error, &count) != 0U);
    CHECK((error == 70) && (count == 1U));
    CHECK(LineTrace_CalcActiveLowWeightedError(0xFCU, &error, &count) != 0U);
    CHECK((error == -60) && (count == 2U));
    CHECK(LineTrace_CountActiveLow(0x00U) == 8U);
    return failures;
}

static uint32_t LineTrace_TestController(void)
{
    uint32_t failures = 0U;
    const LineTrace_ControlConfig config = {
        .fastErrorThreshold = 35,
        .fastDeltaThreshold = 25,
        .centerDeadband = 12,
        .steeringKp = 1.0f,
        .steeringKd = 0.1f,
        .curveSlowdownGain = 0.3f,
        .minimumSpeedRatio = 0.45f,
    };
    LineTrace_Controller controller;
    LineTrace_ControlOutput output;

    LineTrace_ControllerReset(&controller);
    LineTrace_ControllerStep(&controller, &config, 1U, 0, 60.0f, &output);
    CHECK(output.valid != 0U);
    CHECK((output.leftTarget == 60.0f) && (output.rightTarget == 60.0f));

    /* 小偏差只引入 1/4 新值并落在死区内。 */
    LineTrace_ControllerReset(&controller);
    LineTrace_ControllerStep(&controller, &config, 1U, 10, 60.0f, &output);
    CHECK((output.filteredError == 2) && (output.correction == 0.0f));

    /* 大偏差使用 2/3 新值，立即产生转向和弯道降速。 */
    LineTrace_ControllerReset(&controller);
    LineTrace_ControllerStep(&controller, &config, 1U, 70, 60.0f, &output);
    CHECK(output.filteredError == 46);
    CHECK(output.leftTarget < output.rightTarget);
    CHECK(output.baseTarget < 60.0f);

    /* 丢线不产生新输出，上层保持上一拍目标，不触发停车。 */
    LineTrace_ControllerStep(&controller, &config, 0U, 0, 60.0f, &output);
    CHECK(output.valid == 0U);
    return failures;
}

uint32_t LineTrace_RunSelfTest(void)
{
    return LineTrace_TestWeightedError() + LineTrace_TestController();
}
