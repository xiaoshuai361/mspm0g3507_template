#include "dl1a.h"

#include <stddef.h>
#include <string.h>

#include "../../BSP/DL1A/bsp_dl1a.h"
#include "delay.h"

#define DL1A_TIMEOUT_COUNT                                  (0x00FFU) /**< DL1A 等待寄存器状态变化的超时计数。 */
#define DL1A_MIN_TIMING_BUDGET                              (20000UL) /**< DL1A 最小测距预算，单位 us。 */

#define DL1A_GET_START_OVERHEAD                             (1910UL)
#define DL1A_SET_START_OVERHEAD                             (1320UL)
#define DL1A_END_OVERHEAD                                   (960UL)
#define DL1A_TCC_OVERHEAD                                   (590UL)
#define DL1A_DSS_OVERHEAD                                   (690UL)
#define DL1A_MSRC_OVERHEAD                                  (660UL)
#define DL1A_PRERANGE_OVERHEAD                              (660UL)
#define DL1A_FINAL_RANGE_OVERHEAD                           (550UL)

#define DL1A_SYSRANGE_START                                 (0x00U)
#define DL1A_SYSTEM_SEQUENCE_CONFIG                         (0x01U)
#define DL1A_SYSTEM_INTERRUPT_GPIO_CONFIG                   (0x0AU)
#define DL1A_SYSTEM_INTERRUPT_CLEAR                         (0x0BU)
#define DL1A_RESULT_INTERRUPT_STATUS                        (0x13U)
#define DL1A_RESULT_RANGE_STATUS                            (0x14U)
#define DL1A_PRE_RANGE_CONFIG_VCSEL_PERIOD                  (0x50U)
#define DL1A_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI             (0x51U)
#define DL1A_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT    (0x44U)
#define DL1A_FINAL_RANGE_CONFIG_VCSEL_PERIOD                (0x70U)
#define DL1A_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI           (0x71U)
#define DL1A_GLOBAL_CONFIG_SPAD_ENABLES_REF_0               (0xB0U)
#define DL1A_GLOBAL_CONFIG_REF_EN_START_SELECT              (0xB6U)
#define DL1A_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD            (0x4EU)
#define DL1A_DYNAMIC_SPAD_REF_EN_START_OFFSET               (0x4FU)
#define DL1A_MSRC_CONFIG_TIMEOUT_MACROP                     (0x46U)
#define DL1A_MSRC_CONFIG                                    (0x60U)
#define DL1A_IDENTIFICATION_MODEL_ID                        (0xC0U)
#define DL1A_GPIO_HV_MUX_ACTIVE_HIGH                        (0x84U)
#define DL1A_IO_VOLTAGE_CONFIG                              (0x89U)

#define DL1A_MODEL_ID_VALUE                                 (0xEEU) /**< VL53L0X/DL1A 期望芯片 ID。 */
#define DL1A_DEFAULT_RATE_LIMIT_Q7                          (32U)   /**< 0.25 MCPS * 2^7。 */

#define DL1A_DECODE_VCSEL_PERIOD(regVal)                    (((regVal) + 1U) << 1U)
#define DL1A_CALC_MACRO_PERIOD(vcselPeriodPclks)            ((((uint32_t)2304U * (vcselPeriodPclks) * 1655U) + 500U) / 1000U)

typedef enum {
    DL1A_VCSEL_PERIOD_PRE_RANGE,
    DL1A_VCSEL_PERIOD_FINAL_RANGE
} DL1A_VcselPeriodType;

typedef struct {
    uint8_t tcc;
    uint8_t msrc;
    uint8_t dss;
    uint8_t preRange;
    uint8_t finalRange;
} DL1A_SequenceEnables;

typedef struct {
    uint16_t preRangeVcselPeriodPclks;
    uint16_t finalRangeVcselPeriodPclks;
    uint16_t msrcDssTccMclks;
    uint16_t preRangeMclks;
    uint16_t finalRangeMclks;
    uint32_t msrcDssTccUs;
    uint32_t preRangeUs;
    uint32_t finalRangeUs;
} DL1A_SequenceTimeouts;

static bool dl1aInitialized; /**< DL1A 是否已完成初始化。 */
static volatile bool dl1aNewData; /**< DL1A 是否有新距离数据。 */
static uint16_t dl1aDistanceMm = DL1A_INVALID_DISTANCE_MM; /**< DL1A 最近一次有效距离。 */
static DL1A_Status dl1aLastStatus = DL1A_STATUS_NOT_INITIALIZED; /**< DL1A 最近一次模块状态。 */
static uint8_t dl1aLastBusStatus; /**< DL1A 最近一次 I2C 状态。 */

/**
 * @brief 向 DL1A 写入一个 8 位寄存器。
 * @param reg 8 位寄存器地址。
 * @param value 待写入值。
 * @retval true 写成功，false 写失败。
 */
static bool DL1A_Write8(uint8_t reg, uint8_t value)
{
    dl1aLastBusStatus = BSP_DL1A_WriteRegister(reg, &value, 1U);
    return dl1aLastBusStatus == BSP_DL1A_OK;
}

/**
 * @brief 从 DL1A 读取连续 8 位寄存器。
 * @param reg 起始寄存器地址。
 * @param data 读取缓存。
 * @param len 读取长度。
 * @retval true 读成功，false 读失败。
 */
static bool DL1A_ReadRegisters(uint8_t reg, uint8_t *data, uint8_t len)
{
    dl1aLastBusStatus = BSP_DL1A_ReadRegister(reg, data, len);
    return dl1aLastBusStatus == BSP_DL1A_OK;
}

/**
 * @brief 从 DL1A 读取一个 8 位寄存器。
 * @param reg 8 位寄存器地址。
 * @param value 输出寄存器值。
 * @retval true 读成功，false 读失败。
 */
static bool DL1A_Read8(uint8_t reg, uint8_t *value)
{
    return DL1A_ReadRegisters(reg, value, 1U);
}

/**
 * @brief 按逐飞写数组格式写 DL1A 连续寄存器。
 * @param data data[0] 为起始寄存器，后续字节为写入数据。
 * @param len 数组长度。
 * @retval true 写成功，false 写失败。
 */
static bool DL1A_WriteArray(const uint8_t *data, uint8_t len)
{
    if ((data == NULL) || (len < 2U)) {
        dl1aLastBusStatus = BSP_DL1A_ERR_I2C_WRITE;
        return false;
    }

    dl1aLastBusStatus = BSP_DL1A_WriteRegister(data[0], &data[1], (uint8_t)(len - 1U));
    return dl1aLastBusStatus == BSP_DL1A_OK;
}

/**
 * @brief 获取 DL1A SPAD 配置信息。
 * @param index SPAD 数量输出。
 * @param typeIsAperture SPAD 类型输出。
 * @retval true 获取成功，false 获取失败。
 */
static bool DL1A_GetSpadInfo(uint8_t *index, uint8_t *typeIsAperture)
{
    uint8_t tmp = 0U;
    uint16_t loopCount = 0U;

    if ((index == NULL) || (typeIsAperture == NULL)) {
        return false;
    }

    DL1A_Write8(0x80U, 0x01U);
    DL1A_Write8(0xFFU, 0x01U);
    DL1A_Write8(0x00U, 0x00U);
    DL1A_Write8(0xFFU, 0x06U);
    if (!DL1A_Read8(0x83U, &tmp)) {
        return false;
    }
    DL1A_Write8(0x83U, (uint8_t)(tmp | 0x04U));
    DL1A_Write8(0xFFU, 0x07U);
    DL1A_Write8(0x81U, 0x01U);
    DL1A_Write8(0x80U, 0x01U);
    DL1A_Write8(0x94U, 0x6BU);
    DL1A_Write8(0x83U, 0x00U);

    tmp = 0U;
    while ((tmp == 0x00U) || (tmp == 0xFFU)) {
        delay_ms(1U);
        if (!DL1A_Read8(0x83U, &tmp)) {
            return false;
        }
        if (loopCount++ > DL1A_TIMEOUT_COUNT) {
            dl1aLastStatus = DL1A_STATUS_TIMEOUT;
            return false;
        }
    }

    DL1A_Write8(0x83U, 0x01U);
    if (!DL1A_Read8(0x92U, &tmp)) {
        return false;
    }
    *index = (uint8_t)(tmp & 0x7FU);
    *typeIsAperture = (uint8_t)((tmp >> 7U) & 0x01U);

    DL1A_Write8(0x81U, 0x00U);
    DL1A_Write8(0xFFU, 0x06U);
    if (!DL1A_Read8(0x83U, &tmp)) {
        return false;
    }
    DL1A_Write8(0x83U, tmp);
    DL1A_Write8(0xFFU, 0x01U);
    DL1A_Write8(0x00U, 0x01U);
    DL1A_Write8(0xFFU, 0x00U);
    DL1A_Write8(0x80U, 0x00U);
    return dl1aLastBusStatus == BSP_DL1A_OK;
}

/**
 * @brief 把超时计数从 MCLK 转换为微秒。
 * @param timeoutMclks 超时计数。
 * @param vcselPeriodPclks VCSEL 周期。
 * @retval 超时时间，单位 us。
 */
static uint32_t DL1A_TimeoutMclksToUs(uint16_t timeoutMclks, uint8_t vcselPeriodPclks)
{
    const uint32_t macroPeriodNs = DL1A_CALC_MACRO_PERIOD(vcselPeriodPclks);
    return (((uint32_t)timeoutMclks * macroPeriodNs) + (macroPeriodNs / 2U)) / 1000U;
}

/**
 * @brief 把超时时间从微秒转换为 MCLK。
 * @param timeoutUs 超时时间，单位 us。
 * @param vcselPeriodPclks VCSEL 周期。
 * @retval 超时计数。
 */
static uint32_t DL1A_TimeoutUsToMclks(uint32_t timeoutUs, uint8_t vcselPeriodPclks)
{
    const uint32_t macroPeriodNs = DL1A_CALC_MACRO_PERIOD(vcselPeriodPclks);
    return (((timeoutUs * 1000U) + (macroPeriodNs / 2U)) / macroPeriodNs);
}

/**
 * @brief 解码 DL1A 超时寄存器值。
 * @param regValue 寄存器值。
 * @retval MCLK 超时计数。
 */
static uint16_t DL1A_DecodeTimeout(uint16_t regValue)
{
    return (uint16_t)(((regValue & 0x00FFU) << ((regValue & 0xFF00U) >> 8U)) + 1U);
}

/**
 * @brief 编码 DL1A 超时寄存器值。
 * @param timeoutMclks MCLK 超时计数。
 * @retval 编码后的寄存器值。
 */
static uint16_t DL1A_EncodeTimeout(uint16_t timeoutMclks)
{
    uint32_t lsByte;
    uint16_t msByte = 0U;

    if (timeoutMclks == 0U) {
        return 0U;
    }

    lsByte = (uint32_t)timeoutMclks - 1U;
    while ((lsByte & 0xFFFFFF00UL) != 0U) {
        lsByte >>= 1U;
        msByte++;
    }

    return (uint16_t)((msByte << 8U) | ((uint16_t)lsByte & 0x00FFU));
}

/**
 * @brief 获取 DL1A 测距序列开关状态。
 * @param enables 输出测距序列开关。
 * @retval 无。
 */
static void DL1A_GetSequenceStepEnables(DL1A_SequenceEnables *enables)
{
    uint8_t sequenceConfig = 0U;

    (void)DL1A_Read8(DL1A_SYSTEM_SEQUENCE_CONFIG, &sequenceConfig);
    enables->tcc = (uint8_t)((sequenceConfig >> 4U) & 0x01U);
    enables->dss = (uint8_t)((sequenceConfig >> 3U) & 0x01U);
    enables->msrc = (uint8_t)((sequenceConfig >> 2U) & 0x01U);
    enables->preRange = (uint8_t)((sequenceConfig >> 6U) & 0x01U);
    enables->finalRange = (uint8_t)((sequenceConfig >> 7U) & 0x01U);
}

/**
 * @brief 获取 DL1A VCSEL 脉冲周期。
 * @param type 预量程或最终量程。
 * @retval VCSEL 脉冲周期。
 */
static uint8_t DL1A_GetVcselPulsePeriod(DL1A_VcselPeriodType type)
{
    uint8_t value = 0U;

    if (type == DL1A_VCSEL_PERIOD_PRE_RANGE) {
        (void)DL1A_Read8(DL1A_PRE_RANGE_CONFIG_VCSEL_PERIOD, &value);
        return (uint8_t)DL1A_DECODE_VCSEL_PERIOD(value);
    }

    if (type == DL1A_VCSEL_PERIOD_FINAL_RANGE) {
        (void)DL1A_Read8(DL1A_FINAL_RANGE_CONFIG_VCSEL_PERIOD, &value);
        return (uint8_t)DL1A_DECODE_VCSEL_PERIOD(value);
    }

    return 255U;
}

/**
 * @brief 获取 DL1A 测距序列超时参数。
 * @param enables 序列开关状态。
 * @param timeouts 输出序列超时参数。
 * @retval 无。
 */
static void DL1A_GetSequenceStepTimeouts(const DL1A_SequenceEnables *enables, DL1A_SequenceTimeouts *timeouts)
{
    uint8_t buffer[2] = {0U, 0U};
    uint16_t reg16Value;

    timeouts->preRangeVcselPeriodPclks = DL1A_GetVcselPulsePeriod(DL1A_VCSEL_PERIOD_PRE_RANGE);

    (void)DL1A_ReadRegisters(DL1A_MSRC_CONFIG_TIMEOUT_MACROP, buffer, 1U);
    timeouts->msrcDssTccMclks = (uint16_t)buffer[0] + 1U;
    timeouts->msrcDssTccUs = DL1A_TimeoutMclksToUs(timeouts->msrcDssTccMclks,
                                                   (uint8_t)timeouts->preRangeVcselPeriodPclks);

    (void)DL1A_ReadRegisters(DL1A_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI, buffer, 2U);
    reg16Value = (uint16_t)(((uint16_t)buffer[0] << 8U) | buffer[1]);
    timeouts->preRangeMclks = DL1A_DecodeTimeout(reg16Value);
    timeouts->preRangeUs = DL1A_TimeoutMclksToUs(timeouts->preRangeMclks,
                                                 (uint8_t)timeouts->preRangeVcselPeriodPclks);

    timeouts->finalRangeVcselPeriodPclks = DL1A_GetVcselPulsePeriod(DL1A_VCSEL_PERIOD_FINAL_RANGE);

    (void)DL1A_ReadRegisters(DL1A_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI, buffer, 2U);
    reg16Value = (uint16_t)(((uint16_t)buffer[0] << 8U) | buffer[1]);
    timeouts->finalRangeMclks = DL1A_DecodeTimeout(reg16Value);
    if (enables->preRange != 0U) {
        timeouts->finalRangeMclks = (uint16_t)(timeouts->finalRangeMclks - timeouts->preRangeMclks);
    }
    timeouts->finalRangeUs = DL1A_TimeoutMclksToUs(timeouts->finalRangeMclks,
                                                   (uint8_t)timeouts->finalRangeVcselPeriodPclks);
}

/**
 * @brief 执行 DL1A 单次参考校准。
 * @param vhvInitByte 校准参数。
 * @retval true 校准成功，false 校准失败。
 */
static bool DL1A_PerformSingleRefCalibration(uint8_t vhvInitByte)
{
    uint8_t value = 0U;
    uint16_t loopCount = 0U;

    (void)DL1A_Write8(DL1A_SYSRANGE_START, (uint8_t)(0x01U | vhvInitByte));
    (void)DL1A_Read8(DL1A_MSRC_CONFIG_TIMEOUT_MACROP, &value);
    while ((value & 0x07U) == 0U) {
        delay_ms(1U);
        (void)DL1A_Read8(DL1A_MSRC_CONFIG_TIMEOUT_MACROP, &value);
        if (loopCount++ > DL1A_TIMEOUT_COUNT) {
            dl1aLastStatus = DL1A_STATUS_TIMEOUT;
            return false;
        }
    }

    (void)DL1A_Write8(DL1A_SYSTEM_INTERRUPT_CLEAR, 0x01U);
    (void)DL1A_Write8(DL1A_SYSRANGE_START, 0x00U);
    return dl1aLastBusStatus == BSP_DL1A_OK;
}

/**
 * @brief 设置 DL1A 测距时间预算。
 * @param budgetUs 时间预算，单位 us。
 * @retval true 设置成功，false 设置失败。
 */
static bool DL1A_SetMeasurementTimingBudget(uint32_t budgetUs)
{
    uint8_t buffer[3];
    uint16_t data;
    uint32_t usedBudgetUs;
    uint32_t finalRangeTimeoutUs;
    uint16_t finalRangeTimeoutMclks;
    DL1A_SequenceEnables enables;
    DL1A_SequenceTimeouts timeouts;

    if (budgetUs < DL1A_MIN_TIMING_BUDGET) {
        return false;
    }

    usedBudgetUs = DL1A_SET_START_OVERHEAD + DL1A_END_OVERHEAD;
    DL1A_GetSequenceStepEnables(&enables);
    DL1A_GetSequenceStepTimeouts(&enables, &timeouts);

    if (enables.tcc != 0U) {
        usedBudgetUs += timeouts.msrcDssTccUs + DL1A_TCC_OVERHEAD;
    }
    if (enables.dss != 0U) {
        usedBudgetUs += 2U * (timeouts.msrcDssTccUs + DL1A_DSS_OVERHEAD);
    } else if (enables.msrc != 0U) {
        usedBudgetUs += timeouts.msrcDssTccUs + DL1A_MSRC_OVERHEAD;
    }
    if (enables.preRange != 0U) {
        usedBudgetUs += timeouts.preRangeUs + DL1A_PRERANGE_OVERHEAD;
    }

    if (enables.finalRange == 0U) {
        return true;
    }

    usedBudgetUs += DL1A_FINAL_RANGE_OVERHEAD;
    if (usedBudgetUs > budgetUs) {
        return false;
    }

    finalRangeTimeoutUs = budgetUs - usedBudgetUs;
    finalRangeTimeoutMclks = (uint16_t)DL1A_TimeoutUsToMclks(finalRangeTimeoutUs,
                                                             (uint8_t)timeouts.finalRangeVcselPeriodPclks);
    if (enables.preRange != 0U) {
        finalRangeTimeoutMclks = (uint16_t)(finalRangeTimeoutMclks + timeouts.preRangeMclks);
    }

    data = DL1A_EncodeTimeout(finalRangeTimeoutMclks);
    buffer[0] = DL1A_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI;
    buffer[1] = (uint8_t)((data >> 8U) & 0xFFU);
    buffer[2] = (uint8_t)(data & 0xFFU);
    return DL1A_WriteArray(buffer, sizeof(buffer));
}

/**
 * @brief 获取 DL1A 当前测距时间预算。
 * @param 无。
 * @retval 时间预算，单位 us。
 */
static uint32_t DL1A_GetMeasurementTimingBudget(void)
{
    uint32_t budgetUs = DL1A_GET_START_OVERHEAD + DL1A_END_OVERHEAD;
    DL1A_SequenceEnables enables;
    DL1A_SequenceTimeouts timeouts;

    DL1A_GetSequenceStepEnables(&enables);
    DL1A_GetSequenceStepTimeouts(&enables, &timeouts);

    if (enables.tcc != 0U) {
        budgetUs += timeouts.msrcDssTccUs + DL1A_TCC_OVERHEAD;
    }
    if (enables.dss != 0U) {
        budgetUs += 2U * (timeouts.msrcDssTccUs + DL1A_DSS_OVERHEAD);
    } else if (enables.msrc != 0U) {
        budgetUs += timeouts.msrcDssTccUs + DL1A_MSRC_OVERHEAD;
    }
    if (enables.preRange != 0U) {
        budgetUs += timeouts.preRangeUs + DL1A_PRERANGE_OVERHEAD;
    }
    if (enables.finalRange != 0U) {
        budgetUs += timeouts.finalRangeUs + DL1A_FINAL_RANGE_OVERHEAD;
    }

    return budgetUs;
}

/**
 * @brief 设置 DL1A 返回信号速率限制。
 * @param limitQ7 Q9.7 格式 MCPS，0.25 MCPS 对应 32。
 * @retval true 设置成功，false 设置失败。
 */
static bool DL1A_SetSignalRateLimit(uint16_t limitQ7)
{
    uint8_t buffer[3];

    buffer[0] = DL1A_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT;
    buffer[1] = (uint8_t)((limitQ7 >> 8U) & 0xFFU);
    buffer[2] = (uint8_t)(limitQ7 & 0xFFU);
    return DL1A_WriteArray(buffer, sizeof(buffer));
}

/**
 * @brief 写入 DL1A 官方默认调校寄存器。
 * @param 无。
 * @retval true 写入成功，false 写入失败。
 */
static bool DL1A_WriteDefaultTuning(void)
{
    static const uint8_t pairs[][2] = {
        {0xFFU, 0x01U}, {0x00U, 0x00U}, {0xFFU, 0x00U}, {0x09U, 0x00U},
        {0x10U, 0x00U}, {0x11U, 0x00U}, {0x24U, 0x01U}, {0x25U, 0xFFU},
        {0x75U, 0x00U}, {0xFFU, 0x01U}, {0x4EU, 0x2CU}, {0x48U, 0x00U},
        {0x30U, 0x20U}, {0xFFU, 0x00U}, {0x30U, 0x09U}, {0x54U, 0x00U},
        {0x31U, 0x04U}, {0x32U, 0x03U}, {0x40U, 0x83U}, {0x46U, 0x25U},
        {0x60U, 0x00U}, {0x27U, 0x00U}, {0x50U, 0x06U}, {0x51U, 0x00U},
        {0x52U, 0x96U}, {0x56U, 0x08U}, {0x57U, 0x30U}, {0x61U, 0x00U},
        {0x62U, 0x00U}, {0x64U, 0x00U}, {0x65U, 0x00U}, {0x66U, 0xA0U},
        {0xFFU, 0x01U}, {0x22U, 0x32U}, {0x47U, 0x14U}, {0x49U, 0xFFU},
        {0x4AU, 0x00U}, {0xFFU, 0x00U}, {0x7AU, 0x0AU}, {0x7BU, 0x00U},
        {0x78U, 0x21U}, {0xFFU, 0x01U}, {0x23U, 0x34U}, {0x42U, 0x00U},
        {0x44U, 0xFFU}, {0x45U, 0x26U}, {0x46U, 0x05U}, {0x40U, 0x40U},
        {0x0EU, 0x06U}, {0x20U, 0x1AU}, {0x43U, 0x40U}, {0xFFU, 0x00U},
        {0x34U, 0x03U}, {0x35U, 0x44U}, {0xFFU, 0x01U}, {0x31U, 0x04U},
        {0x4BU, 0x09U}, {0x4CU, 0x05U}, {0x4DU, 0x04U}, {0xFFU, 0x00U},
        {0x44U, 0x00U}, {0x45U, 0x20U}, {0x47U, 0x08U}, {0x48U, 0x28U},
        {0x67U, 0x00U}, {0x70U, 0x04U}, {0x71U, 0x01U}, {0x72U, 0xFEU},
        {0x76U, 0x00U}, {0x77U, 0x00U}, {0xFFU, 0x01U}, {0x0DU, 0x01U},
        {0xFFU, 0x00U}, {0x80U, 0x01U}, {0x01U, 0xF8U}, {0xFFU, 0x01U},
        {0x8EU, 0x01U}, {0x00U, 0x01U}, {0xFFU, 0x00U}, {0x80U, 0x00U}
    };
    uint32_t i;

    for (i = 0U; i < (sizeof(pairs) / sizeof(pairs[0])); i++) {
        if (!DL1A_Write8(pairs[i][0], pairs[i][1])) {
            return false;
        }
    }

    return true;
}

/**
 * @brief 初始化逐飞 DL1A ToF 模块。
 * @param 无。
 * @retval true 初始化成功，false 初始化失败。
 */
bool DL1A_Init(void)
{
    uint32_t timingBudgetUs;
    uint8_t stopVariable = 0U;
    uint8_t value = 0U;
    uint8_t refSpadMap[6] = {0U};
    uint8_t dataBuffer[7] = {0U};
    uint8_t i;
    uint8_t spadCount;
    uint8_t firstSpadToEnable;
    uint8_t spadsEnabled;

    dl1aInitialized = false;
    dl1aNewData = false;
    dl1aDistanceMm = DL1A_INVALID_DISTANCE_MM;
    dl1aLastStatus = DL1A_STATUS_NOT_INITIALIZED;
    dl1aLastBusStatus = BSP_DL1A_OK;

    BSP_DL1A_InitPins();
    delay_ms(100U);
    BSP_DL1A_SetXs(0U);
    delay_ms(50U);
    BSP_DL1A_SetXs(1U);
    delay_ms(100U);

    if (!DL1A_Read8(DL1A_IDENTIFICATION_MODEL_ID, &value)) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }
    if (value != DL1A_MODEL_ID_VALUE) {
        dl1aLastStatus = DL1A_STATUS_BOOT_ERROR;
        return false;
    }

    if (!DL1A_Read8(DL1A_IO_VOLTAGE_CONFIG, &value) ||
        !DL1A_Write8(DL1A_IO_VOLTAGE_CONFIG, (uint8_t)(value | 0x01U)) ||
        !DL1A_Write8(0x88U, 0x00U)) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    DL1A_Write8(0x80U, 0x01U);
    DL1A_Write8(0xFFU, 0x01U);
    DL1A_Write8(0x00U, 0x00U);
    if (!DL1A_Read8(0x91U, &stopVariable)) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }
    DL1A_Write8(0x00U, 0x01U);
    DL1A_Write8(0xFFU, 0x00U);
    DL1A_Write8(0x80U, 0x00U);

    if (!DL1A_Read8(DL1A_MSRC_CONFIG, &value) ||
        !DL1A_Write8(DL1A_MSRC_CONFIG, (uint8_t)(value | 0x12U)) ||
        !DL1A_SetSignalRateLimit(DL1A_DEFAULT_RATE_LIMIT_Q7) ||
        !DL1A_Write8(DL1A_SYSTEM_SEQUENCE_CONFIG, 0xFFU)) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    if (!DL1A_GetSpadInfo(&dataBuffer[0], &dataBuffer[1])) {
        if (dl1aLastStatus != DL1A_STATUS_TIMEOUT) {
            dl1aLastStatus = DL1A_STATUS_INIT_ERROR;
        }
        return false;
    }

    if (!DL1A_ReadRegisters(DL1A_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, refSpadMap, sizeof(refSpadMap))) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    DL1A_Write8(0xFFU, 0x01U);
    DL1A_Write8(DL1A_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00U);
    DL1A_Write8(DL1A_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2CU);
    DL1A_Write8(0xFFU, 0x00U);
    DL1A_Write8(DL1A_GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4U);

    spadCount = dataBuffer[0];
    firstSpadToEnable = (dataBuffer[1] != 0U) ? 12U : 0U;
    spadsEnabled = 0U;
    for (i = 0U; i < 48U; i++) {
        if ((i < firstSpadToEnable) || (spadsEnabled == spadCount)) {
            refSpadMap[i / 8U] &= (uint8_t)~(1U << (i % 8U));
        } else if (((refSpadMap[i / 8U] >> (i % 8U)) & 0x01U) != 0U) {
            spadsEnabled++;
        }
    }

    dataBuffer[0] = DL1A_GLOBAL_CONFIG_SPAD_ENABLES_REF_0;
    for (i = 1U; i < 7U; i++) {
        dataBuffer[i] = refSpadMap[i - 1U];
    }
    if (!DL1A_WriteArray(dataBuffer, sizeof(dataBuffer)) || !DL1A_WriteDefaultTuning()) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    DL1A_Write8(DL1A_SYSTEM_INTERRUPT_GPIO_CONFIG, 0x04U);
    if (!DL1A_Read8(DL1A_GPIO_HV_MUX_ACTIVE_HIGH, &value) ||
        !DL1A_Write8(DL1A_GPIO_HV_MUX_ACTIVE_HIGH, (uint8_t)(value & (uint8_t)~0x10U)) ||
        !DL1A_Write8(DL1A_SYSTEM_INTERRUPT_CLEAR, 0x01U)) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    timingBudgetUs = DL1A_GetMeasurementTimingBudget();
    if (!DL1A_Write8(DL1A_SYSTEM_SEQUENCE_CONFIG, 0xE8U) ||
        !DL1A_SetMeasurementTimingBudget(timingBudgetUs)) {
        dl1aLastStatus = DL1A_STATUS_INIT_ERROR;
        return false;
    }

    if (!DL1A_Write8(DL1A_SYSTEM_SEQUENCE_CONFIG, 0x01U) ||
        !DL1A_PerformSingleRefCalibration(0x40U) ||
        !DL1A_Write8(DL1A_SYSTEM_SEQUENCE_CONFIG, 0x02U) ||
        !DL1A_PerformSingleRefCalibration(0x00U) ||
        !DL1A_Write8(DL1A_SYSTEM_SEQUENCE_CONFIG, 0xE8U)) {
        if (dl1aLastStatus != DL1A_STATUS_TIMEOUT) {
            dl1aLastStatus = DL1A_STATUS_CALIBRATION_ERROR;
        }
        return false;
    }

    delay_ms(100U);
    DL1A_Write8(0x80U, 0x01U);
    DL1A_Write8(0xFFU, 0x01U);
    DL1A_Write8(0x00U, 0x00U);
    DL1A_Write8(0x91U, stopVariable);
    DL1A_Write8(0x00U, 0x01U);
    DL1A_Write8(0xFFU, 0x00U);
    DL1A_Write8(0x80U, 0x00U);
    if (!DL1A_Write8(DL1A_SYSRANGE_START, 0x02U)) {
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    dl1aInitialized = true;
    dl1aLastStatus = DL1A_STATUS_OK;
    return true;
}

/**
 * @brief 轮询 DL1A 测距数据。
 * @param 无。
 * @retval true 本次获得有效距离，false 本次未获得有效距离。
 */
bool DL1A_Update(void)
{
    uint8_t status = 0U;
    uint8_t distanceBytes[2] = {0U, 0U};
    uint16_t distance = DL1A_INVALID_DISTANCE_MM;

    if (!dl1aInitialized) {
        dl1aLastStatus = DL1A_STATUS_NOT_INITIALIZED;
        return false;
    }

    if (!DL1A_Read8(DL1A_RESULT_INTERRUPT_STATUS, &status)) {
        dl1aDistanceMm = DL1A_INVALID_DISTANCE_MM;
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    if ((status & 0x07U) == 0U) {
        dl1aLastStatus = DL1A_STATUS_NOT_READY;
        return false;
    }

    if ((status & 0x10U) != 0U) {
        (void)DL1A_ReadRegisters((uint8_t)(DL1A_RESULT_RANGE_STATUS + 10U), distanceBytes, sizeof(distanceBytes));
        (void)DL1A_Write8(DL1A_SYSTEM_INTERRUPT_CLEAR, 0x01U);
        dl1aDistanceMm = DL1A_INVALID_DISTANCE_MM;
        dl1aLastStatus = DL1A_STATUS_RANGE_INVALID;
        return false;
    }

    if (!DL1A_ReadRegisters((uint8_t)(DL1A_RESULT_RANGE_STATUS + 10U), distanceBytes, sizeof(distanceBytes)) ||
        !DL1A_Write8(DL1A_SYSTEM_INTERRUPT_CLEAR, 0x01U)) {
        dl1aDistanceMm = DL1A_INVALID_DISTANCE_MM;
        dl1aLastStatus = DL1A_STATUS_I2C_ERROR;
        return false;
    }

    if (!DL1A_DecodeDistance(distanceBytes[0], distanceBytes[1], &distance)) {
        dl1aDistanceMm = DL1A_INVALID_DISTANCE_MM;
        dl1aLastStatus = DL1A_STATUS_RANGE_INVALID;
        return false;
    }

    dl1aDistanceMm = distance;
    dl1aNewData = true;
    dl1aLastStatus = DL1A_STATUS_OK;
    return true;
}

bool DL1A_HasNewData(void)
{
    return dl1aNewData;
}

void DL1A_ClearNewDataFlag(void)
{
    dl1aNewData = false;
}

uint16_t DL1A_GetDistanceMm(void)
{
    return dl1aDistanceMm;
}

DL1A_Status DL1A_GetLastStatus(void)
{
    return dl1aLastStatus;
}

uint8_t DL1A_GetLastBusStatus(void)
{
    return dl1aLastBusStatus;
}

bool DL1A_DecodeDistanceValue(uint16_t distanceMm)
{
    return (distanceMm > 0U) && (distanceMm <= DL1A_MAX_VALID_DISTANCE_MM);
}

bool DL1A_DecodeDistance(uint8_t highByte, uint8_t lowByte, uint16_t *distanceMm)
{
    const uint16_t rawDistance = (uint16_t)(((uint16_t)highByte << 8U) | lowByte);

    if ((distanceMm == NULL) || !DL1A_DecodeDistanceValue(rawDistance)) {
        return false;
    }

    *distanceMm = rawDistance;
    return true;
}
