import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
from PIL import Image

# === 1. 繪製 Volume UI ===
def draw_volume_ui(volume=50, bar_width=130, bar_height=20, transpose=False):
    """
    volume: 音量百分比 0~100
    bar_width, bar_height: 音量條大小
    transpose: True 則將整張圖逆時針旋轉 90 度
    """
    width = int(bar_width * 1.3)
    height = int(bar_height * 3)
    dpi = 100
    fig_width = width / dpi
    fig_height = height / dpi
    fig, ax = plt.subplots(figsize=(fig_width, fig_height), dpi=dpi, facecolor='none')

    # Volume 條位置
    margin_x = width * 0.05
    margin_y = height * 0.4
    bar_x, bar_y = margin_x, margin_y

    # 背景灰色條
    bg = patches.FancyBboxPatch(
        (bar_x, bar_y),
        bar_width,
        bar_height,
        boxstyle=f"round,pad=0.0,rounding_size={bar_height/2}",
        linewidth=0,
        facecolor='lightgray'
    )
    ax.add_patch(bg)

    # 前景白色條
    fg = patches.FancyBboxPatch(
        (bar_x, bar_y),
        bar_width * volume / 100,
        bar_height,
        boxstyle=f"round,pad=0.0,rounding_size={bar_height/2}",
        linewidth=0,
        facecolor='white'
    )
    ax.add_patch(fg)

    # 左上角 Volume 標籤
    font_size_label = max(1, int(bar_width * 0.15 / 2))
    ax.text(bar_x, bar_y + bar_height + 5, "Volume",
            va='bottom', ha='left', fontsize=font_size_label, fontweight='bold', color='black')

    # 右側百分比數字
    font_size_num = max(1, int(bar_height))
    ax.text(bar_x + bar_width + 5, bar_y + bar_height/2, f"{volume}%",
            va='center', ha='left', fontsize=font_size_num, fontweight='bold', color='black')

    ax.set_xlim(0, width)
    ax.set_ylim(0, height)
    ax.axis('off')

    fig.canvas.draw()
    img_argb = np.frombuffer(fig.canvas.tostring_argb(), dtype=np.uint8).reshape(height, width, 4)
    plt.close(fig)

    # ARGB -> RGBA
    img_rgba = np.roll(img_argb, -1, axis=2)

    # 逆時針旋轉 90 度
    if transpose:
        img_pil = Image.fromarray(img_rgba, "RGBA")
        img_pil = img_pil.rotate(90, expand=True)
        img_rgba = np.array(img_pil)

    return img_rgba

# === 2. 儲存 PNG / BMP / C header ===
def save_images_and_header(img_rgba, filename_base="image_data", bmp_bg=(0,0,0)):
    # PNG
    png_file = f"{filename_base}.png"
    Image.fromarray(img_rgba, "RGBA").save(png_file)
    print(f"✅ PNG saved: {png_file} ({img_rgba.shape[1]}x{img_rgba.shape[0]})")

    # BMP RGB888
    img_pil = Image.fromarray(img_rgba, "RGBA")
    background = Image.new("RGB", img_pil.size, bmp_bg)
    background.paste(img_pil, mask=img_pil.split()[-1])
    bmp_file = f"{filename_base}.bmp"
    background.save(bmp_file, format="BMP")
    print(f"✅ BMP saved: {bmp_file} ({background.width}x{background.height})")

    # C Header
    with open(bmp_file, "rb") as f:
        bmp_bytes = f.read()
    header_file = f"{filename_base}.h"
    with open(header_file, "w") as f:
        f.write("#ifndef IMAGE_DATA_H\n#define IMAGE_DATA_H\n\n#include <stdint.h>\n\n")
        f.write("const uint8_t bmp_data[] = {\n")
        for i,b in enumerate(bmp_bytes):
            if i%12==0:
                f.write("    ")
            f.write(f"0x{b:02X}, ")
            if (i+1)%12==0:
                f.write("\n")
        f.write("\n};\n")
        f.write(f"\nconst uint32_t bmp_size = {len(bmp_bytes)};\n\n")
        f.write("#endif // IMAGE_DATA_H\n")
    print(f"✅ C header saved: {header_file} ({len(bmp_bytes)} bytes)")

# === 範例使用 ===
if __name__ == "__main__":
    volume = 70
    img = draw_volume_ui(volume, bar_width=130, bar_height=40, transpose=True)
    save_images_and_header(img, filename_base="image_data")
