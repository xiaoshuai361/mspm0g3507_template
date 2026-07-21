#include "Encoder_XZ.h"

int32_t Encoder_XZ_Value; /**< Encoder_XZ_Value 全局状态或配置变量。 */

/**
 * @brief 使能辅助编码器接口。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Encoder_XZ_Enable()
{
	NVIC_EnableIRQ(Encoder_XZ_INT_IRQN);    
}

void Encoder_XZ_Disable()
{
	NVIC_DisableIRQ(Encoder_XZ_INT_IRQN);    
}


/**
 * @brief 设置辅助编码器状态。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Encoder_XZ_Set(bool state)
{
	if(state==1)
	{NVIC_EnableIRQ(Encoder_XZ_INT_IRQN); }
	else
	{NVIC_DisableIRQ(Encoder_XZ_INT_IRQN);}
	  
}

/**
 * @brief 翻转辅助编码器状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Encoder_XZ_toggle()
{
	uint32_t state = NVIC_GetEnableIRQ(Encoder_XZ_INT_IRQN);
	if(state==0U){Encoder_XZ_Set(1);}
	else {Encoder_XZ_Set(0);}
}

