#include "dl1b.h"

#include <stddef.h>

#include "bsp_dl1b.h"
#include "delay.h"

#define DL1B_TIMEOUT_COUNT                                      (1000U) /**< DL1B 等待状态变化的超时计数。 */
#define DL1B_I2C_SLAVE_DEVICE_ADDRESS                           (0x0001U) /**< DL1B 配置表写入起始寄存器地址。 */
#define DL1B_GPIO_TIO_HV_STATUS                                 (0x0031U) /**< DL1B 数据就绪/中断状态寄存器地址。 */
#define DL1B_SYSTEM_INTERRUPT_CLEAR                             (0x0086U) /**< DL1B 中断清除寄存器地址。 */
#define DL1B_RESULT_RANGE_STATUS                                (0x0089U) /**< DL1B 测距状态寄存器地址。 */
#define DL1B_RESULT_FINAL_RANGE_MM                              (0x0096U) /**< DL1B 距离结果寄存器地址。 */
#define DL1B_FIRMWARE_SYSTEM_STATUS                             (0x00E5U) /**< DL1B 固件启动状态寄存器地址。 */
#define DL1B_RANGE_STATUS_VALID                                 (0x89U) /**< DL1B 表示距离有效的状态值。 */
#define DL1B_MAX_VALID_DISTANCE_MM                              (4000U) /**< DL1B 本工程接受的最大有效距离，单位 mm。 */

extern const uint8_t dl1b_config_file[135]; /**< DL1B 官方初始化配置表。 */

static bool dl1bInitialized; /**< DL1B 是否已完成初始化。 */
static volatile bool dl1bNewData; /**< DL1B 是否有新距离数据。 */
static uint16_t dl1bDistanceMm = DL1B_INVALID_DISTANCE_MM; /**< DL1B 最近一次有效距离。 */
static DL1B_Status dl1bLastStatus = DL1B_STATUS_NOT_INITIALIZED; /**< DL1B 最近一次模块状态。 */
static uint8_t dl1bLastBusStatus; /**< DL1B 最近一次 I2C 状态。 */

/**
 * @brief 读取 DL1B 8 位寄存器。
 * @param reg 寄存器地址。
 * @param value 寄存器值。
 * @note DL1B 底层使用 16 位寄存器地址。
 * @retval true 读取成功，false 读取失败。
 */
static bool DL1B_Read8(uint16_t reg, uint8_t *value)
{
    dl1bLastBusStatus = BSP_DL1B_ReadRegister(reg, value, 1U);
    return dl1bLastBusStatus == BSP_DL1B_OK;
}

/**
 * @brief 写入 DL1B 8 位寄存器。
 * @param reg 寄存器地址。
 * @param value 寄存器值。
 * @retval true 写入成功，false 写入失败。
 */
static bool DL1B_Write8(uint16_t reg, uint8_t value)
{
    dl1bLastBusStatus = BSP_DL1B_WriteRegister(reg, &value, 1U);
    return dl1bLastBusStatus == BSP_DL1B_OK;
}

/**
 * @brief 判断 DL1B 测距状态是否有效。
 * @param rangeStatus 测距状态字节。
 * @retval true 有效，false 无效。
 */
bool DL1B_DecodeRangeStatus(uint8_t rangeStatus)
{
    return rangeStatus == DL1B_RANGE_STATUS_VALID;
}

bool DL1B_DecodeDistance(uint8_t highByte, uint8_t lowByte, uint16_t *distanceMm)
{
    const uint16_t rawDistance = (uint16_t)(((uint16_t)highByte << 8U) | lowByte);

    if ((distanceMm == NULL) || (rawDistance > DL1B_MAX_VALID_DISTANCE_MM)) {
        return false;
    }

    *distanceMm = rawDistance;
    return true;
}

/**
 * @brief 初始化 DL1B ToF 模块。
 * @param 无。
 * @retval true 初始化成功，false 初始化失败。
 */
bool DL1B_Init(void)
{
    uint8_t status = 0U;
    uint16_t timeout = 0U;

    BSP_DL1B_InitPins();

    delay_ms(50U);
    BSP_DL1B_SetXs(0U);
    delay_ms(10U);
    BSP_DL1B_SetXs(1U);
    delay_ms(50U);

    if (!DL1B_Read8(DL1B_FIRMWARE_SYSTEM_STATUS, &status)) {
        dl1bLastStatus = DL1B_STATUS_I2C_ERROR;
        return false;
    }
    if ((status & 0x01U) == 0U) {
        dl1bLastStatus = DL1B_STATUS_BOOT_ERROR;
        return false;
    }

    dl1bLastBusStatus = BSP_DL1B_WriteRegister(DL1B_I2C_SLAVE_DEVICE_ADDRESS,
                                               dl1b_config_file,
                                               sizeof(dl1b_config_file));
    if (dl1bLastBusStatus != BSP_DL1B_OK) {
        dl1bLastStatus = DL1B_STATUS_I2C_ERROR;
        return false;
    }

    while (timeout <= DL1B_TIMEOUT_COUNT) {
        if (!DL1B_Read8(DL1B_GPIO_TIO_HV_STATUS, &status)) {
            dl1bLastStatus = DL1B_STATUS_I2C_ERROR;
            return false;
        }
        if ((status & 0x01U) == 0U) {
            dl1bInitialized = true;
            dl1bLastStatus = DL1B_STATUS_OK;
            return true;
        }
        timeout++;
        delay_ms(1U);
    }

    dl1bLastStatus = DL1B_STATUS_TIMEOUT;
    return false;
}

/**
 * @brief 轮询 DL1B 测距状态并更新距离。
 * @param 无。
 * @retval true 本次获得有效距离，false 本次未获得有效距离。
 */
bool DL1B_Update(void)
{
    uint8_t value = 0U;
    uint8_t distanceBytes[2] = {0U, 0U};
    uint16_t decodedDistance = DL1B_INVALID_DISTANCE_MM;

    if (!dl1bInitialized) {
        dl1bLastStatus = DL1B_STATUS_NOT_INITIALIZED;
        return false;
    }

    if (!DL1B_Read8(DL1B_GPIO_TIO_HV_STATUS, &value)) {
        dl1bDistanceMm = DL1B_INVALID_DISTANCE_MM;
        dl1bLastStatus = DL1B_STATUS_I2C_ERROR;
        return false;
    }
    if (value == 0U) {
        dl1bLastStatus = DL1B_STATUS_NOT_READY;
        return false;
    }

    if (!DL1B_Write8(DL1B_SYSTEM_INTERRUPT_CLEAR, 0x01U)) {
        dl1bDistanceMm = DL1B_INVALID_DISTANCE_MM;
        dl1bLastStatus = DL1B_STATUS_I2C_ERROR;
        return false;
    }

    if (!DL1B_Read8(DL1B_RESULT_RANGE_STATUS, &value)) {
        dl1bDistanceMm = DL1B_INVALID_DISTANCE_MM;
        dl1bLastStatus = DL1B_STATUS_I2C_ERROR;
        return false;
    }
    if (!DL1B_DecodeRangeStatus(value)) {
        dl1bDistanceMm = DL1B_INVALID_DISTANCE_MM;
        dl1bLastStatus = DL1B_STATUS_RANGE_INVALID;
        return false;
    }

    dl1bLastBusStatus = BSP_DL1B_ReadRegister(DL1B_RESULT_FINAL_RANGE_MM,
                                              distanceBytes,
                                              sizeof(distanceBytes));
    if (dl1bLastBusStatus != BSP_DL1B_OK) {
        dl1bDistanceMm = DL1B_INVALID_DISTANCE_MM;
        dl1bLastStatus = DL1B_STATUS_I2C_ERROR;
        return false;
    }

    if (!DL1B_DecodeDistance(distanceBytes[0], distanceBytes[1], &decodedDistance)) {
        dl1bDistanceMm = DL1B_INVALID_DISTANCE_MM;
        dl1bLastStatus = DL1B_STATUS_RANGE_INVALID;
        return false;
    }

    dl1bDistanceMm = decodedDistance;
    dl1bNewData = true;
    dl1bLastStatus = DL1B_STATUS_OK;
    return true;
}

bool DL1B_HasNewData(void)
{
    return dl1bNewData;
}

void DL1B_ClearNewDataFlag(void)
{
    dl1bNewData = false;
}

uint16_t DL1B_GetDistanceMm(void)
{
    return dl1bDistanceMm;
}

DL1B_Status DL1B_GetLastStatus(void)
{
    return dl1bLastStatus;
}

uint8_t DL1B_GetLastBusStatus(void)
{
    return dl1bLastBusStatus;
}
