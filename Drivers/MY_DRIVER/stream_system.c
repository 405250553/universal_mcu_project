#include "image_data.h"
#include "stream_system.h"
#include "stream_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

__IO AviHandle gAviHandle  = {0};

__attribute__((section(".sdram_data"))) uint8_t layer1_buff[2][LAYER1_FRAME_SIZE];
static uint8_t curr_buff=0;

void MY_Front_LCD_DrawBitmap(uint32_t Xpos, uint32_t Ypos, uint8_t *pbmp, uint8_t buff_idx);

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

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    TS_StateTypeDef gTS_State;
    if(GPIO_Pin == TS_INT_PIN)
    {
        
        BSP_TS_GetState(&gTS_State);   // 讀座標，清除 IC 中斷
        BSP_TS_ITClear();             // 清除 pending bit
        AVI_SYS_DEBUG("X=%d Y=%d\r\n",gTS_State.touchX[0],gTS_State.touchY[0]);
        /*
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

typedef enum
{
    TS_STATE_NONE=0,
    TS_STATE_CHECK,
    TS_STATE_VOLUME,
}TS_SelfStateTypeDef;

typedef struct
{
    TS_StateTypeDef bsp_state;
    TS_SelfStateTypeDef self_state;
    uint8_t delay_ms;
    uint8_t bar_len;
    uint8_t old_x;
    uint8_t old_y;
    uint8_t startX;
    uint8_t startY;
    uint8_t check_time;
}TS_SelfHandleTypeDef;


void ts_handle_init(TS_SelfHandleTypeDef *ts_handle)
{
    ts_handle->self_state = TS_STATE_NONE;
    ts_handle->delay_ms = 50;
    ts_handle->bar_len = DEFAULT_BAR_LEN;
    ts_handle->old_x = 0;
    ts_handle->old_y = 0;
    ts_handle->startX = 0;
    ts_handle->startY = 0;
    ts_handle->check_time = 0;
}


void test_gesture_task(void *pvParameters) {
    static int16_t startY = -1;      // 手指按下的起點
    const int barLen = 130;          // 對應整個音量條長度
    TS_StateTypeDef gTS_State;
    uint8_t curr_vol = SAL_VOLUME_INIT_VAL;           // 初始音量
    int8_t curr_idx = (curr_vol * 20) / 100;  // bitmap index 初始值
    uint8_t delay_ms=100;
    int new_vol=0;

    for (;;) {
        BSP_TS_GetState(&gTS_State);

        if (gTS_State.touchDetected > 0) 
        {
            AVI_SYS_DEBUG("x=%d,y=%d\r\n",gTS_State.touchX[0],gTS_State.touchY[0]);
            int16_t y = gTS_State.touchY[0];

            if (gTS_State.touchEventId[0] == TOUCH_EVENT_PRESS_DOWN) {
                startY = y;  // 記錄起點
                delay_ms=25;
            } 
            else if (gTS_State.touchEventId[0] == TOUCH_EVENT_CONTACT) 
            {
                BSP_LCD_SetTransparency_NoReload(1,0xff);
                BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);
                if (startY >= 0) {
                    // 直接用位移計算音量，不累加 curr_vol
                    int16_t deltaY = startY - y;  // 向上滑正，向下滑負
                    new_vol = ((deltaY * 100) / barLen) + curr_vol;

                    // 限制範圍 0~100
                    if (new_vol > 100) new_vol = 100;
                    if (new_vol < 0) new_vol = 0;

                    // 更新音量
                    AviSetVolume(new_vol);

                    // 計算 bitmap index
                    int8_t bmp_index = (new_vol * 20) / 100;
                    if (bmp_index != curr_idx) {
                        curr_idx = bmp_index;
                        curr_buff = (curr_buff+1)%2;
                        MY_Front_LCD_DrawBitmap(416, 72, (uint8_t*)bmp_data[curr_idx], curr_buff);
                        BSP_LCD_SetLayerAddress_NoReload(1, (uint32_t)layer1_buff[curr_buff]);
                        BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);
                    }
                }
                delay_ms=25;
            }
            // 如果手指放開，更新 curr_vol 作為下一次基準
            else{
                curr_vol = new_vol;
                BSP_LCD_SetTransparency_NoReload(1,0x00);
                BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);   
                startY = -1;  // 重置起點
                delay_ms = 100;
            }
        }
        else
        {
                BSP_LCD_SetTransparency_NoReload(1,0x00);
                BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);   
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/*
void test_gesture_task(void *pvParameters) {
    uint8_t curr_vol = SAL_VOLUME_INIT_VAL;           // 初始音量
    int8_t curr_idx = (curr_vol * 20) / 100;  // bitmap index 初始值
    int new_vol=0;
    uint8_t curr_x, curr_y;

    TS_SelfHandleTypeDef ts_handle;
    ts_handle_init(&ts_handle);

    while (1) {
        BSP_TS_GetState(&ts_handle.bsp_state);
        curr_x=ts_handle.bsp_state.touchX[0];
        curr_y=ts_handle.bsp_state.touchY[0];

        //if (ts_handle.bsp_state.touchDetected > 0) 
        {
            AVI_SYS_DEBUG("x=%d,y=%d\r\n",curr_x,curr_y);
            if (ts_handle.self_state==TS_STATE_NONE && (ts_handle.bsp_state.touchEventId[0] == TOUCH_EVENT_PRESS_DOWN
                                                        || ts_handle.bsp_state.touchEventId[0] == TOUCH_EVENT_CONTACT)) {
                ts_handle.old_x = curr_x;
                ts_handle.old_y = curr_y;
                ts_handle.startX = curr_x;
                ts_handle.startY = curr_y;
                ts_handle.delay_ms=25;
                ts_handle.self_state=TS_STATE_CHECK;
                //AVI_SYS_DEBUG("change state to TS_STATE_CHECK\r\n");
            } 
            else if (ts_handle.bsp_state.touchEventId[0] == TOUCH_EVENT_CONTACT) 
            {
                //AVI_SYS_DEBUG("in TOUCH_EVENT_CONTACT\r\n");
                int16_t deltaX = ts_handle.startX - curr_x;  // x 向右是next, 左是prev
                int16_t deltaY = ts_handle.startY - curr_y;  // volume向上滑正，向下滑負
                
                if(ts_handle.self_state==TS_STATE_CHECK)
                {
                    ts_handle.check_time += ts_handle.delay_ms;
                    if(ts_handle.check_time <=500) //0~0.5s 的判定
                    {
                        if(abs(deltaX)>10 && abs(deltaY) < 20)
                        {
                            ts_handle.old_x=curr_x;
                            ts_handle.old_y=curr_y;
                            if(ts_handle.check_time==500)
                            {
                                if(deltaX<0)  AviSetNext();
                                if(deltaX>0)  AviSetPrev();
                                goto ts_state_restart;
                                AVI_SYS_DEBUG("execute next/prev\r\n");
                            }
                        }

                        //if(abs(deltaX)+abs(deltaY)>10)
                        //    goto ts_state_restart;
                    }
                    else //0.5~1.0s 的判定
                    {
                        //if(abs(deltaX)+abs(deltaY)>10)
                        //    goto ts_state_restart;
                        //else
                        {
                            if(ts_handle.check_time==1000)
                            {
                                ts_handle.self_state=TS_STATE_VOLUME;
                                BSP_LCD_SetTransparency_NoReload(1,0xff);
                                BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);
                                AVI_SYS_DEBUG("change state to TS_STATE_VOLUME\r\n");
                                continue;
                            }
                        }
                    }
                }
                else if(ts_handle.self_state==TS_STATE_VOLUME)
                {
                    if (ts_handle.startY >= 0) {
                        // 直接用位移計算音量，不累加 curr_vol
                        new_vol = ((deltaY * 100) / ts_handle.bar_len) + curr_vol;

                        // 限制範圍 0~100
                        if (new_vol > 100) new_vol = 100;
                        if (new_vol < 0) new_vol = 0;

                        // 更新音量
                        AviSetVolume(new_vol);

                        // 計算 bitmap index
                        int8_t bmp_index = (new_vol * 20) / 100;
                        if (bmp_index != curr_idx) {
                            curr_idx = bmp_index;
                            curr_buff = (curr_buff+1)%2;
                            MY_Front_LCD_DrawBitmap(416, 72, (uint8_t*)bmp_data[curr_idx], curr_buff);
                            BSP_LCD_SetLayerAddress_NoReload(1, (uint32_t)layer1_buff[curr_buff]);
                            BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);
                        }
                    }
                }
            }
            else
            {
    ts_state_restart:
                curr_vol = new_vol;
                ts_handle.self_state=TS_STATE_NONE;
                ts_handle.check_time = 0;
                ts_handle.delay_ms = 50;
                BSP_LCD_SetTransparency_NoReload(1,0x00);
                BSP_LCD_Reload(LCD_RELOAD_VERTICAL_BLANKING);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ts_handle.delay_ms));
    }
}
*/

/**
  * @brief  Draws a bitmap picture loaded in the internal Flash in ARGB888 format (32 bits per pixel).
  * @param  Xpos: Bmp X position in the LCD
  * @param  Ypos: Bmp Y position in the LCD
  * @param  pbmp: Pointer to Bmp picture address in the internal Flash
  * @retval None
  */
void MY_Front_LCD_DrawBitmap(uint32_t Xpos, uint32_t Ypos, uint8_t *pbmp, uint8_t buff_idx)
{
  uint32_t index = 0, width = 0, height = 0, bit_pixel = 0;
  uint32_t address;
  
  /* Get bitmap data address offset */
  index = pbmp[10] + (pbmp[11] << 8) + (pbmp[12] << 16)  + (pbmp[13] << 24);

  /* Read bitmap width */
  width = pbmp[18] + (pbmp[19] << 8) + (pbmp[20] << 16)  + (pbmp[21] << 24);

  /* Read bitmap height */
  height = pbmp[22] + (pbmp[23] << 8) + (pbmp[24] << 16)  + (pbmp[25] << 24);

  /* Read bit/pixel */
  bit_pixel = pbmp[28] + (pbmp[29] << 8);  
  
  /* Set the address */
  address = layer1_buff[buff_idx] + (((BSP_LCD_GetXSize()*Ypos) + Xpos)*(4));
  
  /* Bypass the bitmap header */
  pbmp += (index + (width * (height - 1) * (bit_pixel/8)));  
  
  /* Convert picture to ARGB8888 pixel format */
  for(index=0; index < height; index++)
  {
    /* Pixel format conversion */
    //MY_ConvertLineToARGB8888((uint32_t *)pbmp, (uint32_t *)address, width);
    memcpy((void*)address,pbmp,width*4);
    
    /* Increment the source and destination buffers */
    address+=  (BSP_LCD_GetXSize()*4);
    pbmp -= width*(bit_pixel/8);
  } 
}