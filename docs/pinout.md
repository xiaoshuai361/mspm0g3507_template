# MSPM0G3507 自制板引脚表

此表来自旧工程 `test_23H_cy/配置.md`，并加入本次五向键 ADC。最终配置以工程根目录 `empty.syscfg` 和生成的 `Debug/ti_msp_dl_config.h` 为准。

| 模块 | MCU 外设/引脚 | 默认 App | 说明 |
|---|---|---:|---|
| 五向键 | PB24 / ADC0_CH5 | 启用 | 原理图网络名 `ADC1_1`；物理 PB24 由 SysConfig 映射到 ADC0 通道 5 |
| 调试串口 | PA10 TX / PA11 RX / UART0 115200 | 启用 | PA10/PA11 为天猛星默认 UART 特殊脚，不用于其它功能 |
| OLED | PB9 SCL / PB8 SDA | 启用 | 4P 软件 I²C OLED |
| LED | PB22 | 启用 | 按键按下时翻转 |
| 电机 PWM | PA8 CCP0 / PA9 CCP1 / TIMA0 | 未启动 | 计数 2000，分频后 1 MHz；调用 `Set_Speed` 后启动 |
| 电机方向 | PB15 L1 / PB17 L2 / PB13 R1 / PB16 R2 | 未使用 | 默认输出保持 SysConfig 初始安全态 |
| 左右编码器 | PB4 / PB5 / PB23 / PB12 | 未启用 NVIC | GPIO 上升沿接口已保留 |
| X/Z 编码器 | PA30 / PA31 | 未启用 NVIC | GPIO 上升沿接口已保留 |
| 蓝牙 | PA21 TX / PA22 RX / UART2 9600 | 未启用 NVIC | 沿用旧板接线；PA21 是板级特殊脚，不要另作 GPIO |
| 灰度上传 | PA14 TX / PA13 RX / UART3 115200 | 未调用 | UART3 发送接口位于 BSP/UART |
| 八路灰度 | PB25 CLK / PA24 SL / PA25 SDO | 未调用 | 74HC165 并进串出 |
| 灰度循迹 | 复用八路灰度 | 未调用 | `Module/LineTrace` 将 8 位灰度值解码为前进/修正/原地转/停止 |
| ICM42688 | PA1 SCL / PA0 SDA | 未初始化 | 软件 I²C，需上拉；地址 0x68/0x69 取决于 AD0 |
| DL1B ToF | PA1 SCL / PA0 SDA / PB14 XS / INT 未接 | 默认关闭 | 软件 I²C 地址 0x29；当前 20ms 轮询读取，`INT/GPIO1` 不使用 |
| 双 GPIO 键 | PA28 / PA29 | 未轮询 | 旧工程短按/双击/长按模块 |
| 蜂鸣器 | PB21 | 未调用 | GPIO 输出 |
| 周期定时器 | TIMA1，1 ms | 未启动 | `TimeA1_Init()` 显式启用 NVIC 并启动 |
| SWD | PA20 SWCLK / PA19 SWDIO | 调试 | DAPLink/CMSIS-DAP |

## Key5D ADC 窗口

| 键 | ADC 窗口 | 源工程中心值 |
|---|---:|---:|
| 上 | 2020–2060 | 2040 |
| 下 | 3390–3430 | 3410 |
| 左 | 2710–2750 | 2730 |
| 右 | 3250–3290 | 3270 |
| 中 | 3050–3090 | 3070 |
| 无按键 | 其它值 | 通常大于 3800 |

如果实板 ADC 落在窗口外，先从 OLED/UART 记录五个方向各 20 次原始值，再按每组最小/最大值留出噪声余量调整窗口；不要改生成的 `ti_msp_dl_config.c/h`。
