#ifndef APP_APP_CONFIG_H
#define APP_APP_CONFIG_H /**< APP_APP_CONFIG_H 头文件重复包含保护宏。 */

/*
 * MSPM0G3507 没有硬件 FPU，float 姿态解算、OLED 软件 I2C 全屏刷新、
 * UART 格式化日志都不适合高频运行。这里集中管理 App 层周期，后续调车
 * 优先改本文件，不要分散到各个 app_xxx.c 里到处找。
 */

#define APP_KEY_SAMPLE_PERIOD_MS (10U)            /**< 五向按键 ADC 采样周期，单位 ms；保持 100Hz 保证手感。 */
#define APP_KEY_TEST_DISPLAY_PERIOD_MS (100U)     /**< 五向按键测试页 OLED 刷新周期，单位 ms。 */
#define APP_KEY_TEST_SERIAL_PERIOD_MS (200U)      /**< 五向按键测试串口输出周期，单位 ms。 */
#define APP_MENU_DYNAMIC_DISPLAY_PERIOD_MS (200U) /**< 菜单动态页面刷新周期，单位 ms；降低 OLED 占用。 */

#define APP_BATTERY_SAMPLE_PERIOD_MS (5000U) /**< 电池电压采样周期，单位 ms；电池变化慢，5s 更新一次即可。 */
#define APP_BATTERY_LOW_MV (10800U)          /**< 3S 18650 低电量提醒阈值，单位 mV；约 3.6V/节。 */

#define APP_IMU_SAMPLE_PERIOD_MS (20U)       /**< IMU 采样/姿态解算周期，单位 ms；50Hz 适合无 FPU 模板。 */
#define APP_IMU_DIAGNOSTIC_FRAME_COUNT (5U)  /**< IMU 初始化后串口诊断输出帧数。 */
#define APP_IMU_DIAGNOSTIC_PERIOD_MS (1000U) /**< IMU 异常诊断串口输出周期，单位 ms。 */

#define APP_TOF_POLL_PERIOD_MS (50U) /**< ToF 轮询测距周期，单位 ms；20Hz 足够避障/测距显示。 */
#define APP_TOF_LOG_PERIOD_MS (500U) /**< ToF 串口日志输出周期，单位 ms。 */

#define APP_VEHICLE_DEFAULT_SPEED (50.0f)   /**< 车辆默认目标速度。 */
#define APP_VEHICLE_LINE_PERIOD_MS (10U)    /**< 灰度循迹读取周期，单位 ms；灰度读取轻，保留快速响应。 */
#define APP_VEHICLE_SPEED_PERIOD_MS (20U)   /**< 编码器测速周期，单位 ms；降低抖动和 CPU 占用。 */
#define APP_VEHICLE_CONTROL_PERIOD_MS (20U) /**< 速度 PID 控制周期，单位 ms；50Hz 控制兼顾响应和负载。 */
#define APP_VEHICLE_DEBUG_PERIOD_MS (1000U) /**< 车辆状态串口调试输出周期，单位 ms。 */

#define APP_VEHICLE_LINE_DIFF_GAIN (1.20f) /**< 循迹加权偏差到左右轮差速修正的比例。 */
#define APP_VEHICLE_LINE_DIFF_MAX (45.0f)  /**< 循迹单侧最大差速修正，避免急转时目标速度过大。 */

#define APP_VEHICLE_PID_OUT_MAX (1800.0f)  /**< 速度 PID 输出上限。 */
#define APP_VEHICLE_PID_OUT_MIN (-1800.0f) /**< 速度 PID 输出下限。 */
#define APP_VEHICLE_PID_ERR_MAX (120.0f)   /**< 速度 PID 误差限幅。 */
#define APP_VEHICLE_PID_DELTA_MAX (500.0f) /**< 速度 PID 单次输出变化限幅。 */

#endif
