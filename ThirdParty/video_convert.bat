@echo off
chcp 65001 >nul
SETLOCAL ENABLEDELAYEDEXPANSION

REM 使用方式：batch_convert.bat [format] [fps] [file]
REM format: rgb565_raw / argb8888_raw / argb8888_avi / mjpeg

SET FORMAT=%1
SET USER_FPS=%2
SET FILE=%3

IF "%FORMAT%"=="" (
    ECHO 請指定輸出格式: rgb565_raw / argb8888_raw / argb8888_avi / mjpeg
    EXIT /B 1
)

REM 判斷檔案列表
IF "%FILE%"=="" (
    SET FILES=
    FOR %%f IN (*.MP4) DO (
        SET FILES=!FILES! %%f
    )
) ELSE (
    SET FILES=%FILE%
)

FOR %%f IN (!FILES!) DO (
    ECHO 開始轉檔：%%f

    REM 設定 FPS 參數
    SET FPS_ARG=
    IF NOT "%USER_FPS%"=="" (
        SET FPS_ARG=-r %USER_FPS%
    )

    REM 取得原檔名不含副檔名
    SET BASENAME=%%~nf

    REM 轉檔
    IF /I "%FORMAT%"=="rgb565_raw" (
        ffmpeg -i "%%f" !FPS_ARG! -s 272x480 -f rawvideo -pix_fmt rgb565le -an "!BASENAME!_rgb565.raw"
    ) ELSE IF /I "%FORMAT%"=="argb8888_raw" (
        ffmpeg -i "%%f" !FPS_ARG! -s 272x480 -f rawvideo -pix_fmt bgra -an "!BASENAME!_argb8888.raw"
    ) ELSE IF /I "%FORMAT%"=="argb8888_avi" (
        ffmpeg -i "%%f" !FPS_ARG! -s 272x480 -vcodec rawvideo -pix_fmt bgra -acodec pcm_s16le -ar 44100 -ac 1 -f avi "!BASENAME!_argb8888.avi"
    ) ELSE IF /I "%FORMAT%"=="mjpeg" (
        ffmpeg -i "%%f" !FPS_ARG! -s 272x480 -vcodec mjpeg -pix_fmt yuvj422p -q:v 3 -acodec pcm_s16le -ar 44100 -ac 1 -f avi "!BASENAME!_mjpeg.avi"
    ) ELSE (
        ECHO 不支援的格式：%FORMAT%
        EXIT /B 1
    )

    ECHO 完成：%%f
)

ECHO 全部轉檔完成！
ENDLOCAL
