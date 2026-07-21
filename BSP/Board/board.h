
#ifndef __BOARD_H__
#define __BOARD_H__ /**< __BOARD_H__ 头文件重复包含保护宏。 */

#include "ti_msp_dl_config.h"

#ifndef u8
#define u8 uint8_t /**< u8 基础整数类型别名。 */
#endif

#ifndef u16
#define u16 uint16_t /**< u16 基础整数类型别名。 */
#endif

#ifndef u32
#define u32 uint32_t /**< u32 基础整数类型别名。 */
#endif

#ifndef u64
#define u64 uint64_t /**< u64 基础整数类型别名。 */
#endif



/**
 * @brief 计算浮点数绝对值。
 * @param num 输入的浮点数。
 * @note 简单工具函数，不处理 NaN 等特殊浮点场景。
 * @retval 返回 num 的非负值。
 */
float apf(float num);
/**
 * @brief 计算整数绝对值。
 * @param num 输入的整数。
 * @note 当 num 为 int 最小值时，取负可能溢出，调用方需避免该边界值。
 * @retval 返回 num 的非负值。
 */
int apc(int num);

#endif

