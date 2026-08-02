#ifndef __STREAM_SYNC_H
#define __STREAM_SYNC_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdbool.h>

void SyncInit(void);

void SyncDisplayReady(void);
void SyncAudioReady(void);

void SyncProducerFileFinished(void);
bool SyncIsProducerFinished(void);

void SyncDisplayExited(void);
void SyncAudioExited(void);
void SyncProducerAwaitBothExited(void);

#ifdef __cplusplus
}
#endif

#endif /* __STREAM_SYNC_H */
