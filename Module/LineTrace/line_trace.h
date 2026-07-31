#ifndef MODULE_LINE_TRACE_H
#define MODULE_LINE_TRACE_H

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

typedef enum {
    CROSS_LINE_NONE = 0,
    CROSS_LINE_DETECTED = 1,
} CrossLine_Type;

typedef struct {
    int16_t cruisePwm;
    int16_t lostPwm;
    int16_t rampUpStep;
    int16_t rampDownStep;
    int16_t curveSlowdownGain;
    int16_t slowdownEntryError;
    int16_t steeringKp;
    int16_t edgeSteeringKp;
    int16_t edgeSteeringThreshold;
    int16_t steeringMax;
    int16_t leftPwmBias;
    int16_t rightTurnBoost;
    int16_t centerDeadband;
    int16_t fastErrorThreshold;
    int16_t fastDeltaThreshold;
    int16_t lostSearchStartError;
    uint8_t lostSearchStepFrames;
    uint8_t lostHoldFrames;
    uint8_t slowdownConfirmFrames;
    uint16_t lostStopFrames;
} LineTrace_ControlConfig;

typedef struct {
    int16_t filteredError;
    int16_t previousRawError;
    int16_t basePwm;
    int8_t lastDirection;
    uint8_t slowdownFrames;
    uint16_t lostFrames;
} LineTrace_Controller;

typedef struct {
    int16_t leftPwm;
    int16_t rightPwm;
    int16_t filteredError;
    int16_t basePwm;
    int16_t correction;
    uint8_t lineLost;
    uint8_t shouldStop;
} LineTrace_ControlOutput;

LineTrace_State LineTrace_DecodeState(uint8_t raw);
LineTrace_State LineTrace_DecodeActiveLowRaw(uint8_t raw);
uint8_t LineTrace_CalcWeightedError(uint8_t raw, int16_t *errorTenths,
                                    uint8_t *activeCount);
uint8_t LineTrace_CalcActiveLowWeightedError(uint8_t raw,
                                             int16_t *errorTenths,
                                             uint8_t *activeCount);
const char *LineTrace_StateName(LineTrace_State state);

void LineTrace_ControllerReset(LineTrace_Controller *controller);
void LineTrace_ControllerStep(LineTrace_Controller *controller,
                              const LineTrace_ControlConfig *config,
                              uint8_t hasLine, int16_t rawError,
                              LineTrace_ControlOutput *output);

/* Stop immediately when exactly three sensors are active, regardless of position. */
CrossLine_Type LineTrace_DetectCrossLine(uint8_t raw, uint8_t activeCount,
                                         uint16_t *lockoutFrames,
                                         uint8_t *confirmCount);
void LineTrace_ResetCrossDetect(uint16_t *lockoutFrames,
                                uint8_t *confirmCount);

#endif /* MODULE_LINE_TRACE_H */
