#ifndef __ENCODER_H__
#define __ENCODER_H__ /**< __ENCODER_H__ 头文件重复包含保护宏。 */

#include "stdio.h"
#include "ti_msp_dl_config.h"

//电机基本参数
#define ENCODE_13X 13 		//编码器线数
#define JIANSUBI 28    	//减速比
#define BEIPIN 8            //倍频
#define SAMPLE_TIME 0.02f	// 与 App 速度环一致的 20 ms 采样时间
#define CC (ENCODE_13X*JIANSUBI*BEIPIN*SAMPLE_TIME) /**< 编码器速度换算系数。 */

#define PI  3.1415f /**< 圆周率近似值。 */
#define RR  42.5f    		//车轮半径单位cm

/*编码器端口读取宏定义*/
//#define Read_Encoder_A 	(DL_GPIO_readPins(Encoder_PORT,Encoder_A_PIN)==Encoder_A_PIN)?0:1//左轮 A相
//#define Read_Encoder_B  	(DL_GPIO_readPins(Encoder_PORT,Encoder_B_PIN)==Encoder_B_PIN)?0:1//左轮 B相
//#define Read_Encoder_C 	(DL_GPIO_readPins(Encoder_PORT,Encoder_C_PIN)==Encoder_C_PIN)?0:1//右轮 A相
//#define Read_Encoder_D 	(DL_GPIO_readPins(Encoder_PORT,Encoder_D_PIN)==Encoder_D_PIN)?0:1//右轮 B相


//?0:1
//!=0?0x01:0x00
extern float Motor1_Speed;    /**< 左轮原始速度(cm/s)。 */
extern float Motor2_Speed;    /**< 右轮原始速度(cm/s)。 */
extern float Motor1_SpeedFlt; /**< 左轮滤波速度(cm/s)。 */
extern float Motor2_SpeedFlt; /**< 右轮滤波速度(cm/s)。 */
extern float Motor1_Accel;    /**< 左轮加速度(cm/s²)。 */
extern float Motor2_Accel;    /**< 右轮加速度(cm/s²)。 */
extern float Measure_Distance; /**< Measure_Distance 全局状态或配置变量。 */

extern int32_t Motor1_Encoder_Value; /**< 左轮编码器累计计数。 */
extern int32_t Motor2_Encoder_Value; /**< 右轮编码器累计计数。 */
extern int32_t Encoder_CumulativeL;  /**< 左轮上电后累计值（不清零）。 */
extern int32_t Encoder_CumulativeR;  /**< 右轮上电后累计值（不清零）。 */
extern volatile uint32_t g_encoder_gpioa_isr_count; /**< g_encoder_gpioa_isr_count 全局状态或配置变量。 */
extern volatile uint32_t g_encoder_gpiob_isr_count; /**< g_encoder_gpiob_isr_count 全局状态或配置变量。 */
extern volatile uint32_t g_encoder_empty_isr_count; /**< g_encoder_empty_isr_count 全局状态或配置变量。 */

/**
 * @brief 初始化左右轮编码器 GPIO 中断。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Encoder_Init();
void Motor1_Get_Speed(void);
void Motor2_Get_Speed(void);
//测量所有电机速度
/**
 * @brief 根据编码器计数计算左右轮速度 + 滤波速度 + 加速度。
 * @param 无。
 * @note 每20ms调用一次。内部自动更新 Motor1_SpeedFlt、Motor1_Accel 等。
 * @retval 无。
 */
void MEASURE_MOTORS_SPEED(void);

int16_t Encoder_Get(void);
int Encoder_iGet(void);
#endif
