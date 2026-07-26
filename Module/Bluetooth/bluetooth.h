#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H /**< __BLUETOOTH_H 头文件重复包含保护宏。 */
#include "ti_msp_dl_config.h"
#include "stdio.h"
/**
 * @brief 初始化 UART2 蓝牙串口。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 无。
 */
void uart2_init(void);
void uart2_send_char(char ch);
void uart2_send_string(const char *str);
void bluetooth_work(void);
/**
 * @brief 原子读取 UART2 最近一个有效蓝牙字节。
 * @param data 输出读取到的字节。
 * @note 会在极短临界区内同时读取数据并清除接收标志，避免主循环和 UART2 ISR 竞态。
 * @retval 1 表示读到新字节，0 表示没有新字节或参数无效。
 */
uint8_t Bluetooth_ReadByte(uint8_t *data);
/**
 * @brief 执行 Get  Rx Flag 功能。
 * @param 无。
 * @note 根据当前工程三层结构封装，供上层模块调用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t Get_RxFlag(void);
uint8_t Get_RxData(void);
#endif
