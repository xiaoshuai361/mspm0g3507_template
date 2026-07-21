
#include "board.h"
#include "stdio.h"


/**
 * @brief 计算浮点数绝对值。
 * @param num 输入的浮点数。
 * @note 仅按正负号处理，不额外处理 NaN。
 * @retval 返回 num 的非负值。
 */
float apf(float num)
{
	if (num > 0)
	{
		return num;
	}
	else
	{
		return -num;
	}
}
/**
 * @brief 计算整数绝对值。
 * @param num 输入的整数。
 * @note int 最小值取负会溢出，调用方需避开该边界值。
 * @retval 返回 num 的非负值。
 */
int apc(int num)
{
	if (num > 0)
	{
		return num;
	}
	else
	{
		return -num;
	}
}
