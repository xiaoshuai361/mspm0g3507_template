#include "key5d.h"
#include "key5d_test.h"

#define CHECK(condition)         \
    do {                         \
        if (!(condition)) {      \
            failures++;          \
        }                        \
    } while (0)

uint32_t Key5D_RunSelfTest(void)
{
    uint32_t failures = 0U;
    Key5D_Diagnostic diagnostic;
    Key5D_State state;

    CHECK(Key5D_Decode(4095U) == KEY5D_KEY_NONE);
    CHECK(Key5D_Decode(2040U) == KEY5D_KEY_UP);
    CHECK(Key5D_Decode(3410U) == KEY5D_KEY_DOWN);
    CHECK(Key5D_Decode(2730U) == KEY5D_KEY_LEFT);
    CHECK(Key5D_Decode(3270U) == KEY5D_KEY_DOWN);
    CHECK(Key5D_Decode(3070U) == KEY5D_KEY_CENTER);
    CHECK(Key5D_Decode(2500U) == KEY5D_KEY_LEFT);

    /* Measured on the cy_template board's PB24 resistor ladder. */
    CHECK(Key5D_Decode(4000U) == KEY5D_KEY_NONE);
    CHECK(Key5D_Decode(1980U) == KEY5D_KEY_UP);
    CHECK(Key5D_Decode(2650U) == KEY5D_KEY_LEFT);
    CHECK(Key5D_Decode(2980U) == KEY5D_KEY_CENTER);
    CHECK(Key5D_Decode(3180U) == KEY5D_KEY_RIGHT);
    CHECK(Key5D_Decode(3300U) == KEY5D_KEY_DOWN);

    Key5D_Init(&state);
    CHECK(Key5D_Update(&state, 2040U) == KEY5D_EVENT_NONE);
    CHECK(Key5D_Update(&state, 2040U) == KEY5D_EVENT_NONE);
    CHECK(Key5D_Update(&state, 2040U) == KEY5D_EVENT_PRESSED);
    CHECK(Key5D_GetKey(&state) == KEY5D_KEY_UP);
    CHECK(Key5D_Update(&state, 2040U) == KEY5D_EVENT_NONE);

    CHECK(Key5D_Update(&state, 4095U) == KEY5D_EVENT_NONE);
    CHECK(Key5D_Update(&state, 4095U) == KEY5D_EVENT_NONE);
    CHECK(Key5D_Update(&state, 4095U) == KEY5D_EVENT_RELEASED);
    CHECK(Key5D_GetKey(&state) == KEY5D_KEY_NONE);

    Key5D_DiagnosticInit(&diagnostic, 4095U);
    CHECK(diagnostic.rawAdc == 4095U);
    CHECK(diagnostic.minimumAdc == 4095U);
    CHECK(diagnostic.maximumAdc == 4095U);
    CHECK(diagnostic.instantKey == KEY5D_KEY_NONE);
    CHECK(diagnostic.stableKey == KEY5D_KEY_NONE);
    CHECK(diagnostic.lastEvent == KEY5D_EVENT_NONE);

    Key5D_DiagnosticUpdate(&diagnostic, 2040U, KEY5D_KEY_UP,
                           KEY5D_KEY_UP, 3U, KEY5D_EVENT_PRESSED);
    CHECK(diagnostic.rawAdc == 2040U);
    CHECK(diagnostic.minimumAdc == 2040U);
    CHECK(diagnostic.maximumAdc == 4095U);
    CHECK(diagnostic.instantKey == KEY5D_KEY_UP);
    CHECK(diagnostic.candidateKey == KEY5D_KEY_UP);
    CHECK(diagnostic.stableKey == KEY5D_KEY_UP);
    CHECK(diagnostic.consecutiveSamples == 3U);
    CHECK(diagnostic.lastEvent == KEY5D_EVENT_PRESSED);

    Key5D_DiagnosticUpdate(&diagnostic, 3410U, KEY5D_KEY_DOWN,
                           KEY5D_KEY_UP, 1U, KEY5D_EVENT_NONE);
    CHECK(diagnostic.rawAdc == 3410U);
    CHECK(diagnostic.minimumAdc == 2040U);
    CHECK(diagnostic.maximumAdc == 4095U);
    CHECK(diagnostic.instantKey == KEY5D_KEY_DOWN);
    CHECK(diagnostic.candidateKey == KEY5D_KEY_DOWN);
    CHECK(diagnostic.stableKey == KEY5D_KEY_UP);
    CHECK(diagnostic.lastEvent == KEY5D_EVENT_PRESSED);

    CHECK(Key5D_GetEventName(KEY5D_EVENT_NONE)[0] == 'N');
    CHECK(Key5D_GetEventName(KEY5D_EVENT_PRESSED)[0] == 'P');
    CHECK(Key5D_GetEventName(KEY5D_EVENT_RELEASED)[0] == 'R');

    Key5D_Init(&state);
    CHECK(Key5D_Update(&state, 3180U) == KEY5D_EVENT_NONE);
    CHECK(Key5D_Update(&state, 4000U) == KEY5D_EVENT_NONE);
    CHECK(Key5D_Update(&state, 3180U) == KEY5D_EVENT_NONE);
    CHECK(Key5D_GetKey(&state) == KEY5D_KEY_NONE);

    return failures;
}
