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

/**
 * @brief 轻量巡线控制参数，全部使用整数，适合 Cortex-M0+ 高频调用。
 */
typedef struct {
    int16_t cruisePwm;              /**< 直线巡航 PWM。 */
    int16_t lostPwm;                /**< 持续丢线搜索时的基准 PWM。 */
    int16_t rampUpStep;             /**< 每个控制周期的起步增量。 */
    int16_t rampDownStep;           /**< 每个控制周期的基准减速量。 */
    int16_t curveSlowdownGain;      /**< 每单位绝对偏差对应的基准降速量。 */
    int16_t steeringKp;             /**< 死区外偏差到差速修正的整数比例。 */
    int16_t edgeSteeringKp;         /**< 大偏差区使用的增强转向比例。 */
    int16_t edgeSteeringThreshold;  /**< 启用增强转向的死区后偏差阈值。 */
    int16_t steeringMax;            /**< 最大单侧差速修正。 */
    int16_t centerDeadband;         /**< 中心死区，单位 0.1 路间距。 */
    int16_t fastErrorThreshold;     /**< 进入快速滤波的绝对偏差阈值。 */
    int16_t fastDeltaThreshold;     /**< 进入快速滤波的偏差跳变阈值。 */
    int16_t lostSearchStartError;   /**< 丢线搜索的初始等效偏差。 */
    uint8_t lostSearchStepFrames;   /**< 搜索偏差每增加 1 所需的帧数。 */
    uint8_t lostHoldFrames;         /**< 短时丢线保持上一方向的帧数。 */
    uint16_t lostStopFrames;        /**< 连续丢线达到该帧数后停车。 */
} LineTrace_ControlConfig;

/**
 * @brief 巡线控制器跨帧状态。
 */
typedef struct {
    int16_t filteredError;
    int16_t previousRawError;
    int16_t basePwm;
    int8_t lastDirection;
    uint16_t lostFrames;
} LineTrace_Controller;

/**
 * @brief 单次巡线控制输出和诊断量。
 */
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
uint8_t LineTrace_CalcWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount);
uint8_t LineTrace_CalcActiveLowWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount);
const char *LineTrace_StateName(LineTrace_State state);

void LineTrace_ControllerReset(LineTrace_Controller *controller);
void LineTrace_ControllerStep(LineTrace_Controller *controller,
                              const LineTrace_ControlConfig *config,
                              uint8_t hasLine, int16_t rawError,
                              LineTrace_ControlOutput *output);

/* A点停车线检测：D3~D5 或 D4~D6 连续为黑 + 连续2帧确认。 */
CrossLine_Type LineTrace_DetectCrossLine(uint8_t raw, uint8_t activeCount,
                                          uint16_t *lockoutFrames, uint8_t *confirmCount);
void LineTrace_ResetCrossDetect(uint16_t *lockoutFrames, uint8_t *confirmCount);

#endif /* MODULE_LINE_TRACE_H */
