#include "bsp_adc.h"

#include "ti_msp_dl_config.h"

/**
 * @brief 读取按键 ADC 的原始转换值。
 * @param 无。
 * @note 启动转换后等待 ADC 空闲，再读取 ADCMEM0 结果。
 * @retval 返回 ADC_KEY_ADCMEM_0 的原始采样值。
 */
uint16_t BSP_ADC_KeyReadRaw(void)
{
    /* 启动单次转换并等待硬件完成。 */
    DL_ADC12_startConversion(ADC_KEY_INST);
    while (DL_ADC12_getStatus(ADC_KEY_INST) !=
           DL_ADC12_STATUS_CONVERSION_IDLE) {
    }
    DL_ADC12_stopConversion(ADC_KEY_INST);

    /* 读取结果后重新允许后续转换。 */
    const uint16_t result =
        DL_ADC12_getMemResult(ADC_KEY_INST, ADC_KEY_ADCMEM_0);
    DL_ADC12_enableConversions(ADC_KEY_INST);
    return result;
}

/**
 * @brief 将电池 ADC 原始值换算为电池端电压。
 * @param raw ADC 原始采样值。
 * @note 满量程电压来自 3.3V * (9.09k + 1k) / 1k = 33.297V。
 * @retval 电池端电压，单位 mV。
 */
uint16_t BSP_ADC_BatteryRawToMv(uint16_t raw)
{
    uint32_t voltageMv;

    /* 加半个除数实现四舍五入，最大中间值约 1.36e8，不会溢出 uint32_t。 */
    voltageMv = ((uint32_t)raw * BSP_ADC_BATTERY_FULL_SCALE_MV +
                 (BSP_ADC_MAX_RAW / 2U)) / BSP_ADC_MAX_RAW;
    return (uint16_t)voltageMv;
}

/**
 * @brief 读取电池电压 ADC 原始值。
 * @param 无。
 * @note 启动 ADC0 sequence 后读取 ADCMEM1；ADCMEM0 仍保留给五向按键。
 * @retval 电池 ADC 原始采样值。
 */
uint16_t BSP_ADC_BatteryReadRaw(void)
{
    DL_ADC12_startConversion(ADC_KEY_INST);
    while (DL_ADC12_getStatus(ADC_KEY_INST) !=
           DL_ADC12_STATUS_CONVERSION_IDLE) {
    }
    DL_ADC12_stopConversion(ADC_KEY_INST);

    const uint16_t result =
        DL_ADC12_getMemResult(ADC_KEY_INST, ADC_KEY_ADCMEM_1);
    DL_ADC12_enableConversions(ADC_KEY_INST);
    return result;
}

/**
 * @brief 读取电池端电压。
 * @param 无。
 * @note 供 App 层周期采样，菜单显示和低电压提醒共用本结果。
 * @retval 电池端电压，单位 mV。
 */
uint16_t BSP_ADC_BatteryReadMv(void)
{
    return BSP_ADC_BatteryRawToMv(BSP_ADC_BatteryReadRaw());
}
