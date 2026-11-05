#ifdef LAB4
#include "measurement.h"
#include "main.h"
extern uint32_t _estack, _Min_Stack_Size;
/* Internal metrics are intentionally1ms. IndependentGPIO/VCD supplies fine timing.
 * Shared high-resolution extension failed the bounded reentrancy probe. */
void TIM14_IRQHandler(void) { TIM14->SR=0; HAL_IncTick();
#ifdef LAB4_DIAGNOSTIC
 extern void lab4_test_isr(void);
 static unsigned count; if(++count==50){count=0;lab4_test_isr();}
#endif
}
void measurement_init(void) {
 __HAL_RCC_TIM14_CLK_ENABLE(); TIM14->PSC=47; TIM14->ARR=999; TIM14->EGR=1; TIM14->SR=0;
 TIM14->DIER=TIM_DIER_UIE; HAL_NVIC_SetPriority(TIM14_IRQn,2,0); HAL_NVIC_EnableIRQ(TIM14_IRQn); TIM14->CR1=1;
 SysTick->LOAD=SystemCoreClock/1000u-1u; SysTick->VAL=0;
 SysTick->CTRL=SysTick_CTRL_ENABLE_Msk|SysTick_CTRL_CLKSOURCE_Msk;
}
uint32_t measurement_us(void) { return HAL_GetTick()*1000u; }
uint32_t measurement_ms(void) { return HAL_GetTick(); }
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

#else
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

#endif
