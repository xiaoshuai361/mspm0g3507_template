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
