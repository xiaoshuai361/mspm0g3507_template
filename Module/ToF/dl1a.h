#ifndef MODULE_TOF_DL1A_H
#define MODULE_TOF_DL1A_H /**< MODULE_TOF_DL1A_H 头文件重复包含保护宏。 */

#include <stdbool.h>
#include <stdint.h>

#define DL1A_INVALID_DISTANCE_MM (8192U) /**< DL1A 无效距离占位值，单位 mm。 */
#define DL1A_MAX_VALID_DISTANCE_MM (4000U) /**< 当前模板接受的最大有效距离，单位 mm。 */

typedef enum {
    DL1A_STATUS_OK = 0,
    DL1A_STATUS_NOT_INITIALIZED,
    DL1A_STATUS_I2C_ERROR,
    DL1A_STATUS_BOOT_ERROR,
    DL1A_STATUS_TIMEOUT,
    DL1A_STATUS_RANGE_INVALID,
    DL1A_STATUS_NOT_READY,
    DL1A_STATUS_INIT_ERROR,
    DL1A_STATUS_CALIBRATION_ERROR
} DL1A_Status;

/**
 * @brief 初始化逐飞 DL1A ToF 模块。
 * @param 无。
 * @note 使用当前模板 PA1/PA0 软件 I2C 和 PB14 XS/XSHUT，INT 脚不启用。
 * @retval true 初始化成功，false 初始化失败。
 */
bool DL1A_Init(void);

/**
 * @brief 轮询 DL1A 测距数据。
 * @param 无。
 * @note 有新数据时会更新内部距离缓存并置位新数据标志。
 * @retval true 本次获得有效距离，false 本次未获得有效距离。
 */
bool DL1A_Update(void);

/**
 * @brief 判断 DL1A 是否有尚未读取的新距离数据。
 * @param 无。
 * @retval true 有新数据，false 无新数据。
 */
bool DL1A_HasNewData(void);

/**
 * @brief 清除 DL1A 新数据标志。
 * @param 无。
 * @retval 无。
 */
void DL1A_ClearNewDataFlag(void);

/**
 * @brief 获取 DL1A 最近一次有效距离。
 * @param 无。
 * @retval 距离值，单位 mm；无效时返回 DL1A_INVALID_DISTANCE_MM。
 */
uint16_t DL1A_GetDistanceMm(void);

/**
 * @brief 获取 DL1A 最近一次模块状态。
 * @param 无。
 * @retval DL1A_Status 状态码。
 */
DL1A_Status DL1A_GetLastStatus(void);

/**
 * @brief 获取 DL1A 最近一次 I2C 总线状态。
 * @param 无。
 * @retval 0 表示成功，其他值表示 BSP I2C 阶段错误。
 */
uint8_t DL1A_GetLastBusStatus(void);

/**
 * @brief 判断 DL1A 距离原始值是否在模板允许范围内。
 * @param distanceMm 距离原始值，单位 mm。
 * @retval true 有效，false 无效。
 */
bool DL1A_DecodeDistanceValue(uint16_t distanceMm);

/**
 * @brief 把两个距离寄存器字节解析成毫米值。
 * @param highByte 距离高字节。
 * @param lowByte 距离低字节。
 * @param distanceMm 输出距离，单位 mm。
 * @retval true 解析成功且距离有效，false 参数或距离无效。
 */
bool DL1A_DecodeDistance(uint8_t highByte, uint8_t lowByte, uint16_t *distanceMm);

#endif
