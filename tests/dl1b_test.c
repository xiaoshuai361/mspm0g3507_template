#include "dl1b.h"
#include "dl1b_test.h"

#define CHECK(condition)    \
    do {                    \
        if (!(condition)) { \
            failures++;     \
        }                   \
    } while (0)

uint32_t DL1B_RunSelfTest(void)
{
    uint32_t failures = 0U;
    uint16_t distance = 0U;

    CHECK(DL1B_DecodeRangeStatus(0x89U));
    CHECK(!DL1B_DecodeRangeStatus(0x00U));
    CHECK(!DL1B_DecodeRangeStatus(0x01U));

    CHECK(DL1B_DecodeDistance(0x00U, 0x64U, &distance));
    CHECK(distance == 100U);
    CHECK(DL1B_DecodeDistance(0x0FU, 0xA0U, &distance));
    CHECK(distance == 4000U);
    CHECK(!DL1B_DecodeDistance(0x0FU, 0xA1U, &distance));
    CHECK(!DL1B_DecodeDistance(0x20U, 0x00U, &distance));
    CHECK(!DL1B_DecodeDistance(0x00U, 0x00U, (void *)0));

    return failures;
}
