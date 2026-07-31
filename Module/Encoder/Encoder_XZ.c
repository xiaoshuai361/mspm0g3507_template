#include "Encoder_XZ.h"

volatile int32_t Encoder_XZ_Value; /**< Encoder_XZ_Value 全局状态或配置变量。 */
static bool encoderXZEnabled;

/**
 * @brief 使能辅助编码器接口。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Encoder_XZ_Enable(void)
{
	/* GPIOA/GPIOB share GROUP1 IRQ. Gate only the rotary source so the
	 * wheel encoders keep running when Task 5 finishes. */
	DL_GPIO_disableInterrupt(Encoder_XZ_PORT, Encoder_XZ_XZ_B_PIN);
	DL_GPIO_clearInterruptStatus(
		Encoder_XZ_PORT, Encoder_XZ_XZ_A_PIN | Encoder_XZ_XZ_B_PIN);
	DL_GPIO_enableInterrupt(Encoder_XZ_PORT, Encoder_XZ_XZ_A_PIN);
	encoderXZEnabled = true;
	NVIC_EnableIRQ(Encoder_XZ_INT_IRQN);
}

void Encoder_XZ_Disable(void)
{
	DL_GPIO_disableInterrupt(
		Encoder_XZ_PORT, Encoder_XZ_XZ_A_PIN | Encoder_XZ_XZ_B_PIN);
	DL_GPIO_clearInterruptStatus(
		Encoder_XZ_PORT, Encoder_XZ_XZ_A_PIN | Encoder_XZ_XZ_B_PIN);
	encoderXZEnabled = false;
}


/**
 * @brief 设置辅助编码器状态。
 * @param state 状态枚举值。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Encoder_XZ_Set(bool state)
{
	if (state) {
		Encoder_XZ_Enable();
	} else {
		Encoder_XZ_Disable();
	}
}

int32_t Encoder_XZ_GetValue(void)
{
	uint32_t primask = __get_PRIMASK();
	int32_t value;

	__disable_irq();
	value = Encoder_XZ_Value;
	__set_PRIMASK(primask);

	return value;
}

void Encoder_XZ_SetValue(int32_t value)
{
	uint32_t primask = __get_PRIMASK();

	__disable_irq();
	Encoder_XZ_Value = value;
	__set_PRIMASK(primask);
}

int32_t Encoder_XZ_GetClampedValue(int32_t minValue, int32_t maxValue)
{
	uint32_t primask = __get_PRIMASK();
	int32_t value;

	__disable_irq();
	value = Encoder_XZ_Value;
	if (value < minValue) {
		value = minValue;
		Encoder_XZ_Value = value;
	} else if (value > maxValue) {
		value = maxValue;
		Encoder_XZ_Value = value;
	}
	__set_PRIMASK(primask);

	return value;
}

/**
 * @brief 翻转辅助编码器状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Encoder_XZ_toggle(void)
{
	Encoder_XZ_Set(!encoderXZEnabled);
}

