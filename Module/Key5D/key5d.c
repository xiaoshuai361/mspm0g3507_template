#include "key5d.h"

#define KEY5D_DEBOUNCE_SAMPLES (3U) /**< KEY5D_DEBOUNCE_SAMPLES 模块配置或状态宏。 */

/* PB24 values measured on the cy_template board:
 * UP 1980, LEFT 2650, CENTER 2980, RIGHT 3180, DOWN 3300, idle 4000.
 * Boundaries are midpoints between adjacent measured levels. */
#define KEY5D_UP_MIN      (1500U) /**< KEY5D_UP_MIN 模块配置或状态宏。 */
#define KEY5D_UP_MAX      (2314U) /**< KEY5D_UP_MAX 模块配置或状态宏。 */
#define KEY5D_LEFT_MIN    (2315U) /**< KEY5D_LEFT_MIN 模块配置或状态宏。 */
#define KEY5D_LEFT_MAX    (2814U) /**< KEY5D_LEFT_MAX 模块配置或状态宏。 */
#define KEY5D_CENTER_MIN  (2815U) /**< KEY5D_CENTER_MIN 模块配置或状态宏。 */
#define KEY5D_CENTER_MAX  (3079U) /**< KEY5D_CENTER_MAX 模块配置或状态宏。 */
#define KEY5D_RIGHT_MIN   (3080U) /**< KEY5D_RIGHT_MIN 模块配置或状态宏。 */
#define KEY5D_RIGHT_MAX   (3239U) /**< KEY5D_RIGHT_MAX 模块配置或状态宏。 */
#define KEY5D_DOWN_MIN    (3240U) /**< KEY5D_DOWN_MIN 模块配置或状态宏。 */
#define KEY5D_DOWN_MAX    (3649U) /**< KEY5D_DOWN_MAX 模块配置或状态宏。 */

/**
 * @brief 判断 ADC 值是否落在指定按键范围。
 * @param value 寄存器值。
 * @param minimum minimum 参数。
 * @param maximum maximum 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回整型结果。
 */
static int Key5D_InRange(uint16_t value, uint16_t minimum, uint16_t maximum)
{
    return (value >= minimum) && (value <= maximum);
}

/**
 * @brief 根据 ADC 原始值解码五向按键方向。
 * @param adcValue adcValue 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回五向按键枚举。
 */
Key5D_Key Key5D_Decode(uint16_t adcValue)
{
    if (Key5D_InRange(adcValue, KEY5D_UP_MIN, KEY5D_UP_MAX)) {
        return KEY5D_KEY_UP;
    }
    if (Key5D_InRange(adcValue, KEY5D_LEFT_MIN, KEY5D_LEFT_MAX)) {
        return KEY5D_KEY_LEFT;
    }
    if (Key5D_InRange(adcValue, KEY5D_CENTER_MIN, KEY5D_CENTER_MAX)) {
        return KEY5D_KEY_CENTER;
    }
    if (Key5D_InRange(adcValue, KEY5D_RIGHT_MIN, KEY5D_RIGHT_MAX)) {
        return KEY5D_KEY_RIGHT;
    }
    if (Key5D_InRange(adcValue, KEY5D_DOWN_MIN, KEY5D_DOWN_MAX)) {
        return KEY5D_KEY_DOWN;
    }
    return KEY5D_KEY_NONE;
}

/**
 * @brief 初始化五向按键消抖状态。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Key5D_Init(Key5D_State *state)
{
    state->candidate = KEY5D_KEY_NONE;
    state->stable = KEY5D_KEY_NONE;
    state->consecutiveSamples = 0U;
}

/**
 * @brief 更新五向按键消抖状态并产生边沿事件。
 * @param state 状态枚举值。
 * @param adcValue adcValue 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回执行结果或状态。
 */
Key5D_Event Key5D_Update(Key5D_State *state, uint16_t adcValue)
{
    const Key5D_Key sample = Key5D_Decode(adcValue);

    if (sample != state->candidate) {
        state->candidate = sample;
        state->consecutiveSamples = 1U;
        return KEY5D_EVENT_NONE;
    }

    if (state->consecutiveSamples < KEY5D_DEBOUNCE_SAMPLES) {
        state->consecutiveSamples++;
    }
    if ((state->consecutiveSamples < KEY5D_DEBOUNCE_SAMPLES) ||
        (sample == state->stable)) {
        return KEY5D_EVENT_NONE;
    }

    state->stable = sample;
    return (sample == KEY5D_KEY_NONE) ? KEY5D_EVENT_RELEASED
                                      : KEY5D_EVENT_PRESSED;
}

/**
 * @brief 获取五向按键稳定值。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回五向按键枚举。
 */
Key5D_Key Key5D_GetKey(const Key5D_State *state)
{
    return state->stable;
}

const char *Key5D_GetName(Key5D_Key key)
{
    switch (key) {
        case KEY5D_KEY_UP:
            return "UP";
        case KEY5D_KEY_DOWN:
            return "DOWN";
        case KEY5D_KEY_LEFT:
            return "LEFT";
        case KEY5D_KEY_RIGHT:
            return "RIGHT";
        case KEY5D_KEY_CENTER:
            return "CENTER";
        case KEY5D_KEY_NONE:
        default:
            return "NONE";
    }
}

/**
 * @brief 执行 Key5D_GetEventName 功能。
 * @param event 输出按键边沿事件。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回状态名称字符串指针。
 */
const char *Key5D_GetEventName(Key5D_Event event)
{
    switch (event) {
        case KEY5D_EVENT_PRESSED:
            return "PRESS";
        case KEY5D_EVENT_RELEASED:
            return "RELEASE";
        case KEY5D_EVENT_NONE:
        default:
            return "NONE";
    }
}

void Key5D_DiagnosticInit(Key5D_Diagnostic *diagnostic,
                          uint16_t initialAdc)
{
    diagnostic->rawAdc = initialAdc;
    diagnostic->minimumAdc = initialAdc;
    diagnostic->maximumAdc = initialAdc;
    diagnostic->instantKey = Key5D_Decode(initialAdc);
    diagnostic->candidateKey = KEY5D_KEY_NONE;
    diagnostic->stableKey = KEY5D_KEY_NONE;
    diagnostic->consecutiveSamples = 0U;
    diagnostic->lastEvent = KEY5D_EVENT_NONE;
}

void Key5D_DiagnosticUpdate(Key5D_Diagnostic *diagnostic, uint16_t adcValue,
                            Key5D_Key candidateKey, Key5D_Key stableKey,
                            uint8_t consecutiveSamples, Key5D_Event event)
{
    diagnostic->rawAdc = adcValue;
    if (adcValue < diagnostic->minimumAdc) {
        diagnostic->minimumAdc = adcValue;
    }
    if (adcValue > diagnostic->maximumAdc) {
        diagnostic->maximumAdc = adcValue;
    }

    diagnostic->instantKey = Key5D_Decode(adcValue);
    diagnostic->candidateKey = candidateKey;
    diagnostic->stableKey = stableKey;
    diagnostic->consecutiveSamples = consecutiveSamples;
    if (event != KEY5D_EVENT_NONE) {
        diagnostic->lastEvent = event;
    }
}
