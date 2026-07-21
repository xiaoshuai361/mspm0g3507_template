#ifndef GRAYSCALE_SENSOR__
#define GRAYSCALE_SENSOR__ /**< GRAYSCALE_SENSOR__ 头文件重复包含保护宏。 */

#include "ti_msp_dl_config.h"

/*
 * 8通道灰度传感器驱动（74HC165 并进串出）
 * 管脚配置（与 DataCollect GPIO 对应）：
 *   GS_CLK  (时钟输出)    → PB25
 *   GS_SL   (并行置位)    → PA24
 *   GS_SDO  (串行数据输入) → PA25
 *
 * 传感器位序：D8 = 最左通道（Q7/MSB），D1 = 最右通道（Q0/LSB）
 * 检测到线 = 1，未检测到线 = 0
 */

#define SCK_PORT (GPIOB) /**< 灰度传感器时钟 GPIO 端口。 */
#define SCK_PIN (DL_GPIO_PIN_25) /**< 灰度传感器时钟 GPIO 引脚。 */
#define SL_PORT (GPIOA) /**< 灰度传感器锁存 GPIO 端口。 */
#define SL_PIN (DL_GPIO_PIN_24) /**< 灰度传感器锁存 GPIO 引脚。 */
#define SDO_PORT (GPIOA) /**< 灰度传感器串行数据 GPIO 端口。 */
#define SDO_PIN (DL_GPIO_PIN_25) /**< 灰度传感器串行数据 GPIO 引脚。 */

/**
 * @brief 8 路灰度传感器通道数据。
 * @note D1 是最右通道 Q0，D8 是最左通道 Q7；当前工程灰度原始值还可通过 Grayscale_GetRaw() 获取。
 */
typedef struct
{
    uint8_t D1; /**< 最右通道 Q0。 */
    uint8_t D2; /**< 右侧第 2 路通道。 */
    uint8_t D3; /**< 右侧第 3 路通道。 */
    uint8_t D4; /**< 中间偏右通道。 */
    uint8_t D5; /**< 中间偏左通道。 */
    uint8_t D6; /**< 左侧第 3 路通道。 */
    uint8_t D7; /**< 左侧第 2 路通道。 */
    uint8_t D8; /**< 最左通道 Q7。 */
} define_Data;

extern define_Data sensor_data; /**< sensor_data 全局状态或配置变量。 */

/* 读取一帧 8 通道数据，结果写入全局 sensor_data */
/**
 * @brief 读取 8 路灰度传感器移位寄存器数据。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void Grayscale_Read(void);

/* 返回原始 8 位数值（D8=bit7, D1=bit0） */
uint8_t Grayscale_GetRaw(void);

#endif /* GRAYSCALE_SENSOR__ */

