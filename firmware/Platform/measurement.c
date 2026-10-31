#include "measurement.h"
#include "main.h"

extern uint32_t _estack, _Min_Stack_Size;

/* SysTick is the HAL's 1 ms timebase. Microsecond values are scaled milliseconds,
 * not microsecond-resolution measurements. A flash stall can delay SysTick. */
void measurement_init(void) {}
uint32_t measurement_ms(void) { return HAL_GetTick(); }
uint32_t measurement_us(void) { return HAL_GetTick() * 1000u; }

void measurement_paint(void) {
    uint32_t *bottom=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    uint32_t *sp=(uint32_t *)__get_MSP();
    for (uint32_t *p=bottom;p<sp-16;++p) *p=0xdeadbeefu;
}
uint32_t measurement_stack(void) {
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    while (p<&_estack && *p==0xdeadbeefu) ++p;
    return (uint32_t)((uintptr_t)&_estack-(uintptr_t)p);
}
