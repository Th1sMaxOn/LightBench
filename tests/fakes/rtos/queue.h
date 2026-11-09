#ifndef HOST_RTOS_QUEUE_H
#define HOST_RTOS_QUEUE_H
#include "FreeRTOS.h"
typedef struct {uint8_t *data;unsigned capacity,item_size,head,tail,count;} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(unsigned,unsigned,uint8_t *,StaticQueue_t *);
BaseType_t xQueueSend(QueueHandle_t,const void *,TickType_t);
BaseType_t xQueueReceive(QueueHandle_t,void *,TickType_t);
#endif
