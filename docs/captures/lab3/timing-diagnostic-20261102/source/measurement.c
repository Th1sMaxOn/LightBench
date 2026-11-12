#include "measurement.h"
#include "main.h"

extern uint32_t _estack, _Min_Stack_Size;

/* Diagnostic candidate only: SysTick fraction and pending rollover.
 * Requires independent VCD validation. Flash stalls can delay or lose ticks. */
static uint16_t previous_count;
static uint32_t accumulated_us, previous_ms, clock_gaps;
void measurement_init(void) {
    __HAL_RCC_TIM3_CLK_ENABLE();
    TIM3->CR1=0; TIM3->PSC=SystemCoreClock/1000000u-1u;
    TIM3->ARR=65535u; TIM3->EGR=TIM_EGR_UG; TIM3->SR=0;
    TIM3->DIER=0; TIM3->CR1=TIM_CR1_CEN;
    previous_count=(uint16_t)TIM3->CNT;
    previous_ms=HAL_GetTick(); accumulated_us=0; clock_gaps=0;
}
uint32_t measurement_ms(void) { return HAL_GetTick(); }
uint32_t measurement_us(void) {
    uint16_t now=(uint16_t)TIM3->CNT;
    uint32_t ms=HAL_GetTick();
    if ((uint32_t)(ms-previous_ms)>=60u) ++clock_gaps;
    accumulated_us+=(uint16_t)(now-previous_count);
    previous_count=now; previous_ms=ms;
    return accumulated_us;
}
uint32_t measurement_clock_gaps(void) { return clock_gaps; }
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
