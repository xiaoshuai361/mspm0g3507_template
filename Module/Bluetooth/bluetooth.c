#include "bluetooth.h"
volatile uint8_t BT_RxFlag; /**< 蓝牙接收完成标志。 */
volatile uint8_t BT_RxData; /**< 蓝牙最近接收字节。 */
volatile uint32_t g_uart2_isr_count; /**< UART2 中断进入计数。 */
volatile uint32_t g_uart2_rx_count; /**< UART2 接收字节计数。 */
volatile uint32_t g_uart2_other_irq_count; /**< UART2 非 RX 中断计数。 */

#define UART2_TX_TIMEOUT_COUNT (100000UL) /**< UART2 发送等待超时计数，避免蓝牙串口阻塞主循环。 */

/**
 * @brief 初始化 UART2 蓝牙串口。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart2_init(void)
{
    BT_RxFlag = 0U;
    BT_RxData = 0U;

    NVIC_DisableIRQ(UART_2_INST_INT_IRQN);
    while (DL_UART_Main_isRXFIFOEmpty(UART_2_INST) == false)
    {
        (void)DL_UART_Main_receiveData(UART_2_INST);
    }
    DL_UART_clearInterruptStatus(UART_2_INST, UART_2_INST->CPU_INT.RIS);
    NVIC_ClearPendingIRQ(UART_2_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_2_INST_INT_IRQN);

    /* UART2 初始化完成后，从 UART2 自己发出确认信息，方便检查蓝牙串口接线与发送通路。 */
    uart2_send_string("UART2 bluetooth OK\r\n");
}


//1发送单个字符串
/**
 * @brief 通过 UART2 发送一个字符。
 * @param ch 待发送字符。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart2_send_char(char ch)
{
    uint32_t timeout = UART2_TX_TIMEOUT_COUNT;

    while ((DL_UART_Main_isBusy(UART_2_INST) == true) && (timeout > 0U))
    {
        timeout--;
    }

    if (timeout == 0U)
    {
        return;
    }

    DL_UART_Main_transmitData(UART_2_INST,ch);
}
//2发送字符串
/**
 * @brief 通过 UART2 发送字符串。
 * @param str 待发送字符串。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart2_send_string(const char *str)
{
    if (str == 0)
    {
        return;
    }

    while(*str!=0)
    {
        uart2_send_char(*str++);
    }
}

/**
 * @brief UART2 中断服务函数。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void UART2_IRQHandler()
{
    g_uart2_isr_count++;

    switch (DL_UART_getPendingInterrupt(UART_2_INST))
    {
    case DL_UART_IIDX_RX:
        while (DL_UART_Main_isRXFIFOEmpty(UART_2_INST) == false)
        {
            BT_RxData = DL_UART_Main_receiveData(UART_2_INST);
            BT_RxFlag = 1U;
            g_uart2_rx_count++;
        }
        break;
    default:
        g_uart2_other_irq_count++;
        break;
    }

    DL_UART_clearInterruptStatus(UART_2_INST, UART_2_INST->CPU_INT.RIS);
}

/**
 * @brief 获取蓝牙接收标志。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t Get_RxFlag(void)
{
	if(BT_RxFlag == 1)
	{
		BT_RxFlag = 0;
		return 1;
	}
	else
		return 0;
}
/**
 * @brief 获取蓝牙最近接收字节并清标志。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t Get_RxData(void)
{
	return BT_RxData;
}


