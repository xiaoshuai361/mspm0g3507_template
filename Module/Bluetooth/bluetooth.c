#include "bluetooth.h"

volatile uint8_t BT_RxFlag; /**< 蓝牙接收完成标志。 */
volatile uint8_t BT_RxData; /**< 蓝牙最近接收字节。 */
volatile uint32_t g_uart2_isr_count; /**< UART2 中断进入计数。 */
volatile uint32_t g_uart2_rx_count; /**< UART2 接收字节计数。 */
volatile uint32_t g_uart2_other_irq_count; /**< UART2 非 RX 中断计数。 */

#define UART2_TX_TIMEOUT_COUNT (100000UL) /**< UART2 发送等待超时计数，避免蓝牙串口阻塞主循环。 */

/**
 * @brief 判断 UART2 收到的字节是否为行结束符。
 * @param data UART2 接收字节。
 * @note 手机蓝牙助手常会在命令后追加 CR/LF；这里忽略它们，避免覆盖前一个有效命令。
 * @retval 1 表示 CR/LF，0 表示普通字节。
 */
static uint8_t Bluetooth_IsLineEnding(uint8_t data)
{
    return ((data == (uint8_t)'\r') || (data == (uint8_t)'\n')) ? 1U : 0U;
}

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
/* 蓝牙已替换为香橙派通信，ISR移至 Module/OrangePi/opipi.c */
#if 0
void UART2_IRQHandler()
{
    g_uart2_isr_count++;

    switch (DL_UART_getPendingInterrupt(UART_2_INST))
    {
    case DL_UART_IIDX_RX:
        while (DL_UART_Main_isRXFIFOEmpty(UART_2_INST) == false)
        {
            const uint8_t data = DL_UART_Main_receiveData(UART_2_INST);
            if (Bluetooth_IsLineEnding(data) == 0U)
            {
                BT_RxData = data;
                BT_RxFlag = 1U;
            }
            g_uart2_rx_count++;
        }
        break;
    default:
        g_uart2_other_irq_count++;
        break;
    }

    DL_UART_clearInterruptStatus(UART_2_INST, UART_2_INST->CPU_INT.RIS);
}
#endif /* 0 - 蓝牙替换为香橙派 */

/**
 * @brief 原子读取 UART2 最近一个有效蓝牙字节。
 * @param data 输出读取到的字节。
 * @note 同时读取数据和清标志，避免 Get_RxFlag()/Get_RxData() 分离造成竞态。
 * @retval 1 表示读到新字节，0 表示没有新字节或参数无效。
 */
uint8_t Bluetooth_ReadByte(uint8_t *data)
{
    uint8_t hasData = 0U;
    uint32_t primask;

    if (data == 0)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (BT_RxFlag == 1U)
    {
        *data = BT_RxData;
        BT_RxFlag = 0U;
        hasData = 1U;
    }
    __set_PRIMASK(primask);

    return hasData;
}

/**
 * @brief 获取蓝牙接收标志。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t Get_RxFlag(void)
{
    uint8_t unused;

    return Bluetooth_ReadByte(&unused);
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


