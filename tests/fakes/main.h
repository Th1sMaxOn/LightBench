#ifndef TEST_HAL_MAIN_H
#define TEST_HAL_MAIN_H
#include <stdint.h>
uint32_t HAL_GetTick(void);
typedef struct { int id; } UART_HandleTypeDef;
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);
void __DMB(void);
#endif
