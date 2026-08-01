#include "menu.h"

#include <stdio.h>

#include "oled.h"

#define MENU_MAIN_ITEM_COUNT (3U) /**< MENU_MAIN_ITEM_COUNT 应用层配置宏。 */
#define MENU_TASK_ITEM_COUNT (6U) /**< MENU_TASK_ITEM_COUNT 应用层配置宏。 */
#define MENU_LINE_LENGTH     (16U) /**< MENU_LINE_LENGTH 应用层配置宏。 */

static const char *const mainItems[MENU_MAIN_ITEM_COUNT] = {
    "Task Setup", "Speed", "Car Status"
};

static const char *const taskItems[MENU_TASK_ITEM_COUNT] = {
    "Task 2F", "Task 2L", "Task 3", "Task 4", "Task 5", "Task 6"
};

/* Task 2L uses code 6 so the existing internal task IDs stay unchanged. */
static const uint8_t taskCodes[MENU_TASK_ITEM_COUNT] = {
    1U, 6U, 2U, 3U, 4U, 5U
};

static uint8_t Menu_MoveSelection(uint8_t selection, uint8_t count,
                                  Menu_Input input)
{
    if (input == MENU_INPUT_UP) {
        return (selection == 0U) ? (uint8_t) (count - 1U)
                                 : (uint8_t) (selection - 1U);
    }
    if (input == MENU_INPUT_DOWN) {
        return (uint8_t) ((selection + 1U) % count);
    }
    return selection;
}

/**
 * @brief 初始化菜单状态机。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Menu_Init(Menu_State *state)
{
    state->page = MENU_PAGE_MAIN;
    state->mainSelection = 0U;
    state->taskSelection = 0U;
    state->activeTask = 0U;
    state->dirty = true;
}

/**
 * @brief 处理菜单输入事件并更新菜单状态。
 * @param state 状态枚举值。
 * @param input input 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Menu_HandleInput(Menu_State *state, Menu_Input input)
{
    uint8_t nextSelection;

    if (input == MENU_INPUT_NONE) {
        return;
    }

    if (state->page == MENU_PAGE_MAIN) {
        nextSelection = Menu_MoveSelection(state->mainSelection,
                                           MENU_MAIN_ITEM_COUNT, input);
        if (nextSelection != state->mainSelection) {
            state->mainSelection = nextSelection;
            state->dirty = true;
            return;
        }

        if (input == MENU_INPUT_ENTER) {
            state->page = (Menu_Page) (MENU_PAGE_TASKS + state->mainSelection);
            state->dirty = true;
        }
        return;
    }

    if ((state->page == MENU_PAGE_TIMER) &&
        (input == MENU_INPUT_BACK)) {
        state->page = MENU_PAGE_TASKS;
        state->activeTask = 0U;
        state->dirty = true;
        return;
    }

    if (input == MENU_INPUT_BACK) {
        state->page = MENU_PAGE_MAIN;
        state->dirty = true;
        return;
    }

    if (state->page == MENU_PAGE_TASKS) {
        nextSelection = Menu_MoveSelection(state->taskSelection,
                                           MENU_TASK_ITEM_COUNT, input);
        if (nextSelection != state->taskSelection) {
            state->taskSelection = nextSelection;
            state->dirty = true;
            return;
        }

        if (input == MENU_INPUT_ENTER) {
            const uint8_t selectedTask = taskCodes[state->taskSelection];
            if (state->activeTask != selectedTask) {
                state->activeTask = selectedTask;
                state->dirty = true;
            }
        }
    }

}

/**
 * @brief 获取当前菜单页面。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回菜单页面枚举。
 */
Menu_Page Menu_GetPage(const Menu_State *state)
{
    return state->page;
}

uint8_t Menu_GetMainSelection(const Menu_State *state)
{
    return state->mainSelection;
}

/**
 * @brief 获取任务菜单选择项。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t Menu_GetTaskSelection(const Menu_State *state)
{
    return state->taskSelection;
}

uint8_t Menu_GetActiveTask(const Menu_State *state)
{
    return state->activeTask;
}

/**
 * @brief 执行 Menu_GetMainItemName 功能。
 * @param index index 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回状态名称字符串指针。
 */
const char *Menu_GetMainItemName(uint8_t index)
{
    if (index >= MENU_MAIN_ITEM_COUNT) {
        return "";
    }
    return mainItems[index];
}

/**
 * @brief 判断菜单是否需要重绘。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval true 表示成功或满足条件，false 表示失败或未满足。
 */
bool Menu_IsDirty(const Menu_State *state)
{
    return state->dirty;
}

static uint16_t g_timerSeconds;

void Menu_SetTaskTime(uint16_t seconds)
{
    g_timerSeconds = seconds;
}

uint16_t Menu_GetTaskTime(void)
{
    return g_timerSeconds;
}

void Menu_ForcePage(Menu_State *state, Menu_Page page)
{
    state->page = page;
    state->dirty = true;
}

void Menu_ReturnToTaskList(Menu_State *state)
{
    state->page = MENU_PAGE_TASKS;
    state->activeTask = 0U;
    state->dirty = true;
}

bool Menu_IsDynamicPage(const Menu_State *state)
{
    return (state->page == MENU_PAGE_MAIN) ||
           (state->page == MENU_PAGE_PARAMETERS) ||
           (state->page == MENU_PAGE_STATUS) ||
           (state->page == MENU_PAGE_TIMER);
}

/**
 * @brief 标记菜单已完成渲染。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Menu_MarkRendered(Menu_State *state)
{
    state->dirty = false;
}

static void Menu_ClearFrame(void)
{
    OLED_ClearBuffer();
}

/**
 * @brief 绘制菜单单行文本。
 * @param row row 参数。
 * @param text text 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void Menu_DrawLine(uint8_t row, const char *text)
{
    if (row >= MENU_VISIBLE_LINE_COUNT) {
        return;
    }

    OLED_ShowString(0U, (uint8_t) (row * MENU_FONT_SIZE),
                    text, MENU_FONT_SIZE, 1U);
}

static void Menu_FormatSigned(char *line, size_t size, const char *label,
                              int16_t value, uint16_t divisor,
                              uint8_t fractionalDigits)
{
    int32_t signedValue = value;
    char sign = '+';

    if (signedValue < 0) {
        sign = '-';
        signedValue = -signedValue;
    }

    if (fractionalDigits == 2U) {
        (void) snprintf(line, size, "%s:%c%ld.%02ld", label, sign,
                        (long) (signedValue / divisor),
                        (long) (signedValue % divisor));
    } else {
        (void) snprintf(line, size, "%s:%c%ld.%01ld", label, sign,
                        (long) (signedValue / divisor),
                        (long) (signedValue % divisor));
    }
}

/**
 * @brief 渲染一级菜单。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
/**
 * @brief 渲染一级菜单底部电池状态行。
 * @param data 菜单显示数据缓存。
 * @note 电量正常显示 Bat:xx.xV，低电量显示 LOW BAT CHARGE。
 * @retval 无。
 */
static void Menu_RenderBatteryLine(const Menu_ViewData *data)
{
    char line[MENU_LINE_LENGTH + 1U];

    if (!data->batteryValid) {
        Menu_DrawLine(3U, "Bat: --.-V");
        return;
    }

    if (data->batteryLow) {
        Menu_DrawLine(3U, "LOW BAT CHARGE");
        return;
    }

    (void) snprintf(line, sizeof(line), "Bat:%u.%uV",
                    (unsigned int)(data->batteryMv / 1000U),
                    (unsigned int)((data->batteryMv % 1000U) / 100U));
    Menu_DrawLine(3U, line);
}

/**
 * @brief 渲染一级菜单。
 * @param state 状态枚举值。
 * @param data 数据缓冲区。
 * @note 前三行显示菜单项，第四行固定显示电池电压或低电量提醒。
 * @retval 无。
 */
static void Menu_RenderMain(const Menu_State *state, const Menu_ViewData *data)
{
    uint8_t index;
    char line[MENU_LINE_LENGTH + 1U];

    for (index = 0U; index < MENU_MAIN_ITEM_COUNT; index++) {
        (void) snprintf(line, sizeof(line), "%c %s",
                        (index == state->mainSelection) ? '>' : ' ',
                        Menu_GetMainItemName(index));
        Menu_DrawLine(index, line);
    }

    Menu_RenderBatteryLine(data);
}

/**
 * @brief 渲染任务选择菜单。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void Menu_RenderTasks(const Menu_State *state)
{
    uint8_t i;
    uint8_t start;  /* 滚动窗口起始索引 */
    char line[MENU_LINE_LENGTH + 1U];

    /* 计算滚动窗口：保证当前选中项在可见区域内 */
    if (MENU_TASK_ITEM_COUNT <= MENU_VISIBLE_LINE_COUNT) {
        start = 0U;
    } else {
        start = state->taskSelection;
        if (start > (uint8_t)(MENU_TASK_ITEM_COUNT - MENU_VISIBLE_LINE_COUNT)) {
            start = (uint8_t)(MENU_TASK_ITEM_COUNT - MENU_VISIBLE_LINE_COUNT);
        }
    }

    for (i = 0U; i < MENU_VISIBLE_LINE_COUNT && (start + i) < MENU_TASK_ITEM_COUNT; i++) {
        uint8_t idx = (uint8_t)(start + i);
        (void) snprintf(line, sizeof(line), "%c%c %s",
                        (idx == state->taskSelection) ? '>' : ' ',
                        (taskCodes[idx] == state->activeTask) ? '*' : ' ',
                        taskItems[idx]);
        Menu_DrawLine(i, line);
    }
}

/**
 * @brief 渲染参数/速度页面。
 * @param data 数据缓冲区。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
static void Menu_RenderParameters(const Menu_ViewData *data)
{
    char line[MENU_LINE_LENGTH + 1U];

    if (data->carSpeedValid) {
        Menu_FormatSigned(line, sizeof(line), "Tgt",
                          data->targetSpeedTenths, 10U, 1U);
        Menu_DrawLine(0U, line);
        Menu_FormatSigned(line, sizeof(line), "Act",
                          data->actualSpeedTenths, 10U, 1U);
        Menu_DrawLine(1U, line);
    } else {
        Menu_DrawLine(0U, "Tgt: --");
        Menu_DrawLine(1U, "Act: --");
    }

    if (data->speedValid) {
        Menu_FormatSigned(line, sizeof(line), "L",
                          data->leftSpeedTenths, 10U, 1U);
        Menu_DrawLine(2U, line);
        Menu_FormatSigned(line, sizeof(line), "R",
                          data->rightSpeedTenths, 10U, 1U);
        Menu_DrawLine(3U, line);
    } else {
        Menu_DrawLine(2U, "L: --");
        Menu_DrawLine(3U, "R: --");
    }
}

/**
 * @brief 渲染小车状态页面。
 * @param data 数据缓冲区。
 * @note 第一二行显示 IMU yaw/pitch，第三行显示左右轮累计编码器平均值，
 *       第四行显示 ToF 距离。
 * @retval 无。
 */
static void Menu_RenderStatus(const Menu_ViewData *data)
{
    char line[MENU_LINE_LENGTH + 1U];

    if (data->imuValid) {
        Menu_FormatSigned(line, sizeof(line), "Y", data->yawTenths, 10U, 1U);
        Menu_DrawLine(0U, line);
        Menu_FormatSigned(line, sizeof(line), "P", data->pitchTenths, 10U, 1U);
        Menu_DrawLine(1U, line);
    } else {
        Menu_DrawLine(0U, "IMU: OFF");
    }

    /* 累计编码器平均值：不跳变 */
    {
        int32_t avg = (data->encoderLeft + data->encoderRight) / 2;
        char signM = (avg >= 0) ? '+' : '-';
        int32_t absM = (avg >= 0) ? avg : -avg;

        (void)snprintf(line, sizeof(line), "EncM:%c%ld",
                       signM, (long)absM);
        Menu_DrawLine(2U, line);
    }

    if (data->tofValid) {
        (void) snprintf(line, sizeof(line), "TOF:%umm",
                        (unsigned int) data->tofDistanceMm);
        Menu_DrawLine(3U, line);
    } else {
        Menu_DrawLine(3U, "TOF: OFF");
    }
}

/**
 * @brief 渲染计时页面（全屏，仅第一行显示 Time:xxs）。
 * @param 无。
 */
static void Menu_RenderTimer(void)
{
    char line[MENU_LINE_LENGTH + 1U];
    (void) snprintf(line, sizeof(line), "Time:%us", (unsigned int)g_timerSeconds);
    Menu_DrawLine(0U, line);
}

/**
 * @brief 根据当前页面渲染 OLED 菜单。
 * @param state 状态枚举值。
 * @param data 数据缓冲区。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Menu_Render(Menu_State *state, const Menu_ViewData *data)
{
    Menu_ClearFrame();

    switch (state->page) {
        case MENU_PAGE_TIMER:
            Menu_RenderTimer();
            break;
        case MENU_PAGE_TASKS:
            Menu_RenderTasks(state);
            break;
        case MENU_PAGE_PARAMETERS:
            Menu_RenderParameters(data);
            break;
        case MENU_PAGE_STATUS:
            Menu_RenderStatus(data);
            break;
        case MENU_PAGE_MAIN:
        default:
            Menu_RenderMain(state, data);
            break;
    }

    OLED_Refresh();
    Menu_MarkRendered(state);
}
