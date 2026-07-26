#ifndef TESTS_BLUETOOTH_COMMAND_TEST_H
#define TESTS_BLUETOOTH_COMMAND_TEST_H

#include <stdint.h>

/**
 * @brief 执行蓝牙车辆命令归一化自检。
 * @param 无。
 * @note 只测试字节解析，不访问 UART2 或电机硬件。
 * @retval 失败项数量，0 表示通过。
 */
uint32_t BluetoothCommand_RunSelfTest(void);

#endif
