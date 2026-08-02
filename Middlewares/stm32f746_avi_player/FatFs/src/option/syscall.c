/*------------------------------------------------------------------------*/
/* FreeRTOS-dependent controls for FatFs                                  */
/*------------------------------------------------------------------------*/
/* (C)ChaN, 2014                                                          */
/* Portions COPYRIGHT 2017 STMicroelectronics                             */
/*------------------------------------------------------------------------*/

#include "../ff.h"
#include "freertos_includes.h"

#if _FS_REENTRANT

/* Define _SYNC_t as SemaphoreHandle_t in ffconf.h:
   #define _SYNC_t SemaphoreHandle_t
*/

/*------------------------------------------------------------------------*/
/* Create a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
int ff_cre_syncobj (
    BYTE vol,
    _SYNC_t *sobj
)
{
    *sobj = xSemaphoreCreateMutex();
    return (*sobj != NULL) ? 1 : 0;
}

/*------------------------------------------------------------------------*/
/* Delete a Synchronization Object                                        */
/*------------------------------------------------------------------------*/
int ff_del_syncobj (
    _SYNC_t sobj
)
{
    vSemaphoreDelete(sobj);
    return 1;
}

/*------------------------------------------------------------------------*/
/* Request Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
int ff_req_grant (
    _SYNC_t sobj
)
{
    if (xSemaphoreTake(sobj, pdMS_TO_TICKS(_FS_TIMEOUT)) == pdTRUE)
        return 1;   /* got the lock */
    else
        return 0;   /* timeout */
}

/*------------------------------------------------------------------------*/
/* Release Grant to Access the Volume                                     */
/*------------------------------------------------------------------------*/
void ff_rel_grant (
    _SYNC_t sobj
)
{
    xSemaphoreGive(sobj);
}

#endif  /* _FS_REENTRANT */


/*------------------------------------------------------------------------*/
/* LFN heap memory allocation (for _USE_LFN == 3)                        */
/*------------------------------------------------------------------------*/
#if _USE_LFN == 3

void* ff_memalloc (UINT msize)
{
    return ff_malloc(msize);
}

void ff_memfree (void* mblock)
{
    ff_free(mblock);
}

#endif  /* _USE_LFN == 3 */
