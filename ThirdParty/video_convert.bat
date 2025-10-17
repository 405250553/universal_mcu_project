@echo off
REM ===============================================
REM 使用方式：
REM batch_convert.bat rgb565_raw [fps] [filename]
REM batch_convert.bat argb8888_raw [fps] [filename]
REM batch_convert.bat argb8888_avi [fps] [filename]
REM batch_convert.bat mjpeg [fps] [filename]
REM ===============================================

SET FORMAT=%1
SET USER_FPS=%2
SET FILE=%3

IF "%FORMAT%"=="" (
    ECHO 請指定輸出格式: rgb565_raw / argb8888_raw / argb8888_avi / mjpeg
    EXIT /B 1
)

REM 判斷檔案列表
IF NOT "%FILE%"=="" (
    SET FILES=%FILE%
) ELSE (
    SET FILES=
    FOR %%f IN (*.MP4) DO (
        SET FILES=!FILES! %%f
    )
)

REM 啟用延遲變數解析
SETLOCAL ENABLEDELAYEDEXPANSION

FOR %%f IN (*.MP4) DO (
    ECHO 開始轉檔：%%f

    REM 如果沒給 FPS，使用 ffprobe 取得影片原始幀率
    IF "%USER_FPS%"=="" (
        FOR /F "usebackq tokens=*" %%r IN (`ffprobe -v 0 -of csv=p^=0 -select_streams v:0 -show_entries stream=r_frame_rate "%%f"`) DO (
            SET RAW_FPS=%%r
        )
        REM 計算比例，例如 30000/1001
        FOR /F "tokens=1,2 delims=/" %%a IN ("!RAW_FPS!") DO (
            SET /A NUM=%%a
            SET /A DEN=%%b
            SET /A FPS=!NUM! / !DEN!
        )
        SET FPS_ARG=-r !FPS!
    ) ELSE (
        SET FPS_ARG=-r %USER_FPS%
    )

    REM 轉檔
    IF /I "%FORMAT%"=="rgb565_raw" (
        ffmpeg -i "%%f" -f rawvideo -pix_fmt rgb565le !FPS_ARG! -s 272x480 -an "%%~nf_rgb565.raw"
    ) ELSE IF /I "%FORMAT%"=="argb8888_raw" (
        ffmpeg -i "%%f" -f rawvideo -pix_fmt bgra !FPS_ARG! -s 272x480 -an "%%~nf_raw_argb8888.raw"
    ) ELSE IF /I "%FORMAT%"=="argb8888_avi" (
        ffmpeg -i "%%f" -vcodec rawvideo -pix_fmt bgra !FPS_ARG! -s 272x480 -an "%%~nf_argb8888.avi"
    ) ELSE IF /I "%FORMAT%"=="mjpeg" (
        ffmpeg -i "%%f" -vcodec mjpeg -q:v 3 !FPS_ARG! -s 272x480 -pix_fmt yuvj422p -acodec pcm_u8 -ar 11025 "%%~nf_mjpeg_audio.avi"
    ) ELSE (
        ECHO 不支援的格式：%FORMAT%
        EXIT /B 1
    )

    ECHO 完成：%%f
)

ECHO 全部轉檔完成！
ENDLOCAL
