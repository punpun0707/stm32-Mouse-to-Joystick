/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 *****************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_customhid.h"
#include "tm_stm32_usb_hid_device.h"
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
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */
 
/* USER CODE BEGIN PFP */
#ifdef __GNUC__                                    //重定向printf 到usart，保证测试输出
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f) // for printf
#endif 
PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart1 , (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
/* USER CODE END PFP */
/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;
extern TM_USB_HIDDEVICE_Gamepad_t Gamepad1;
extern TM_USB_HIDDEVICE_DualShock4_t DS4_1; // 手柄HID描述
extern TM_USB_HIDDEVICE_Status_t TM_USB_HIDDEVICE_INT_Status;

uint8_t buffer[10] = {0};
uint8_t buf[10] = {3, 5, 7, 9, 11, 13, 15, 19};
uint8_t rx_buf[12];
uint8_t tx_buf[] = {0x57, 0xAB, 0x12, 0x00,0x00,0x00,0x00,0xFF,0x80,0x00,0x20}; 
/*此为第一个bug点.初始化后立即清除ch9350陈旧请求报文，防止报文对指令产生干扰.
0	0x57	帧头标志 1，表示数据包开始。
1	0xAB	帧头标志 2，表示数据包的同步信号。
2	0x12	命令字节，用于指定清除报文的具体操作。
3-6	0x00	数据字段，默认状态字节。
7	0xFF	状态字段，可能表示目标设备应进入的清除状态。
8	0x80	参数字段，可能用于控制清除动作的具体细节。
9	0x00	保留字段或附加控制参数。
10	0x20	结束标志或校验字段，标识数据包完成。*/
int16_t angle, u8angle;
uint8_t init_done = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void); //系统时钟配置
/* USER CODE BEGIN PFP */
void Key_Scan(void);
void D_PAD(TM_USB_HIDDEVICE_DualShock4_t *p);
void Systick_isr(void); //时钟中断
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

usartRecvType_t usart2Recv = {0};

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  SysTick_Config(72000000 / 5000); //72Mhz / 50000 = 144000
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init(); //GPIO配置
  MX_DMA_Init(); //DMA配置
  MX_USB_DEVICE_Init(); //USB设备
  MX_USART1_UART_Init(); //调试串口
  MX_USART2_UART_Init(); //ch9350串口
  /* USER CODE BEGIN 2 */
  TM_USB_HIDEVICE_DualShock4_StructInit(&DS4_1); //自定义模拟USB HID配置
  /* USER CODE END 2 */
	printf("hello world!\r\n");
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	//HAL_UART_Receive_IT(&huart2, (uint8_t *)&usart2Recv.recvData, 1);
	__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE); //串口空闲中断 (UART_IT_IDLE) 以检测数据帧的结束，结束后传输数据。
	HAL_UART_Receive_DMA(&huart2, (uint8_t*)usart2Recv.recvBuff, BUFF_MAX_SIZE);  //采用DMA传输，从 CH9350 的串口接收数据到缓冲区 usart2Recv.recvBuff
	HAL_UART_Transmit(&huart2, tx_buf, sizeof(tx_buf), 100); //发送响应指令
  while (1)
  {
    /* USER CODE END WHILE */
		//串口接收指令并处理
		if(usart2Recv.recvFlag)
    {
				for(uint8_t i = 0;i < 11;i++)
				{
					printf("%X ",usart2Recv.recvBuff[i]); //打印，确保数据正常格式输出
				}
				printf("\r\n");
				if(usart2Recv.recvBuff[0] == 0x57 && usart2Recv.recvBuff[1] == 0xAB)
				{
					if(usart2Recv.recvBuff[2] == 0x88) //功能码
					{
						usart2Recv.recvBuff[5] == 1 ? (DS4_1.L1 = 1): (DS4_1.L1 = 0); //检测左右键按键
						usart2Recv.recvBuff[5] == 2 ? (DS4_1.R1 = 1): (DS4_1.R1 = 0);
						usart2Recv.recvBuff[6] == 1 ? (DS4_1.LeftYAxis=127): (DS4_1.LeftYAxis=0); 
						usart2Recv.recvBuff[7] == 1 ? (DS4_1.LeftYAxis=-127): (DS4_1.LeftYAxis=0); 
						usart2Recv.recvBuff[8] == 1 ? (DS4_1.LeftXAxis=127): (DS4_1.LeftYAxis=0); 
						usart2Recv.recvBuff[9] == 1 ? (DS4_1.LeftXAxis=-127): (DS4_1.LeftYAxis=0); //WASD键
						
						DS4_1.RightXAxis = usart2Recv.recvBuff[6] - 127; //鼠标速度映射为手柄摇杆偏移。因为是相对鼠标，发送的数据是每帧的位移，相当于速度
						DS4_1.RightYAxis = usart2Recv.recvBuff[7] - 127; 
						
					}
				}
        usart2Recv.recvFlag = 0;
				usart2Recv.recvNum = 0;
				memset(usart2Recv.recvBuff,0,BUFF_MAX_SIZE); //清空串口数据
     }
    init_done ++;
    if(init_done > 101)
    init_done = 101;
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

void Systick_isr(void)
{
  if (init_done >= 100)
  TM_USB_HIDDEVICE_DualShock4_Send(0x01, &DS4_1);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
