#include "opipi.h"
#include "ti_msp_dl_config.h"

#define OPI_RX_QUEUE_SIZE (8U)
#define OPI_RX_QUEUE_MASK (OPI_RX_QUEUE_SIZE - 1U)
#define OPI_FORWARD_QUEUE_SIZE (64U)
#define OPI_FORWARD_QUEUE_MASK (OPI_FORWARD_QUEUE_SIZE - 1U)

static volatile uint8_t opiRxQueue[OPI_RX_QUEUE_SIZE];
static volatile uint8_t opiRxHead;
static volatile uint8_t opiRxTail;
static volatile uint8_t opiRxHasHeader;
static volatile uint8_t opiForwardQueue[OPI_FORWARD_QUEUE_SIZE];
static volatile uint8_t opiForwardHead;
static volatile uint8_t opiForwardTail;

static uint8_t OPi_IsValidRxStatus(uint8_t code)
{
    return (code == OPI_STATUS_BOOT_READY) ||
           (code == OPI_STATUS_CONTROL_READY) ||
           (code == OPI_STATUS_TASK3_DONE);
}

static uint8_t OPi_IsValidTwoByteCommand(uint8_t code)
{
    return (code == OPI_CMD_TASK2) ||
           (code == OPI_CMD_TASK3) ||
           (code == OPI_CMD_TASK4) ||
           (code == OPI_CMD_TASK5) ||
           (code == OPI_CMD_TASK3_ACTION) ||
           (code == OPI_CMD_ABORT);
}

static void OPi_QueueForwardByte(uint8_t data)
{
    uint8_t nextHead;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    nextHead = (uint8_t)((opiForwardHead + 1U) &
                         OPI_FORWARD_QUEUE_MASK);
    if (nextHead != opiForwardTail) {
        opiForwardQueue[opiForwardHead] = data;
        opiForwardHead = nextHead;
    }
    __set_PRIMASK(primask);
}

static void OPi_ParseRxByte(uint8_t data)
{
    if (opiRxHasHeader == 0U) {
        if (data == OPI_FRAME_HEAD) {
            opiRxHasHeader = 1U;
        }
    } else if (data == OPI_FRAME_HEAD) {
        /* 连续帧头时保留最后一个帧头，快速重新同步。 */
        opiRxHasHeader = 1U;
    } else {
        const uint8_t nextHead = (uint8_t)(
            (opiRxHead + 1U) & OPI_RX_QUEUE_MASK);

        if ((OPi_IsValidRxStatus(data) != 0U) &&
            (nextHead != opiRxTail)) {
            opiRxQueue[opiRxHead] = data;
            opiRxHead = nextHead;
        }
        opiRxHasHeader = 0U;
    }
}

static void OPi_SendByte(uint8_t data)
{
    while (DL_UART_Main_isBusy(UART_2_INST)) {}
    DL_UART_Main_transmitData(UART_2_INST, data);
    /* 不把 M0 发出的字节放入转发队列，UART0 只显示香橙派发来的原始数据。 */
}

void OPi_Init(void)
{
    NVIC_DisableIRQ(UART_2_INST_INT_IRQN);

    DL_UART_Main_disable(UART_2_INST);
    /* UART2时钟32MHz、16倍过采样，对应115200 baud。 */
    DL_UART_Main_setOversampling(UART_2_INST,
                                 DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART_2_INST, 17U, 23U);
    DL_UART_Main_enable(UART_2_INST);

    opiRxHead = 0U;
    opiRxTail = 0U;
    opiRxHasHeader = 0U;
    opiForwardHead = 0U;
    opiForwardTail = 0U;
    while (!DL_UART_Main_isRXFIFOEmpty(UART_2_INST)) {
        const uint8_t data = DL_UART_Main_receiveData(UART_2_INST);

        OPi_QueueForwardByte(data);
        OPi_ParseRxByte(data);
    }
    DL_UART_clearInterruptStatus(UART_2_INST, UART_2_INST->CPU_INT.RIS);
    NVIC_ClearPendingIRQ(UART_2_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_2_INST_INT_IRQN);
}

void OPi_SendCmd(uint8_t code)
{
    /* Task6 must always use OPi_SendTask6() so POS cannot be omitted. */
    if (OPi_IsValidTwoByteCommand(code) == 0U) {
        return;
    }

    OPi_SendByte(OPI_FRAME_HEAD);
    OPi_SendByte(code);
}

void OPi_SendTask6(int8_t positionTenthsCm)
{
    if (positionTenthsCm < -125) {
        positionTenthsCm = -125;
    } else if (positionTenthsCm > 125) {
        positionTenthsCm = 125;
    }

    OPi_SendByte(OPI_FRAME_HEAD);
    OPi_SendByte(OPI_CMD_TASK6);
    OPi_SendByte((uint8_t)positionTenthsCm);
}

uint8_t OPi_ReadFrame(uint8_t *code)
{
    uint8_t hasFrame = 0U;
    uint32_t primask;

    if (code == 0) {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (opiRxTail != opiRxHead) {
        *code = opiRxQueue[opiRxTail];
        opiRxTail = (uint8_t)((opiRxTail + 1U) & OPI_RX_QUEUE_MASK);
        hasFrame = 1U;
    }
    __set_PRIMASK(primask);
    return hasFrame;
}

uint8_t OPi_ReadForwardByte(uint8_t *byte)
{
    uint8_t hasByte = 0U;
    uint32_t primask;

    if (byte == 0) {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (opiForwardTail != opiForwardHead) {
        *byte = opiForwardQueue[opiForwardTail];
        opiForwardTail = (uint8_t)(
            (opiForwardTail + 1U) & OPI_FORWARD_QUEUE_MASK);
        hasByte = 1U;
    }
    __set_PRIMASK(primask);
    return hasByte;
}

void OPi_FlushRx(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    opiRxHead = 0U;
    opiRxTail = 0U;
    opiRxHasHeader = 0U;
    while (!DL_UART_Main_isRXFIFOEmpty(UART_2_INST)) {
        OPi_QueueForwardByte(DL_UART_Main_receiveData(UART_2_INST));
    }
    __set_PRIMASK(primask);
}

void UART2_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(UART_2_INST))
    {
    case DL_UART_IIDX_RX:
        while (DL_UART_Main_isRXFIFOEmpty(UART_2_INST) == false)
        {
            const uint8_t data = DL_UART_Main_receiveData(UART_2_INST);

            OPi_QueueForwardByte(data);
            OPi_ParseRxByte(data);
        }
        break;
    default:
        break;
    }
    DL_UART_clearInterruptStatus(UART_2_INST, UART_2_INST->CPU_INT.RIS);
}
