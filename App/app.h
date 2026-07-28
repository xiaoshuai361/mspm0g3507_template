#ifndef APP_APP_H
#define APP_APP_H /**< APP_APP_H 应用层配置宏。 */

#include <stdbool.h>
#include <stdint.h>

extern volatile uint8_t g_active_task; /**< 当前菜单选择的任务编号。 */
extern volatile uint8_t g_electromagnet_enabled; /**< 电磁铁输出状态，1 表示 PA26 高电平打开。 */
extern volatile uint32_t g_electromagnet_toggle_count; /**< PA28 按键触发切换次数。 */
extern volatile uint8_t g_electromagnet_button_raw; /**< PA28 独立按键原始按下状态。 */

/**
 * @brief 初始化 App 层公共资源和可选模块状态。
 * @param 无。
 * @note 初始化输入、菜单、UART0、OLED，并输出软件自检结果。
 * @retval 无。
 */
void App_Init(void);

/**
 * @brief 执行 App 层主调度。
 * @param 无。
 * @note 在 while(1) 中反复调用，通过注释或保留任务调用行选择启用的模块。
 * @retval 无。
 */
void App_Run(void);

/**
 * @brief 运行五向按键 OLED/串口测试页面。
 * @param 无。
 * @note 非阻塞函数应由 App_Run 或对应周期任务重复调用。
 * @retval 无。
 */
void App_Key5DTestRun(void);

/**
 * @brief 运行 OLED 多级菜单任务。
 * @param 无。
 * @note 处理五向按键输入、更新菜单数据，并按需刷新动态页面。
 * @retval 无。
 */
void App_MenuRun(void);

/**
 * @brief 初始化电磁铁 PA26 输出和 PA28 按键状态机。
 * @param 无。
 * @note 初始默认关闭电磁铁，PA26 输出低电平。
 * @retval 无。
 */
void App_ElectromagnetInit(void);

/**
 * @brief 周期扫描 PA28 独立按键并控制 PA26 电磁铁 MOS。
 * @param 无。
 * @note 每次稳定按下 PA28 会切换一次电磁铁开关状态。
 * @retval 无。
 */
void App_ElectromagnetRun(void);

/**
 * @brief 直接设置电磁铁开关状态。
 * @param enabled true 打开，false 关闭。
 * @note 打开时 PA26 输出高电平，关闭时 PA26 输出低电平。
 * @retval 无。
 */
void App_ElectromagnetSetEnabled(bool enabled);

/**
 * @brief 翻转电磁铁开关状态。
 * @param 无。
 * @retval 无。
 */
void App_ElectromagnetToggle(void);

/**
 * @brief 读取电磁铁当前状态。
 * @param 无。
 * @retval true 表示打开，false 表示关闭。
 */
bool App_ElectromagnetIsEnabled(void);

/**
 * @brief 运行 IMU 初始化、采样和菜单数据更新任务。
 * @param 无。
 * @note 非阻塞函数应由 App_Run 或对应周期任务重复调用。
 * @retval 无。
 */
void App_ImuRun(void);

/**
 * @brief 运行当前启用的 DL1A ToF 测距任务。
 * @param 无。
 * @note 首次调用自动初始化，之后周期读取距离并更新菜单。
 * @retval 无。
 */
void App_ToFRun(void);

/**
 * @brief 运行车辆相关任务，包括蓝牙、循迹、测速和速度闭环。
 * @param 无。
 * @note 非阻塞函数应由 App_Run 或对应周期任务重复调用。
 * @retval 无。
 */
void App_VehicleRun(void);

/**
 * @brief 运行蓝牙命令处理任务。
 * @param 无。
 * @note 命令 1~7 控制小车手动运动和循迹开关。
 * @retval 无。
 */
void App_BluetoothRun(void);

/**
 * @brief 读取灰度循迹数据，并在循迹模式下更新目标速度。
 * @param 无。
 * @note 非阻塞函数应由 App_Run 或对应周期任务重复调用。
 * @retval 无。
 */
void App_LineTraceRun(void);

/**
 * @brief 运行车辆速度闭环控制任务。
 * @param 无。
 * @note 周期测速、PID 计算并输出 PWM。
 * @retval 无。
 */
void App_VehicleControlRun(void);

/**
 * @brief 将蓝牙接收到的字节归一化为车辆命令编号。
 * @param rawCommand UART2 收到的原始字节，可为数值 1~7 或 ASCII 字符 '1'~'7'。
 * @note 返回 0 表示不是有效车辆命令。
 * @retval 1~7 为有效命令，0 为无效命令。
 */
uint8_t App_VehicleNormalizeBluetoothCommand(uint8_t rawCommand);

/**
 * @brief 执行菜单选择的 Task 1 逻辑。
 * @param 无。
 * @note 用户任务接口，保持非阻塞；在菜单选择 Task 1 后由 App_TasksRun() 周期调用。
 * @retval 无。
 */
void App_Task1Run(void);

/**
 * @brief 执行菜单选择的 Task 2 逻辑。
 * @param 无。
 * @note 用户任务接口，保持非阻塞；在菜单选择 Task 2 后由 App_TasksRun() 周期调用。
 * @retval 无。
 */
void App_Task2Run(void);

/**
 * @brief 执行菜单选择的 Task 3 逻辑。
 * @param 无。
 * @note 用户任务接口，保持非阻塞；在菜单选择 Task 3 后由 App_TasksRun() 周期调用。
 * @retval 无。
 */
void App_Task3Run(void);

/**
 * @brief 执行菜单选择的 Task 4 逻辑。
 * @param 无。
 * @note 用户任务接口，保持非阻塞；在菜单选择 Task 4 后由 App_TasksRun() 周期调用。
 * @retval 无。
 */
void App_Task4Run(void);

/**
 * @brief 根据菜单当前任务编号分发执行 Task 1~4。
 * @param 无。
 * @note 在 App_Run() 中周期调用；未选择任务时不执行任何任务逻辑。
 * @retval 无。
 */
void App_TasksRun(void);

/**
 * @brief 更新参数查询页保留的 PID 参数。
 * @param kpHundredths Kp 的 0.01 单位整数值。
 * @param kiHundredths Ki 的 0.01 单位整数值。
 * @param kdHundredths Kd 的 0.01 单位整数值。
 * @note 当前 Speed 页不显示 PID 参数，保留给后续扩展使用。
 * @retval 无。
 */
void App_MenuSetPidData(int16_t kpHundredths, int16_t kiHundredths,
                        int16_t kdHundredths);

/**
 * @brief 更新参数查询页中的小车目标速度和实际速度。
 * @param targetTenths 目标速度，单位为 0.1。
 * @param actualTenths 实际速度，单位为 0.1。
 * @note 车辆任务和菜单动态刷新都会调用本接口同步 Speed 页数据。
 * @retval 无。
 */
void App_MenuSetCarSpeedData(int16_t targetTenths, int16_t actualTenths);

/**
 * @brief 更新小车状态页中的 IMU 姿态角。
 * @param yawTenths yaw 角，单位为 0.1 度。
 * @param pitchTenths pitch 角，单位为 0.1 度。
 * @param rollTenths roll 角，单位为 0.1 度。
 * @note 由 IMU 任务在获得有效姿态帧后调用。
 * @retval 无。
 */
void App_MenuSetImuData(int16_t yawTenths, int16_t pitchTenths,
                        int16_t rollTenths);

/**
 * @brief 更新小车状态页中的左右轮速度。
 * @param leftTenths 左轮速度，单位为 0.1。
 * @param rightTenths 右轮速度，单位为 0.1。
 * @note 用于 OLED 状态页观察左右轮闭环效果。
 * @retval 无。
 */
void App_MenuSetSpeedData(int16_t leftTenths, int16_t rightTenths);

/**
 * @brief 更新小车状态页中的 ToF 距离。
 * @param distanceMm 距离值，单位为 mm。
 * @note 由 ToF 任务在获得有效测距数据后调用。
 * @retval 无。
 */
void App_MenuSetTofData(uint16_t distanceMm);

/**
 * @brief 更新一级菜单底部显示的电池电压。
 * @param batteryMv 电池端电压，单位 mV。
 * @param batteryLow true 表示低电量，需要显示充电提醒。
 * @note 由 App 电池采样任务周期调用，菜单主页面第四行显示。
 * @retval 无。
 */
void App_MenuSetBatteryData(uint16_t batteryMv, bool batteryLow);

/**
 * @brief 标记 IMU 数据无效。
 * @param 无。
 * @note 菜单状态页会显示 OFF，通常用于初始化失败或读取异常。
 * @retval 无。
 */
void App_MenuInvalidateImu(void);

/**
 * @brief 标记左右轮速度数据无效。
 * @param 无。
 * @note 菜单状态页会显示 OFF。
 * @retval 无。
 */
void App_MenuInvalidateSpeed(void);

/**
 * @brief 标记 Speed 页小车速度数据无效。
 * @param 无。
 * @note Speed 页会显示 --，用于区分“暂无数据”和速度为 0。
 * @retval 无。
 */
void App_MenuInvalidateCarSpeed(void);

/**
 * @brief 标记 ToF 数据无效。
 * @param 无。
 * @note 菜单状态页会显示 OFF，通常用于初始化失败或测距状态异常。
 * @retval 无。
 */
void App_MenuInvalidateTof(void);

#endif
