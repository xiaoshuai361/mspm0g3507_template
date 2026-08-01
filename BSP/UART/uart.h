#ifndef __UART_H
#define __UART_H /**< __UART_H 头文件重复包含保护宏。 */
#include "ti_msp_dl_config.h"
#include "stdio.h"
#include "string.h"

/**
 * @brief 初始化 UART0 调试串口。
 * @param 无。
 * @note 使能 UART0 中断并发送就绪提示。
 * @retval 无。
 */
void uart0_init(void);
/**
 * @brief 通过 UART0 发送单个字符。
 * @param ch 待发送字符。
 * @note 发送前会等待 UART0 忙状态结束。
 * @retval 无。
 */
void uart0_send_char(char ch);
/**
 * @brief 通过 UART0 发送 C 字符串。
 * @param str 待发送字符串指针。
 * @note str 为 NULL 时直接返回。
 * @retval 无。
 */
void uart0_send_string(const char *str);

/**
 * @brief 非阻塞写入 UART0 发送队列。
 * @param data 待发送字节数组。
 * @param len 字节数。
 * @note 数据会整块入队；队列空间不足时整块丢弃，不等待硬件发送完成。
 * @retval 1 表示成功入队，0 表示参数无效或队列空间不足。
 */
uint8_t uart0_write_nonblocking(const uint8_t *data, uint16_t len);

/**
 * @brief 仅供UART协议桥接使用的非阻塞发送入口。
 * @param data 待透传的原始字节。
 * @param len 字节数。
 * @note 本地日志关闭时此入口仍可发送，确保UART0只输出桥接原始数据。
 * @retval 1表示成功入队，0表示参数无效或发送队列空间不足。
 */
/* ======== VOFA+ JustFloat 协议 ========
 * 使用方式：在VOFA+中选择 JustFloat 协议
 * 每帧格式：[float0][float1]...[floatN-1][0x00 0x00 0x80 0x7F]
 */
/**
 * @brief 按 VOFA+ JustFloat 协议发送浮点数组。
 * @param data 浮点数据缓冲区。
 * @param len 浮点数据个数。
 * @note 帧尾固定发送 00 00 80 7F。
 * @retval 1 表示成功入队，0 表示参数无效或发送队列已满。
 */
uint8_t vofa_justfloat_send(const float *data, uint8_t len);

/* ======== VOFA+ 命令接收接口 ========
 * 通过串口终端发送 ASCII 命令调整PID参数：
 *   KP,11.5  设置两路速度环 Kp
 *   KI,0.3   设置两路速度环 Ki
 *   KD,0.1   设置两路速度环 Kd
 *   ki,1.0   设置灰度方向环 Kp（大小写敏感）
 *   S,60     设置 Task 1 定速值和 Task 2 巡航速度
 */
/**
 * @brief 获取 VOFA+ 命令接收完成标志。
 * @param 无。
 * @note 读取到 1 时会自动清除内部 ready 标志。
 * @retval 1 表示有新命令，0 表示暂无完整命令。
 */
uint8_t vofa_get_cmd_ready(void);
/**
 * @brief 获取最近一次接收的 VOFA+ 命令字符串。
 * @param 无。
 * @note 返回内部静态缓冲区指针，下次接收完整命令后内容会更新。
 * @retval 返回命令字符串指针。
 */
const char *vofa_get_cmd(void);

/**
 * @brief 原子读取一条完整的 VOFA+ ASCII 命令。
 * @param buffer 输出缓冲区。
 * @param bufferSize 输出缓冲区大小。
 * @note 读取成功后自动清除命令就绪标志，避免 ISR 与主循环同时改写命令。
 * @retval 1 表示读到命令，0 表示暂无命令或参数无效。
 */
uint8_t vofa_read_command(char *buffer, uint8_t bufferSize);

#endif
