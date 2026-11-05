#ifndef LIGHTBENCH_RTOS_H
#define LIGHTBENCH_RTOS_H
#include <stdint.h>
#include <stdbool.h>
void rtos_start(void);
void rtos_report(void);
void rtos_uart_irq(void);
void rtos_uart_irq_at(uint32_t entered_us);
void rtos_tick(void);
void runtime_adc_task(void *);
void runtime_imu_task(void *);
uint64_t rtos_time64(void);
extern volatile uint32_t rtos_sample_lost;
#endif
