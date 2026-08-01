# MSPM0G3507 自制板模板工程

## 当前两任务调参固件

- `Task 1 Speed`：平滑加速到 `S`，定速 3 秒，平滑减速到零，停稳后换向循环。
- `Task 2 Trace`：8 路灰度加权循迹；停车线触发后继续循迹，反向速度PID公共输出主动制动，速度接近零后关断。
- 当前只配置 UART0（115200 baud），UART2/香橙派和 UART3 已移除。

VOFA+ 下行命令以换行结束，逗号也可替换成 `=` 或 `:`：

```text
KP,11.5    速度环 Kp
KI,0.3     速度环 Ki
KD,0.1     速度环 Kd
ki,1.0     灰度方向环 Kp（小写，大小写敏感）
S,60       Task 1 定速值 / Task 2 巡航速度（cm/s）
```

VOFA+ 上行使用 JustFloat，共 11 个通道：左目标、左实际、右目标、右实际、速度 Kp、速度 Ki、速度 Kd、灰度 Kp、设定速度、灰度误差、方向修正。

### 当前循迹控制链路

```text
74HC165 读取 8 路灰度（低有效）
    → D1~D8 加权重心计算
    → 统一低强度灰度滤波（约1/3旧 + 2/3当前）+ 中心死区
    → 灰度 PD 方向修正 + 弯道基准降速
    → 左右轮目标速度（base ± correction）
    → 编码器 20 ms 测速
    → 左右独立增量式速度 PID
    → PID 输出限幅
    → 电机驱动层输出方向和 PWM
```

所以链路可以概括为“读取灰度 → 加权计算 → 输出目标差速 → 输入速度环 → 输出 PWM”。这里的“目标差速”是左右轮的目标速度，不是直接修改 PWM。业务层没有开环 PWM 接口；只有速度 PID 能调用电机输出，另保留一个零输出安全停机接口。

### 默认参数与修改位置

| 参数 | 当前默认值 | 固化修改位置 | UART0 在线修改 |
|---|---:|---|---|
| 基准速度 `S` | 60 cm/s | `App/app_config.h` 的 `APP_VEHICLE_DEFAULT_SPEED` | `S,60` |
| 速度环 Kp / Ki / Kd | 11.5 / 0.3 / 0.1 | `App/app_vehicle.c` 的 `leftSpeedPid`、`rightSpeedPid` | `KP,11.5` / `KI,0.3` / `KD,0.1` |
| 灰度 Kp | 0.85 | `App/app_config.h` 的 `APP_LINE_DEFAULT_KP` | `ki,0.85` |
| 灰度 Kd | 0.08 | `App/app_config.h` 的 `APP_LINE_FIXED_KD` | 当前固定，需改代码 |
| 8 路权重 | -50,-42,-28,-10,10,28,42,50 | `Module/LineTrace/line_trace.c` 的 `lineWeights` | 不支持 |
| 灰度滤波 | 约33%旧值 + 67%当前值 | `Module/LineTrace/line_trace.c` | 不支持 |
| 中心死区 | 4 | `APP_LINE_CENTER_DEADBAND` | 不支持 |
| 弯道降速 / 最低速度比例 | 0.30 / 0.45 | `APP_LINE_CURVE_SLOWDOWN_GAIN` / `APP_LINE_MINIMUM_SPEED_RATIO` | 不支持 |
| 修正限幅 / 每周期变化 | 30 / 6 cm/s | `APP_LINE_CORRECTION_MAX` / `APP_LINE_CORRECTION_SLEW_STEP` | 不支持 |
| 速度 PID 输出限幅 | ±1800 | `APP_VEHICLE_PID_OUT_MAX/MIN` | 不支持 |

## 多人协作先看：`.vscode` 不上传

本仓库已经在 `.gitignore` 中忽略整个 `.vscode/`：

```gitignore
.vscode/
```

所以以后每个人自己的 `.vscode` 不会被上传，也不会因为普通修改被 Git 管。

注意一个 Git 机制问题：

这次因为 `.vscode` 之前已经上传过，所以别人第一次 `git pull` 到移除 `.vscode` 的提交时，Git 可能会删除之前被仓库跟踪的 `.vscode` 文件。这个无法完全避免，因为 Git 要同步“仓库里删除了这些文件”的状态。

但之后 Git 就不会再管 `.vscode` 了。

如果队友已经 clone 过，稳妥操作是：

```powershell
Copy-Item .vscode .vscode_backup -Recurse
git pull
.\setup.ps1
```

如果 `.vscode` 已经被删了，直接运行：

```powershell
.\setup.ps1
```

脚本会按自己的电脑路径重新生成 `.vscode`。以后再 `git pull`，本机 `.vscode` 就不会再被远端影响。

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
└─ tools/vscode/           共享构建/下载脚本和 J-Link 目标配置
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

该脚本会检查 CCS、MSPM0 SDK、SysConfig、J-Link/OpenOCD 等路径，并生成本机 `.vscode` 相关配置。由于每个人的安装路径不同，`.vscode/` 整个目录都作为本机私有配置处理，默认不会提交。

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

当前车辆控制只使用 UART0 VOFA、8 路灰度、编码器、速度 PID 和电机驱动。UART2/香橙派控制链路已删除。任务状态机与灰度目标生成在 `App/app.c`，灰度算法在 `Module/LineTrace/line_trace.c`，速度闭环在 `App/app_vehicle.c`，PWM 寄存器只在 `Module/Motor/motor.c` 内访问。

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
.vscode/
*.out
*.o
*.map
*.d
```

`Debug/ti_msp_dl_config.*` 是 SysConfig 生成文件，不要手动编辑，也不要提交。

## 烧录建议

推荐使用 CCS 或 UniFlash/DSLite 配合 J-Link。DAPLink/OpenOCD 可作为备用方案，但需要使用支持 MSPM0 的 OpenOCD 目标脚本。

本机命令行下载入口：

```powershell
cmd /d /c tools\vscode\flash-dslite-jlink.cmd "%CD%" "D:\APPs\TI\Unflsh\dslite.bat"
```

共享 J-Link 配置为 `tools/vscode/MSPM0G3507-jlink.ccxml`，不再依赖被 Git 忽略的本机 `targetConfigs/`。

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

### 多人开发时 `.vscode` 怎么处理？

`.vscode/` 整个目录不提交。仓库里的共享 VS Code 辅助脚本放在 `tools/vscode/`，带有绝对路径的 VS Code 配置由每个人自己生成。clone 后运行：

```powershell
.\setup.ps1
```

脚本会按自己的电脑环境重新生成 `.vscode/settings.json`、`.vscode/tasks.json`、`.vscode/launch.json`、`.vscode/c_cpp_properties.json` 和 `.vscode/extensions.json`。
