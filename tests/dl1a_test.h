#ifndef TESTS_DL1A_TEST_H
#define TESTS_DL1A_TEST_H

#include <stdint.h>

/**
 * @brief 执行 DL1A 纯逻辑自检。
 * @param 无。
 * @note 只检查距离字节解析，不访问真实 I2C 硬件。
 * @retval 失败项数量，0 表示通过。
 */
uint32_t DL1A_RunSelfTest(void);

#endif
