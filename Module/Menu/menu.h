#ifndef MODULE_MENU_H
#define MODULE_MENU_H /**< MODULE_MENU_H 头文件重复包含保护宏。 */

#include <stdbool.h>
#include <stdint.h>

#define MENU_FONT_SIZE          (16U) /**< MENU_FONT_SIZE 应用层配置宏。 */
#define MENU_VISIBLE_LINE_COUNT (4U) /**< MENU_VISIBLE_LINE_COUNT 应用层配置宏。 */

typedef enum {
    MENU_PAGE_MAIN = 0,
    MENU_PAGE_TASKS,
    MENU_PAGE_PARAMETERS,
    MENU_PAGE_STATUS,
    MENU_PAGE_TIMER
} Menu_Page;

typedef enum {
    MENU_INPUT_NONE = 0,
    MENU_INPUT_UP,
    MENU_INPUT_DOWN,
    MENU_INPUT_BACK,
    MENU_INPUT_ENTER
} Menu_Input;

/**
 * @brief 菜单状态机运行状态。
 * @note 记录当前页面、当前选中项和是否需要重绘；只描述“菜单在哪、选了什么”。
 */
typedef struct {
    Menu_Page page;             /**< 当前所在菜单页面。 */
    uint8_t mainSelection;      /**< 一级菜单当前选中项索引。 */
    uint8_t taskSelection;      /**< 任务布置二级菜单当前选中项索引。 */
    uint8_t activeTask;         /**< 已确认进入/选择的任务编号。 */
    bool dirty;                 /**< 菜单脏标志，true 表示需要重新渲染 OLED。 */
} Menu_State;

/**
 * @brief 菜单显示数据缓存。
 * @note App 层把电机、IMU、ToF、按键等模块数据写入这里，Menu_Render() 只负责读取并显示。
 */
typedef struct {

    bool pidValid;              /**< PID 参数是否有效，false 时参数页显示占位内容。 */
    int16_t pidKpHundredths;    /**< Kp 参数，单位 0.01。 */
    int16_t pidKiHundredths;    /**< Ki 参数，单位 0.01。 */
    int16_t pidKdHundredths;    /**< Kd 参数，单位 0.01。 */

    bool carSpeedValid;         /**< 小车平均速度数据是否有效。 */
    int16_t targetSpeedTenths;  /**< 小车目标速度，单位 0.1。 */
    int16_t actualSpeedTenths;  /**< 小车实际平均速度，单位 0.1。 */

    bool imuValid;              /**< IMU 姿态数据是否有效。 */
    int16_t yawTenths;          /**< yaw 航向角，单位 0.1 度。 */
    int16_t pitchTenths;        /**< pitch 俯仰角，单位 0.1 度。 */
    int16_t rollTenths;         /**< roll 横滚角，单位 0.1 度。 */

    bool speedValid;            /**< 左右轮速度数据是否有效。 */
    int16_t leftSpeedTenths;    /**< 左轮实际速度，单位 0.1。 */
    int16_t rightSpeedTenths;   /**< 右轮实际速度，单位 0.1。 */

    bool tofValid;              /**< ToF 距离数据是否有效。 */
    uint16_t tofDistanceMm;     /**< ToF 最近测距值，单位 mm。 */

    bool batteryValid;          /**< 电池电压数据是否有效。 */
    bool batteryLow;            /**< 电池电压是否低于提醒阈值。 */
    uint16_t batteryMv;         /**< 电池端电压，单位 mV。 */

    uint16_t taskTimeSeconds;   /**< 当前任务运行秒数，0 表示无任务运行。 */

    uint16_t keyAdc;            /**< 五向按键 ADC 最近一次原始值。 */
    int32_t encoderLeft;        /**< 左轮上电累计值。 */
    int32_t encoderRight;       /**< 右轮上电累计值。 */
    uint16_t encoderLines;      /**< 编码器线数。 */
    uint16_t gearRatio;         /**< 电机减速比。 */
    uint8_t encoderMultiplier;  /**< 编码器计数倍频。 */
    uint16_t samplePeriodMs;    /**< 速度采样周期，单位 ms。 */
    uint16_t pwmPeriod;         /**< 电机 PWM 周期计数值。 */
} Menu_ViewData;

/**
 * @brief 执行 Menu  Init 功能。
 * @param state 循迹解码状态。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Menu_Init(Menu_State *state);
void Menu_HandleInput(Menu_State *state, Menu_Input input);

Menu_Page Menu_GetPage(const Menu_State *state);
/**
 * @brief 执行 Menu  Get Main Selection 功能。
 * @param state 循迹解码状态。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t Menu_GetMainSelection(const Menu_State *state);
uint8_t Menu_GetTaskSelection(const Menu_State *state);
uint8_t Menu_GetActiveTask(const Menu_State *state);
const char *Menu_GetMainItemName(uint8_t index);
/**
 * @brief 执行 Menu  Is Dirty 功能。
 * @param state 循迹解码状态。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval true 表示满足条件，false 表示未满足。
 */
bool Menu_IsDirty(const Menu_State *state);
bool Menu_IsDynamicPage(const Menu_State *state);
void Menu_MarkRendered(Menu_State *state);
void Menu_ForcePage(Menu_State *state, Menu_Page page);
void Menu_ReturnToTaskList(Menu_State *state);
void Menu_SetTaskTime(uint16_t seconds);
uint16_t Menu_GetTaskTime(void);

/**
 * @brief 根据菜单状态和数据刷新 OLED 菜单显示。
 * @param state 循迹解码状态。
 * @param data 数据缓冲区。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Menu_Render(Menu_State *state, const Menu_ViewData *data);

#endif
