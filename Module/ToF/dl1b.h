#ifndef MODULE_TOF_DL1B_H
#define MODULE_TOF_DL1B_H /**< MODULE_TOF_DL1B_H 头文件重复包含保护宏。 */

#include <stdbool.h>
#include <stdint.h>

#define DL1B_INVALID_DISTANCE_MM (8192U) /**< DL1B_INVALID_DISTANCE_MM 底层驱动配置/状态宏。 */

typedef enum {
    DL1B_STATUS_OK = 0,
    DL1B_STATUS_NOT_INITIALIZED,
    DL1B_STATUS_I2C_ERROR,
    DL1B_STATUS_BOOT_ERROR,
    DL1B_STATUS_TIMEOUT,
    DL1B_STATUS_RANGE_INVALID,
    DL1B_STATUS_NOT_READY
} DL1B_Status;

/**
 * @brief 初始化 DL1B ToF 模块。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval true 表示满足条件，false 表示未满足。
 */
bool DL1B_Init(void);
bool DL1B_Update(void);
bool DL1B_HasNewData(void);
void DL1B_ClearNewDataFlag(void);
/**
 * @brief 执行 D L1 B  Get Distance Mm 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回 16 位数据。
 */
uint16_t DL1B_GetDistanceMm(void);
DL1B_Status DL1B_GetLastStatus(void);
uint8_t DL1B_GetLastBusStatus(void);

/**
 * @brief 执行 D L1 B  Decode Range Status 功能。
 * @param rangeStatus rangeStatus 参数。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval true 表示满足条件，false 表示未满足。
 */
bool DL1B_DecodeRangeStatus(uint8_t rangeStatus);
bool DL1B_DecodeDistance(uint8_t highByte, uint8_t lowByte, uint16_t *distanceMm);

#endif
