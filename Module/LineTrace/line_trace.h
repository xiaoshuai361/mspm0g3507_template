#ifndef MODULE_LINE_TRACE_H
#define MODULE_LINE_TRACE_H

#include <stdint.h>

typedef enum {
    CROSS_LINE_NONE = 0,
    CROSS_LINE_DETECTED = 1,
} CrossLine_Type;

typedef struct {
    int16_t centerDeadband;
    float steeringKp;
    float steeringKd;
    float curveSlowdownGain;
    float minimumSpeedRatio;
    float correctionMax;
    float correctionSlewStep;
} LineTrace_ControlConfig;

typedef struct {
    int16_t filteredError;
    int16_t lastSteeringError;
    float appliedCorrection;
} LineTrace_Controller;

typedef struct {
    float leftTarget;
    float rightTarget;
    float baseTarget;
    float correction;
    int16_t filteredError;
    uint8_t valid;
} LineTrace_ControlOutput;

/* 灰度板为低有效：raw 位为 0 表示对应探头检测到黑线。 */
uint8_t LineTrace_CalcActiveLowWeightedError(uint8_t raw,
                                             int16_t *errorTenths,
                                             uint8_t *activeCount);
uint8_t LineTrace_CountActiveLow(uint8_t raw);

/* 任意位置恰好三路低有效时判定停车线。 */
CrossLine_Type LineTrace_DetectCrossLine(uint8_t raw, uint8_t activeCount,
                                         uint16_t *lockoutFrames,
                                         uint8_t *confirmCount);
void LineTrace_ResetCrossDetect(uint16_t *lockoutFrames,
                                uint8_t *confirmCount);

void LineTrace_ControllerReset(LineTrace_Controller *controller);
void LineTrace_ControllerStep(LineTrace_Controller *controller,
                              const LineTrace_ControlConfig *config,
                              uint8_t hasLine, int16_t rawError,
                              float cruiseSpeed,
                              LineTrace_ControlOutput *output);

#endif
