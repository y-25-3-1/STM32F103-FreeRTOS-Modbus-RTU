#include "modbus_slave.h"
#include "modbus_crc.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>


/*
 * 功能码0x03：
 * 读取保持寄存器。
 */
#define MODBUS_FUNCTION_READ_HOLDING_REGISTERS     0x03U


/*
 * 功能码0x06：
 * 写单个保持寄存器。
 */
#define MODBUS_FUNCTION_WRITE_SINGLE_REGISTER      0x06U


/*
 * 功能码0x03最多读取125个寄存器。
 */
#define MODBUS_MAX_READ_REGISTER_QUANTITY          125U


/*
 * Modbus异常码。
 */
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION          0x01U
#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS      0x02U
#define MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE        0x03U


/*
 * LED控制值。
 */
#define MODBUS_LED_OFF_VALUE                       0U
#define MODBUS_LED_ON_VALUE                        1U


/*
 * 设备状态寄存器中的各个位。
 */
#define MODBUS_STATUS_RUNNING                      0x0001U
#define MODBUS_STATUS_LED_ON                       0x0002U
#define MODBUS_STATUS_VALID_FRAME_RECEIVED         0x0004U
#define MODBUS_STATUS_CRC_ERROR_OCCURRED           0x0008U
#define MODBUS_STATUS_EXCEPTION_OCCURRED           0x0010U


/*
 * 8个保持寄存器。
 */
static uint16_t g_holding_registers[
    MODBUS_HOLDING_REGISTER_COUNT
];


/*
 * 对计数寄存器加1。
 *
 * 到达65535后不再增加，
 * 防止重新变成0。
 */
static void Modbus_IncrementRegister(
    uint16_t register_address)
{
    if (g_holding_registers[register_address] <
        0xFFFFU)
    {
        g_holding_registers[register_address]++;
    }
}


/*
 * 读取PC13 LED的真实状态。
 *
 * PC13板载LED通常是低电平点亮。
 */
static void Modbus_UpdateLedActualState(void)
{
    GPIO_PinState pin_state;


    pin_state =
        HAL_GPIO_ReadPin(
            STATUS_LED_GPIO_Port,
            STATUS_LED_Pin);


    if (pin_state == GPIO_PIN_RESET)
    {
        g_holding_registers[
            MODBUS_REG_LED_ACTUAL_STATE
        ] = MODBUS_LED_ON_VALUE;
    }
    else
    {
        g_holding_registers[
            MODBUS_REG_LED_ACTUAL_STATE
        ] = MODBUS_LED_OFF_VALUE;
    }
}


/*
 * 更新设备状态寄存器。
 */
static void Modbus_UpdateDeviceStatus(void)
{
    uint16_t status;


    /*
     * 程序正常运行后，
     * bit0始终设置为1。
     */
    status = MODBUS_STATUS_RUNNING;


    /*
     * LED实际点亮时设置bit1。
     */
    if (g_holding_registers[
            MODBUS_REG_LED_ACTUAL_STATE
        ] == MODBUS_LED_ON_VALUE)
    {
        status |= MODBUS_STATUS_LED_ON;
    }


    /*
     * 至少收到过一条有效帧时设置bit2。
     */
    if (g_holding_registers[
            MODBUS_REG_VALID_FRAME_COUNT
        ] > 0U)
    {
        status |=
            MODBUS_STATUS_VALID_FRAME_RECEIVED;
    }


    /*
     * 曾经出现CRC错误时设置bit3。
     */
    if (g_holding_registers[
            MODBUS_REG_CRC_ERROR_COUNT
        ] > 0U)
    {
        status |=
            MODBUS_STATUS_CRC_ERROR_OCCURRED;
    }


    /*
     * 曾经返回过异常响应时设置bit4。
     */
    if (g_holding_registers[
            MODBUS_REG_EXCEPTION_COUNT
        ] > 0U)
    {
        status |=
            MODBUS_STATUS_EXCEPTION_OCCURRED;
    }


    g_holding_registers[
        MODBUS_REG_DEVICE_STATUS
    ] = status;
}


/*
 * 根据寄存器3控制PC13 LED。
 */
static void Modbus_ApplyLedControl(
    uint16_t control_value)
{
    if (control_value == MODBUS_LED_ON_VALUE)
    {
        /*
         * PC13低电平点亮。
         */
        HAL_GPIO_WritePin(
            STATUS_LED_GPIO_Port,
            STATUS_LED_Pin,
            GPIO_PIN_RESET);
    }
    else
    {
        /*
         * PC13高电平熄灭。
         */
        HAL_GPIO_WritePin(
            STATUS_LED_GPIO_Port,
            STATUS_LED_Pin,
            GPIO_PIN_SET);
    }


    /*
     * 控制后立即读取真实引脚状态。
     */
    Modbus_UpdateLedActualState();

    Modbus_UpdateDeviceStatus();
}


/*
 * 生成标准异常响应。
 */
static uint16_t Modbus_BuildExceptionResponse(
    uint8_t function_code,
    uint8_t exception_code,
    uint8_t *response,
    uint16_t response_max_length)
{
    uint16_t crc;


    if ((response == NULL) ||
        (response_max_length < 5U))
    {
        return 0U;
    }


    response[0] = MODBUS_SLAVE_ADDRESS;

    response[1] =
        (uint8_t)(function_code | 0x80U);

    response[2] = exception_code;


    crc = Modbus_CRC16(response, 3U);


    response[3] =
        (uint8_t)(crc & 0x00FFU);

    response[4] =
        (uint8_t)(crc >> 8U);


    /*
     * 成功生成异常响应后，
     * 异常计数加1。
     */
    /*
		 * 异常计数和设备状态属于共享寄存器，
		 * 修改期间禁止任务切换。
		 */
		taskENTER_CRITICAL();


		Modbus_IncrementRegister(
				MODBUS_REG_EXCEPTION_COUNT);


		Modbus_UpdateDeviceStatus();


		taskEXIT_CRITICAL();


    return 5U;
}


void Modbus_SlaveInit(void)
{
    uint16_t register_index;


    /*
     * 所有寄存器清零。
     */
    for (register_index = 0U;
         register_index <
         MODBUS_HOLDING_REGISTER_COUNT;
         register_index++)
    {
        g_holding_registers[
            register_index
        ] = 0U;
    }


    /*
     * LED控制寄存器默认设为0。
     */
    g_holding_registers[
        MODBUS_REG_LED_CONTROL
    ] = MODBUS_LED_OFF_VALUE;


    /*
     * 关闭LED并同步实际状态。
     */
    Modbus_ApplyLedControl(
        MODBUS_LED_OFF_VALUE);


    /*
     * 更新第一组动态数据。
     */
    Modbus_UpdateDeviceData();
}


void Modbus_UpdateDeviceData(void)
{
    uint32_t uptime_seconds;


    /*
     * 获取系统启动后的运行秒数。
     *
     * 这一步暂时没有访问共享寄存器，
     * 所以不需要进入临界区。
     */
    uptime_seconds =
        HAL_GetTick() / 1000U;


    /*
     * 下面会连续更新多个共享寄存器。
     *
     * 更新完成前禁止FreeRTOS切换到其他任务，
     * 防止Modbus读取到一半新、一半旧的数据。
     */
    taskENTER_CRITICAL();


    g_holding_registers[
        MODBUS_REG_UPTIME_SECONDS
    ] =
        (uint16_t)(
            uptime_seconds & 0xFFFFU
        );


    Modbus_UpdateLedActualState();


    Modbus_UpdateDeviceStatus();


    taskEXIT_CRITICAL();
}


void Modbus_RecordValidFrame(
    uint8_t function_code)
{
    /*
     * 有效帧计数、最近功能码和设备状态
     * 必须作为一组完整更新。
     */
    taskENTER_CRITICAL();


    Modbus_IncrementRegister(
        MODBUS_REG_VALID_FRAME_COUNT);


    g_holding_registers[
        MODBUS_REG_LAST_FUNCTION_CODE
    ] = (uint16_t)function_code;


    Modbus_UpdateDeviceStatus();


    taskEXIT_CRITICAL();
}


void Modbus_RecordCrcError(void)
{
    /*
     * CRC错误计数和设备状态
     * 必须作为一组完整更新。
     */
    taskENTER_CRITICAL();


    Modbus_IncrementRegister(
        MODBUS_REG_CRC_ERROR_COUNT);


    Modbus_UpdateDeviceStatus();


    taskEXIT_CRITICAL();
}


uint16_t Modbus_SlaveProcessRequest(
    const uint8_t *request,
    uint16_t request_length,
    uint8_t *response,
    uint16_t response_max_length)
{
    uint8_t function_code;

    uint16_t start_address;
    uint16_t register_quantity;

    uint16_t register_address;
    uint16_t register_value;
    uint16_t register_index;

    uint16_t response_data_length;
    uint16_t response_total_length;

    uint16_t crc;


    if ((request == NULL) ||
        (response == NULL))
    {
        return 0U;
    }


    if (request_length < 2U)
    {
        return 0U;
    }


    /*
     * 不是发给地址1的报文，
     * 本从机保持沉默。
     */
    if (request[0] != MODBUS_SLAVE_ADDRESS)
    {
        return 0U;
    }


    function_code = request[1];


    switch (function_code)
    {
        /*
         * 功能码0x03：
         * 读取保持寄存器。
         */
        case MODBUS_FUNCTION_READ_HOLDING_REGISTERS:
        {
            if (request_length != 8U)
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                    response,
                    response_max_length);
            }


            start_address =
                ((uint16_t)request[2] << 8U) |
                (uint16_t)request[3];


            register_quantity =
                ((uint16_t)request[4] << 8U) |
                (uint16_t)request[5];


            if ((register_quantity == 0U) ||
                (register_quantity >
                 MODBUS_MAX_READ_REGISTER_QUANTITY))
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                    response,
                    response_max_length);
            }


            if (start_address >=
                MODBUS_HOLDING_REGISTER_COUNT)
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                    response,
                    response_max_length);
            }


            if (register_quantity >
                (MODBUS_HOLDING_REGISTER_COUNT -
                 start_address))
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                    response,
                    response_max_length);
            }


            response_data_length =
                3U + (register_quantity * 2U);


            response_total_length =
                response_data_length + 2U;


            if (response_total_length >
                response_max_length)
            {
                return 0U;
            }


            response[0] =
                MODBUS_SLAVE_ADDRESS;

            response[1] =
                MODBUS_FUNCTION_READ_HOLDING_REGISTERS;

            response[2] =
                (uint8_t)(register_quantity * 2U);


            /*
						 * 在复制所有寄存器期间禁止任务切换。
						 *
						 * 这样一条Modbus响应中的8个寄存器，
						 * 来自同一个完整的数据时刻。
						 */
						taskENTER_CRITICAL();


						for (register_index = 0U;
								 register_index <
								 register_quantity;
								 register_index++)
						{
								register_value =
										g_holding_registers[
												start_address +
												register_index
										];


								/*
								 * 寄存器数据高字节在前。
								 */
								response[
										3U + register_index * 2U
								] =
										(uint8_t)(
												register_value >> 8U
										);


								/*
								 * 寄存器数据低字节在后。
								 */
								response[
										4U + register_index * 2U
								] =
										(uint8_t)(
												register_value & 0x00FFU
										);
						}


						taskEXIT_CRITICAL();


            crc =
                Modbus_CRC16(
                    response,
                    response_data_length);


            /*
             * CRC低字节在前。
             */
            response[response_data_length] =
                (uint8_t)(crc & 0x00FFU);

            response[
                response_data_length + 1U
            ] =
                (uint8_t)(crc >> 8U);


            return response_total_length;
        }


        /*
         * 功能码0x06：
         * 写单个保持寄存器。
         */
        case MODBUS_FUNCTION_WRITE_SINGLE_REGISTER:
        {
            if (request_length != 8U)
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                    response,
                    response_max_length);
            }


            register_address =
                ((uint16_t)request[2] << 8U) |
                (uint16_t)request[3];


            register_value =
                ((uint16_t)request[4] << 8U) |
                (uint16_t)request[5];


            if (register_address >=
                MODBUS_HOLDING_REGISTER_COUNT)
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                    response,
                    response_max_length);
            }


            /*
             * 只有寄存器3允许写入。
             */
            if (register_address !=
                MODBUS_REG_LED_CONTROL)
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS,
                    response,
                    response_max_length);
            }


            /*
             * LED只允许写0或1。
             */
            if ((register_value !=
                 MODBUS_LED_OFF_VALUE) &&
                (register_value !=
                 MODBUS_LED_ON_VALUE))
            {
                return Modbus_BuildExceptionResponse(
                    function_code,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE,
                    response,
                    response_max_length);
            }


            /*
						 * LED控制值、LED实际状态和设备状态
						 * 必须作为一组完整更新。
						 */
						taskENTER_CRITICAL();


						g_holding_registers[
								MODBUS_REG_LED_CONTROL
						] = register_value;


						Modbus_ApplyLedControl(
								register_value);


						taskEXIT_CRITICAL();


            if (response_max_length < 8U)
            {
                return 0U;
            }


            /*
             * 功能码06成功响应：
             * 原样回显前6字节。
             */
            response[0] = request[0];
            response[1] = request[1];
            response[2] = request[2];
            response[3] = request[3];
            response[4] = request[4];
            response[5] = request[5];


            crc =
                Modbus_CRC16(response, 6U);


            response[6] =
                (uint8_t)(crc & 0x00FFU);

            response[7] =
                (uint8_t)(crc >> 8U);


            return 8U;
        }


        /*
         * 其他功能码不支持。
         */
        default:
        {
            return Modbus_BuildExceptionResponse(
                function_code,
                MODBUS_EXCEPTION_ILLEGAL_FUNCTION,
                response,
                response_max_length);
        }
    }
}