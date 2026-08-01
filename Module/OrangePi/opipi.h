#ifndef MODULE_OPIPI_H
#define MODULE_OPIPI_H

#include <stdint.h>

/* 协议帧头，唯一同步字节。 */
#define OPI_FRAME_HEAD  0xAAU

/* ================================================================
 * MSPM0 → 香橙派 命令
 * ================================================================ */
#define OPI_CMD_TASK2          0x02U  /**< 启动 Task2（视觉PD+IMU 0cm）。 */
#define OPI_CMD_TASK3          0x03U  /**< 启动 Task3（专用位置控制 0cm）。 */
#define OPI_CMD_TASK4          0x04U  /**< 启动 Task4（视觉PD+IMU 0cm）。 */
#define OPI_CMD_TASK5          0x05U  /**< 启动 Task5（视觉PD+IMU 0cm）。 */
#define OPI_CMD_TASK3_ACTION   0x07U  /**< Task3 已稳定后执行 +5cm → -5cm 动作。 */
#define OPI_CMD_ABORT          0xFFU  /**< 当前任务立即失能。 */

/* ================================================================
 * 香橙派 → MSPM0 状态
 * ================================================================ */
#define OPI_STATUS_BOOT_READY      0x01U  /**< 初始化完成，仅发送一次。 */
#define OPI_STATUS_CONTROL_READY   0x11U  /**< 当前任务稳定就绪（Task3/4/5/6）。 */
#define OPI_STATUS_TASK3_DONE      0x12U  /**< Task3 的 +5→-5cm 动作完成，继续保持-5cm。 */

/* ================================================================
 * API
 * ================================================================ */
void OPi_Init(void);

/** 发送两字节命令帧。无效命令以及缺少 POS 的 Task6 命令不会发送。 */
void OPi_SendCmd(uint8_t code);

/** 发送三字节帧 AA 06 POS，直接启动 Task6 并指定目标位置。
 *  @param positionTenthsCm 有符号目标位置，单位 0.1cm，范围 -125..125。 */
void OPi_SendTask6(int8_t positionTenthsCm);

/** 从已解码的帧队列中取出一帧（仅 code 字节）。
 *  @retval 1 取到一帧，0 队列空。 */
uint8_t OPi_ReadFrame(uint8_t *code);

/** 读取转发队列中的原始字节（用于调试输出到 UART0）。
 *  @retval 1 取到一个字节，0 队列空。 */
uint8_t OPi_ReadForwardByte(uint8_t *byte);

/** 清空接收帧队列和 UART FIFO。 */
void OPi_FlushRx(void);

#endif
