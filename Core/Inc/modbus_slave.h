#ifndef __MODBUS_SLAVE_H
#define __MODBUS_SLAVE_H

#include <stdint.h>


/*
 * 本机Modbus从机地址。
 */
#define MODBUS_SLAVE_ADDRESS                 0x01U


/*
 * 当前一共建立8个保持寄存器。
 */
#define MODBUS_HOLDING_REGISTER_COUNT        8U


/*
 * 保持寄存器地址映射。
 */
#define MODBUS_REG_UPTIME_SECONDS            0U
#define MODBUS_REG_VALID_FRAME_COUNT         1U
#define MODBUS_REG_CRC_ERROR_COUNT           2U
#define MODBUS_REG_LED_CONTROL               3U
#define MODBUS_REG_LED_ACTUAL_STATE          4U
#define MODBUS_REG_LAST_FUNCTION_CODE        5U
#define MODBUS_REG_EXCEPTION_COUNT           6U
#define MODBUS_REG_DEVICE_STATUS             7U


/*
 * 初始化Modbus从机。
 */
void Modbus_SlaveInit(void);


/*
 * 周期更新运行时间、LED实际状态等动态数据。
 */
void Modbus_UpdateDeviceData(void);


/*
 * 记录一条CRC正确并且发给本机的报文。
 */
void Modbus_RecordValidFrame(
    uint8_t function_code);


/*
 * 记录一次CRC错误。
 */
void Modbus_RecordCrcError(void);


/*
 * 处理一条Modbus请求并生成响应。
 */
uint16_t Modbus_SlaveProcessRequest(
    const uint8_t *request,
    uint16_t request_length,
    uint8_t *response,
    uint16_t response_max_length);


#endif