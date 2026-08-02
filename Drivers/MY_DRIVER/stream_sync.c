#include "stream_sync.h"
#include "freertos_includes.h"

/*
Producer(SdProduceTask)與兩個 Consumer(DisplayTask、AudioplayTask)之間的會合協定：
開機互等、檔案讀完通知、收工互等，全部收在這個 event group 裡，外部一律透過本檔案的函式操作，
不再直接碰 bit 巨集。
*/
static EventGroupHandle_t xConsumerGroup;

#define DISPLAY_READY_BIT      (1<<0)
#define AUDIO_READY_BIT        (1<<1)
#define FILE_READ_FINISH_BIT   (1<<2)
#define DISPLAY_EXIT_BIT       (1<<3)
#define AUDIO_EXIT_BIT         (1<<4)

void SyncInit(void)
{
    xConsumerGroup = xEventGroupCreate();
}

void SyncDisplayReady(void)
{
    xEventGroupSetBits(xConsumerGroup, DISPLAY_READY_BIT);
    xEventGroupWaitBits(xConsumerGroup,
                        AUDIO_READY_BIT,
                        pdTRUE, /* BIT_0 & BIT_4 should be cleared before returning.*/
                        pdTRUE, /* 不是等待所有都要置位，只要有一个满足条件就好 */
                        portMAX_DELAY);
}

void SyncAudioReady(void)
{
    xEventGroupSetBits(xConsumerGroup, AUDIO_READY_BIT);
    xEventGroupWaitBits(xConsumerGroup, DISPLAY_READY_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
}

void SyncProducerFileFinished(void)
{
    xEventGroupSetBits(xConsumerGroup, FILE_READ_FINISH_BIT);
}

bool SyncIsProducerFinished(void)
{
    return (xEventGroupGetBits(xConsumerGroup) & FILE_READ_FINISH_BIT) != 0;
}

void SyncDisplayExited(void)
{
    xEventGroupSetBits(xConsumerGroup, DISPLAY_EXIT_BIT);
}

void SyncAudioExited(void)
{
    xEventGroupSetBits(xConsumerGroup, AUDIO_EXIT_BIT);
}

void SyncProducerAwaitBothExited(void)
{
    xEventGroupWaitBits(xConsumerGroup, DISPLAY_EXIT_BIT | AUDIO_EXIT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    // 收工後重置全部 bit，供下一部影片重新使用同一個 event group
    xEventGroupClearBits(xConsumerGroup, DISPLAY_READY_BIT | AUDIO_READY_BIT | FILE_READ_FINISH_BIT
                                        | DISPLAY_EXIT_BIT | AUDIO_EXIT_BIT);
}
