#ifndef LIGHTBENCH_TRANSPORT_TEST_HAL_H
#define LIGHTBENCH_TRANSPORT_TEST_HAL_H
#include <stdint.h>
typedef struct { volatile uint32_t CR1,CR2,ISR,ICR,TXDR,RXDR; } I2C_TypeDef;
typedef struct { volatile uint32_t LOAD,VAL; } SysTick_Type;
extern I2C_TypeDef test_i2c;
extern SysTick_Type test_systick;
extern uint32_t SystemCoreClock;
#define I2C1 (&test_i2c)
#define SysTick (&test_systick)
#define I2C_CR1_PE (1u<<0)
#define I2C_CR2_RD_WRN (1u<<10)
#define I2C_CR2_START (1u<<13)
#define I2C_CR2_STOP (1u<<14)
#define I2C_CR2_AUTOEND (1u<<25)
#define I2C_ISR_TXIS (1u<<1)
#define I2C_ISR_RXNE (1u<<2)
#define I2C_ISR_NACKF (1u<<4)
#define I2C_ISR_STOPF (1u<<5)
#define I2C_ISR_BERR (1u<<8)
#define I2C_ISR_ARLO (1u<<9)
#define I2C_ISR_OVR (1u<<10)
#define I2C_ISR_BUSY (1u<<15)
#define I2C_ICR_NACKCF I2C_ISR_NACKF
#define I2C_ICR_STOPCF I2C_ISR_STOPF
#define I2C_ICR_BERRCF I2C_ISR_BERR
#define I2C_ICR_ARLOCF I2C_ISR_ARLO
#define I2C_ICR_OVRCF I2C_ISR_OVR
uint32_t HAL_GetTick(void);
#endif
