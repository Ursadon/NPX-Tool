#include <stm32f10x.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_flash.h>
#include <stm32f10x_usart.h>
#include <string.h> /* memset */
#include "hd44780\hd44780_driver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "KWP.h"
#include "functions.h"

#define USART_BUFFER_SIZE 256
int8_t temp_coolant = 0;
uint8_t unread_packet = 0;
uint8_t KWP_answer[USART_BUFFER_SIZE];

void vScanKline(void *pvParameters) {
	uint8_t usart_bufer[USART_BUFFER_SIZE];
	uint8_t pos = Fmt;
	for (;;) {
		if ((USART1->SR & USART_FLAG_RXNE) != (u16) RESET) {
			usart_bufer[pos] = USART_ReceiveData(USART1);
			pos++;
			if(pos == USART_BUFFER_SIZE){ // Что-то пошло не так "ERR: KLINE BUFFER OVERLOAD"
				memset(&usart_bufer[0], 0, sizeof(usart_bufer));
				pos = Fmt;
			} else if ((usart_bufer[Fmt] == 0x80) && (pos == usart_bufer[Sld] + 1)) { // 4х байтный заголовок
				unread_packet = 4;
				pos = Fmt;
				memcpy(KWP_answer, usart_bufer, USART_BUFFER_SIZE);
				memset(&usart_bufer[0], 0, sizeof(usart_bufer));
			} else if((usart_bufer[Fmt] != 0x80) && (pos == (usart_bufer[Fmt] - 0x80 + 1))) { // 3x байтный заголовок
				unread_packet = 3;
				pos = Fmt;
				memcpy(KWP_answer, usart_bufer, USART_BUFFER_SIZE);
				memset(&usart_bufer[0], 0, sizeof(usart_bufer));
			}
		}
		taskYIELD();
	}
}

void SetSysClockTo24(void) {
	ErrorStatus HSEStartUpStatus;
	/* SYSCLK, HCLK, PCLK2 and PCLK1 configuration -----------------------------*/
	/* RCC system reset(for debug purpose) */
	RCC_DeInit();

	/* Enable HSE */
	RCC_HSEConfig(RCC_HSE_ON);

	/* Wait till HSE is ready */
	HSEStartUpStatus = RCC_WaitForHSEStartUp();

	if (HSEStartUpStatus == SUCCESS) {
		/* Enable Prefetch Buffer */
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
		/* Flash 0 wait state */
		FLASH_SetLatency(FLASH_Latency_0);
		/* HCLK = SYSCLK */
		RCC_HCLKConfig(RCC_SYSCLK_Div1);
		/* PCLK2 = HCLK */
		RCC_PCLK2Config(RCC_HCLK_Div1);
		/* PCLK1 = HCLK */
		RCC_PCLK1Config(RCC_HCLK_Div1);
		/* PLLCLK = 8MHz * 3 = 24 MHz */
		RCC_PLLConfig(0x00010000, 0x00040000);
		/* Enable PLL */
		RCC_PLLCmd(ENABLE);
		/* Wait till PLL is ready */
		while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET) {
		}
		/* Select PLL as system clock source */
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		/* Wait till PLL is used as system clock source */
		while (RCC_GetSYSCLKSource() != 0x08) {
		}
	} else { /* If HSE fails to start-up, the application will have wrong clock configuration.
	 User can add here some code to deal with this error */
		/* Go to infinite loop */
		while (1) {
		}
	}
}
void vLCD(void *pvParameters) {
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); //Вкл порт С
	lcd_init(); //Инициализируем дисплей
	lcd_set_state(LCD_ENABLE, CURSOR_DISABLE, NO_BLINK); //Включаем курсор и мигалку
	lcd_out("LOADING...");
	char Buffer[3];
	for (;;) {
		while(!KWP_initialized) {
			taskYIELD();
		}
		while(KWP_initialized) {
			// Get coolant temp
			KWP_send(readDataByLocalIdentifier);
			lcd_clear();
			lcd_out("t=");
			itoa( temp_coolant, Buffer );
			lcd_out(Buffer);
			vTaskDelay(1000);
		}

	}
}
void vLed(void *pvParameters) {
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	for (;;) {
		GPIO_SetBits(GPIOC, GPIO_Pin_8);
		vTaskDelay(1000);
		GPIO_SetBits(GPIOC, GPIO_Pin_9);
		vTaskDelay(1000);
		GPIO_ResetBits(GPIOC, GPIO_Pin_8);
		vTaskDelay(1000);
		GPIO_ResetBits(GPIOC, GPIO_Pin_9);
		vTaskDelay(1000);
	}
}
void vScanButton(void *pvParameters) {
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	for (;;) {
		if (GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)) {
			while(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0)) {
				taskYIELD();
			}
			KWP_initialized = !KWP_initialized;
		}
		taskYIELD();
	}
}
void vKWP_parser(void *pvParameters) {
	for (;;) {
		if(unread_packet == header_3_byte) {
			switch (KWP_answer[Sld]) {
				case 0xC1: // startCommunication - ACK
					if (KWP_answer[KWP_DATA_OFFSET + 0] == 0x6B && KWP_answer[KWP_DATA_OFFSET + 1] == 0x8F) {
						KWP_initialized = 1;
					} else {
						// Что-то пошло не так "ERR: startCommunication NACK"
					}
					break;
				default:
					// Что-то пошло не так "ERR: unknown response from ECU"
					break;
			}
			memset(&KWP_answer[0], 0, sizeof(KWP_answer));
		} else if(unread_packet == header_4_byte) {
			switch (KWP_answer[Sld + 1]) {
				case 0x61: // readDataByLocalIdentifier - ACK
					if (KWP_answer[KWP_DATA_OFFSET + 1 + 0] == 0x01) {
						temp_coolant = KWP_answer[KWP_DATA_OFFSET + 1 + 10] - 40;
					} else {
						// Что-то пошло не так "ERR: startCommunication NACK"
					}
					break;
				default:
					// Что-то пошло не так "ERR: unknown response from ECU"
					break;
			}
			memset(&KWP_answer[0], 0, sizeof(KWP_answer));
		}
		taskYIELD();
	}
}
void vKWP(void *pvParameters) {
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 10400;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	//USART_Init(USART1, &USART_InitStructure);
	//USART_Cmd(USART1, ENABLE);


	for (;;) {
		while(!KWP_initialized) {
			lcd_clear();
			lcd_out("Initialization...");

//			USART_DeInit(USART1);
//			USART_InitStructure.USART_BaudRate = 400;
//			USART_InitStructure.USART_Parity = USART_Parity_Odd;
//			USART_Init(USART1, &USART_InitStructure);
//			USART_Cmd(USART1, ENABLE);
//
//			usartSendChr(0x00);
//
//			USART_DeInit(USART1);
//			USART_InitStructure.USART_BaudRate = 10400;
//			USART_InitStructure.USART_Parity = USART_Parity_No;
//			USART_Init(USART1, &USART_InitStructure);
//			USART_Cmd(USART1, ENABLE);
			USART_DeInit(USART1);
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
			GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
			GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
			GPIO_Init(GPIOA, &GPIO_InitStructure);

			GPIO_SetBits(GPIOA, GPIO_Pin_9);
			vTaskDelay(300);
			GPIO_ResetBits(GPIOA, GPIO_Pin_9);
			vTaskDelay(25);
			GPIO_SetBits(GPIOA, GPIO_Pin_9);
			vTaskDelay(25);
			GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
			GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
			GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
			GPIO_Init(GPIOA, &GPIO_InitStructure);

			USART_Init(USART1, &USART_InitStructure);
			USART_Cmd(USART1, ENABLE);

			KWP_send(startCommunication);
			vTaskDelay(300);
		}
		while(KWP_initialized) {
			// Get coolant temp
			KWP_send(readDataByLocalIdentifier);
			vTaskDelay(300);
		}
		taskYIELD();
	}
}
int main(void)
{
	SetSysClockTo24();
	xTaskCreate( vLed, ( signed char * ) "vLed",
			configMINIMAL_STACK_SIZE, NULL, 0, ( xTaskHandle * ) NULL);
	xTaskCreate( vKWP, ( signed char * ) "vKWP",
			configMINIMAL_STACK_SIZE, NULL, 0, ( xTaskHandle * ) NULL);
	xTaskCreate( vScanKline, ( signed char * ) "vScanKline",
			configMINIMAL_STACK_SIZE, NULL, 0, ( xTaskHandle * ) NULL);
	xTaskCreate( vScanButton, ( signed char * ) "vScanButton",
			configMINIMAL_STACK_SIZE, NULL, 0, ( xTaskHandle * ) NULL);
	xTaskCreate( vKWP_parser, ( signed char * ) "vKWP_parser",
			configMINIMAL_STACK_SIZE, NULL, 0, ( xTaskHandle * ) NULL);
	xTaskCreate( vLCD, ( signed char * ) "vLCD",
			configMINIMAL_STACK_SIZE, NULL, 0, ( xTaskHandle * ) NULL);
	vTaskStartScheduler();
	return 0;
}
