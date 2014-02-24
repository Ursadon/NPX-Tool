#include <stm32f10x.h>
#include <stm32f10x_usart.h>

void usartSendChr(uint16_t data) {
//	int j = 0;
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
		;
	USART_SendData(USART1, data);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
		;
//	for (j = 0; j < 24000; ++j) {
//		asm("nop");
//	}
}
void KWP_send(unsigned char *s)
{
	unsigned char ch;
  	while(*s!='\0')
	{
		ch = *s;
		usartSendChr(ch);
		s++;
	}
  	//need_ack = 1;
}
