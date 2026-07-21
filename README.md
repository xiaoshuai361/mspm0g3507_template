# MSPM0G3507 自制板模板工程

面向 TI MSPM0G3507 LQFP-64 自制板的 CCS 模板工程。工程按 `BSP → Module → App` 三层组织，适合作为电赛小车、传感器测试板和课程项目的起始模板。

> 配置源：`empty.syscfg`  
> IDE：Code Composer Studio / CCS Theia  
> MCU：MSPM0G3507  
> 架构：BSP 板级驱动层、Module 功能模块层、App 应用调度层

## 已集成功能

| 功能 | 状态 | 说明 |
|---|---:|---|
| 五向按键 | 已启用 | PB24 ADC 电阻梯，支持消抖和 OLED 测试页 |
| OLED 菜单 | 已启用 | 128×64 OLED，两级菜单，Task 1~4 用户接口 |
| 电池电压检测 | 已启用 | PA27/BAT_ADC，3S 18650 低电量提醒 |
| UART0 调试 | 已启用 | PA10/PA11，115200 |
| UART2 蓝牙 | 可选 | PA21/PA22，9600，车辆遥控命令 |
| 电机/编码器/PID | 可选 | 双路 PWM、方向控制、轮速闭环 |
| 灰度循迹 | 可选 | 74HC165 八路灰度，循迹状态解码 |
| IMU | 可选 | ICM42688，软件 I²C，姿态角菜单显示 |
| DL1B ToF | 可选 | 逐飞 DL1B，PA1/PA0 I²C，PB14 XS |

## 目录结构

```text
cy_template/
├─ empty.c                 入口文件，只做系统初始化和 App 调用
├─ empty.syscfg            SysConfig 配置源，引脚/外设以它为准
├─ App/                    应用层：任务调度、菜单数据、车辆逻辑
├─ BSP/                    板级层：ADC、UART、Delay、SoftI2C 等底层封装
├─ Module/                 模块层：OLED、Key5D、Menu、Motor、IMU、ToF 等
├─ tests/                  编入固件的模块自检
├─ third_party/            第三方依赖，目前包含逐飞配置库
├─ tools/                  辅助脚本
├─ docs/                   引脚和设计说明
└─ targetConfigs/          CCS/J-Link 目标配置
```

调用方向应保持：

```text
empty.c → App → Module → BSP → DriverLib/SysConfig
```

不要让 BSP 反向依赖 Module 或 App。

## 快速开始

### 1. 安装环境

建议版本：

| 工具 | 推荐版本 |
|---|---|
| CCS / CCS Theia | 支持 MSPM0 的版本 |
| MSPM0 SDK | 2.07.x |
| SysConfig | 1.25.x 或 CCS 自带版本 |
| 编译器 | TI Arm Clang 4.x |

如果使用 VS Code 辅助脚本，可先运行：

```powershell
.\setup.ps1
```

该脚本会检查 CCS、MSPM0 SDK、SysConfig、J-Link/OpenOCD 等路径，并生成本机 `.vscode` 相关配置。`.vscode/local.env.json` 属于本机配置，默认不会提交。

### 2. 导入 CCS

1. 打开 CCS。
2. `File → Import Project`。
3. 选择本仓库目录。
4. 导入 `cy_template`。
5. 选择 `Debug` 配置后 Build。

首次构建时，CCS 会根据 `empty.syscfg` 生成 `Debug/ti_msp_dl_config.*` 和 makefile。`Debug/` 是生成目录，不应提交。

### 3. 命令行构建

在 CCS 已经生成 `Debug/` 构建目录后，可使用：

```powershell
& '<CCS_INSTALL_DIR>\ccs\utils\bin\gmake.exe' -C Debug all -j4
```

示例：

```powershell
& 'D:\APPs\TI\CCS\ccs\utils\bin\gmake.exe' -C Debug all -j4
```

## App 层开关

主要开关在 `App/app.c` 的 `App_Run()`：

```c
void App_Run(void)
{
    // App_Key5DTestRun();   // 五向按键测试页
    App_BatteryRun();        // 电池电压检测
    App_MenuRun();           // OLED 菜单

    App_ImuRun();            // IMU，可按需注释
    App_ToFRun();            // DL1B ToF，可按需注释
    App_VehicleRun();        // 蓝牙/循迹/电机/编码器/PID

    App_TasksRun();          // 菜单 Task 1~4 用户接口
}
```

使用原则：

- OLED 菜单和五向按键测试页不要同时启用。
- 用户任务写在 `App_Task1Run()` 到 `App_Task4Run()`。
- 用户任务必须非阻塞，不要在里面写 `while(1)` 或长时间 `delay_ms()`。

## 菜单说明

一级菜单：

```text
Task Setup
Speed
Car Status
Bat:xx.xV / LOW BAT CHARGE
```

按键映射：

| 按键 | 功能 |
|---|---|
| 上 | 向上选择 |
| 下 | 向下选择 |
| 右 | 进入 |
| 左 | 返回 |
| 中 | 当前预留 |

`Task Setup` 下有 `Task 1` 到 `Task 4`。选中后会周期调用对应的 `App_TaskXRun()`，用于用户自行添加比赛任务逻辑。

## 主要引脚

完整引脚表见 [`docs/pinout.md`](docs/pinout.md) 和 [`模板.md`](模板.md)。

| 功能 | 引脚 |
|---|---|
| UART0 TX/RX | PA10 / PA11 |
| UART2 蓝牙 TX/RX | PA21 / PA22 |
| UART3 TX/RX | PA14 / PA13 |
| OLED SCL/SDA | PB9 / PB8 |
| 软件 I²C SCL/SDA | PA1 / PA0 |
| 五向按键 ADC | PB24 |
| 电池电压 ADC | PA27 |
| DL1B XS | PB14 |
| 电机 PWM | PA8 / PA9 |
| SWD | PA19 / PA20 |

## 电池电压检测

板上电池检测使用：

```text
BAT_ADC → PA27 → ADC0 CH0 → ADC_KEY_ADCMEM_1
```

分压电阻：

```text
R12 = 9.09k
R13 = 1k
```

代码使用整数 mV 换算：

```c
BAT_mV = raw * 33297 / 4095
```

低电量阈值在 `App/app_config.h`：

```c
#define APP_BATTERY_LOW_MV (10800U)
```

该值适合 3 节 18650 串联，约等于每节 3.6V。

## 车辆控制说明

车辆逻辑在 `App/app_vehicle.c`：

- UART2 蓝牙命令处理
- 灰度循迹状态更新
- 编码器测速
- 速度 PID 输出

当前没有采用“所有任务塞进 1ms 总中断”的写法。工程使用 SysTick 提供毫秒时基，在 `App_Run()` 中做非阻塞时间片调度。中断只处理短操作，例如 SysTick 计数、UART 接收、编码器边沿。

这样做的原因是：I²C、OLED、串口格式化、PID 和菜单刷新不适合放进高频 ISR。主循环时间片更容易调试，也更适合模板工程。

## 第三方依赖

当前仓库包含：

```text
third_party/seekfree/zf_device_config.lib
```

它是当前 DL1B/逐飞相关移植所需的小型配置库文件，已经放在仓库内，因此 clone 后不需要再手动建立软链接。

注意：逐飞 MSPM0G3507 开源库声明使用 GPL3.0。若将本工程作为公开项目继续发布，请保留逐飞相关版权声明，并遵守其许可证要求。

## 不要提交的文件

`.gitignore` 已排除：

```text
Debug/
Release/
.headroom-cache/
.vscode/local.env.json
*.out
*.o
*.map
*.d
```

`Debug/ti_msp_dl_config.*` 是 SysConfig 生成文件，不要手动编辑，也不要提交。

## 烧录建议

推荐使用 CCS 或 UniFlash/DSLite 配合 J-Link。DAPLink/OpenOCD 可作为备用方案，但需要使用支持 MSPM0 的 OpenOCD 目标脚本。

详细烧录和调试经验见 [`模板.md`](模板.md)。

## 常见问题

### clone 后为什么没有 Debug 目录？

`Debug/` 是本机生成目录，不提交。用 CCS 导入工程并 Build 后会自动生成。

### 找不到 `ti_msp_dl_config.h`？

先在 CCS 中构建一次，让 SysConfig 生成文件。不要手动复制 `Debug/ti_msp_dl_config.h`。

### VS Code 里跳转或代码提示不对？

先运行：

```powershell
.\setup.ps1
```

然后检查 `.vscode/local.env.json` 和 `.vscode/c_cpp_properties.json` 中的 CCS、SDK、编译器路径是否匹配本机。
