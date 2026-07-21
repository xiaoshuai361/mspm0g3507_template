
#include "Encoder.h"
#include "Encoder_XZ.h"
#include "oled.h"

int32_t Motor1_Encoder_Value=0; /**< 左轮编码器累计计数。 */
int32_t Motor2_Encoder_Value=0; /**< 右轮编码器累计计数。 */
volatile uint32_t g_encoder_gpioa_isr_count; /**< g_encoder_gpioa_isr_count 全局状态或配置变量。 */
volatile uint32_t g_encoder_gpiob_isr_count; /**< g_encoder_gpiob_isr_count 全局状态或配置变量。 */
volatile uint32_t g_encoder_empty_isr_count; /**< g_encoder_empty_isr_count 全局状态或配置变量。 */

float Motor1_Speed = 0.0f; /**< 左轮实测速度。 */
float Motor2_Speed = 0.0f; /**< 右轮实测速度。 */

volatile bool Read_Encoder_A; /**< Read_Encoder_A 全局状态或配置变量。 */
volatile bool Read_Encoder_B; /**< Read_Encoder_B 全局状态或配置变量。 */
volatile bool Read_Encoder_C; /**< Read_Encoder_C 全局状态或配置变量。 */
volatile bool Read_Encoder_D; /**< Read_Encoder_D 全局状态或配置变量。 */


volatile bool Read_Encoder_XZ_A; /**< Read_Encoder_XZ_A 全局状态或配置变量。 */
volatile bool Read_Encoder_XZ_B; /**< Read_Encoder_XZ_B 全局状态或配置变量。 */

//外部中断读取编码器的值 

/**
 * @brief 初始化左右轮编码器 GPIO 中断。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Encoder_Init()
{
	DL_GPIO_clearInterruptStatus(Encoder_PORT, Encoder_A_PIN | Encoder_B_PIN | Encoder_C_PIN | Encoder_D_PIN);
	NVIC_ClearPendingIRQ(Encoder_INT_IRQN);
	NVIC_EnableIRQ(Encoder_INT_IRQN);    
}
/**
 * @brief 读取并清零指定编码器计数。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回有符号 16 位结果。
 */
int16_t Encoder_Get(void)
{
	int16_t Temp;
	// Temp = TIM_GetCounter(TIM3);
	// TIM_SetCounter(TIM3, 0);
	Temp = Motor1_Encoder_Value;
	return Temp;
}
int i = 0; /**< i 全局状态或配置变量。 */
/**
 * @brief 按方向修正编码器计数。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回整型结果。
 */
int Encoder_iGet(void)
{
	return i;
}
/**
 * @brief 执行 G R O U P1  I R Q Handler 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void GROUP1_IRQHandler(void){
	//OLED_ShowNum(0, 48, i++, 1, 16, 1);
	//旋转编码器
	i++;
	if(DL_Interrupt_getStatusGroup(DL_INTERRUPT_GROUP_1,DL_INTERRUPT_GROUP1_GPIOA)){
		g_encoder_gpioa_isr_count++;
		uint32_t Encoder_GPIO_Int = DL_GPIO_getEnabledInterruptStatus(Encoder_XZ_PORT,Encoder_XZ_XZ_A_PIN | Encoder_XZ_XZ_B_PIN);
		if (Encoder_GPIO_Int == 0U) {
			g_encoder_empty_isr_count++;
		}
		Read_Encoder_XZ_A=DL_GPIO_readPins(Encoder_XZ_PORT,Encoder_XZ_XZ_A_PIN)!=0?0x01:0x00;
		Read_Encoder_XZ_B=DL_GPIO_readPins(Encoder_XZ_PORT,Encoder_XZ_XZ_B_PIN)!=0?0x01:0x00;
	
		//通道1 A相
		if ((Encoder_GPIO_Int & Encoder_XZ_XZ_A_PIN) == Encoder_XZ_XZ_A_PIN){
			
			DL_GPIO_clearInterruptStatus(Encoder_XZ_PORT, Encoder_XZ_XZ_A_PIN);
			if(Read_Encoder_XZ_A == 1){ //上升沿
				if (Read_Encoder_XZ_B == 0){
					Encoder_XZ_Value++;
				}
				else if(Read_Encoder_XZ_B == 1){
					Encoder_XZ_Value--;
				}
			}
			else if(Read_Encoder_XZ_A == 0){//下降沿
				if (Read_Encoder_XZ_B == 0){
					Encoder_XZ_Value--;
				}
				else if(Read_Encoder_XZ_B == 1){
					Encoder_XZ_Value++;
				}
			}
		}		
		//通道2 B相
//		if ((Encoder_GPIO_Int & Encoder_XZ_XZ_B_PIN) == Encoder_XZ_XZ_B_PIN){
//			
//			
//			DL_GPIO_clearInterruptStatus(Encoder_XZ_PORT,Encoder_XZ_XZ_B_PIN);
//			if(Read_Encoder_XZ_B == 1){ //上升沿
//				if (Read_Encoder_XZ_A == 0){
//					Encoder_XZ_Value--;
//				}
//				else if (Read_Encoder_XZ_A == 1){
//					Encoder_XZ_Value++;
//				}
//			}
//			else if(Read_Encoder_XZ_B == 0){//下降沿
//				if (Read_Encoder_XZ_A == 0){
//					Encoder_XZ_Value++;
//				}
//				else if (Read_Encoder_XZ_A == 1){
//					Encoder_XZ_Value--;
//				}
//			}
//		}
	}
/*******************************************************/
	if(DL_Interrupt_getStatusGroup(DL_INTERRUPT_GROUP_1,DL_INTERRUPT_GROUP1_GPIOB)){
		g_encoder_gpiob_isr_count++;
		
		uint32_t Encoder_GPIO_Int = DL_GPIO_getEnabledInterruptStatus(Encoder_PORT,Encoder_A_PIN | Encoder_B_PIN | Encoder_C_PIN | Encoder_D_PIN);
		if (Encoder_GPIO_Int == 0U) {
			g_encoder_empty_isr_count++;
		}
		
		Read_Encoder_A=DL_GPIO_readPins(Encoder_PORT,Encoder_A_PIN)!=0?0x01:0x00;
		Read_Encoder_B=DL_GPIO_readPins(Encoder_PORT,Encoder_B_PIN)!=0?0x01:0x00;
		Read_Encoder_C=DL_GPIO_readPins(Encoder_PORT,Encoder_C_PIN)!=0?0x01:0x00;
		Read_Encoder_D=DL_GPIO_readPins(Encoder_PORT,Encoder_D_PIN)!=0?0x01:0x00;
		
        //通道1 左轮A相
		if ((Encoder_GPIO_Int & Encoder_A_PIN) == Encoder_A_PIN){
			
			DL_GPIO_clearInterruptStatus(Encoder_PORT, Encoder_A_PIN);
			if(Read_Encoder_A == 1){ //上升沿
				if (Read_Encoder_B == 0){
					Motor1_Encoder_Value++;
				}
				else if(Read_Encoder_B == 1){
					Motor1_Encoder_Value--;
				}
			}
			else if(Read_Encoder_A == 0){//下降沿
				if (Read_Encoder_B == 0){
					Motor1_Encoder_Value--;
				}
				else if(Read_Encoder_B == 1){
					Motor1_Encoder_Value++;
				}
			}
		}		
		//通道2 左轮B相
		if ((Encoder_GPIO_Int & Encoder_B_PIN) == Encoder_B_PIN){
			
			
			DL_GPIO_clearInterruptStatus(Encoder_PORT,Encoder_B_PIN);
			if(Read_Encoder_B == 1){ //上升沿
				if (Read_Encoder_A == 0){
					Motor1_Encoder_Value--;
				}
				else if (Read_Encoder_A == 1){
					Motor1_Encoder_Value++;
				}
			}
			else if(Read_Encoder_B == 0){//下降沿
				if (Read_Encoder_A == 0){
					Motor1_Encoder_Value++;
				}
				else if (Read_Encoder_A == 1){
					Motor1_Encoder_Value--;
				}
			}
		}
		//通道3 右轮A相		
		if ((Encoder_GPIO_Int & Encoder_C_PIN) == Encoder_C_PIN){
			
			DL_GPIO_clearInterruptStatus(Encoder_PORT, Encoder_C_PIN);
			if(Read_Encoder_C == 1){ //上升沿
				if (Read_Encoder_D  == 0){
					Motor2_Encoder_Value--;
				}
				else if (Read_Encoder_D  == 1){
					Motor2_Encoder_Value++;
				}
			}
			else if(Read_Encoder_C == 0){//下降沿
				if (Read_Encoder_D  == 0){
					Motor2_Encoder_Value++;
				}
				else if (Read_Encoder_D  == 1){
					Motor2_Encoder_Value--;
				}
			}
		}		
		//通道4 右轮B相
		if ((Encoder_GPIO_Int & Encoder_D_PIN) == Encoder_D_PIN){
			
			DL_GPIO_clearInterruptStatus(Encoder_PORT,Encoder_D_PIN);
			if(Read_Encoder_D == 1){ //上升沿
				if (Read_Encoder_C  == 0){
					Motor2_Encoder_Value++;
				}
				else if (Read_Encoder_C  == 1){
					Motor2_Encoder_Value--;
				}
			}
			else if(Read_Encoder_D == 0){//下降沿
				if (Read_Encoder_C  == 0){
					Motor2_Encoder_Value--;
				}
				else if (Read_Encoder_C  == 1){
					Motor2_Encoder_Value++;
				}
			}
		}
	}
}

//编码器1计算速度
/**
 * @brief 执行 Motor1  Get  Speed 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Motor1_Get_Speed(void){
    short Encoder_TIM = 0;
    float Speed = 0;
    Encoder_TIM= Motor1_Encoder_Value;
    Motor1_Encoder_Value=0;
    Speed =(float)Encoder_TIM/(CC)*PI*RR;//计算速度
    Motor1_Speed = Speed;
}

//编码器2计算速度
/**
 * @brief 执行 Motor2  Get  Speed 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Motor2_Get_Speed(void){
    short Encoder_TIM = 0;
    float Speed = 0;
    Encoder_TIM= Motor2_Encoder_Value;
    Motor2_Encoder_Value=0;
    Speed =(float)Encoder_TIM/(CC)*PI*RR;//计算速度
    Motor2_Speed = Speed;
}

float Motor1_Lucheng,Motor2_Lucheng;
float Measure_Distance = 0; /**< Measure_Distance 全局状态或配置变量。 */

//测量所有电机速度
/**
 * @brief 根据编码器计数计算左右轮速度。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void MEASURE_MOTORS_SPEED(void){
	Motor1_Get_Speed();Motor2_Get_Speed();
	
	Motor1_Lucheng += Motor1_Speed*SAMPLE_TIME;//路程累计
	Motor2_Lucheng += Motor2_Speed*SAMPLE_TIME;//路程累计
	Measure_Distance = Motor1_Lucheng/2.0 +Motor2_Lucheng/2.0;
	
	if(Measure_Distance>10000)
	{
		Measure_Distance = 0;
		Motor1_Lucheng = 0;
		Motor2_Lucheng = 0;
		
	}
}
