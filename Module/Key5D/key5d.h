#ifndef MODULE_KEY5D_H
#define MODULE_KEY5D_H /**< MODULE_KEY5D_H 头文件重复包含保护宏。 */

#include <stdint.h>

typedef enum {
    KEY5D_KEY_NONE = 0,
    KEY5D_KEY_UP,
    KEY5D_KEY_DOWN,
    KEY5D_KEY_LEFT,
    KEY5D_KEY_RIGHT,
    KEY5D_KEY_CENTER
} Key5D_Key;

typedef enum {
    KEY5D_EVENT_NONE = 0,
    KEY5D_EVENT_PRESSED,
    KEY5D_EVENT_RELEASED
} Key5D_Event;

/**
 * @brief 五向按键消抖状态。
 * @note 保存候选按键、稳定按键和连续采样次数，用于从 ADC 抖动中提取可靠按键。
 */
typedef struct {
    Key5D_Key candidate;        /**< 当前候选按键，连续多次一致后才会变成稳定按键。 */
    Key5D_Key stable;           /**< 当前消抖后的稳定按键。 */
    uint8_t consecutiveSamples; /**< 候选按键连续出现的采样次数。 */
} Key5D_State;

/**
 * @brief 五向按键诊断数据。
 * @note 用于 OLED 测试页和串口调试，帮助观察 ADC 范围、瞬时按键和消抖结果。
 */
typedef struct {
    uint16_t rawAdc;            /**< 最近一次 ADC 原始值。 */
    uint16_t minimumAdc;        /**< 诊断期间观察到的最小 ADC 值。 */
    uint16_t maximumAdc;        /**< 诊断期间观察到的最大 ADC 值。 */
    Key5D_Key instantKey;       /**< 未消抖的瞬时按键解码结果。 */
    Key5D_Key candidateKey;     /**< 当前候选按键。 */
    Key5D_Key stableKey;        /**< 当前稳定按键。 */
    uint8_t consecutiveSamples; /**< 候选按键连续出现次数。 */
    Key5D_Event lastEvent;      /**< 最近一次按键边沿事件。 */
} Key5D_Diagnostic;

/**
 * @brief 根据 ADC 原始值解码五向按键方向。
 * @param adcValue adcValue 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回执行结果或状态。
 */
Key5D_Key Key5D_Decode(uint16_t adcValue);
void Key5D_Init(Key5D_State *state);
Key5D_Event Key5D_Update(Key5D_State *state, uint16_t adcValue);
Key5D_Key Key5D_GetKey(const Key5D_State *state);
/**
 * @brief 执行 Key5 D  Get Name 功能。
 * @param key 按键方向。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回字符或状态。
 */
const char *Key5D_GetName(Key5D_Key key);
const char *Key5D_GetEventName(Key5D_Event event);

void Key5D_DiagnosticInit(Key5D_Diagnostic *diagnostic,
                          uint16_t initialAdc);
void Key5D_DiagnosticUpdate(Key5D_Diagnostic *diagnostic, uint16_t adcValue,
                            Key5D_Key candidateKey, Key5D_Key stableKey,
                            uint8_t consecutiveSamples, Key5D_Event event);

#endif
