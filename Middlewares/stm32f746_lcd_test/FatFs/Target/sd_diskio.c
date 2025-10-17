/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @brief   SD Disk I/O driver (FreeRTOS version)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Note: code generation based on sd_diskio_dma_rtos_template_bspv1.c v2.1.4
   converted to use FreeRTOS directly. */

/* USER CODE BEGIN firstSection */
/* can be used to modify / undefine following code or add new definitions */
/* USER CODE END firstSection*/

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "stm32746g_discovery_sd.h"

#include <string.h>
#include <stdio.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define QUEUE_SIZE         (uint32_t) 10
#define READ_CPLT_MSG      (uint16_t) 1
#define WRITE_CPLT_MSG     (uint16_t) 2
/*
==================================================================
enable the defines below to send custom rtos messages
when an error or an abort occurs.
Notice: depending on the HAL/SD driver the HAL_SD_ErrorCallback()
may not be available.
See BSP_SD_ErrorCallback() and BSP_SD_AbortCallback() below
==================================================================

#define RW_ERROR_MSG       (uint16_t) 3
#define RW_ABORT_MSG       (uint16_t) 4
*/
/*
 * the following Timeout is useful to give the control back to the applications
 * in case of errors in either BSP_SD_ReadCpltCallback() or BSP_SD_WriteCpltCallback()
 * the value by default is as defined in the BSP platform driver otherwise 30 secs
 */
#define SD_TIMEOUT 30 * 1000U  /* milliseconds */

#define SD_DEFAULT_BLOCK_SIZE 512U

/*
 * Depending on the use case, the SD card initialization could be done at the
 * application level: if it is the case define the flag below to disable
 * the BSP_SD_Init() call in the SD_Initialize() and add a call to
 * BSP_SD_Init() elsewhere in the application.
 */
/* USER CODE BEGIN disableSDInit */
/* #define DISABLE_SD_INIT */
/* USER CODE END disableSDInit */

/*
 * when using cacheable memory region, it may be needed to maintain the cache
 * validity. Enable the define below to activate a cache maintenance at each
 * read and write operation.
 * Notice: This is applicable only for cortex M7 based platform.
 */
/* USER CODE BEGIN enableSDDmaCacheMaintenance */
/* #define ENABLE_SD_DMA_CACHE_MAINTENANCE  1 */
/* USER CODE END enableSDDmaCacheMaintenance */

/*
* Some DMA requires 4-Byte aligned address buffer to correctly read/write data,
* in FatFs some accesses aren't thus we need a 4-byte aligned scratch buffer to correctly
* transfer data
*/
/* USER CODE BEGIN enableScratchBuffer */
/* #define ENABLE_SCRATCH_BUFFER */
/* USER CODE END enableScratchBuffer */

/* Private variables ---------------------------------------------------------*/
#if defined(ENABLE_SCRATCH_BUFFER)
#if defined (ENABLE_SD_DMA_CACHE_MAINTENANCE)
ALIGN_32BYTES(static uint8_t scratch[BLOCKSIZE]); // 32-Byte aligned for cache maintenance
#else
__ALIGN_BEGIN static uint8_t scratch[BLOCKSIZE] __ALIGN_END;
#endif
#endif
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/* FreeRTOS queue handle */
static QueueHandle_t SDQueueID = NULL;

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
DRESULT SD_ioctl (BYTE, BYTE, void*);
#endif  /* _USE_IOCTL == 1 */

const Diskio_drvTypeDef  SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if  _USE_WRITE == 1
  SD_write,
#endif /* _USE_WRITE == 1 */

#if  _USE_IOCTL == 1
  SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* USER CODE BEGIN beforeFunctionSection */
/* can be used to modify / undefine following code or add new code */
/* USER CODE END beforeFunctionSection */

/* Private functions ---------------------------------------------------------*/

static int SD_CheckStatusWithTimeout(uint32_t timeout_ms)
{
  TickType_t startTicks = xTaskGetTickCount();
  TickType_t timeoutTicks = pdMS_TO_TICKS(timeout_ms);

  /* block until SDIO peripheral is ready again or a timeout occur */
  while ((xTaskGetTickCount() - startTicks) < timeoutTicks)
  {
    if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
    {
      return 0;
    }
    /* small yield to allow other tasks to run */
    taskYIELD();
  }

  return -1;
}

static DSTATUS SD_CheckStatus(BYTE lun)
{
  Stat = STA_NOINIT;

  if(BSP_SD_GetCardState() == SD_TRANSFER_OK)
  {
    Stat &= ~STA_NOINIT;
  }

  return Stat;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun)
{
  Stat = STA_NOINIT;

  /*
   * check that the kernel has been started before continuing
   * as the Queue API will fail otherwise
   */
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
#if !defined(DISABLE_SD_INIT)

    if(BSP_SD_Init() == MSD_OK)
    {
      Stat = SD_CheckStatus(lun);
    }

#else
    Stat = SD_CheckStatus(lun);
#endif

    /*
    * if the SD is correctly initialized, create the operation queue
    * if not already created
    */

    if (Stat != STA_NOINIT)
    {
      if (SDQueueID == NULL)
      {
        SDQueueID = xQueueCreate(QUEUE_SIZE, sizeof(uint16_t));
      }

      if (SDQueueID == NULL)
      {
        Stat |= STA_NOINIT;
      }
    }
  }

  return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
  return SD_CheckStatus(lun);
}

/* USER CODE BEGIN beforeReadSection */
/* can be used to modify previous code / undefine following code / add new code */
/* USER CODE END beforeReadSection */
/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  uint8_t ret;
  DRESULT res = RES_ERROR;
  TickType_t timerTicks;
  uint16_t event;
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
  uint32_t alignedAddr;
#endif

  /*
  * ensure the SDCard is ready for a new operation
  */
  if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0)
  {
    return res;
  }

#if defined(ENABLE_SCRATCH_BUFFER)
  if (!((uint32_t)buff & 0x3))
  {
#endif
    /* Fast path cause destination buffer is correctly aligned */
    ret = BSP_SD_ReadBlocks_DMA((uint32_t*)buff, (uint32_t)(sector), count);

    if (ret == MSD_OK) {
      /* wait for a message from the queue or a timeout */
      if ((SDQueueID != NULL) &&
          (xQueueReceive(SDQueueID, &event, pdMS_TO_TICKS(SD_TIMEOUT)) == pdTRUE) &&
          (event == READ_CPLT_MSG))
      {
        timerTicks = xTaskGetTickCount();
        /* block until SDIO IP is ready or a timeout occur */
        while ((xTaskGetTickCount() - timerTicks) < pdMS_TO_TICKS(SD_TIMEOUT))
        {
          if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
          {
            res = RES_OK;
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
                /*
                the SCB_InvalidateDCache_by_Addr() requires a 32-Byte aligned address,
                adjust the address and the D-Cache size to invalidate accordingly.
                */
                alignedAddr = (uint32_t)buff & ~0x1F;
                SCB_InvalidateDCache_by_Addr((uint32_t*)alignedAddr, count*BLOCKSIZE + ((uint32_t)buff - alignedAddr));
#endif
            break;
          }
          taskYIELD();
        }
      }
    }

#if defined(ENABLE_SCRATCH_BUFFER)
  }
  else
  {
    /* Slow path, fetch each sector a part and memcpy to destination buffer */
    int i;

    for (i = 0; i < count; i++)
    {
      ret = BSP_SD_ReadBlocks_DMA((uint32_t*)scratch, (uint32_t)sector++, 1);
      if (ret == MSD_OK )
      {
        /* wait until the read is successful or a timeout occurs */
        if ((SDQueueID != NULL) &&
            (xQueueReceive(SDQueueID, &event, pdMS_TO_TICKS(SD_TIMEOUT)) == pdTRUE) &&
            (event == READ_CPLT_MSG))
        {
          timerTicks = xTaskGetTickCount();
          /* block until SDIO IP is ready or a timeout occur */
          ret = MSD_ERROR;
          while ((xTaskGetTickCount() - timerTicks) < pdMS_TO_TICKS(SD_TIMEOUT))
          {
            ret = BSP_SD_GetCardState();

            if (ret == MSD_OK)
            {
              break;
            }
            taskYIELD();
          }

          if (ret != MSD_OK)
          {
            break;
          }
        }
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
        /*
        *
        * invalidate the scratch buffer before the next read to get the actual data instead of the cached one
        */
        SCB_InvalidateDCache_by_Addr((uint32_t*)scratch, BLOCKSIZE);
#endif
        memcpy(buff, scratch, BLOCKSIZE);
        buff += BLOCKSIZE;
      }
      else
      {
        break;
      }
    }

    if ((i == count) && (ret == MSD_OK ))
      res = RES_OK;
  }
#endif
  return res;
}

/* USER CODE BEGIN beforeWriteSection */
/* can be used to modify previous code / undefine following code or add new code */
/* USER CODE END beforeWriteSection */
/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1

DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
  TickType_t timerTicks;
  uint16_t event;

#if defined(ENABLE_SCRATCH_BUFFER)
  int32_t ret;
#endif

  /*
  * ensure the SDCard is ready for a new operation
  */

  if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0)
  {
    return res;
  }

#if defined(ENABLE_SCRATCH_BUFFER)
  if (!((uint32_t)buff & 0x3))
  {
#endif
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
  uint32_t alignedAddr;
  /*
    the SCB_CleanDCache_by_Addr() requires a 32-Byte aligned address
    adjust the address and the D-Cache size to clean accordingly.
  */
  alignedAddr = (uint32_t)buff & ~0x1F;
  SCB_CleanDCache_by_Addr((uint32_t*)alignedAddr, count*BLOCKSIZE + ((uint32_t)buff - alignedAddr));
#endif

  if(BSP_SD_WriteBlocks_DMA((uint32_t*)buff,
                           (uint32_t) (sector),
                           count) == MSD_OK)
  {
    /* Get the message from the queue */
    if ((SDQueueID != NULL) &&
        (xQueueReceive(SDQueueID, &event, pdMS_TO_TICKS(SD_TIMEOUT)) == pdTRUE) &&
        (event == WRITE_CPLT_MSG))
    {
      timerTicks = xTaskGetTickCount();
      /* block until SDIO IP is ready or a timeout occur */
      while ((xTaskGetTickCount() - timerTicks)  < pdMS_TO_TICKS(SD_TIMEOUT))
      {
        if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
        {
          res = RES_OK;
          break;
        }
        taskYIELD();
      }
    }
  }
#if defined(ENABLE_SCRATCH_BUFFER)
  }
  else {
    /* Slow path, fetch each sector a part and memcpy to destination buffer */
    int i;

#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
    /*
     * invalidate the scratch buffer before the next write to get the actual data instead of the cached one
     */
     SCB_InvalidateDCache_by_Addr((uint32_t*)scratch, BLOCKSIZE);
#endif
      for (i = 0; i < count; i++)
      {
        memcpy((void *)scratch, buff, BLOCKSIZE);
        buff += BLOCKSIZE;

        ret = BSP_SD_WriteBlocks_DMA((uint32_t*)scratch, (uint32_t)sector++, 1);
        if (ret == MSD_OK )
        {
          /* wait until the write is successful or a timeout occurs */
          if ((SDQueueID != NULL) &&
              (xQueueReceive(SDQueueID, &event, pdMS_TO_TICKS(SD_TIMEOUT)) == pdTRUE) &&
              (event == WRITE_CPLT_MSG))
          {
            timerTicks = xTaskGetTickCount();
            /* block until SDIO IP is ready or a timeout occur */
            ret = MSD_ERROR;
            while ((xTaskGetTickCount() - timerTicks) < pdMS_TO_TICKS(SD_TIMEOUT))
            {
              ret = BSP_SD_GetCardState();

              if (ret == MSD_OK)
              {
                break;
              }
              taskYIELD();
            }

            if (ret != MSD_OK)
            {
              break;
            }
          }
        }
        else
        {
          break;
        }
      }

      if ((i == count) && (ret == MSD_OK ))
        res = RES_OK;
    }

#endif

  return res;
}
 #endif /* _USE_WRITE == 1 */

/* USER CODE BEGIN beforeIoctlSection */
/* can be used to modify previous code / undefine following code / add new code */
/* USER CODE END beforeIoctlSection */
/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  BSP_SD_CardInfo CardInfo;

  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd)
  {
  /* Make sure that no pending write process */
  case CTRL_SYNC :
    res = RES_OK;
    break;

  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockNbr;
    res = RES_OK;
    break;

  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(WORD*)buff = CardInfo.LogBlockSize;
    res = RES_OK;
    break;

  /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
    res = RES_OK;
    break;

  default:
    res = RES_PARERR;
  }

  return res;
}
#endif /* _USE_IOCTL == 1 */

/* USER CODE BEGIN afterIoctlSection */
/* can be used to modify previous code / undefine following code or add new code */
/* USER CODE END afterIoctlSection */

/* USER CODE BEGIN callbackSection */
/* can be used to modify / following code or add new code */
/* USER CODE END callbackSection */
/**
  * @brief Tx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void BSP_SD_WriteCpltCallback(void)
{
  /*
   * No need to add an "xTaskGetSchedulerState()" check here, as the SD_initialize()
   * is always called before any SD_Read()/SD_Write() call
   */
  if (SDQueueID != NULL)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const uint16_t msg = WRITE_CPLT_MSG;
    (void)xQueueSendFromISR(SDQueueID, (void *)&msg, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void BSP_SD_ReadCpltCallback(void)
{
  /*
   * No need to add an "xTaskGetSchedulerState()" check here, as the SD_initialize()
   * is always called before any SD_Read()/SD_Write() call
   */
  if (SDQueueID != NULL)
  {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    const uint16_t msg = READ_CPLT_MSG;
    (void)xQueueSendFromISR(SDQueueID, (void *)&msg, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/* USER CODE BEGIN ErrorAbortCallbacks */
/*
void BSP_SD_AbortCallback(void)
{
   if (SDQueueID != NULL)
   {
     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
     const uint16_t msg = RW_ABORT_MSG;
     (void)xQueueSendFromISR(SDQueueID, (void *)&msg, &xHigherPriorityTaskWoken);
     portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
   }
}
*/
/* USER CODE END ErrorAbortCallbacks */

/* USER CODE BEGIN lastSection */
/* can be used to modify / undefine previous code or add new code */
/* USER CODE END lastSection */
