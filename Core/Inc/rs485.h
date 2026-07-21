#ifndef __RS485_H
#define __RS485_H

#include "main.h"
#include <stdint.h>


/*
 * RS485接收缓冲区最大长度。
 *
 * 当前最多保存128个字节。
 */
#define RS485_RX_BUFFER_SIZE    128U


/*
 * 初始化RS485，并启动USART2中断接收。
 */
void RS485_Init(void);


/*
 * 切换成发送模式。
 */
void RS485_SetTransmitMode(void);


/*
 * 切换成接收模式。
 */
void RS485_SetReceiveMode(void);


/*
 * 通过RS485发送指定长度的数据。
 */
HAL_StatusTypeDef RS485_Send(const uint8_t *data,
                             uint16_t length,
                             uint32_t timeout);


/*
 * 检查是否已经收到一整帧。
 *
 * 收到完整帧：
 * 返回帧长度，大于0。
 *
 * 当前还没有完整帧：
 * 返回0。
 */
uint16_t RS485_ReadFrame(uint8_t *data,
                         uint16_t max_length);


#endif