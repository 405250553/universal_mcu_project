from PIL import Image
import numpy as np

input_file = "charizard-mega-x.png"
output_c = "image_data.h"
width, height = 272, 272
bg_color = (0, 0, 0)  # 背景顏色：黑色，可改成 (255,255,255) 白色

# 讀取圖片
img = Image.open(input_file)

# 調整大小
img = img.resize((width, height))

# 如果有透明通道，將背景填充
if img.mode in ('RGBA', 'LA') or ('transparency' in img.info):
    background = Image.new("RGB", img.size, bg_color)
    background.paste(img, mask=img.split()[-1])  # 用 alpha channel 做 mask
    img = background
else:
    img = img.convert("RGB")

# 儲存 BMP
bmp_file = "temp.bmp"
img.save(bmp_file, "BMP")

# 讀取 BMP 內容
with open(bmp_file, "rb") as f:
    bmp_bytes = f.read()

# 生成 C array
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