# MSPM0G3507 Board Template Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use Markdown checkboxes for tracking.

**Goal:** Convert `cy_template` into a safe BSP–Module–App board template and port the ADC resistor-ladder five-way key to physical pin PB24.

**Architecture:** SysConfig remains the source of truth for MSPM0G3507 LQFP-64 pinmux and peripheral instances. `BSP/` owns DriverLib and generated macros, `Module/` owns reusable devices/algorithms, `App/` owns the demo task, and `empty.c` only performs generated initialization and enters the app.

**Tech Stack:** MSPM0 SDK 2.07.00.05, SysConfig 1.25.0, TI Arm Clang 4.0.4 LTS, CCS generated GNU Make flow, CMSIS-DAP/OpenOCD.

---

### Task 1: Key decoding contract and RED test

**Files:**
- Create: `tests/key5d_test.c`
- Create: `Module/Key5D/key5d.h`
- Create: `Module/Key5D/key5d.c`

- [x] Define `Key5D_Key`, `Key5D_Event`, `Key5D_Decode(uint16_t)`, `Key5D_Init`, and `Key5D_Update` in the test-facing API.
- [x] Compile `tests/key5d_test.c` before the implementation exists and confirm failure is caused by missing `Module/Key5D/key5d.c`/symbols.
- [x] Implement the five source-project voltage windows (UP 2020–2060, DOWN 3390–3430, LEFT 2710–2750, RIGHT 3250–3290, CENTER 3050–3090, released above 3800) and a three-sample debounce edge state machine.
- [x] Compile the test and the real module with TI Arm Clang using `-I Module/Key5D`; expect exit code 0. The same assertions are also run at target startup and publish a failure count.

### Task 2: PB24 ADC BSP and SysConfig

**Files:**
- Modify: `empty.syscfg`
- Create: `BSP/ADC/bsp_adc.h`
- Create: `BSP/ADC/bsp_adc.c`

- [x] Add one ADC12 instance assigned by physical pin PB24 and let SysConfig resolve its legal instance/channel; preserve all metadata and existing peripherals.
- [x] Generate into a temporary directory with the exact local SysConfig 1.25.0 / SDK 2.07 product command and inspect the generated header for `PB24`, ADC instance, memory index, and `SYSCFG_DL_init`.
- [x] Implement `BSP_ADC_KeyReadRaw()` using only generated macros and DriverLib conversion start/idle/stop/result calls.

### Task 3: Three-layer source layout

**Files:**
- Create/relocate: `BSP/Delay/*`, `BSP/UART/*`, `Module/OLED/*`
- Create: `BSP/Board/*`, `BSP/SoftI2C/*`, `BSP/Timer/*`
- Create: `Module/{Beep,Bluetooth,Encoder,Grayscale,IMU,Motor,PID}/*`
- Modify: `.cproject`

- [x] Move the current delay/UART/OLED code to its owning layer and change includes without changing runtime behavior.
- [x] Port reusable old-project drivers into the owning layer; remove `User.h` dependencies and keep motor/timer actions opt-in.
- [x] Add only the exact layer include directories required by CCS; do not edit generated `Debug/*.mk` files.
- [x] Keep PA10/PA11 as the existing UART0 debug pins; retain old-project PA21/PA22 UART2 only as documented existing board wiring and never start it from the default app.

### Task 4: Safe template app

**Files:**
- Create: `App/app.h`
- Create: `App/app.c`
- Modify: `empty.c`

- [x] Implement `App_Init()` to initialize only the safe OLED/UART/key demo and run Key5D self-tests.
- [x] Implement `App_Run()` as a non-blocking polling loop: sample every 10 ms, update key debounce, show raw ADC/key name on OLED, send one UART line per press, and toggle PB22 on a press.
- [x] Reduce `empty.c` to `SYSCFG_DL_init(); App_Init(); while (1) App_Run();`.

### Task 5: Configuration, build, and hardware verification

**Files:**
- Modify: `README.md`
- Create: `docs/pinout.md`

- [x] Run `check_syscfg.py cy_template` and the exact SysConfig CLI command; require no errors and report warnings separately.
- [x] Run `gmake clean all` in `Debug`; require exit code 0 and inspect generated ADC/PB24 macros plus the linker map.
- [x] Run `check_syscfg.py cy_template --probe`, then the OpenOCD helper `probe`; do not flash if CMSIS-DAP remains unknown, multiple, locked, or conflicts.
- [x] If probe succeeds, flash the explicit `Debug/cy_template.out` with verify and reset-run, then read registers or target test status without performing unlock/mass erase.
- [x] Document the three layers, enabled-by-default behavior, full inherited pin table, ADC mapping, test commands, and OpenOCD result.

### Self-review

- [x] Every requested source is mapped: PRO_05 key algorithm, test_23H_cy BSP peripherals, PB24 ADC, CCS build, DAPLink/OpenOCD, and BSP–Module–App layout.
- [x] No generated SysConfig/build file is edited.
- [x] No motor or timer control loop starts from the default app.
- [x] All new public functions are either covered by the key self-test or compile/link integration verification.
