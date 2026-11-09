#include "board.h"
#include "main.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static USART_TypeDef device;
UART_HandleTypeDef huart2={&device};
static uint32_t mask;
static uint8_t received;
static unsigned rx_count, errors;
uint32_t __get_PRIMASK(void) {return mask;}
void __disable_irq(void) {mask=1;}
void __enable_irq(void) {mask=0;}
void __DMB(void) {}
void board_uart_rx_byte(uint8_t byte) {received=byte;++rx_count;}
void board_uart_rx_error(void) {++errors;}
static unsigned drain(uint8_t *out) {
 unsigned n=0;
 while(device.CR1 & USART_CR1_TXEIE_TXFNFIE) {
  unsigned before=board_tx_free();device.ISR=USART_ISR_TXE_TXFNF;
  assert(!board_uart_irq());
  if(board_tx_free()!=before)out[n++]=(uint8_t)device.TDR;
 }
 return n;
}
int main(void) {
 uint8_t input[1023],out[2048];
 for(unsigned i=0;i<sizeof(input);i++)input[i]=(uint8_t)(i*31u);
 assert(board_tx_free()==1023);
 for(unsigned round=0;round<20;round++) {
  assert(board_tx_write(input,1000));assert(!board_tx_write(input,24));assert(board_tx_free()==23);
  assert(!mask);mask=1;board_tx_service();assert(mask==1);mask=0;
  unsigned n=drain(out);assert(n==1000&&!memcmp(input,out,n));assert(board_tx_free()==1023);
 }
 assert(board_tx_write(input,sizeof(input)));assert(!board_tx_write(input,1));
 assert(drain(out)==sizeof(input)&&!memcmp(out,input,sizeof(input)));
 device.CR1=USART_CR1_RXNEIE_RXFNEIE;device.ISR=USART_ISR_RXNE_RXFNE;device.RDR=0x1d5;
 assert(board_uart_irq()&&received==0xd5&&rx_count==1);
 for(unsigned bit=0;bit<4;bit++) {
  device.ISR=1u<<bit;assert(board_uart_irq());assert(device.ICR==15);
 }
 assert(errors==4);
 device.ISR=0;assert(!board_uart_irq());
 assert(!board_tx_overflow());
 puts("USART2 register adapter: 21023 TX bytes, wrap, atomic reject, IRQ masking, RX and error flags passed");
}
