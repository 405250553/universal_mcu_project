#!/bin/bash
# 使用方式：
# ./video_convert.sh [format] [fps] [file] [rotate90]

set -e

FORMAT="$1"
USER_FPS=""
FILE=""
ROTATE=""

# 輸出資料夾
OUTDIR="output"

# 判斷第二個參數
if [[ "$2" == "rotate90" ]]; then
    ROTATE="rotate90"
elif [[ -n "$2" ]]; then
    USER_FPS="$2"
fi

# 判斷第三個參數
if [[ "$3" == "rotate90" ]]; then
    ROTATE="rotate90"
elif [[ -n "$3" ]]; then
    FILE="$3"
fi

# 第四個參數
if [[ "$4" == "rotate90" ]]; then
    ROTATE="rotate90"
fi

## TODO: rgb565_mov / rgb565_avi can not play
## mov divide frame data to per mdat, avi is a complete frame
# 檢查 format
if [[ -z "$FORMAT" ]]; then
    echo "請指定輸出格式:"
    echo "  rgb565_mov / rgb565_avi / argb8888_avi / mjpeg_avi / mjpeg_mov"
    echo "  bgr888_mov / bgr888_avi / rgb888_mov / rgb888_avi"
    exit 1
fi

# 建立輸出資料夾
mkdir -p "$OUTDIR"

# 判斷檔案列表
if [[ -z "$FILE" ]]; then
    FILES=(*.MP4 *.mp4)
else
    FILES=("$FILE")
fi

for f in "${FILES[@]}"; do
    [[ -f "$f" ]] || continue
    echo "開始轉檔：$f"

    FPS_ARG=()
    if [[ -n "$USER_FPS" ]]; then
        FPS_ARG=(-r "$USER_FPS")
    fi

    ROTATE_ARG=()
    WIDTH=272
    HEIGHT=480
    BASENAME="$(basename "$f" | sed 's/\.[^.]*$//')"

    if [[ "$ROTATE" == "rotate90" ]]; then
        ROTATE_ARG=(-vf "transpose=2")
        BASENAME="${BASENAME}_rotate90"
        WIDTH=480
        HEIGHT=272
    fi

    case "$FORMAT" in
        rgb565_mov)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec rawvideo -pix_fmt rgb565le -acodec pcm_s16le -ar 44100 -ac 1 \
                -f mov "${OUTDIR}/${BASENAME}_rgb565.mov"
            ;;
        rgb565_avi)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec rawvideo -pix_fmt rgb565le -acodec pcm_s16le -ar 44100 -ac 1 \
                -f avi "${OUTDIR}/${BASENAME}_rgb565.avi"
            ;;
        argb8888_avi)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec rawvideo -pix_fmt bgra -acodec pcm_s16le -ar 44100 -ac 1 \
                -f avi "${OUTDIR}/${BASENAME}_argb8888.avi"
            ;;
        mjpeg_avi)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec mjpeg -pix_fmt yuvj422p -q:v 3 -acodec pcm_s16le -ar 44100 -ac 1 \
                -f avi "${OUTDIR}/${BASENAME}_mjpeg.avi"
            ;;
        mjpeg_mov)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec mjpeg -pix_fmt yuvj422p -q:v 3 -acodec pcm_s16le -ar 44100 -ac 1 \
                -f mov "${OUTDIR}/${BASENAME}_mjpeg.mov"
            ;;
        bgr888_mov)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec rawvideo -pix_fmt bgr24 -acodec pcm_s16le -ar 44100 -ac 1 \
                -f mov "${OUTDIR}/${BASENAME}_bgr888.mov"
            ;;
        bgr888_avi)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec rawvideo -pix_fmt bgr24 -acodec pcm_s16le -ar 44100 -ac 1 \
                -f avi "${OUTDIR}/${BASENAME}_bgr888.avi"
            ;;
        rgb888_mov)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec rawvideo -pix_fmt rgb24 -acodec pcm_s16le -ar 44100 -ac 1 \
                -f mov "${OUTDIR}/${BASENAME}_rgb888.mov"
            ;;
        rgb888_avi)
            ffmpeg -i "$f" "${FPS_ARG[@]}" "${ROTATE_ARG[@]}" -s ${WIDTH}x${HEIGHT} \
                -vcodec rawvideo -pix_fmt rgb24 -acodec pcm_s16le -ar 44100 -ac 1 \
                -f avi "${OUTDIR}/${BASENAME}_rgb888.avi"
            ;;
        *)
            echo "不支援的格式：$FORMAT"
            exit 1
            ;;
    esac

    echo "完成：$f"
done

echo "全部轉檔完成，輸出在資料夾：$OUTDIR"
