#ifndef APP_APP_INTERNAL_H
#define APP_APP_INTERNAL_H /**< APP_APP_INTERNAL_H 应用层配置宏。 */

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "key5d.h"
#include "menu.h"

extern volatile uint32_t g_app_sample_count; /**< 按键采样累计次数。 */

/* 初始化五向按键状态机和诊断数据。 */
/**
 * @brief 执行 App  Input Init 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void App_InputInit(void);

/* 按固定周期读取按键 ADC，并更新消抖状态和边沿事件。 */
bool App_InputPoll(uint32_t now, Key5D_Event *event);

/* 获取当前消抖后的稳定按键。 */
/**
 * @brief 执行 App  Input Get Stable Key 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回执行结果或状态。
 */
Key5D_Key App_InputGetStableKey(void);

/* 获取最近一次按键 ADC 原始值。 */
uint16_t App_InputGetLastAdc(void);

/* 获取五向按键诊断结构体，用于 OLED 测试页显示。 */
/**
 * @brief 执行 App  Input Get Diagnostic 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回执行结果或状态。
 */
const Key5D_Diagnostic *App_InputGetDiagnostic(void);

/* 将五向按键方向转换成菜单输入事件。 */
Menu_Input App_InputToMenu(Key5D_Key key);

/* 串口输出一次按键按下事件，并翻转板载 LED。 */
/**
 * @brief 执行 App  Log Key Press 功能。
 * @param key 按键方向。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void App_LogKeyPress(Key5D_Key key);

/* 初始化菜单状态和默认显示参数。 */
void App_MenuInitData(void);

/* 循迹实时阶段仅处理菜单按键，不执行显示数据换算和 OLED 刷新。 */
void App_MenuInputRun(void);

/* 结束当前任务并返回任务列表，供拥有独立交互页面的任务使用。 */
void App_MenuReturnToTaskList(void);

/* 设置左右轮速度闭环目标；目标值使用编码器测速的同一单位。 */
void App_VehicleClosedLoopSetTarget(float leftTarget, float rightTarget);

/* 主动刹车：反向速度PID公共输出，保留左右差速用于循迹。 */
void App_VehicleClosedLoopSetActiveBrake(bool enabled);

/* 退出速度闭环并清空 PID 状态，不改写当前电机 PWM。 */
void App_VehicleClosedLoopDisable(void);

/* 退出速度闭环、清空 PID 状态并立即停车。 */
void App_VehicleClosedLoopStop(void);

/* 获取左右轮闭环目标的平均值，供低频菜单刷新使用。 */
float App_VehicleClosedLoopGetAverageTarget(void);

void App_VehicleInit(void);
float App_VehicleGetCommandSpeed(void);
float App_VehicleGetLineKp(void);
void App_VehicleSetLineTelemetry(uint8_t raw, int16_t error,
                                 float correction);

/* 将最近一次按键 ADC 值同步到菜单显示数据中。 */
/**
 * @brief 执行 App  Menu Set Key Adc 功能。
 * @param rawAdc ADC 原始值。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void App_MenuSetKeyAdc(uint16_t rawAdc);

#endif
