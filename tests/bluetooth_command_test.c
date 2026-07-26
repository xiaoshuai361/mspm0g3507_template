#include "app.h"
#include "bluetooth_command_test.h"

#define CHECK(condition)    \
    do {                    \
        if (!(condition)) { \
            failures++;     \
        }                   \
    } while (0)

/**
 * @brief 检查蓝牙命令兼容二进制数值和 ASCII 字符。
 * @param 无。
 * @note 手机蓝牙助手通常发送 ASCII '1'~'7'，旧遥控器可能发送数值 1~7。
 * @retval 失败项数量，0 表示通过。
 */
uint32_t BluetoothCommand_RunSelfTest(void)
{
    uint32_t failures = 0U;

    CHECK(App_VehicleNormalizeBluetoothCommand(1U) == 1U);
    CHECK(App_VehicleNormalizeBluetoothCommand(6U) == 6U);
    CHECK(App_VehicleNormalizeBluetoothCommand(7U) == 7U);

    CHECK(App_VehicleNormalizeBluetoothCommand((uint8_t)'1') == 1U);
    CHECK(App_VehicleNormalizeBluetoothCommand((uint8_t)'6') == 6U);
    CHECK(App_VehicleNormalizeBluetoothCommand((uint8_t)'7') == 7U);

    CHECK(App_VehicleNormalizeBluetoothCommand(0U) == 0U);
    CHECK(App_VehicleNormalizeBluetoothCommand(8U) == 0U);
    CHECK(App_VehicleNormalizeBluetoothCommand((uint8_t)'0') == 0U);
    CHECK(App_VehicleNormalizeBluetoothCommand((uint8_t)'8') == 0U);
    CHECK(App_VehicleNormalizeBluetoothCommand((uint8_t)'A') == 0U);

    return failures;
}
