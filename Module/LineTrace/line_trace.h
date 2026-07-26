#ifndef MODULE_LINE_TRACE_H
#define MODULE_LINE_TRACE_H /**< MODULE_LINE_TRACE_H 头文件重复包含保护宏。 */

#include <stdint.h>

/*
 * 8 路灰度循迹状态。
 * 数值沿用旧工程，便于串口调试或和旧资料对照。
 */
typedef enum {
    LINE_TRACE_FORWARD = 1,
    LINE_TRACE_BACKWARD = 2,
    LINE_TRACE_STOP = 3,
    LINE_TRACE_TURN_RIGHT = 4,
    LINE_TRACE_TURN_LEFT = 5,
    LINE_TRACE_GO_RIGHT = 6,
    LINE_TRACE_GO_LEFT = 7
} LineTrace_State;

/* 按旧工程位定义将 8 位灰度原始值解码为行驶状态。 */
/**
 * @brief 执行 Line Trace  Decode State 功能。
 * @param raw 原始传感器数据。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回执行结果或状态。
 */
LineTrace_State LineTrace_DecodeState(uint8_t raw);

/* 将黑线=0、白底=1 的灰度原始值转换后解码为循迹状态。 */
LineTrace_State LineTrace_DecodeActiveLowRaw(uint8_t raw);

/**
 * @brief 将高有效灰度数据归一化为连续偏差。
 * @param raw 高有效灰度数据，1 表示该路检测到黑线。
 * @param errorTenths 输出偏差，单位为 0.1 路间距；左负右正。
 * @param activeCount 输出检测到黑线的通道数量，可为 NULL。
 * @note D8(bit7) 为最左，D1(bit0) 为最右；无通道检测到黑线时返回 0。
 * @retval 1 表示成功得到偏差，0 表示丢线。
 */
uint8_t LineTrace_CalcWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount);

/**
 * @brief 将低有效灰度原始值归一化为连续偏差。
 * @param raw 低有效灰度数据，0 表示该路检测到黑线。
 * @param errorTenths 输出偏差，单位为 0.1 路间距；左负右正。
 * @param activeCount 输出检测到黑线的通道数量，可为 NULL。
 * @retval 1 表示成功得到偏差，0 表示丢线。
 */
uint8_t LineTrace_CalcActiveLowWeightedError(uint8_t raw, int16_t *errorTenths, uint8_t *activeCount);

/* 返回状态名，方便串口日志阅读。 */
/**
 * @brief 执行 Line Trace  State Name 功能。
 * @param state 循迹解码状态。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回字符或状态。
 */
const char *LineTrace_StateName(LineTrace_State state);

#endif /* MODULE_LINE_TRACE_H */
