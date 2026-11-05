#ifndef LIGHTBENCH_FREERTOS_CONFIG_H
#define LIGHTBENCH_FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
uint32_t measurement_us(void);
void rtos_assert(const char *, int);
void rtos_trace(void *);
#ifndef LAB4_TICK_HZ
#define LAB4_TICK_HZ 1000
#endif
#ifndef LAB4_HEAP
#define LAB4_HEAP 4096
#endif
#define configENABLE_MPU 0
#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configCPU_CLOCK_HZ SystemCoreClock
#define configTICK_RATE_HZ LAB4_TICK_HZ
#define configMAX_PRIORITIES 4
#define configMINIMAL_STACK_SIZE 80
#define configMAX_TASK_NAME_LEN 8
#define configUSE_16_BIT_TICKS 0
#ifdef LAB4_DIAGNOSTIC
#define configTOTAL_HEAP_SIZE LAB4_HEAP
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_MUTEXES 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#else
/* Production uses a queue and fixed priorities; mutex experiments are separate. */
#define configTOTAL_HEAP_SIZE 0
#define configSUPPORT_DYNAMIC_ALLOCATION 0
#define configSUPPORT_STATIC_ALLOCATION 1
#define configUSE_MALLOC_FAILED_HOOK 0
#define configUSE_MUTEXES 0
#define INCLUDE_vTaskSuspend 0
#define INCLUDE_vTaskPrioritySet 0
#define INCLUDE_uxTaskPriorityGet 0
#endif
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_TIMERS 0
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#ifndef LAB4_STACK_CHECK
#define LAB4_STACK_CHECK 2
#endif
#define configCHECK_FOR_STACK_OVERFLOW LAB4_STACK_CHECK
#define configUSE_TRACE_FACILITY 1
#define configGENERATE_RUN_TIME_STATS 1
#define configRUN_TIME_COUNTER_TYPE uint64_t
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() ((void)0)
#define portGET_RUN_TIME_COUNTER_VALUE() rtos_time64()
uint64_t rtos_time64(void);
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configUSE_NEWLIB_REENTRANT 0
#define configUSE_TASK_NOTIFICATIONS 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskDelayUntil 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define configASSERT(x) do { if (!(x)) rtos_assert(__FILE__,__LINE__); } while(0)
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define traceTASK_SWITCHED_IN() rtos_trace((void *)pxCurrentTCB)
#endif
