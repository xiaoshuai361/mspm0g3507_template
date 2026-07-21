#include "pid.h"
#include "math.h"

/* ================================================================
 * 增量式 PI/PID 控制器（10ms 调节周期）
 *
 * 公式：ΔU[n] = Kp*(e[n]-e[n-1]) + Ki*e[n] + Kd*(e[n]-2*e[n-1]+e[n-2])
 *       U[n]  = clip( U[n-1] + ΔU[n], OutMin, OutMax )
 *
 * 新增限幅：
 *   ErrMax   > 0 时：误差限幅，防止起步/堵转时极大误差导致输出暴冲
 *   DeltaMax > 0 时：单步增量限幅，限制每次输出变化量，使加速更平滑
 *
 * 字段复用（增量式）：
 *   Error0   → e[n]    当前误差（限幅后）
 *   Error1   → e[n-1]  上一拍误差
 *   ErrorInt → e[n-2]  两拍前误差（D 项用）
 *   Deriv    → 滤波后的二阶差分
 *   Out      → 累积输出（reset 时不清零）
 * ================================================================ */
/**
 * @brief 执行一次 PID 计算并限制输出。
 * @param p PID 控制器句柄。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void PID_Update(PID_t *p)
{
	/* 移位误差历史 */
	p->ErrorInt = p->Error1;		   /* e[n-2] */
	p->Error1 = p->Error0;			   /* e[n-1] */
	p->Error0 = p->Target - p->Actual; /* e[n] 原始误差 */

	/* ------- 误差限幅 -------
	 * 防止电机起步/堵转时误差过大，导致 ΔU 一步暴冲
	 * ErrMax = 0 时禁用，建议设为目标速度的 1.5~2 倍 */
	if (p->ErrMax > 0.0f)
	{
		if (p->Error0 > p->ErrMax)
			p->Error0 = p->ErrMax;
		if (p->Error0 < -p->ErrMax)
			p->Error0 = -p->ErrMax;
	}

	/* 增量：比例项 + 积分项 */
	float delta = p->Kp * (p->Error0 - p->Error1) + p->Ki * p->Error0;

	/* D 项可选（速度环建议 Kd=0）*/
	if (p->Kd != 0.0f)
	{
		float d2 = p->Error0 - 2.0f * p->Error1 + p->ErrorInt;
		p->Deriv = 0.6f * p->Deriv + 0.4f * d2;
		delta += p->Kd * p->Deriv;
	}

	/* 累加输出，输出限幅即天然抗饱和 */
	p->Out += delta;
	if (p->Out > p->OutMax)
		p->Out = p->OutMax;
	if (p->Out < p->OutMin)
		p->Out = p->OutMin;
}

