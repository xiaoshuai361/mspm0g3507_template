#include "dl1a.h"
#include "dl1a_test.h"

#define CHECK(condition)    \
    do {                    \
        if (!(condition)) { \
            failures++;     \
        }                   \
    } while (0)

/**
 * @brief 执行 DL1A 距离解析自检。
 * @param 无。
 * @note 覆盖有效值、越界值、0 值和空指针，避免 App 自检依赖真实 ToF 硬件。
 * @retval 失败项数量，0 表示通过。
 */
uint32_t DL1A_RunSelfTest(void)
{
    uint32_t failures = 0U;
    uint16_t distance = 0U;

    CHECK(DL1A_DecodeDistanceValue(1U));
    CHECK(DL1A_DecodeDistanceValue(4000U));
    CHECK(!DL1A_DecodeDistanceValue(0U));
    CHECK(!DL1A_DecodeDistanceValue(4001U));

    CHECK(DL1A_DecodeDistance(0x00U, 0x64U, &distance));
    CHECK(distance == 100U);
    CHECK(DL1A_DecodeDistance(0x0FU, 0xA0U, &distance));
    CHECK(distance == 4000U);
    CHECK(!DL1A_DecodeDistance(0x0FU, 0xA1U, &distance));
    CHECK(!DL1A_DecodeDistance(0x00U, 0x00U, &distance));
    CHECK(!DL1A_DecodeDistance(0x00U, 0x64U, (void *)0));

    return failures;
}
