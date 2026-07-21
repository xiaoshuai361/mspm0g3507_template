#include "Grayscale_Sensor.h"

define_Data sensor_data; /**< sensor_data 全局状态或配置变量。 */

/*
 * Grayscale_Read()
 *
 * 时序（74HC165 手册）：
 *   1. 拉低 SL → 所有输入并行加载到寄存器
 *   2. 拉高 SL → 锁定，允许移位
 *   3. 立即读取 Q7（MSB，对应 D8）
 *   4. 循环 7 次：拉高 SCK → 读取当前 GS_SDO → 拉低 SCK
 *      依次得到 Q6(D7)...Q0(D1)
 */
/**
 * @brief 读取 8 路灰度传感器移位寄存器数据。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void Grayscale_Read(void)
{
    uint8_t i;
    uint8_t bit;

    /* 并行加载：SL 低脉冲 */
    DL_GPIO_clearPins(SL_PORT, SL_PIN);
    delay_cycles(32000000 / 1000 / 1000); /* ~1 us @ 32 MHz */
    DL_GPIO_setPins(SL_PORT, SL_PIN);     /* 锁定数据 */
    delay_cycles(32000000 / 1000 / 1000);

    /* 读 Q7 → D8（MSB，最左通道） */
    sensor_data.D8 = (DL_GPIO_readPins(SDO_PORT, SDO_PIN) > 0) ? 1u : 0u;

    /* 移位读取 Q6(D7) … Q0(D1) */
    for (i = 0; i < 7; i++)
    {
        DL_GPIO_setPins(SCK_PORT, SCK_PIN); /* 上升沿：移位 */
        delay_cycles(32000000 / 1000 / 1000);
        bit = (DL_GPIO_readPins(SDO_PORT, SDO_PIN) > 0) ? 1u : 0u;
        DL_GPIO_clearPins(SCK_PORT, SCK_PIN); /* 下降沿 */
        delay_cycles(32000000 / 1000 / 1000);

        switch (i)
        {
        case 0:
            sensor_data.D7 = bit;
            break;
        case 1:
            sensor_data.D6 = bit;
            break;
        case 2:
            sensor_data.D5 = bit;
            break;
        case 3:
            sensor_data.D4 = bit;
            break;
        case 4:
            sensor_data.D3 = bit;
            break;
        case 5:
            sensor_data.D2 = bit;
            break;
        case 6:
            sensor_data.D1 = bit;
            break;
        default:
            break;
        }
    }
}

/**
 * @brief 获取最近一次灰度原始数据。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t Grayscale_GetRaw(void)
{
    return (uint8_t)((sensor_data.D8 << 7) |
                     (sensor_data.D7 << 6) |
                     (sensor_data.D6 << 5) |
                     (sensor_data.D5 << 4) |
                     (sensor_data.D4 << 3) |
                     (sensor_data.D3 << 2) |
                     (sensor_data.D2 << 1) |
                     (sensor_data.D1 << 0));
}

