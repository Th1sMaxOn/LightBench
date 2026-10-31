#ifndef LIGHTBENCH_MEASUREMENT_H
#define LIGHTBENCH_MEASUREMENT_H
#include <stdint.h>
void measurement_init(void);
uint32_t measurement_us(void);
uint32_t measurement_ms(void);
void measurement_paint(void);
uint32_t measurement_stack(void);
uint32_t measurement_heap(void);
#endif
