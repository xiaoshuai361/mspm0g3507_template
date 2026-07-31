#ifndef MODULE_OPIPI_H
#define MODULE_OPIPI_H

#include <stdint.h>

/* 协议帧头 */
#define OPI_FRAME_HEAD  0xAAU

/* MSPM0发给香橙派的命令 */
#define OPI_CMD_TASK3    0x03U   /* 执行题目3(Task2) */
#define OPI_CMD_TASK4    0x04U   /* 执行题目4(Task3) */
#define OPI_CMD_TASK5    0x05U   /* 执行题目5(Task4) */
#define OPI_CMD_TASK6    0x06U   /* 执行题目6(Task5) */
#define OPI_CMD_FINISH   0x0FU   /* 小车运动结束 */
#define OPI_CMD_ABORT    0xFFU   /* 中止任务 */

/* 香橙派发给MSPM0的应答 */
#define OPI_ACK_OK       0x00U   /* 就绪/稳定/结束 */
#define OPI_ACK_ERROR    0xEEU   /* 无法执行 */
#define OPI_ACK_TIMEOUT  0xEFU   /* 超时 */

void OPi_Init(void);
void OPi_SendCmd(uint8_t code);
/* Send 0xAA followed by a little-endian IEEE754 float ps value. */
void OPi_SendPsValue(float ps);
uint8_t OPi_ReadByte(uint8_t *byte);
void OPi_FlushRx(void);

#endif
