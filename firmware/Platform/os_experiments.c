#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "measurement.h"
#include "board.h"
#include "main.h"
#include "text_format.h"
#include <stdarg.h>
#include <stdint.h>
static TaskHandle_t high, medium, low;
static SemaphoreHandle_t mutex, binary;
static QueueHandle_t queue;
static volatile unsigned mode;
static volatile uint32_t notify_start, isr_start, low_runs, mid_runs;
static TaskStatus_t states[4];
extern UART_HandleTypeDef huart2;
extern volatile uint32_t tick_n, tick_sum, tick_max;
unsigned lab4_trace_code(void *task) {
 return task==(void *)high?3u:task==(void *)medium?2u:task==(void *)low?1u:0u;
}
static void emit(const char *fmt,...) {
 char text[192];va_list args;va_start(args,fmt);int n=lb_vsnprintf(text,sizeof(text),fmt,args);va_end(args);
 if(n>0&&(unsigned)n<sizeof(text))HAL_UART_Transmit(&huart2,(uint8_t *)text,(uint16_t)n,1000);
}
static void work(uint32_t us) {uint32_t start=measurement_us();while((uint32_t)(measurement_us()-start)<us){} }
void lab4_test_isr(void) {
 if(mode==4 && high) {GPIOB->BSRR=1u<<3;isr_start=measurement_us();BaseType_t wake=pdFALSE;vTaskNotifyGiveFromISR(high,&wake);portYIELD_FROM_ISR(wake);}
}
static void low_task(void *arg) {
 (void)arg;
 for(;;) {
  ulTaskNotifyTake(pdTRUE,portMAX_DELAY);++low_runs;
  if(mode==1||mode==2) {
   SemaphoreHandle_t sem=mode==1?mutex:binary;
   xSemaphoreTake(sem,portMAX_DELAY);xTaskNotifyGive(high);work(10000);xSemaphoreGive(sem);
  } else if(mode==3) {GPIOB->BSRR=1u<<3;notify_start=measurement_us();xTaskNotifyGive(high);}
 }
}
static void mid_task(void *arg) {(void)arg;for(;;){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);++mid_runs;work(40000);}}
static void high_task(void *arg) {
 (void)arg;vTaskDelay(pdMS_TO_TICKS(20));
 for(mode=1;mode<=2;mode++)for(unsigned i=0;i<30;i++) {
  xTaskNotifyGive(low);ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
  xTaskNotifyGive(medium);uint32_t start=measurement_us();
  SemaphoreHandle_t sem=mode==1?mutex:binary;
  xSemaphoreTake(sem,portMAX_DELAY);uint32_t delay=measurement_us()-start;xSemaphoreGive(sem);
  emit("INV kind=%s n=%u delay_us=%lu\r\n",mode==1?"mutex":"binary",i,(unsigned long)delay);
  vTaskDelay(pdMS_TO_TICKS(60));
 }
 mode=3;
 for(unsigned i=0;i<40;i++) {xTaskNotifyGive(low);ulTaskNotifyTake(pdTRUE,portMAX_DELAY);GPIOB->BSRR=1u<<(3+16);uint32_t dt=measurement_us()-notify_start;emit("CTX n=%u us=%lu\r\n",i,(unsigned long)dt);vTaskDelay(pdMS_TO_TICKS(5));}
 mode=4;
 for(unsigned i=0;i<40;i++){ulTaskNotifyTake(pdTRUE,portMAX_DELAY);GPIOB->BSRR=1u<<(3+16);uint32_t dt=measurement_us()-isr_start;emit("ISR_TASK n=%u us=%lu\r\n",i,(unsigned long)dt);}
 mode=5;
 uint32_t before_low=low_runs,before_mid=mid_runs;uint64_t total;uxTaskGetSystemState(states,4,&total);
 uint64_t idle_before=0;for(unsigned i=0;i<4;i++)if(states[i].uxCurrentPriority==0)idle_before=states[i].ulRunTimeCounter;
 xTaskNotifyGive(low);xTaskNotifyGive(medium);uint32_t start=measurement_us();work(1000000);
 uint32_t dt=measurement_us()-start;uxTaskGetSystemState(states,4,&total);uint64_t idle_after=0;for(unsigned i=0;i<4;i++)if(states[i].uxCurrentPriority==0)idle_after=states[i].ulRunTimeCounter;
 emit("STARVE duration_us=%lu low_delta=%lu mid_delta=%lu idle_delta_us=%lu\r\n",(unsigned long)dt,(unsigned long)(low_runs-before_low),(unsigned long)(mid_runs-before_mid),(unsigned long)(idle_after-idle_before));
 mode=6;vTaskDelay(pdMS_TO_TICKS(60));
 unsigned sent=0,lost=0,value;
 for(value=0;value<100;value++){if(xQueueSend(queue,&value,0)==pdTRUE)++sent;else++lost;}
 unsigned received=0;while(xQueueReceive(queue,&value,0)==pdTRUE)++received;
 uint32_t t0=measurement_us();BaseType_t a=xQueueReceive(queue,&value,pdMS_TO_TICKS(10));
 emit("QUEUE attempted=100 sent=%u lost=%u received=%u timeout_result=%d timeout_us=%lu\r\n",sent,lost,received,(int)a,(unsigned long)(measurement_us()-t0));
 UBaseType_t n=uxTaskGetSystemState(states,4,&total);
 for(unsigned i=0;i<n;i++)emit("DIAG_TASK name=%s runtime_us=%lu free_bytes=%u\r\n",states[i].pcTaskName,(unsigned long)states[i].ulRunTimeCounter,(unsigned)(states[i].usStackHighWaterMark*4));
 emit("DIAG_HEAP total=%u free=%u min=%u\r\n",(unsigned)configTOTAL_HEAP_SIZE,(unsigned)xPortGetFreeHeapSize(),(unsigned)xPortGetMinimumEverFreeHeapSize());
 emit("DIAG_TICK n=%lu sum_us=%lu max_us=%lu\r\n",(unsigned long)tick_n,(unsigned long)tick_sum,(unsigned long)tick_max);
 emit("DIAG_DONE ms=%lu\r\n",(unsigned long)measurement_ms());
 for(;;)vTaskDelay(pdMS_TO_TICKS(1000));
}
__attribute__((noinline)) static void consume_stack(void) {
 volatile uint8_t bytes[160];for(unsigned i=0;i<sizeof(bytes);i++)bytes[i]=(uint8_t)i;
 vTaskDelay(1);if(bytes[100]!=100)emit("CORRUPTION observed\r\n");
}
static void overflow_task(void *arg) {(void)arg;emit("STACK_PROBE begin check=%u words=48\r\n",(unsigned)configCHECK_FOR_STACK_OVERFLOW);consume_stack();emit("STACK_PROBE continued\r\n");for(;;)vTaskDelay(1);}
void lab4_experiment_start(void) {
#if LAB4_DIAGNOSTIC == 2 || LAB4_DIAGNOSTIC == 3
 configASSERT(xTaskCreate(overflow_task,"small",48,NULL,3,&high)==pdPASS);
#else
 mutex=xSemaphoreCreateMutex();binary=xSemaphoreCreateBinary();configASSERT(mutex&&binary);xSemaphoreGive(binary);
 queue=xQueueCreate(8,sizeof(unsigned));configASSERT(queue);
 configASSERT(xTaskCreate(high_task,"high",224,NULL,3,&high)==pdPASS);
 configASSERT(xTaskCreate(mid_task,"mid",96,NULL,2,&medium)==pdPASS);
 configASSERT(xTaskCreate(low_task,"low",128,NULL,1,&low)==pdPASS);
#endif
 vTaskStartScheduler();for(;;){}
}
