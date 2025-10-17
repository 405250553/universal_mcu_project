#!/bin/bash

# 使用方式：
# ./batch_convert.sh rgb565_raw [fps] [filename]
# ./batch_convert.sh argb8888_raw [fps] [filename]
# ./batch_convert.sh argb8888_avi [fps] [filename]
# ./batch_convert.sh mjpeg [fps] [filename]

FORMAT=$1
USER_FPS=$2
FILE=$3
MAX_JOBS=4  # 同時轉檔數量，可依CPU核心數調整

if [ -z "$FORMAT" ]; then
  echo "請指定輸出格式: rgb565_raw / argb8888_raw / argb8888_avi / mjpeg"
  exit 1
fi

# 取得檔案列表
if [ ! -z "$FILE" ]; then
  FILES=("$FILE")
else
  FILES=(*.MP4)
fi

for f in "${FILES[@]}"; do
  (
    echo "開始轉檔：$f"

    # 如果沒給 fps，使用 ffprobe 取得影片原始幀率
    if [ -z "$USER_FPS" ]; then
      RAW_FPS=$(ffprobe -v 0 -of csv=p=0 -select_streams v:0 -show_entries stream=r_frame_rate "$f")
      # RAW_FPS 可能是 30000/1001 這種形式，需要計算
      FPS=$(echo "scale=3; $RAW_FPS" | bc -l)
      FPS_ARG="-r $FPS"
    else
      FPS_ARG="-r $USER_FPS"
    fi

    case "$FORMAT" in
      rgb565_raw)
        ffmpeg -i "$f" \
          -f rawvideo -pix_fmt rgb565le $FPS_ARG -s 272x480 -an \
          "${f%.MP4}_rgb565.raw"
        ;;
      argb8888_raw)
        ffmpeg -i "$f" \
          -f rawvideo -pix_fmt bgra $FPS_ARG -s 272x480 -an \
          "${f%.MP4}_raw_argb8888.raw"
        ;;
      argb8888_avi)
        ffmpeg -i "$f" \
          -vcodec rawvideo -pix_fmt bgra $FPS_ARG -s 272x480 -an \
          "${f%.MP4}_argb8888.avi"
        ;;
      mjpeg)
        ffmpeg -i "$f" \
          -vcodec mjpeg -q:v 3 $FPS_ARG -s 272x480 -pix_fmt yuvj422p \
          -acodec pcm_u8 -ar 11025 \
          "${f%.MP4}_mjpeg_audio.avi"
        ;;
      *)
        echo "不支援的格式：$FORMAT"
        exit 1
        ;;
    esac

    echo "完成：$f"
  ) &

  # 若背景任務數達上限，暫停直到有空位
  while [ "$(jobs -r | wc -l)" -ge "$MAX_JOBS" ]; do
    sleep 1
  done
done

wait
echo "全部轉檔完成！"
