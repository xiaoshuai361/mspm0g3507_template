#include "opipi.h"
#include "ti_msp_dl_config.h"

static volatile uint8_t opiRxFlag;
static volatile uint8_t opiRxData;

static void OPi_SendByte(uint8_t data)
{
    while (DL_UART_Main_isBusy(UART_2_INST)) {}
    DL_UART_Main_transmitData(UART_2_INST, data);
}

void OPi_Init(void)
{
    /* 用蓝牙模块的初始化流程（已验证的UART2配置），然后改波特率 */
    extern void uart2_init(void);
    uart2_init();

    DL_UART_Main_disable(UART_2_INST);
    DL_UART_Main_setBaudRateDivisor(UART_2_INST, 17U, 23U);
    DL_UART_Main_enable(UART_2_INST);

    while (!DL_UART_Main_isRXFIFOEmpty(UART_2_INST)) {
        (void)DL_UART_Main_receiveData(UART_2_INST);
    }
    opiRxFlag = 0U;
    opiRxData = 0U;
}

void OPi_SendCmd(uint8_t code)
{
    OPi_SendByte(OPI_FRAME_HEAD);
    OPi_SendByte(code);
}

void OPi_SendPsValue(float ps)
{
    union {
        float value;
        uint8_t bytes[sizeof(float)];
    } payload;
    uint32_t i;

    payload.value = ps;
    OPi_SendByte(OPI_FRAME_HEAD);
    for (i = 0U; i < (uint32_t)sizeof(payload.bytes); i++) {
        OPi_SendByte(payload.bytes[i]);
    }
}

uint8_t OPi_ReadByte(uint8_t *byte)
{
    if (opiRxFlag == 0U) return 0U;
    *byte = opiRxData;
    opiRxFlag = 0U;
    return 1U;
}

void OPi_FlushRx(void)
{
    opiRxFlag = 0U;
    while (!DL_UART_Main_isRXFIFOEmpty(UART_2_INST)) {
        (void)DL_UART_Main_receiveData(UART_2_INST);
    }
}

void UART2_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(UART_2_INST))
    {
    case DL_UART_IIDX_RX:
        while (DL_UART_Main_isRXFIFOEmpty(UART_2_INST) == false)
        {
            const uint8_t data = DL_UART_Main_receiveData(UART_2_INST);
            if (data != (uint8_t)'\r' && data != (uint8_t)'\n')
            {
                opiRxData = data;
                opiRxFlag = 1U;
            }
        }
        break;
    default:
        break;
    }
    DL_UART_clearInterruptStatus(UART_2_INST, UART_2_INST->CPU_INT.RIS);
}
