#include "uart.h"

enum {
    UART0_TX_BUFFER_SIZE = 256U,
    VOFA_COMMAND_BUFFER_SIZE = 32U,
    VOFA_JUSTFLOAT_MAX_CHANNELS = 8U
};

static uint8_t uart0TxBuffer[UART0_TX_BUFFER_SIZE];
static volatile uint16_t uart0TxHead;
static volatile uint16_t uart0TxTail;

static uint16_t uart0_next_index(uint16_t index)
{
    index++;
    if (index >= UART0_TX_BUFFER_SIZE)
    {
        index = 0U;
    }
    return index;
}

static uint16_t uart0_tx_free(void)
{
    const uint16_t head = uart0TxHead;
    const uint16_t tail = uart0TxTail;

    if (head >= tail)
    {
        return (uint16_t)(UART0_TX_BUFFER_SIZE - (head - tail) - 1U);
    }
    return (uint16_t)(tail - head - 1U);
}

static void uart0_tx_service(void)
{
    while ((uart0TxTail != uart0TxHead) &&
           (DL_UART_Main_isTXFIFOFull(UART_0_INST) == false))
    {
        DL_UART_Main_transmitData(UART_0_INST, uart0TxBuffer[uart0TxTail]);
        uart0TxTail = uart0_next_index(uart0TxTail);
    }

    if (uart0TxTail == uart0TxHead)
    {
        DL_UART_Main_disableInterrupt(UART_0_INST,
                                      DL_UART_MAIN_INTERRUPT_TX);
    }
    else
    {
        DL_UART_Main_enableInterrupt(UART_0_INST,
                                     DL_UART_MAIN_INTERRUPT_TX);
    }
}

uint8_t uart0_write_nonblocking(const uint8_t *data, uint16_t len)
{
    uint16_t writeIndex;
    uint16_t i;
    uint32_t primask;

    if ((data == NULL) || (len == 0U))
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (len > uart0_tx_free())
    {
        __set_PRIMASK(primask);
        return 0U;
    }

    writeIndex = uart0TxHead;
    for (i = 0U; i < len; i++)
    {
        uart0TxBuffer[writeIndex] = data[i];
        writeIndex = uart0_next_index(writeIndex);
    }
    uart0TxHead = writeIndex;

    /* 先填满硬件 FIFO，再由 TX 中断继续搬运剩余字节。 */
    uart0_tx_service();
    __set_PRIMASK(primask);
    return 1U;
}

/* ================================================================
 *  UART3 — 灰度传感器数据上传（TX=PA14, RX=PA13, 115200bps）
 * ================================================================ */
/**
 * @brief 通过 UART3 发送一个字符。
 * @param ch 待发送字符。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart3_send_char(char ch)
{
    while (DL_UART_Main_isBusy(UART_3_INST) == true)
        ;
    DL_UART_Main_transmitData(UART_3_INST, ch);
}

/**
 * @brief 通过 UART3 发送字符串。
 * @param str 待发送字符串。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart3_send_string(const char *str)
{
    if (str == NULL)
        return;
    while (*str != '\0')
        uart3_send_char(*str++);
}

/* ================================================================
 *  UART0 基础收发
 * ================================================================ */
/**
 * @brief 初始化 UART0 调试串口。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart0_init(void)
{
    DL_UART_Main_setTXFIFOThreshold(UART_0_INST,
                                    DL_UART_MAIN_TX_FIFO_LEVEL_EMPTY);
    DL_UART_Main_disableInterrupt(UART_0_INST, DL_UART_MAIN_INTERRUPT_TX);
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    uart0_send_string("VOFA+ Ready\r\n");
}

/**
 * @brief 通过 UART0 发送一个字符。
 * @param ch 待发送字符。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart0_send_char(char ch)
{
    const uint8_t data = (uint8_t)ch;

    (void)uart0_write_nonblocking(&data, 1U);
}

/**
 * @brief 通过 UART0 发送字符串。
 * @param str 待发送字符串。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart0_send_string(const char *str)
{
    size_t len;

    if (str == NULL)
        return;
    len = strlen(str);
    if (len > UINT16_MAX)
        return;
    (void)uart0_write_nonblocking((const uint8_t *)str, (uint16_t)len);
}

/**
 * @brief 重定向 fputc / fputs 到 UART0（printf 等函数使用）。
 * @param ch 待发送字符。
 * @param stream stream 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回整型结果。
 */
int fputc(int ch, FILE *stream)
{
    const uint8_t data = (uint8_t)ch;

    (void)stream;
    (void)uart0_write_nonblocking(&data, 1U);
    return ch;
}

/**
 * @brief 重定向字符串输出到 UART0。
 * @param s s 参数。
 * @param stream stream 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回整型结果。
 */
int fputs(const char *restrict s, FILE *restrict stream)
{
    size_t len;

    (void)stream;
    if (s == NULL)
        return EOF;
    len = strlen(s);
    if (len > UINT16_MAX)
        return EOF;
    return uart0_write_nonblocking((const uint8_t *)s, (uint16_t)len)
               ? (int)len
               : EOF;
}

/**
 * @brief 重定向 puts 输出到 UART0。
 * @param _ptr _ptr 参数。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回整型结果。
 */
int puts(const char *_ptr)
{
    if (_ptr == NULL)
        return EOF;
    uart0_send_string(_ptr);
    uart0_send_string("\r\n");
    return 0;
}

/* ================================================================
 *  VOFA+ JustFloat transmit
 *  Format: [float0]...[floatN-1][00 00 80 7F]
 * ================================================================ */
uint8_t vofa_justfloat_send(const float *data, uint8_t len)
{
    uint8_t frame[(VOFA_JUSTFLOAT_MAX_CHANNELS * sizeof(float)) + 4U];
    const uint16_t payloadSize = (uint16_t)len * (uint16_t)sizeof(float);

    if ((data == NULL) || (len == 0U) ||
        (len > VOFA_JUSTFLOAT_MAX_CHANNELS))
    {
        return 0U;
    }

    memcpy(frame, data, payloadSize);
    frame[payloadSize + 0U] = 0x00U;
    frame[payloadSize + 1U] = 0x00U;
    frame[payloadSize + 2U] = 0x80U;
    frame[payloadSize + 3U] = 0x7FU;
    return uart0_write_nonblocking(frame, (uint16_t)(payloadSize + 4U));
}

/* ================================================================
 *  VOFA+ \u547d\u4ee4\u63a5\u6536\uff08ASCII \u5355\u884c\u547d\u4ee4\uff09
 *  Example: send "KP=1.5\n" from VOFA to update Kp.
 * ================================================================ */
static char vofa_rx_buf[VOFA_COMMAND_BUFFER_SIZE];
static char vofa_cmd_buf[VOFA_COMMAND_BUFFER_SIZE]; /**< vofa_cmd_buf 全局状态或配置变量。 */
static uint8_t vofa_cmd_idx = 0; /**< vofa_cmd_idx 全局状态或配置变量。 */
static volatile uint8_t vofa_cmd_ready = 0; /**< vofa_cmd_ready 全局状态或配置变量。 */

/**
 * @brief 获取 VOFA 命令接收完成状态。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回 8 位状态或数据。
 */
uint8_t vofa_get_cmd_ready(void)
{
    if (vofa_cmd_ready)
    {
        vofa_cmd_ready = 0;
        return 1;
    }
    return 0;
}

/**
 * @brief 执行 vofa_get_cmd 功能。
 * @param 无。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 返回状态名称字符串指针。
 */
const char *vofa_get_cmd(void)
{
    return vofa_cmd_buf;
}

uint8_t vofa_read_command(char *buffer, uint8_t bufferSize)
{
    uint8_t hasCommand = 0U;
    uint32_t primask;

    if ((buffer == NULL) || (bufferSize == 0U))
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (vofa_cmd_ready != 0U)
    {
        (void)strncpy(buffer, vofa_cmd_buf, (size_t)bufferSize - 1U);
        buffer[bufferSize - 1U] = '\0';
        vofa_cmd_ready = 0U;
        hasCommand = 1U;
    }
    __set_PRIMASK(primask);
    return hasCommand;
}

void UART0_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST))
    {
    case DL_UART_MAIN_IIDX_RX:
        while (DL_UART_Main_isRXFIFOEmpty(UART_0_INST) == false)
        {
            const uint8_t ch = DL_UART_Main_receiveData(UART_0_INST);

            if ((ch == '\n') || (ch == '\r'))
            {
                if ((vofa_cmd_idx > 0U) && (vofa_cmd_ready == 0U))
                {
                    vofa_rx_buf[vofa_cmd_idx] = '\0';
                    memcpy(vofa_cmd_buf, vofa_rx_buf,
                           (size_t)vofa_cmd_idx + 1U);
                    vofa_cmd_ready = 1U;
                }
                vofa_cmd_idx = 0;
            }
            else if (vofa_cmd_idx < (VOFA_COMMAND_BUFFER_SIZE - 1U))
            {
                vofa_rx_buf[vofa_cmd_idx++] = (char)ch;
            }
            else
            {
                vofa_cmd_idx = 0U;
            }
        }
        break;

    case DL_UART_MAIN_IIDX_TX:
        uart0_tx_service();
        break;

    default:
        break;
    }
}
