@echo off
chcp 65001 >nul
SETLOCAL ENABLEDELAYEDEXPANSION

REM 使用方式：
REM batch_convert.bat [format] [fps] [file] [rotate90]

SET FORMAT=%1
SET USER_FPS=
SET FILE=
SET ROTATE=

REM 輸出資料夾
SET OUTDIR=output

REM 判斷第二個參數
IF /I "%2%"=="rotate90" (
    SET ROTATE=rotate90
) ELSE IF NOT "%2%"=="" (
    SET USER_FPS=%2
)

REM 判斷第三個參數
IF /I "%3%"=="rotate90" (
    SET ROTATE=rotate90
) ELSE IF NOT "%3%"=="" (
    SET FILE=%3
)

REM 第四個參數
IF /I "%4%"=="rotate90" (
    SET ROTATE=rotate90
)

REM 檢查 format
IF "%FORMAT%"=="" (
    ECHO 請指定輸出格式: rgb565_raw / argb8888_raw / argb8888_avi / mjpeg
    EXIT /B 1
)

REM 建立輸出資料夾
IF NOT EXIST "%OUTDIR%" (
    mkdir "%OUTDIR%"
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

    REM 設定旋轉參數與尺寸
    SET ROTATE_ARG=
    SET WIDTH=272
    SET HEIGHT=480
    SET BASENAME=%%~nf
    IF /I "%ROTATE%"=="rotate90" (
        REM 頭在左邊的旋轉
        SET ROTATE_ARG=-vf "transpose=2"
        SET BASENAME=!BASENAME!_rotate90
        SET WIDTH=480
        SET HEIGHT=272
    )

    REM 轉檔
    IF /I "%FORMAT%"=="rgb565_raw" (
        ffmpeg -i "%%f" !FPS_ARG! !ROTATE_ARG! -s !WIDTH!x!HEIGHT! -f rawvideo -pix_fmt rgb565le -an "%OUTDIR%\!BASENAME!_rgb565.raw"
    ) ELSE IF /I "%FORMAT%"=="argb8888_raw" (
        ffmpeg -i "%%f" !FPS_ARG! !ROTATE_ARG! -s !WIDTH!x!HEIGHT! -f rawvideo -pix_fmt bgra -an "%OUTDIR%\!BASENAME!_argb8888.raw"
    ) ELSE IF /I "%FORMAT%"=="argb8888_avi" (
        ffmpeg -i "%%f" !FPS_ARG! !ROTATE_ARG! -s !WIDTH!x!HEIGHT! -vcodec rawvideo -pix_fmt bgra -acodec pcm_s16le -ar 44100 -ac 1 -f avi "%OUTDIR%\!BASENAME!_argb8888.avi"
    ) ELSE IF /I "%FORMAT%"=="mjpeg" (
        ffmpeg -i "%%f" !FPS_ARG! !ROTATE_ARG! -s !WIDTH!x!HEIGHT! -vcodec mjpeg -pix_fmt yuvj422p -q:v 3 -acodec pcm_s16le -ar 44100 -ac 1 -f avi "%OUTDIR%\!BASENAME!_mjpeg.avi"
    ) ELSE (
        ECHO 不支援的格式：%FORMAT%
        EXIT /B 1
    )

    ECHO 完成：%%f
)

ECHO 全部轉檔完成，輸出在資料夾：%OUTDIR%
ENDLOCAL
