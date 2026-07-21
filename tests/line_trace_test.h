#ifndef TESTS_LINE_TRACE_TEST_H
#define TESTS_LINE_TRACE_TEST_H

#include <stdint.h>

/* 循迹灰度解码自检：返回失败用例数量，0 表示通过。 */
uint32_t LineTrace_RunSelfTest(void);

#endif /* TESTS_LINE_TRACE_TEST_H */
