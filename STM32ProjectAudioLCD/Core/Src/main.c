/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "fatfs.h"
#include "i2c.h"
#include "i2s.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "AudioBoard/WM8960.h"
#include "stdio.h"
#include "AudioBoard/Play_WAV.h"
#include "LCD/lcd_driver.h"
#include "Touch/Touch.h"
#include "BMPImages/fatfs_storage.h"
#include "Images/mainImg.c"
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

/* USER CODE BEGIN PV */
char* pDirectoryFiles[MAX_BMP_FILES];
FATFS microSDFatFs;
uint8_t str[20];
uint8_t res;
uint8_t pos;
volatile uint8_t pageNumber=0;
volatile uint8_t playFlag=0;
volatile uint8_t displayImgFlag=0;
uint32_t tick=0;
uint8_t volumeLevel=112;


uint32_t bmplen = 0x00;
uint32_t checkstatus = 0x00;
uint32_t filesnumbers = 0x00;
uint32_t bmpcounter = 0x00;
DIR directory;
FRESULT res;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void display_images(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}

static void display_images_init(void);
/* USER CODE END 0 */

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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2S2_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
  //disable lcd irq
  HAL_NVIC_DisableIRQ(EXTI3_IRQn);
  lcd_init();
  tp_init();
  tp_adjust();

  //enable lcd irq
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* Check the mounted device */
  res = WM89060_Init();
  if(res == 0)  {
	printf("WM89060_Init complete !!\r\n");
  }
  else  {
	printf("WM89060_Init fail ! Error code: %d\r\n", res);
	while(1)  {
	  HAL_Delay(1000);
	}
  }

  if(f_mount(&microSDFatFs, (TCHAR const*)"/", 0) != FR_OK)  {
	printf("f_mount fail ! Error code: %d\r\n");
	while(1);
  }
  else{
	printf("f_mount completed !!\r\n");
	for (int counter = 0; counter < MAX_BMP_FILES; counter++){
	  pDirectoryFiles[counter] = malloc(13);
	}
  }
  ScanWavefiles("0:/music");//Place the WAV file in the music folder

  setRotation(3);
  lcd_draw_image(0);
  display_images_init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (playFlag==1) {
	 		//refresh song name on toolbar
	 		lcd_copy(0, 0, 320, 20, &toolbarImg, sizeof(toolbarImg));
	 	    lcd_display_string(5,1,(const uint8_t *)"Now Play:",FONT_1608,RED);
	 	    lcd_display_string(80,1,(const uint8_t *)Play_List[Music_Num]+9,FONT_1608,RED);


	 		PlayWaveFile();
	 		Music_Num++;
	 		if(Music_Num >= Music_Num_MAX)
	 			Music_Num = 0;
	 	}

	 	  if (displayImgFlag==1) {
	 		display_images();
	 		displayImgFlag=0;
	 	}
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2S|RCC_PERIPHCLK_USART2
                              |RCC_PERIPHCLK_I2C1;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.I2sClockSelection = RCC_I2SCLKSOURCE_SYSCLK;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
static void display_images_init(void)
{
	/* Open directory */
    res= f_opendir(&directory, "/");
    if((res != FR_OK))
    {
      if(res == FR_NO_FILESYSTEM)
      {
        /* Display message: SD card not FAT formated */
        lcd_display_string(0, 32, (const uint8_t *)"SD_CARD_NOT_FORMATTED", 16, RED);

      }
      else
      {
        /* Display message: Fail to open directory */
         lcd_display_string(0, 48, (const uint8_t *)"SD_CARD_OPEN_FAIL", 16, RED);
      }
    }

    /* Get number of bitmap files */
    filesnumbers = Storage_GetDirectoryBitmapFiles ("/", pDirectoryFiles);
	printf("filesnumbers %d",filesnumbers);
    /* Set bitmap counter to display first image */
	f_closedir(&directory);

}

static void display_images(void){

		sprintf((char*)str, "%-11.11s", pDirectoryFiles[bmpcounter -1]);
		HAL_NVIC_DisableIRQ(EXTI3_IRQn);
		Storage_OpenReadFile(0, 0, (const char*)str);
		HAL_NVIC_EnableIRQ(EXTI3_IRQn);

}

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)  {

  I2S_Flag = I2S_Half_Callback;

}


void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  /* Manage the remaining file size and new address offset: */

  /* Check if the end of file has been reached */
	I2S_Flag = I2S_Callback;
  if(WAV_LastData >= WAV_BUFFER_SIZE)  {
    HAL_I2S_Transmit_DMA(&hi2s2,(uint16_t *)WAV_Buffer, WAV_BUFFER_SIZE/2);
  }
  else  {
    Play_Flag = 0;
    End_Flag = 1;
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if (HAL_GetTick()-tick>100) {
	switch (pageNumber) {
		case 0:
			tp_scan(0);
			if (s_tTouch.chStatus & TP_PRESS_DOWN) {
				if (s_tTouch.hwXpos < LCD_WIDTH && s_tTouch.hwYpos < LCD_HEIGHT) {
					if ((s_tTouch.hwYpos > 57 && s_tTouch.hwYpos < 118) && (s_tTouch.hwXpos < 118 && s_tTouch.hwXpos>60)) {
						lcd_copy(203, 60, 74, 58, &pressedphonesImg, sizeof(pressedphonesImg));
						HAL_Delay(100);
						pageNumber=1;
						lcd_draw_image(1);
					}
					else if ((s_tTouch.hwYpos > 210 && s_tTouch.hwYpos < 270) && (s_tTouch.hwXpos < 118 && s_tTouch.hwXpos>60)) {
						bmpcounter=1;
						lcd_copy(50, 60, 59, 58, &pressedphotoImg, sizeof(pressedphotoImg));
						HAL_Delay(100);
						setRotation(0);
						displayImgFlag=1;
						pageNumber=2;
					}
				}
			}
			break;
		case 1:
				//HAL_I2S_DMAPause(&hi2s2);
				tp_scan(0);
				//HAL_I2S_DMAResume(&hi2s2);
				if (s_tTouch.chStatus & TP_PRESS_DOWN) {
					if (s_tTouch.hwXpos < LCD_WIDTH && s_tTouch.hwYpos < LCD_HEIGHT) {
						if ((s_tTouch.hwXpos > 30 && s_tTouch.hwXpos < 55) && (s_tTouch.hwYpos < 315 && s_tTouch.hwYpos>295)) {
							lcd_copy(5, 30, 24, 20, &pressedbackImg, sizeof(pressedbackImg));
							HAL_I2S_DMAStop(&hi2s2);
							pageNumber=0;
							playFlag=0;
							End_Flag=1;
							Play_Flag=0;
							HAL_Delay(100);
							lcd_draw_image(0);
						}
						else if ((s_tTouch.hwXpos > 60 && s_tTouch.hwXpos < 105) && (s_tTouch.hwYpos < 55 && s_tTouch.hwYpos>10)) {
							lcd_copy(266, 60, 44, 44, &pressedplusImg, sizeof(pressedplusImg));
							if (volumeLevel<112) {
								volumeLevel+=4;
								WM8960_Write_Reg(0x02, volumeLevel | 0x0100);  //LOUT1 Volume Set
								WM8960_Write_Reg(0x03, volumeLevel | 0x0100);
							}

							lcd_copy(266, 60, 44, 44, &plusImg, sizeof(plusImg));
						}
						else if ((s_tTouch.hwXpos > 120 && s_tTouch.hwXpos < 165) && (s_tTouch.hwYpos < 55 && s_tTouch.hwYpos>10)) {
							lcd_copy(266, 120, 44, 44, &pressedminusImg, sizeof(pressedminusImg));
							if (volumeLevel>60) {
								volumeLevel-=4;
								WM8960_Write_Reg(0x02, volumeLevel | 0x0100);  //LOUT1 Volume Set
								WM8960_Write_Reg(0x03, volumeLevel | 0x0100);
							}
							lcd_copy(266, 120, 44, 44, &minusImg, sizeof(minusImg));
						}
						else if ((s_tTouch.hwXpos > 180 && s_tTouch.hwXpos < 240) && (s_tTouch.hwYpos < 69 && s_tTouch.hwYpos>11)) {
							lcd_copy(251, 181, 58, 58, &pressedrightArrowImg, sizeof(pressedrightArrowImg));
							Play_Flag=0;
							End_Flag = 1;
							HAL_I2S_DMAStop(&hi2s2);

							HAL_Delay(100);
							lcd_copy(251, 181, 58, 58, &rightArrowImg, sizeof(rightArrowImg));
						}
						else if ((s_tTouch.hwXpos > 180 && s_tTouch.hwXpos < 240) && (s_tTouch.hwYpos < 149 && s_tTouch.hwYpos>91)) {
							lcd_copy(171, 181, 58, 58, &pressedpauseImg, sizeof(pressedpauseImg));
							lcd_copy(91, 181, 58, 58, &playImg, sizeof(playImg));
							if (playFlag==1) {
								HAL_I2S_DMAPause(&hi2s2);
							}
							HAL_Delay(100);
							lcd_copy(171, 181, 58, 58, &pauseImg, sizeof(pauseImg));
						}
						else if ((s_tTouch.hwXpos > 180 && s_tTouch.hwXpos < 240) && (s_tTouch.hwYpos < 229 && s_tTouch.hwYpos>171)) {
							lcd_copy(91, 181, 58, 58, &pressedplayImg, sizeof(pressedplayImg));
							playFlag=1;
							HAL_I2S_DMAResume(&hi2s2);
						}
						else if ((s_tTouch.hwXpos > 180 && s_tTouch.hwXpos < 240) && (s_tTouch.hwYpos < 309 && s_tTouch.hwYpos>251)) {
							lcd_copy(11, 181, 58, 58, &pressedleftArrowImg, sizeof(pressedleftArrowImg));
							Music_Num -= 2;
							if(Music_Num < 0)
								Music_Num += Music_Num_MAX;
							Play_Flag = 0;
							End_Flag = 1;
							HAL_I2S_DMAStop(&hi2s2);

							HAL_Delay(100);
							lcd_copy(11, 181, 58, 58, &leftArrowImg, sizeof(leftArrowImg));
						}
					}
				}
			break;
		case 2:
			tp_scan(0);
			if (s_tTouch.chStatus & TP_PRESS_DOWN) {
				if (s_tTouch.hwXpos < LCD_WIDTH && s_tTouch.hwYpos < LCD_HEIGHT) {
					if ((s_tTouch.hwXpos > 0 && s_tTouch.hwXpos < 120) && (s_tTouch.hwYpos < 220 && s_tTouch.hwYpos>0)) {
						if (bmpcounter>1) {
							bmpcounter--;
							displayImgFlag=1;
						}
						else{
							bmpcounter=filesnumbers;
							displayImgFlag=1;
						}
						//prev
					}
					else if ((s_tTouch.hwXpos > 120 && s_tTouch.hwXpos < 240) && (s_tTouch.hwYpos < 220 && s_tTouch.hwYpos>0)) {
				        if(bmpcounter > filesnumbers)
				        {
				          bmpcounter = 1;
				          displayImgFlag=1;
				        }
				        else{
				        	 bmpcounter++;
				        	 displayImgFlag=1;
				        }
					}
					else if ((s_tTouch.hwXpos > 0 && s_tTouch.hwXpos < 240) && (s_tTouch.hwYpos < 320 && s_tTouch.hwYpos>220)) {
						bmpcounter=1;
						displayImgFlag=0;
						pageNumber=0;
						setRotation(3);
						lcd_draw_image(0);
					}
				}
			}
			break;
		default:
			break;
		}

	tick=HAL_GetTick();
	}
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
