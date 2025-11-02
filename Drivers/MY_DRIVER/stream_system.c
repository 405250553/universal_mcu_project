#include "stream_system.h"
#include <string.h>
#include <stdio.h>

__IO AviHandle gAviHandle  = {0};

/*******************************************************************************
                            AVI System Functions
*******************************************************************************/

void AviSystemInit()
{
    gAviHandle.AviState = VIDEO_INIT;
    gAviHandle.AviSpeed = SPEEDx1;
    gAviHandle.AviVolume = SAL_VOLUME_INIT_VAL;
    gAviHandle.CurrPlayIdx = 0;
    gAviHandle.SdProduceTask = NULL;
    gAviHandle.DisplayTask = NULL;
    gAviHandle.AudioplayTask = NULL;
}

static void AviStateChange(AVIPlayState newstate)
{
    if(gAviHandle.AviState!=newstate)
        gAviHandle.AviState = newstate;
}

void AviSetSpeed(AVIPlaySpeed newSpeed)
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviSpeed != newSpeed)
    {
        gAviHandle.AviSpeed = newSpeed;
        AviStateChange(VIDEO_SPEED_CHANGE);
    }
}

void AviSetVolume(uint8_t newVolume)
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviVolume != newVolume)
    {
        gAviHandle.AviVolume = newVolume;
        AviStateChange(VIDEO_VOLUME_CHANGE);
    }
}

void AviSetPause()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviState != VIDEO_PAUSE)
    {
        BSP_AUDIO_OUT_Pause();
        AviStateChange(VIDEO_PAUSE);
    }
}

void AviSetResume()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviState != VIDEO_PLAY)
    {
        BSP_AUDIO_OUT_Resume();
        AviStateChange(VIDEO_PLAY);
    }
}

void AviSetNext()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviState != VIDEO_PLAY_NEXT)
    {
        if(gAviHandle.AviState == VIDEO_PAUSE)
        {
            BSP_AUDIO_OUT_Resume();
        }
        AviStateChange(VIDEO_PLAY_NEXT);
    }
}

void AviSetPrev()
{
    if(gAviHandle.AviState!=VIDEO_INIT && gAviHandle.AviState != VIDEO_PLAY_PREV)
    {
        if(gAviHandle.AviState == VIDEO_PAUSE)
        {
            BSP_AUDIO_OUT_Resume();
        }
        AviStateChange(VIDEO_PLAY_PREV);
    }
}

/*******************************************************************************
                            IRQ Functions
*******************************************************************************/

/*
#define TS_INT_PIN                           GPIO_PIN_13
#define TS_INT_GPIO_PORT                     GPIOI
ft5336 interrupt pin 接到 stm32 gpio PI13
因此對應irq 名稱為 EXTI15_10_IRQHandler (就是gpio10~15為一組)
*/
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(TS_INT_PIN);
}

__IO uint8_t ts_irq_flag;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    TS_StateTypeDef gTS_State;
    if(GPIO_Pin == TS_INT_PIN)
    {
        ts_irq_flag = 1;
        /*
        BSP_TS_GetState(&gTS_State);   // 讀座標，清除 IC 中斷
        BSP_TS_ITClear();             // 清除 pending bit
        AVI_SYS_DEBUG("gTS_State.gestureId=%d\r\n",gTS_State.gestureId);

        switch(gTS_State.gestureId)
        {
            case GEST_ID_NO_GESTURE :
            AVI_SYS_DEBUG("GEST_ID_NO_GESTURE\r\n");
            break;
            case GEST_ID_MOVE_UP :
            AVI_SYS_DEBUG("GEST_ID_MOVE_UP\r\n");
            break;
            case GEST_ID_MOVE_RIGHT :
            AVI_SYS_DEBUG("GEST_ID_MOVE_RIGHT\r\n");
            break;
            case GEST_ID_MOVE_DOWN :
            AVI_SYS_DEBUG("GEST_ID_MOVE_DOWN\r\n");
            break;
            case GEST_ID_MOVE_LEFT :
            AVI_SYS_DEBUG("GEST_ID_MOVE_LEFT\r\n");
            break;
            case GEST_ID_ZOOM_IN :
            AVI_SYS_DEBUG("GEST_ID_ZOOM_IN\r\n");
            break;
            case GEST_ID_ZOOM_OUT :
            AVI_SYS_DEBUG("GEST_ID_ZOOM_OUT\r\n");
            break;
            default :
            AVI_SYS_DEBUG("TS_ERROR\r\n");
            break;
        } /* of switch(gestureId) */
    }
}

void test_gesture_task(void *param)
{
    TS_StateTypeDef gTS_State;
    while(1)
    {
        if(ts_irq_flag)
        {
            BSP_TS_GetState(&gTS_State);
            ts_irq_flag = 0;

            // 打印手勢 ID
            //AVI_SYS_DEBUG("gestureId=0x%02X\n", gTS_State.gestureId);
            for(uint8_t index=0; index < gTS_State.touchDetected; index++)
                AVI_SYS_DEBUG("touchX[%d]=%d,touchY[%d]=%d\n",index, gTS_State.touchX[index],index, gTS_State.touchY[index]);

            switch(gTS_State.gestureId)
            {
                case GEST_ID_NO_GESTURE :
                AVI_SYS_DEBUG("GEST_ID_NO_GESTURE\r\n");
                break;
                case GEST_ID_MOVE_UP :
                AVI_SYS_DEBUG("GEST_ID_MOVE_UP\r\n");
                break;
                case GEST_ID_MOVE_RIGHT :
                AVI_SYS_DEBUG("GEST_ID_MOVE_RIGHT\r\n");
                break;
                case GEST_ID_MOVE_DOWN :
                AVI_SYS_DEBUG("GEST_ID_MOVE_DOWN\r\n");
                break;
                case GEST_ID_MOVE_LEFT :
                AVI_SYS_DEBUG("GEST_ID_MOVE_LEFT\r\n");
                break;
                case GEST_ID_ZOOM_IN :
                AVI_SYS_DEBUG("GEST_ID_ZOOM_IN\r\n");
                break;
                case GEST_ID_ZOOM_OUT :
                AVI_SYS_DEBUG("GEST_ID_ZOOM_OUT\r\n");
                break;
                default :
                AVI_SYS_DEBUG("TS_ERROR\r\n");
                break;
                BSP_TS_ITClear();
            } /* of switch(gestureId) */
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(3));
        }
    }
}