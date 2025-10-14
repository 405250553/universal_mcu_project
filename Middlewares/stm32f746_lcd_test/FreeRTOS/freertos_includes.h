#ifndef FREERTOS_INCLUDES_H
#define FREERTOS_INCLUDES_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

typedef enum freertos_task_pri_e
{
PRIORITY_IDLE=0,
PRIORITY_LOW,
PRIORITY_BelowNormal,
PRIORITY_Normal,
PRIORITY_AboveNormal,
PRIORITY_HIGH,
PRIORITY_REALTIME=configMAX_PRIORITIES-1,
}freertos_task_pri_t;


#endif