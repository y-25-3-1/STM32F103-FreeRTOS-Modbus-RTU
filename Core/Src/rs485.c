#include "rs485.h"
#include "usart.h"
#include <stddef.h>
#include <string.h>


/*
 * 判断一帧结束所使用的静默时间。
 *
 * 连续5ms没有收到新字节，
 * 就认为上一帧已经结束。
 */
#define RS485_FRAME_GAP_MS    5U


/*
 * USART2每次中断临时接收一个字节。
 */
static uint8_t g_rs485_irq_byte;


/*
 * 保存连续收到的数据。
 */
static uint8_t g_rs485_rx_buffer[RS485_RX_BUFFER_SIZE];


/*
 * 当前已经收到多少个字节。
 *
 * volatile表示这个变量可能在中断中发生变化。
 */
static volatile uint16_t g_rs485_rx_length = 0U;


/*
 * 最后一个字节到达的时间。
 */
static volatile uint32_t g_rs485_last_rx_tick = 0U;


/*
 * 给RS485模块的收发切换留出短暂时间。
 */
static void RS485_ShortDelay(void)
{
    volatile uint32_t i;

    for (i = 0U; i < 1000U; i++)
    {
        __NOP();
    }
}


/*
 * 清除USART2中可能残留的数据和错误状态。
 */
static void RS485_ClearUartReceiveState(void)
{
    volatile uint32_t temporary_value;

    temporary_value = huart2.Instance->SR;
    temporary_value = huart2.Instance->DR;

    (void)temporary_value;

    huart2.ErrorCode = HAL_UART_ERROR_NONE;
}


/*
 * 进入RS485发送模式。
 */
void RS485_SetTransmitMode(void)
{
    /*
     * 关闭STM32内部的USART2接收器。
     * 发送期间不监听自己的信号。
     */
    CLEAR_BIT(huart2.Instance->CR1,
              USART_CR1_RE);

    /*
     * 清除可能残留的异常接收数据。
     */
    RS485_ClearUartReceiveState();

    /*
     * EN拉高，外部RS485模块进入发送模式。
     */
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port,
                      RS485_EN_Pin,
                      GPIO_PIN_SET);

    RS485_ShortDelay();
}


/*
 * 进入RS485接收模式。
 */
void RS485_SetReceiveMode(void)
{
    /*
     * EN拉低，释放A/B总线。
     */
    HAL_GPIO_WritePin(RS485_EN_GPIO_Port,
                      RS485_EN_Pin,
                      GPIO_PIN_RESET);

    RS485_ShortDelay();

    /*
     * 清除收发切换时可能产生的残留状态。
     */
    RS485_ClearUartReceiveState();

    /*
     * 重新打开STM32内部USART2接收器。
     */
    SET_BIT(huart2.Instance->CR1,
            USART_CR1_RE);
}


/*
 * 初始化RS485。
 */
void RS485_Init(void)
{
    /*
     * 清空接收记录。
     */
    g_rs485_rx_length = 0U;
    g_rs485_last_rx_tick = 0U;

    /*
     * 默认进入接收状态。
     */
    RS485_SetReceiveMode();

    /*
     * 启动USART2中断接收。
     *
     * 每收到1个字节，就会触发一次接收完成中断。
     */
    HAL_UART_Receive_IT(&huart2,
                        &g_rs485_irq_byte,
                        1U);
}


/*
 * 发送一组数据。
 */
HAL_StatusTypeDef RS485_Send(const uint8_t *data,
                             uint16_t length,
                             uint32_t timeout)
{
    HAL_StatusTypeDef status;

    if ((data == NULL) || (length == 0U))
    {
        return HAL_ERROR;
    }

    /*
     * 切换到发送状态。
     */
    RS485_SetTransmitMode();

    /*
     * 阻塞式发送。
     */
    status = HAL_UART_Transmit(&huart2,
                               (uint8_t *)data,
                               length,
                               timeout);

    /*
     * 发送完成后重新回到接收状态。
     */
    RS485_SetReceiveMode();

    return status;
}


/*
 * USART接收完成回调函数。
 *
 * 每当USART2完整收到1个字节，
 * HAL都会自动调用这个函数。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    /*
     * 系统中可能存在多个串口，
     * 所以先确认是不是USART2触发了回调。
     */
    if (huart->Instance == USART2)
    {
        /*
         * 缓冲区没有装满时，
         * 把本次收到的字节保存进去。
         */
        if (g_rs485_rx_length < RS485_RX_BUFFER_SIZE)
        {
            g_rs485_rx_buffer[g_rs485_rx_length] =
                g_rs485_irq_byte;

            g_rs485_rx_length++;
        }

        /*
         * 记录最后一个字节的到达时间。
         */
        g_rs485_last_rx_tick = HAL_GetTick();

        /*
         * 再次启动下一字节的中断接收。
         *
         * 不重新启动的话，
         * USART2只会接收第一个字节。
         */
        HAL_UART_Receive_IT(&huart2,
                            &g_rs485_irq_byte,
                            1U);
    }
}


/*
 * 尝试取出一整帧数据。
 */
uint16_t RS485_ReadFrame(uint8_t *data,
                         uint16_t max_length)
{
    uint16_t frame_length;
    uint32_t current_tick;
    uint32_t interrupt_state;

    frame_length = 0U;

    if ((data == NULL) || (max_length == 0U))
    {
        return 0U;
    }

    /*
     * 保存进入函数前的中断状态。
     */
    interrupt_state = __get_PRIMASK();

    /*
     * 暂时关闭中断。
     *
     * 防止复制缓冲区的过程中，
     * USART2中断又往缓冲区添加新字节。
     */
    __disable_irq();

    current_tick = HAL_GetTick();

    /*
     * 两个条件同时满足，才认为收到一整帧：
     *
     * 1. 缓冲区中至少有1个字节；
     * 2. 距离最后一个字节到达已经超过5ms。
     */
    if ((g_rs485_rx_length > 0U) &&
        ((current_tick - g_rs485_last_rx_tick) >=
         RS485_FRAME_GAP_MS))
    {
        frame_length = g_rs485_rx_length;

        /*
         * 防止用户提供的目标数组太小。
         */
        if (frame_length > max_length)
        {
            frame_length = max_length;
        }

        /*
         * 把中断接收缓冲区的数据复制出去。
         */
        memcpy(data,
               g_rs485_rx_buffer,
               frame_length);

        /*
         * 清零长度，为接收下一帧做准备。
         */
        g_rs485_rx_length = 0U;
    }

    /*
     * 如果进入函数前中断是开启的，
     * 这里恢复开启中断。
     */
    if (interrupt_state == 0U)
    {
        __enable_irq();
    }

    return frame_length;
}