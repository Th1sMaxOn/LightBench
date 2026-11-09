#ifndef HOST_RTOS_API_H
#define HOST_RTOS_API_H
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define configSUPPORT_STATIC_ALLOCATION 1
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define configASSERT(x) assert(x)
void host_enter_critical(void);
void host_exit_critical(void);
#define taskENTER_CRITICAL() host_enter_critical()
#define taskEXIT_CRITICAL() host_exit_critical()
#endif
