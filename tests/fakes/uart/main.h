#ifndef UART_REGISTER_TEST_MAIN_H
#define UART_REGISTER_TEST_MAIN_H
#include <stdint.h>
typedef struct { volatile uint32_t CR1, CR3, ISR, ICR, RDR, TDR; } USART_TypeDef;
typedef struct { USART_TypeDef *Instance; } UART_HandleTypeDef;
#define USART_ISR_PE (1u<<0)
#define USART_ISR_FE (1u<<1)
#define USART_ISR_NE (1u<<2)
#define USART_ISR_ORE (1u<<3)
#define USART_ISR_RXNE_RXFNE (1u<<5)
#define USART_ISR_TXE_TXFNF (1u<<7)
#define USART_CR1_RXNEIE_RXFNEIE (1u<<5)
#define USART_CR1_TXEIE_TXFNFIE (1u<<7)
#define USART_ICR_PECF (1u<<0)
#define USART_ICR_FECF (1u<<1)
#define USART_ICR_NECF (1u<<2)
#define USART_ICR_ORECF (1u<<3)
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);
void __DMB(void);
#endif
