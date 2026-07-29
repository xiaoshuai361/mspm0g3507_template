#ifndef MODULE_LINE_TRACE_H
#define MODULE_LINE_TRACE_H /**< MODULE_LINE_TRACE_H 头文件重复包含保护宏。 */

#include <stdint.h>

typedef enum {
    LINE_TRACE_FORWARD = 1,
    LINE_TRACE_BACKWARD = 2,
    LINE_TRACE_STOP = 3,
    LINE_TRACE_TURN_RIGHT = 4,
    LINE_TRACE_TURN_LEFT = 5,
    LINE_TRACE_GO_RIGHT = 6,
    LINE_TRACE_GO_LEFT = 7
} LineTrace_State;

/* 横切线检测结果 */
typedef enum {
    CROSS_LINE_NONE = 0,
    CROSS_LINE_DETECTED = 1,
} CrossLine_Type;

LineTrace_State LineTrace_DecodeState(uint8_t raw);
LineTrace_State LineTrace_DecodeActiveLowRaw(uint8_t raw);
uint8_t LineTrace_CalcWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount);
uint8_t LineTrace_CalcActiveLowWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount);
const char *LineTrace_StateName(LineTrace_State state);

/* 横切线检测（A点启停线）：>=4路黑 + 中间D3~D6全黑 + 2帧确认 */
CrossLine_Type LineTrace_DetectCrossLine(uint8_t raw, uint8_t activeCount,
                                          uint16_t *lockoutFrames, uint8_t *confirmCount);
void LineTrace_ResetCrossDetect(uint16_t *lockoutFrames, uint8_t *confirmCount);

#endif /* MODULE_LINE_TRACE_H */
