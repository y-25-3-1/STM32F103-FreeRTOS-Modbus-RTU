/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "rs485.h"
#include <string.h>
#include "modbus_crc.h"
#include <stddef.h>
#include "modbus_slave.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId deviceDataTaskHandle;
/* USER CODE END Variables */
osThreadId SystemTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void Debug_PrintHexFrame(const uint8_t *data,
                                uint16_t length);

void StartDeviceDataTask(
    void const * argument);
/* USER CODE END FunctionPrototypes */

void StartSystemTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of SystemTask */
  osThreadDef(SystemTask, StartSystemTask, osPriorityNormal, 0, 256);
  SystemTaskHandle = osThreadCreate(osThread(SystemTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
	osThreadDef(
    deviceDataTask,
    StartDeviceDataTask,
    osPriorityLow,
    0,
    128);

	deviceDataTaskHandle =
			osThreadCreate(
					osThread(deviceDataTask),
					NULL);
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartSystemTask */
/**
  * @brief  Function implementing the SystemTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSystemTask */
void StartSystemTask(void const * argument)
{
  /* USER CODE BEGIN StartSystemTask */

  const char boot_message[] =
    "\r\n[BOOT] Modbus functions 03/06 slave started\r\n";

  const char self_test_pass_message[] =
      "[CRC SELF TEST] PASS\r\n";

  const char self_test_fail_message[] =
      "[CRC SELF TEST] FAIL\r\n";

  const char rx_prefix[] =
      "[MODBUS RX] ";

  const char tx_prefix[] =
      "[MODBUS TX] ";

  const char crc_ok_message[] =
      "[MODBUS] CRC OK\r\n";

  const char crc_error_message[] =
      "[MODBUS] CRC ERROR\r\n";

  const char frame_too_short_message[] =
      "[MODBUS] FRAME TOO SHORT\r\n";

  const char unsupported_message[] =
      "[MODBUS] REQUEST NOT SUPPORTED\r\n";

  const char tx_ok_message[] =
      "[MODBUS] TX OK\r\n";

  const char tx_error_message[] =
      "[MODBUS] TX ERROR\r\n";


  const uint8_t crc_test_data[] =
  {
      0x01U,
      0x03U,
      0x00U,
      0x00U,
      0x00U,
      0x02U
  };


  uint8_t frame_buffer[RS485_RX_BUFFER_SIZE];

  uint8_t response_buffer[RS485_RX_BUFFER_SIZE];

  uint16_t frame_length;
  uint16_t response_length;
  uint16_t crc_test_result;

  HAL_StatusTypeDef send_status;


  HAL_UART_Transmit(&huart1,
                    (uint8_t *)boot_message,
                    sizeof(boot_message) - 1U,
                    100U);


  crc_test_result =
      Modbus_CRC16(crc_test_data,
                   (uint16_t)sizeof(crc_test_data));


  if (crc_test_result == 0x0BC4U)
  {
    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)self_test_pass_message,
        sizeof(self_test_pass_message) - 1U,
        100U);
  }
  else
  {
    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)self_test_fail_message,
        sizeof(self_test_fail_message) - 1U,
        100U);
  }


  /*
   * 初始化4个保持寄存器。
   */
  Modbus_SlaveInit();


  /*
   * 初始化RS485并启动中断接收。
   */
  RS485_Init();


  for (;;)
  {
    frame_length =
        RS485_ReadFrame(frame_buffer,
                        sizeof(frame_buffer));


    if (frame_length > 0U)
    {
      HAL_UART_Transmit(&huart1,
                        (uint8_t *)rx_prefix,
                        sizeof(rx_prefix) - 1U,
                        100U);


      Debug_PrintHexFrame(frame_buffer,
                          frame_length);


      if (frame_length < 4U)
      {
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)frame_too_short_message,
            sizeof(frame_too_short_message) - 1U,
            100U);
      }
      else if (Modbus_CheckCRC(frame_buffer,
                               frame_length) == 0U)
      {
				Modbus_RecordCrcError();
				
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)crc_error_message,
            sizeof(crc_error_message) - 1U,
            100U);
      }
      else
      {
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)crc_ok_message,
            sizeof(crc_ok_message) - 1U,
            100U);

						
				/*
				 * 只记录发给本机地址1的有效帧。
				 */
				if (frame_buffer[0] ==
						MODBUS_SLAVE_ADDRESS)
				{
						Modbus_RecordValidFrame(
								frame_buffer[1]);
				}
						
						

        /*
         * 解析功能码0x03请求，
         * 并生成合法的Modbus响应。
         */
        response_length =
            Modbus_SlaveProcessRequest(
                frame_buffer,
                frame_length,
                response_buffer,
                sizeof(response_buffer));


        if (response_length > 0U)
        {
          /*
           * 给USB-RS485转换器留出
           * 自动切换发送方向的时间。
           */
          osDelay(20U);


          send_status =
              RS485_Send(response_buffer,
                         response_length,
                         200U);


          /*
           * 在USART1调试窗口打印
           * STM32生成的响应报文。
           */
          HAL_UART_Transmit(
              &huart1,
              (uint8_t *)tx_prefix,
              sizeof(tx_prefix) - 1U,
              100U);


          Debug_PrintHexFrame(
              response_buffer,
              response_length);


          if (send_status == HAL_OK)
          {
            HAL_UART_Transmit(
                &huart1,
                (uint8_t *)tx_ok_message,
                sizeof(tx_ok_message) - 1U,
                100U);
          }
          else
          {
            HAL_UART_Transmit(
                &huart1,
                (uint8_t *)tx_error_message,
                sizeof(tx_error_message) - 1U,
                100U);
          }
        }
        else
        {
          HAL_UART_Transmit(
              &huart1,
              (uint8_t *)unsupported_message,
              sizeof(unsupported_message) - 1U,
              100U);
        }
      }


//      HAL_GPIO_TogglePin(STATUS_LED_GPIO_Port,
//                         STATUS_LED_Pin);
    }


    osDelay(1U);
  }

  /* USER CODE END StartSystemTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void StartDeviceDataTask(
    void const * argument)
{
    (void)argument;


    for (;;)
    {
        /*
         * 每秒更新一次动态寄存器。
         */
        Modbus_UpdateDeviceData();


        osDelay(1000U);
    }
}



/*
 * 把二进制数据以十六进制形式打印到USART1。
 *
 * 例如数组中保存：
 *
 * 0x01 0x03 0x00
 *
 * 窗口1显示：
 *
 * 01 03 00
 */
static void Debug_PrintHexFrame(const uint8_t *data,
                                uint16_t length)
{
    static const char hex_table[] =
        "0123456789ABCDEF";

    const uint8_t line_end[] =
        "\r\n";

    uint8_t text[3];
    uint16_t index;


    if (data == NULL)
    {
        return;
    }


    for (index = 0U;
         index < length;
         index++)
    {
        /*
         * 一个字节由两个十六进制数字显示。
         *
         * 例如0xC4：
         *
         * 高4位是C；
         * 低4位是4。
         */
        text[0] =
            (uint8_t)hex_table[
                (data[index] >> 4U) & 0x0FU
            ];

        text[1] =
            (uint8_t)hex_table[
                data[index] & 0x0FU
            ];

        /*
         * 每个字节后面添加一个空格。
         */
        text[2] = ' ';


        HAL_UART_Transmit(&huart1,
                          text,
                          sizeof(text),
                          100U);
    }


    HAL_UART_Transmit(&huart1,
                      (uint8_t *)line_end,
                      sizeof(line_end) - 1U,
                      100U);
}
/* USER CODE END Application */

