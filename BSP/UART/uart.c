#include "uart.h"

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
    while (DL_UART_Main_isBusy(UART_0_INST) == true)
        ;
    DL_UART_Main_transmitData(UART_0_INST, ch);
}

/**
 * @brief 通过 UART0 发送字符串。
 * @param str 待发送字符串。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void uart0_send_string(const char *str)
{
    if (str == NULL)
        return;
    while (*str != '\0')
        uart0_send_char(*str++);
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
    while (DL_UART_isBusy(UART_0_INST) == true)
        ;
    DL_UART_Main_transmitData(UART_0_INST, ch);
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
    uint16_t char_len = 0;
    while (*s != 0)
    {
        while (DL_UART_isBusy(UART_0_INST) == true)
            ;
        DL_UART_Main_transmitData(UART_0_INST, *s++);
        char_len++;
    }
    return char_len;
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
 *  VOFA+ JustFloat \u53d1\u9001\u51fd\u6570
 *  \u683c\u5f0f\uff1a [float0 4B][float1 4B]...[floatN-1 4B][0x00 0x00 0x80 0x7F]
 *  \u5728VOFA+\u4e2d\u9009\u62e9 "JustFloat" \u534f\u8bae\u5373\u53ef\u81ea\u52a8\u89e3\u6790\u6d6e\u70b9\u6570\u636e
 * ================================================================ */
/**
 * @brief 按 VOFA JustFloat 协议发送浮点数组。
 * @param data 数据缓冲区。
 * @param len 数据长度。
 * @note 按 BSP/Module/App 三层结构封装，便于模板工程复用。
 * @retval 无。
 */
void vofa_justfloat_send(float *data, uint8_t len)
{
    uint8_t i, j;
    for (i = 0; i < len; i++)
    {
        uint8_t *p = (uint8_t *)&data[i];
        for (j = 0; j < 4; j++)
            uart0_send_char((char)p[j]);
    }
    /* JustFloat \u5e27\u5c3e */
    uart0_send_char((char)0x00);
    uart0_send_char((char)0x00);
    uart0_send_char((char)0x80);
    uart0_send_char((char)0x7F);
}

/* ================================================================
 *  VOFA+ \u547d\u4ee4\u63a5\u6536\uff08ASCII \u5355\u884c\u547d\u4ee4\uff09
 *  \u793a\u4f8b\uff1a\u5728VOFA+\u63a7\u5236\u53f0\u53d1\u9001 "KP=1.5\n" \u5373\u53ef\u4fee\u6539 Kp
 * ================================================================ */
static char vofa_cmd_buf[32]; /**< vofa_cmd_buf 全局状态或配置变量。 */
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

void UART0_IRQHandler(void)
{
    if (DL_UART_getPendingInterrupt(UART_0_INST) == DL_UART_IIDX_RX)
    {
        uint8_t ch = DL_UART_Main_receiveData(UART_0_INST);
        if (ch == '\n' || ch == '\r')
        {
            if (vofa_cmd_idx > 0)
            {
                vofa_cmd_buf[vofa_cmd_idx] = '\0';
                vofa_cmd_ready = 1;
                vofa_cmd_idx = 0;
            }
        }
        else if (vofa_cmd_idx < 31)
        {
            vofa_cmd_buf[vofa_cmd_idx++] = (char)ch;
        }
    }
}
