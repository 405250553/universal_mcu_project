from PIL import Image
import numpy as np
import struct

# --------------------------- 配置 ---------------------------
input_file = "c_gen_first_frame.bmp"  # 原始 BMP
output_c = "image_data.h"             # C array 輸出
bmp_file = "output.bmp"               # 生成 BMP 檔
width, height = 480, 272
bg_color = (0, 0, 0)                  # 背景顏色
rotate = False                         # 是否旋轉 90 度
output_format = "argb8888"             # "argb8888" 或 "rgb888"

# --------------------------- 讀取圖片 ---------------------------
img = Image.open(input_file)
if rotate:
    img = img.rotate(90, expand=True)
img = img.resize((width, height))

# 處理透明通道
if img.mode in ('RGBA', 'LA') or ('transparency' in img.info):
    img = img.convert("RGBA")
    background = Image.new("RGBA", img.size, bg_color + (255,))
    background.paste(img, mask=img.split()[-1])
    img = background
else:
    img = img.convert("RGB")

# --------------------------- 生成像素資料 ---------------------------
pixels = np.array(img)
if output_format.lower() == "argb8888":
    # ARGB8888: 32-bit
    if pixels.shape[2] == 3:
        a = np.full((height, width, 1), 255, dtype=np.uint8)
    else:
        a = pixels[:, :, 3:4]
    rgb = pixels[:, :, 0:3]
    pixel_data = np.concatenate([a, rgb], axis=2)
elif output_format.lower() == "rgb888":
    # RGB888: 24-bit
    if pixels.shape[2] == 4:
        pixel_data = pixels[:, :, 0:3]
    else:
        pixel_data = pixels
else:
    raise ValueError("output_format 只能是 'argb8888' 或 'rgb888'")

# BMP 用的是 BGR 順序
if output_format.lower() in ("argb8888", "rgb888"):
    # ARGB / RGB 轉 BGR 排列
    pixel_data_bgr = pixel_data.copy()
    pixel_data_bgr[..., [0, 2]] = pixel_data_bgr[..., [2, 0]]  # R <-> B
else:
    pixel_data_bgr = pixel_data

# --------------------------- 生成 BMP 檔案 ---------------------------
def save_bmp(filename, data, width, height, bits=24):
    row_bytes = ((bits // 8) * width + 3) // 4 * 4  # 每行對齊 4 bytes
    padding = row_bytes - (width * (bits // 8))
    filesize = 14 + 40 + row_bytes * height  # file header + info header + pixel data

    with open(filename, 'wb') as f:
        # --- BMP File Header ---
        f.write(b'BM')
        f.write(struct.pack('<I', filesize))  # 文件大小
        f.write(b'\x00\x00')  # 保留
        f.write(b'\x00\x00')  # 保留
        f.write(struct.pack('<I', 14 + 40))  # offset to pixel data

        # --- BMP Info Header ---
        f.write(struct.pack('<I', 40))  # header size
        f.write(struct.pack('<i', width))  # width
        f.write(struct.pack('<i', height))  # height
        f.write(struct.pack('<H', 1))   # planes
        f.write(struct.pack('<H', bits))  # bits per pixel
        f.write(struct.pack('<I', 0))   # compression
        f.write(struct.pack('<I', row_bytes * height))  # image size
        f.write(struct.pack('<i', 0))  # X ppm
        f.write(struct.pack('<i', 0))  # Y ppm
        f.write(struct.pack('<I', 0))  # colors used
        f.write(struct.pack('<I', 0))  # important colors

        # --- Pixel Data ---
        for y in reversed(range(height)):  # BMP 從底到頂
            row = data[y, :, :]
            f.write(row.tobytes())
            f.write(b'\x00' * padding)

save_bmp(bmp_file, pixel_data_bgr, width, height,
         bits=32 if output_format.lower()=="argb8888" else 24)
print(f"BMP 已儲存: {bmp_file}")

# --------------------------- 生成 C array ---------------------------
with open(bmp_file, "rb") as f:
    bmp_bytes = f.read()

c_array_name = "bmp_data"
with open(output_c, "w") as f:
    f.write("#ifndef IMAGE_DATA_H\n#define IMAGE_DATA_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"const uint8_t {c_array_name}[] = {{\n")
    for i, b in enumerate(bmp_bytes):
        if i % 12 == 0:
            f.write("    ")
        f.write(f"0x{b:02X}, ")
        if (i+1) % 12 == 0:
            f.write("\n")
    f.write("\n};\n")
    f.write(f"\nconst uint32_t bmp_size = {len(bmp_bytes)};\n\n")
    f.write("#endif // IMAGE_DATA_H\n")

print(f"C array 輸出完成: {output_c}, 大小: {len(bmp_bytes)} bytes")
