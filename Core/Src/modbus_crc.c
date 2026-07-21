#include "modbus_crc.h"
#include <stddef.h>


uint16_t Modbus_CRC16(const uint8_t *data,
                      uint16_t length)
{
    uint16_t crc;
    uint16_t byte_index;
    uint8_t bit_index;


    /*
     * Modbus CRC的初始值固定为0xFFFF。
     */
    crc = 0xFFFFU;


    /*
     * data为空，但length不是0，说明参数错误。
     */
    if ((data == NULL) && (length > 0U))
    {
        return 0U;
    }


    /*
     * 依次处理每一个字节。
     */
    for (byte_index = 0U;
         byte_index < length;
         byte_index++)
    {
        /*
         * 当前字节先与CRC低8位进行异或。
         */
        crc ^= data[byte_index];


        /*
         * 一个字节有8位，因此每个字节处理8次。
         */
        for (bit_index = 0U;
             bit_index < 8U;
             bit_index++)
        {
            /*
             * 判断CRC最低位是否为1。
             */
            if ((crc & 0x0001U) != 0U)
            {
                /*
                 * 最低位为1：
                 * 先右移，再与多项式0xA001异或。
                 */
                crc >>= 1U;
                crc ^= 0xA001U;
            }
            else
            {
                /*
                 * 最低位为0：
                 * 只需要向右移动一位。
                 */
                crc >>= 1U;
            }
        }
    }


    return crc;
}

uint8_t Modbus_CheckCRC(const uint8_t *frame,
                        uint16_t length)
{
    uint16_t calculated_crc;
    uint16_t received_crc;


    /*
     * 一条最基本的Modbus RTU报文至少包含：
     *
     * 从机地址：1字节
     * 功能码：  1字节
     * CRC：     2字节
     *
     * 所以长度不能小于4字节。
     */
    if ((frame == NULL) || (length < 4U))
    {
        return 0U;
    }


    /*
     * 最后两个字节是电脑发送过来的CRC，
     * 因此重新计算时，不能把最后两个CRC字节也算进去。
     */
    calculated_crc =
        Modbus_CRC16(frame,
                     length - 2U);


    /*
     * Modbus在线路上先发送CRC低字节，
     * 再发送CRC高字节。
     *
     * 例如报文末尾：
     *
     * C4 0B
     *
     * 组合成16位数字以后是：
     *
     * 0x0BC4
     */
    received_crc =
        (uint16_t)frame[length - 2U];

    received_crc |=
        ((uint16_t)frame[length - 1U] << 8U);


    if (calculated_crc == received_crc)
    {
        return 1U;
    }


    return 0U;
}
