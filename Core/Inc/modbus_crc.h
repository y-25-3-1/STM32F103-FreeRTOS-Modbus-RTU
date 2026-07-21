#ifndef __MODBUS_CRC_H
#define __MODBUS_CRC_H


#include <stdint.h>


/*
 * 计算Modbus RTU使用的CRC16。
 *
 * data：
 * 需要参与计算的数据首地址。
 *
 * length：
 * 参与计算的数据字节数量。
 *
 * 返回值：
 * 16位CRC计算结果。
 */
uint16_t Modbus_CRC16(const uint8_t *data,
                      uint16_t length);

/*
 * 检查一整条Modbus RTU报文的CRC是否正确。
 *
 * frame：
 * 完整报文，包括最后两个CRC字节。
 *
 * length：
 * 完整报文的总长度。
 *
 * 返回值：
 * 1U：CRC正确。
 * 0U：CRC错误，或者参数不合法。
 */
uint8_t Modbus_CheckCRC(const uint8_t *frame,
                        uint16_t length);

#endif