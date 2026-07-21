#ifndef __PID_H
#define __PID_H /**< __PID_H 头文件重复包含保护宏。 */
#include "ti_msp_dl_config.h"
/**
 * @brief PID 控制器参数和运行状态。
 * @note Target/Actual/Out 是运行数据，Kp/Ki/Kd 是控制参数，后面的限幅字段用于保护电机输出。
 */
typedef struct
{
	float Target;   /**< 目标值，例如目标轮速。 */
	float Actual;   /**< 实际反馈值，例如编码器测得轮速。 */
	float Out;      /**< PID 当前输出值，最终会转换为 PWM。 */

	float Kp;       /**< 比例系数。 */
	float Ki;       /**< 积分系数。 */
	float Kd;       /**< 微分系数。 */

	float Error0;   /**< 当前误差。 */
	float Error1;   /**< 上一次误差。 */
	float ErrorInt; /**< 积分累计项。 */
	float Deriv;    /**< 微分低通滤波器状态。 */

	float OutMax;   /**< 输出上限。 */
	float OutMin;   /**< 输出下限。 */
	float ErrMax;   /**< 误差限幅，>0 生效，单位与反馈值相同。 */
	float DeltaMax; /**< 单步输出变化限幅，>0 生效。 */

} PID_t;

/**
 * @brief 执行一次 PID 计算并限制输出。
 * @param p PID 控制器句柄。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void PID_Update(PID_t *p);

#endif

