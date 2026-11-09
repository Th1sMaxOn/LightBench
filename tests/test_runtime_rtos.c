/* Exercises RTOS-specific runtime paths with copy FIFO fakes.
 * This checks business/measurement integration, not scheduling on a Cortex-M. */
#define LIGHTBENCH_RTOS_HOST_FIXTURE
#include "test_runtime.c"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "rtos.h"
#include <setjmp.h>
static jmp_buf task_end;
static unsigned release_budget, critical_depth;
void host_enter_critical(void) {++critical_depth;}
void host_exit_critical(void) {assert(critical_depth);--critical_depth;}
TickType_t xTaskGetTickCount(void) {return now;}
BaseType_t xTaskDelayUntil(TickType_t *last,TickType_t period) {
 assert(!critical_depth);
 if(!release_budget)longjmp(task_end,1);
 --release_budget;*last+=period;now=*last;return pdTRUE;
}
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,uint8_t *data,StaticQueue_t *q) {
 *q=(StaticQueue_t){.data=data,.capacity=n,.item_size=size};return q;
}
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t ticks) {
 (void)ticks;if(q->count==q->capacity)return pdFALSE;
 memcpy(q->data+q->head*q->item_size,item,q->item_size);q->head=(q->head+1)%q->capacity;++q->count;return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t ticks) {
 (void)ticks;if(!q->count)return pdFALSE;
 memcpy(item,q->data+q->tail*q->item_size,q->item_size);q->tail=(q->tail+1)%q->capacity;--q->count;return pdTRUE;
}
void rtos_report(void) {}
static void release(void (*task)(void *),unsigned count) {
 release_budget=count;if(!setjmp(task_end))task(NULL);assert(!critical_depth);
}
int main(void) {
 memset(flash_bytes,255,sizeof(flash_bytes));runtime_init();
 command("run\n");click();assert(markers==1);
 clear_output();command("imuaddr 0x69\nselftest\n");
 release(runtime_imu_task,1);runtime_poll();assert(strstr(output,"probe=imu result=1"));
 release(runtime_imu_task,1);runtime_poll();
 assert(strstr(output,"selftest adc=1 imu=0"));
 command("imuaddr 0x68\nselftest\n");release(runtime_imu_task,1);runtime_poll();
 release(runtime_imu_task,1);runtime_poll();assert(strstr(output,"selftest adc=1 imu=1"));
 command("imuaddr 0x69\n");release(runtime_imu_task,1);runtime_poll();
 clear_output();command("selftest\nimuaddr 0x68\n");
 release(runtime_imu_task,2);runtime_poll();
 assert(strstr(output,"selftest adc=1 imu=0 button=ready bus=1"));
 clear_output();command("metrics reset\n");release(runtime_adc_task,2);now+=200;runtime_poll();
 command("metrics\n");for(unsigned i=0;i<20;i++)runtime_poll();
 assert(strstr(output,"METRIC name=adc_interval n=1 min_us=20000 avg_us=20000 max_us=20000"));
 clear_output();command("selftest\nselftest\nselftest\nselftest\nselftest\nselftest\nselftest\nselftest\nselftest\n");
 assert(strstr(output,"reason=imu_command_queue_full lost=1"));
 for(unsigned i=0;i<8;i++){release(runtime_imu_task,1);runtime_poll();}
 assert(strstr(output,"selftest adc=1 imu=1"));
 clear_output();adc_ok=false;release(runtime_adc_task,1);runtime_poll();
 assert(strstr(output,"reason=adc_fault")&&!led);
 command("run\n");assert(strstr(output,"adc_unavailable"));
 puts("RTOS runtime FIFO fixture: ordered IMU controls, failed-probe invalidation, acquisition timestamps, queue-overflow reporting and ADC fault passed; scheduler not simulated");
}
