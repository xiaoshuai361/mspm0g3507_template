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

    return failures;
}
