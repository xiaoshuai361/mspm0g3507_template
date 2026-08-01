
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
		uint32_t Encoder_GPIO_State = DL_GPIO_readPins(Encoder_XZ_PORT,
			Encoder_XZ_XZ_A_PIN | Encoder_XZ_XZ_B_PIN);
		if (Encoder_GPIO_Int == 0U) {
			g_encoder_empty_isr_count++;
		}
		Read_Encoder_XZ_A=(Encoder_GPIO_State & Encoder_XZ_XZ_A_PIN)!=0U?0x01:0x00;
		Read_Encoder_XZ_B=(Encoder_GPIO_State & Encoder_XZ_XZ_B_PIN)!=0U?0x01:0x00;
	
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
		uint32_t Encoder_GPIO_State = DL_GPIO_readPins(Encoder_PORT,
			Encoder_A_PIN | Encoder_B_PIN | Encoder_C_PIN | Encoder_D_PIN);
		if (Encoder_GPIO_Int == 0U) {
			g_encoder_empty_isr_count++;
		}
		
		Read_Encoder_A=(Encoder_GPIO_State & Encoder_A_PIN)!=0U?0x01:0x00;
		Read_Encoder_B=(Encoder_GPIO_State & Encoder_B_PIN)!=0U?0x01:0x00;
		Read_Encoder_C=(Encoder_GPIO_State & Encoder_C_PIN)!=0U?0x01:0x00;
		Read_Encoder_D=(Encoder_GPIO_State & Encoder_D_PIN)!=0U?0x01:0x00;
		
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
float Measure_Distance = 0;

/* 加速度计算：速度滤波 + 环形缓冲 + 差分 */
float Motor1_SpeedFlt;   /**< 左轮低通滤波速度(cm/s)。 */
float Motor2_SpeedFlt;   /**< 右轮低通滤波速度(cm/s)。 */
float Motor1_Accel;      /**< 左轮加速度(cm/s²)。 */
float Motor2_Accel;      /**< 右轮加速度(cm/s²)。 */

#define ACCEL_BUF_SIZE  (8U)
#define ACCEL_WINDOW_S  ((float)ACCEL_BUF_SIZE * SAMPLE_TIME)  /* 0.16s */

static float  accelBufL[ACCEL_BUF_SIZE];  /* 左轮速度环形缓冲 */
static float  accelBufR[ACCEL_BUF_SIZE];  /* 右轮速度环形缓冲 */
static uint8_t accelIdx;                  /* 环形缓冲写入位置 */
static uint8_t accelFull;                 /* 缓冲是否已填满一轮 */

#define SPEED_FLT_ALPHA  (0.15f)           /* 速度低通：0.85*旧 + 0.15*新 */
#define ACCEL_FLT_ALPHA  (0.10f)           /* 加速度低通：0.9*旧 + 0.1*新 */

int32_t Encoder_CumulativeL;  /**< 左轮上电后累计（不清零）。 */
int32_t Encoder_CumulativeR;  /**< 右轮上电后累计（不清零）。 */

void MEASURE_MOTORS_SPEED(void)
{
    int32_t encoderLeft;
    int32_t encoderRight;

    /* 读取和清零必须处于同一临界区，避免中断脉冲在两步之间丢失。 */
    NVIC_DisableIRQ(Encoder_INT_IRQN);
    encoderLeft = Motor1_Encoder_Value;
    encoderRight = Motor2_Encoder_Value;
    Motor1_Encoder_Value = 0;
    Motor2_Encoder_Value = 0;
    NVIC_EnableIRQ(Encoder_INT_IRQN);

    Encoder_CumulativeL += encoderLeft;
    Encoder_CumulativeR += encoderRight;
    Motor1_Speed = (float)encoderLeft / CC * PI * RR;
    Motor2_Speed = (float)encoderRight / CC * PI * RR;

    Motor1_Lucheng += Motor1_Speed * SAMPLE_TIME;
    Motor2_Lucheng += Motor2_Speed * SAMPLE_TIME;
    Measure_Distance = Motor1_Lucheng / 2.0f + Motor2_Lucheng / 2.0f;

    if (Measure_Distance > 10000.0f)
    {
        Measure_Distance = 0.0f;
        Motor1_Lucheng = 0.0f;
        Motor2_Lucheng = 0.0f;
    }

    /* ---- 速度低通滤波 ---- */
    Motor1_SpeedFlt = (1.0f - SPEED_FLT_ALPHA) * Motor1_SpeedFlt
                    + SPEED_FLT_ALPHA * Motor1_Speed;
    Motor2_SpeedFlt = (1.0f - SPEED_FLT_ALPHA) * Motor2_SpeedFlt
                    + SPEED_FLT_ALPHA * Motor2_Speed;

    /* ---- 环形缓冲：写入当前滤波速度 ---- */
    accelBufL[accelIdx] = Motor1_SpeedFlt;
    accelBufR[accelIdx] = Motor2_SpeedFlt;
    accelIdx++;
    if (accelIdx >= ACCEL_BUF_SIZE) {
        accelIdx = 0U;
        accelFull = 1U;
    }

    /* ---- 加速度 = 最新速度 - 最旧速度 / 窗口时间 ---- */
    if (accelFull != 0U) {
        /* 环形缓冲下一个位置就是"最旧的" */
        uint8_t oldest = accelIdx;  /* 当前 idx 已指向下一个写入位 = 最旧 */
        float aL = (Motor1_SpeedFlt - accelBufL[oldest]) / ACCEL_WINDOW_S;
        float aR = (Motor2_SpeedFlt - accelBufR[oldest]) / ACCEL_WINDOW_S;

        /* 加速度低通滤波 */
        Motor1_Accel = (1.0f - ACCEL_FLT_ALPHA) * Motor1_Accel
                     + ACCEL_FLT_ALPHA * aL;
        Motor2_Accel = (1.0f - ACCEL_FLT_ALPHA) * Motor2_Accel
                     + ACCEL_FLT_ALPHA * aR;
    } else {
        Motor1_Accel = 0.0f;
        Motor2_Accel = 0.0f;
    }
}
