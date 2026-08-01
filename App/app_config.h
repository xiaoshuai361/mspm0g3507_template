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

#define APP_ELECTROMAGNET_KEY_PERIOD_MS (10U) /**< PA28 独立按键扫描周期，单位 ms。 */

#define APP_BATTERY_SAMPLE_PERIOD_MS (5000U) /**< 电池电压采样周期，单位 ms；电池变化慢，5s 更新一次即可。 */
#define APP_BATTERY_LOW_MV (10800U)          /**< 3S 18650 低电量提醒阈值，单位 mV；约 3.6V/节。 */

#define APP_IMU_SAMPLE_PERIOD_MS (20U)       /**< IMU 采样/姿态解算周期，单位 ms；50Hz 适合无 FPU 模板。 */
#define APP_IMU_DIAGNOSTIC_FRAME_COUNT (5U)  /**< IMU 初始化后串口诊断输出帧数。 */
#define APP_IMU_DIAGNOSTIC_PERIOD_MS (1500U) /**< IMU 异常诊断串口输出周期，单位 ms。 */

#define APP_TOF_POLL_PERIOD_MS (50U) /**< ToF 轮询测距周期，单位 ms；20Hz 足够避障/测距显示。 */
#define APP_TOF_LOG_PERIOD_MS (500U) /**< ToF 串口日志输出周期，单位 ms。 */

/* 指定版本的巡线速度和停车参数。 */
#define APP_LINE_CONTROL_PERIOD_MS (20U)
#define APP_TASK2_CROSS_LOCKOUT_FRAMES (80U)
#define APP_TASK2_CROSS_MIN_ENC_AVG (24000U)

/* 3c57b0c：OLED Task 2F。 */
#define APP_TASK2F_LINE_SPEED (100.0f)
#define APP_TASK2F_ACTIVE_BRAKE_DURATION_MS (50U)
#define APP_TASK2F_ACTIVE_BRAKE_PWM_MAX (1000.0f)
#define APP_TASK2F_ACTIVE_BRAKE_STEER_MAX (300.0f)

/* ebf012f：OLED Task 2L；停车阶段降低制动力和纠偏力度。 */
#define APP_TASK2L_LINE_SPEED (10.0f)
#define APP_TASK2L_ACTIVE_BRAKE_DURATION_MS (1000U)
#define APP_TASK2L_ACTIVE_BRAKE_PWM_MAX (600.0f)
#define APP_TASK2L_ACTIVE_BRAKE_STEER_MAX (120.0f)

/* Task 2F/2L 共用的主动制动参数。 */
#define APP_TASK2_ACTIVE_BRAKE_STOP_SPEED (3.0f)

/* OLED Task 4 恢复 85；Task 5/6 保持 8fbb1e3 的 65。 */
#define APP_TASK4_LINE_SPEED (85.0f)
#define APP_TASK56_LINE_SPEED (65.0f)
#define APP_TASK4_STOP_RAMP_STEP (1.0f)
#define APP_TASK56_STOP_RAMP_STEP (4.0f)
#define APP_TASK456_STOP_SPEED (3.0f)
#define APP_TASK4_STOP_TIMEOUT_MS (1800U)
#define APP_TASK56_STOP_TIMEOUT_MS (1500U)

/* 默认基准与 ebf012f/8fbb1e3 一致；Task 2F 单独使用 100.0f。 */
#define APP_VEHICLE_DEFAULT_SPEED (65.0f)
#define APP_VEHICLE_SPEED_PERIOD_MS (20U)   /**< 编码器测速周期，单位 ms；与速度 PID 同步为50Hz。 */
#define APP_VEHICLE_CONTROL_PERIOD_MS (20U) /**< 速度 PID 控制周期，单位 ms；50Hz执行。 */

#define APP_VOFA_PID_GAIN_MAX (200.0f) /**< VOFA 在线设置 Kp/Ki/Kd 的安全上限。 */
#define APP_VOFA_SPEED_MAX (200.0f)
#define APP_VOFA_LINE_KP_MAX (10.0f)

/* 3c57b0c、ebf012f、8fbb1e3 共用的灰度外环参数。 */
#define APP_LINE_CENTER_DEADBAND (4)
#define APP_LINE_DEFAULT_KP (0.85f)
#define APP_LINE_FIXED_KD (0.08f)
#define APP_LINE_CURVE_SLOWDOWN_GAIN (0.30f)
#define APP_LINE_MINIMUM_SPEED_RATIO (0.45f)
#define APP_LINE_CORRECTION_MAX (30.0f)
#define APP_LINE_CORRECTION_SLEW_STEP (6.0f)

#define APP_VEHICLE_PID_OUT_MAX (1800.0f)  /**< 速度 PID 输出上限。 */
#define APP_VEHICLE_PID_OUT_MIN (-1800.0f) /**< 速度 PID 输出下限。 */
#define APP_VEHICLE_PID_ERR_MAX (120.0f)   /**< 速度 PID 误差限幅。 */
#define APP_VEHICLE_PID_DELTA_MAX (120.0f) /**< 指定版本的速度 PID 单次输出变化限幅。 */

/* Task 5/6 钢球稳定任务独立速度PID参数。
 * 钢球任务负载惯性大、速度低，比例和积分系数需单独整定以避免振荡。
 * 左右轮系数分开，适配机械不对称和球仓重心偏移。 */
#define APP_VEHICLE_TASK56_KP_L (11.5f) /**< Task 5/6 左轮 Kp。 */
#define APP_VEHICLE_TASK56_KI_L (0.30f) /**< Task 5/6 左轮 Ki。 */
#define APP_VEHICLE_TASK56_KD_L (0.1f)  /**< Task 5/6 左轮 Kd。 */
#define APP_VEHICLE_TASK56_KP_R (11.5f) /**< Task 5/6 右轮 Kp。 */
#define APP_VEHICLE_TASK56_KI_R (0.30f) /**< Task 5/6 右轮 Ki。 */
#define APP_VEHICLE_TASK56_KD_R (0.1f)  /**< Task 5/6 右轮 Kd。 */

#endif
