#include "menu.h"
#include "menu_test.h"
#include "IMU.h"
#include "Encoder.h"
#include "bsp_adc.h"
#include "oled.h"

extern u8 OLED_GRAM[144][8];

#define CHECK(condition)         \
    do {                         \
        if (!(condition)) {      \
            failures++;          \
        }                        \
    } while (0)

uint32_t Menu_RunSelfTest(void)
{
    uint32_t failures = 0U;
    Menu_State state;

    CHECK(MENU_FONT_SIZE == 16U);
    CHECK(MENU_VISIBLE_LINE_COUNT == 4U);

    Menu_Init(&state);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_MAIN);
    CHECK(Menu_GetMainItemName(1U)[0] == 'S');
    CHECK(Menu_GetMainItemName(1U)[1] == 'p');
    CHECK(Menu_GetMainItemName(1U)[2] == 'e');
    CHECK(Menu_GetMainItemName(1U)[3] == 'e');
    CHECK(Menu_GetMainItemName(1U)[4] == 'd');
    CHECK(Menu_GetMainItemName(1U)[5] == '\0');
    CHECK(Menu_GetMainSelection(&state) == 0U);
    CHECK(Menu_GetTaskSelection(&state) == 0U);
    CHECK(Menu_GetActiveTask(&state) == 0U);
    CHECK(Menu_IsDirty(&state));

    Menu_MarkRendered(&state);
    CHECK(!Menu_IsDirty(&state));
    Menu_HandleInput(&state, MENU_INPUT_NONE);
    CHECK(!Menu_IsDirty(&state));

    Menu_HandleInput(&state, MENU_INPUT_UP);
    CHECK(Menu_GetMainSelection(&state) == 2U);
    Menu_HandleInput(&state, MENU_INPUT_DOWN);
    CHECK(Menu_GetMainSelection(&state) == 0U);

    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_TASKS);
    CHECK(Menu_GetTaskSelection(&state) == 0U);

    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetActiveTask(&state) == 1U);
    Menu_HandleInput(&state, MENU_INPUT_DOWN);
    CHECK(Menu_GetTaskSelection(&state) == 1U);
    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetActiveTask(&state) == 6U);
    Menu_HandleInput(&state, MENU_INPUT_UP);

    Menu_HandleInput(&state, MENU_INPUT_UP);
    CHECK(Menu_GetTaskSelection(&state) == 5U);
    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetActiveTask(&state) == 5U);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_TASKS);

    Menu_ReturnToTaskList(&state);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_TASKS);
    CHECK(Menu_GetActiveTask(&state) == 0U);
    CHECK(Menu_IsDirty(&state));
    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetActiveTask(&state) == 5U);

    Menu_ForcePage(&state, MENU_PAGE_TIMER);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_TIMER);
    Menu_HandleInput(&state, MENU_INPUT_BACK);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_TASKS);
    CHECK(Menu_GetTaskSelection(&state) == 5U);
    CHECK(Menu_GetActiveTask(&state) == 0U);

    /* Returning to the task page must not restart the previous task. */
    Menu_HandleInput(&state, MENU_INPUT_NONE);
    CHECK(Menu_GetActiveTask(&state) == 0U);
    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetActiveTask(&state) == 5U);

    Menu_HandleInput(&state, MENU_INPUT_BACK);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_MAIN);
    CHECK(Menu_GetMainSelection(&state) == 0U);

    Menu_HandleInput(&state, MENU_INPUT_DOWN);
    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_PARAMETERS);
    CHECK(Menu_IsDynamicPage(&state));
    Menu_MarkRendered(&state);
    Menu_HandleInput(&state, MENU_INPUT_UP);
    Menu_HandleInput(&state, MENU_INPUT_DOWN);
    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_PARAMETERS);
    CHECK(!Menu_IsDirty(&state));
    Menu_HandleInput(&state, MENU_INPUT_BACK);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_MAIN);
    CHECK(Menu_GetMainSelection(&state) == 1U);

    Menu_HandleInput(&state, MENU_INPUT_DOWN);
    Menu_HandleInput(&state, MENU_INPUT_ENTER);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_STATUS);
    CHECK(Menu_IsDynamicPage(&state));
    Menu_HandleInput(&state, MENU_INPUT_BACK);
    CHECK(Menu_GetPage(&state) == MENU_PAGE_MAIN);
    CHECK(Menu_GetMainSelection(&state) == 2U);

    OLED_GRAM[0][0] = 0xFFU;
    OLED_GRAM[127][7] = 0xA5U;
    OLED_ClearBuffer();
    CHECK(OLED_GRAM[0][0] == 0U);
    CHECK(OLED_GRAM[127][7] == 0U);

    CHECK(IMU_DegreesToTenths(0.0f) == 0);
    CHECK(IMU_DegreesToTenths(12.34f) == 123);
    CHECK(IMU_DegreesToTenths(12.36f) == 124);
    CHECK(IMU_DegreesToTenths(-12.34f) == -123);
    CHECK(IMU_DegreesToTenths(-12.36f) == -124);
    CHECK(IMU_DegreesToTenths(4000.0f) == INT16_MAX);
    CHECK(IMU_DegreesToTenths(-4000.0f) == INT16_MIN);
    CHECK(Motor1_Speed == 0.0f);
    CHECK(Motor2_Speed == 0.0f);
    CHECK(BSP_ADC_BatteryRawToMv(0U) == 0U);
    CHECK(BSP_ADC_BatteryRawToMv(4095U) == 33297U);

    {
        const float zeroSensorData[6] = {0.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f};
        const float validSensorData[6] = {0.0f, 0.0f, 1000.0f,
                                          0.0f, 0.0f, 0.0f};

        CHECK(IMU_SensorDataIsAllZero(zeroSensorData));
        CHECK(!IMU_SensorDataIsAllZero(validSensorData));
    }

    {
        Menu_ViewData viewData = {0};

        viewData.carSpeedValid = true;
        viewData.speedValid = true;
        viewData.targetSpeedTenths = 500;
        viewData.actualSpeedTenths = 476;
        viewData.leftSpeedTenths = 510;
        viewData.rightSpeedTenths = 442;
        CHECK(viewData.carSpeedValid);
        CHECK(viewData.speedValid);
        CHECK(viewData.targetSpeedTenths == 500);
        CHECK(viewData.actualSpeedTenths == 476);
        CHECK(viewData.leftSpeedTenths == 510);
        CHECK(viewData.rightSpeedTenths == 442);
    }

    return failures;
}
