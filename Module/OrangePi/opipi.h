#ifndef MODULE_OPIPI_H
#define MODULE_OPIPI_H

#include <stdint.h>

/* 协议帧头 */
#define OPI_FRAME_HEAD  0xAAU

/* MSPM0发给香橙派的命令。 */
#define OPI_CMD_TASK2       0x02U
#define OPI_CMD_TASK3       0x03U
#define OPI_CMD_TASK4       0x04U
#define OPI_CMD_TASK5       0x05U
#define OPI_CMD_TASK6       0x06U
#define OPI_CMD_POSITION    0x16U
#define OPI_CMD_FINISH      0x0FU
#define OPI_CMD_ABORT       0xFFU

/* 香橙派发给MSPM0的状态。 */
#define OPI_STATUS_IDLE           0x00U
#define OPI_STATUS_VIDEO_READY    0x10U
#define OPI_STATUS_CONTROL_READY  0x11U
#define OPI_STATUS_CLEANUP_DONE   0x12U
#define OPI_STATUS_STREAM_ERROR   0xE1U
#define OPI_STATUS_COMMAND_ERROR  0xEEU
#define OPI_STATUS_TIMEOUT        0xEFU

void OPi_Init(void);
void OPi_SendCmd(uint8_t code);
void OPi_SendPosition(int8_t positionTenthsCm);
uint8_t OPi_ReadFrame(uint8_t *code);
void OPi_FlushRx(void);

#endif
