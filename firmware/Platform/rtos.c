#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "rtos.h"
#include "runtime.h"
#include "measurement.h"
#include "board.h"
#include "main.h"
#include <stdio.h>
#ifdef LAB4
#include "text_format.h"
#undef snprintf
#undef vsnprintf
#define snprintf lb_snprintf
#define vsnprintf lb_vsnprintf
#endif
#include <string.h>
void _init(void) {}
static TaskHandle_t owner, adc_task, imu_task;
#if configSUPPORT_STATIC_ALLOCATION
static StaticTask_t owner_control, adc_control, imu_control, idle_control;
static StackType_t owner_stack[384], adc_stack[128], imu_stack[192];
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
void vApplicationGetIdleTaskMemory(StaticTask_t **control, StackType_t **stack,
                                   uint32_t *words) {
    *control=&idle_control; *stack=idle_stack; *words=configMINIMAL_STACK_SIZE;
}
#endif
static volatile bool running;
volatile uint32_t tick_n, tick_sum, tick_max;
static volatile uint32_t irq_at;
static uint32_t irq_n, irq_max;
static TaskStatus_t states[4];
uint64_t rtos_time64(void) {
    /* All required runs are below HAL's49-day wrap; no shared extension. */
    return (uint64_t)HAL_GetTick()*1000u;
}

void rtos_trace(void *task) {
    if(!running) return;
#ifdef LAB4_DIAGNOSTIC
    extern unsigned lab4_trace_code(void *);
    uint32_t code=lab4_trace_code(task);
#else
    uint32_t code=task==(void *)owner?1:task==(void *)adc_task?2:task==(void *)imu_task?3:0;
#endif
    GPIOC->BSRR=(((3u^code)<<6)<<16)|(code<<6);
}
void rtos_tick(void) {
    if(!running) { HAL_IncTick(); return; }
    extern void xPortSysTickHandler(void);
    uint32_t start=measurement_us();
    GPIOB->BSRR=1u<<4;
    xPortSysTickHandler();
    GPIOB->BSRR=1u<<(4+16);
    uint32_t dt=measurement_us()-start; tick_sum+=dt; ++tick_n; if(dt>tick_max)tick_max=dt;
}
void rtos_uart_irq_at(uint32_t entered_us) {
    if(!running||!owner) return;
    /* Keep the first unhandled RX/error timestamp, including when owner is busy. */
    if(!irq_at) irq_at=entered_us;
    /* Owner polls the ring once per OS tick; retain the first RX timestamp. */
}
void rtos_uart_irq(void) { rtos_uart_irq_at(measurement_us()); }
static void app(void *arg) {
    (void)arg;
    for(;;) {
        taskENTER_CRITICAL(); uint32_t entered=irq_at; irq_at=0; taskEXIT_CRITICAL();
        if(entered) { uint32_t dt=measurement_us()-entered; ++irq_n; if(dt>irq_max)irq_max=dt; }
        runtime_poll();
        vTaskDelay(1);
    }
}
static void fatal(const char *kind,const char *detail,int value) {
    __disable_irq();
    char text[96]; int n=snprintf(text,sizeof(text),"FATAL kind=%s detail=%s value=%d ms=%lu\r\n",kind,detail,value,(unsigned long)HAL_GetTick());
    extern UART_HandleTypeDef huart2;
    for(int i=0;i<n&&i<(int)sizeof(text);++i) {
        while(!(huart2.Instance->ISR & USART_ISR_TXE_TXFNF)) {}
        huart2.Instance->TDR=(uint8_t)text[i];
    }
    for(;;) {}
}
void rtos_assert(const char *file,int line) { (void)file; fatal("assert","kernel",line); }
void vApplicationStackOverflowHook(TaskHandle_t task,char *name) { (void)task; fatal("stack",name,0); }
#if configSUPPORT_DYNAMIC_ALLOCATION
void vApplicationMallocFailedHook(void) { fatal("heap","allocation",(int)xPortGetFreeHeapSize()); }
#endif
void rtos_report(void) {
    /* Owner is the only producer. Emit one compact report after the timing window. */
    uint64_t total=0; UBaseType_t n=uxTaskGetSystemState(states,4,&total);
    char text[256];
    for(unsigned i=0;i<n;i++) {
        int size=snprintf(text,sizeof(text),"TASK name=%s runtime_us=%lu free_min_bytes=%u priority=%u\r\n",states[i].pcTaskName,(unsigned long)states[i].ulRunTimeCounter,(unsigned)(states[i].usStackHighWaterMark*4),(unsigned)states[i].uxCurrentPriority);
        board_tx_write((uint8_t *)text,(size_t)size);
    }
    size_t heap_free=0, heap_min=0;
#if configSUPPORT_DYNAMIC_ALLOCATION
    heap_free=xPortGetFreeHeapSize(); heap_min=xPortGetMinimumEverFreeHeapSize();
#endif
    int size=snprintf(text,sizeof(text),"OS total_us=%lu heap_free=%u heap_min=%u heap_total=%u tick_n=%lu tick_sum_us=%lu tick_max_us=%lu irq_n=%lu irq_max_us=%lu sample_lost=%lu\r\n",(unsigned long)total,(unsigned)heap_free,(unsigned)heap_min,(unsigned)configTOTAL_HEAP_SIZE,(unsigned long)tick_n,(unsigned long)tick_sum,(unsigned long)tick_max,(unsigned long)irq_n,(unsigned long)irq_max,(unsigned long)rtos_sample_lost);
    if(size>0&&(size_t)size<sizeof(text))board_tx_write((uint8_t *)text,(size_t)size);
}
void rtos_start(void) {
    GPIO_InitTypeDef gpio={.Pin=GPIO_PIN_6|GPIO_PIN_7,.Mode=GPIO_MODE_OUTPUT_PP};
    __HAL_RCC_GPIOC_CLK_ENABLE(); HAL_GPIO_Init(GPIOC,&gpio);
    __HAL_RCC_GPIOB_CLK_ENABLE(); gpio.Pin=GPIO_PIN_3|GPIO_PIN_4; HAL_GPIO_Init(GPIOB,&gpio);
#if configSUPPORT_STATIC_ALLOCATION
    owner=xTaskCreateStatic(app,"owner",384,NULL,2,owner_stack,&owner_control);
    adc_task=xTaskCreateStatic(runtime_adc_task,"adc",128,NULL,3,adc_stack,&adc_control);
    imu_task=xTaskCreateStatic(runtime_imu_task,"imu",192,NULL,3,imu_stack,&imu_control);
    configASSERT(owner && adc_task && imu_task);
#else
    configASSERT(xTaskCreate(app,"owner",384,NULL,2,&owner)==pdPASS);
    configASSERT(xTaskCreate(runtime_adc_task,"adc",128,NULL,3,&adc_task)==pdPASS);
    configASSERT(xTaskCreate(runtime_imu_task,"imu",192,NULL,3,&imu_task)==pdPASS);
#endif
    HAL_NVIC_SetPriority(USART2_IRQn,3,0);
    running=true; vTaskStartScheduler(); fatal("scheduler","returned",0);
}

#ifdef LAB4_DIAGNOSTIC
void rtos_diagnostic_start(void) {
    GPIO_InitTypeDef gpio={.Pin=GPIO_PIN_6|GPIO_PIN_7,.Mode=GPIO_MODE_OUTPUT_PP};
    __HAL_RCC_GPIOC_CLK_ENABLE(); HAL_GPIO_Init(GPIOC,&gpio);
    __HAL_RCC_GPIOB_CLK_ENABLE(); gpio.Pin=GPIO_PIN_3|GPIO_PIN_4; HAL_GPIO_Init(GPIOB,&gpio);
    running=true;
    extern void lab4_experiment_start(void);lab4_experiment_start();
}
#endif
