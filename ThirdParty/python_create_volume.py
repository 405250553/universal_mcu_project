import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
from PIL import Image
import struct
import os

def draw_volume_ui(volume=50, bar_width=130, bar_height=40, transpose=0):
    width = bar_width
    height = bar_height
    dpi = 100
    fig_width = width / dpi
    fig_height = height / dpi
    fig, ax = plt.subplots(figsize=(fig_width, fig_height), dpi=dpi, facecolor='none')

    bar_x, bar_y = 0, 0

    # 背景灰色條 (圓角)
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
    if volume > 0:
        fg_width = bar_width * volume / 100
        # 使用 clip path 避免白色覆蓋透明角
        fg = patches.FancyBboxPatch(
            (bar_x, bar_y),
            fg_width,
            bar_height,
            boxstyle=f"round,pad=0.0,rounding_size={bar_height/2}",
            linewidth=0,
            facecolor='white'
        )
        fg.set_clip_path(bg)  # 🔑 這裡用背景作裁剪
        ax.add_patch(fg)

    ax.set_xlim(0, width)
    ax.set_ylim(0, height)
    ax.axis('off')
    fig.subplots_adjust(0, 0, 1, 1)

    fig.canvas.draw()
    img_argb = np.frombuffer(fig.canvas.tostring_argb(), dtype=np.uint8).reshape(height, width, 4)
    plt.close(fig)

    # ARGB -> RGBA
    img_rgba = np.roll(img_argb, -1, axis=2)

    # 旋轉
    rotate_angle = 0
    if transpose == 1:
        rotate_angle = 90
    elif transpose == 2:
        rotate_angle = 180
    elif transpose == 3:
        rotate_angle = 270

    if rotate_angle != 0:
        img_pil = Image.fromarray(img_rgba, "RGBA")
        img_pil = img_pil.rotate(rotate_angle, expand=True)
        img_rgba = np.array(img_pil)

    return img_rgba


def make_bmp_bytes(img_rgba):
    height, width = img_rgba.shape[:2]
    img_argb = img_rgba[:, :, [2, 1, 0, 3]]  # RGBA -> BGRA

    row_padded = ((width * 4 + 3) // 4) * 4
    bmp_data = bytearray()
    for y in reversed(range(height)):
        row_bytes = img_argb[y, :, :].tobytes()
        bmp_data += row_bytes
        if len(row_bytes) < row_padded:
            bmp_data += b'\x00' * (row_padded - len(row_bytes))

    file_size = 14 + 40 + 16 + len(bmp_data)
    bmp_header = bytearray()
    bmp_header += b'BM'
    bmp_header += struct.pack('<I', file_size)
    bmp_header += struct.pack('<HH', 0, 0)
    bmp_header += struct.pack('<I', 14 + 40 + 16)
    bmp_header += struct.pack('<I', 40)
    bmp_header += struct.pack('<i', width)
    bmp_header += struct.pack('<i', height)
    bmp_header += struct.pack('<H', 1)
    bmp_header += struct.pack('<H', 32)
    bmp_header += struct.pack('<I', 3)  # BI_BITFIELDS
    bmp_header += struct.pack('<I', len(bmp_data))
    bmp_header += struct.pack('<i', 2835)
    bmp_header += struct.pack('<i', 2835)
    bmp_header += struct.pack('<I', 0)
    bmp_header += struct.pack('<I', 0)
    bmp_header += struct.pack('<I', 0x00FF0000)  # R
    bmp_header += struct.pack('<I', 0x0000FF00)  # G
    bmp_header += struct.pack('<I', 0x000000FF)  # B
    bmp_header += struct.pack('<I', 0xFF000000)  # A

    return bmp_header + bmp_data

def generate_volume_bmp_header(step=2, width=130, height=32, transpose=1, output_dir="output_bmp", header_name="volume_bar"):
    os.makedirs(output_dir, exist_ok=True)
    n_states = 100 // step + 1
    bmp_arrays = []

    for i in range(n_states):
        vol = i * step
        img = draw_volume_ui(vol, width, height, transpose)
        bmp_bytes = make_bmp_bytes(img)
        bmp_arrays.append(bmp_bytes)
        # optional: save each bmp to disk
        bmp_file = os.path.join(output_dir, f"{header_name}_{vol}.bmp")
        with open(bmp_file, "wb") as f:
            f.write(bmp_bytes)
        print(f"✅ Saved BMP for volume {vol}% -> {bmp_file}")

    # Write C header
    header_file = os.path.join(output_dir, f"{header_name}.h")
    max_len = max(len(b) for b in bmp_arrays)
    with open(header_file, "w") as f:
        f.write(f"#ifndef {header_name.upper()}_H\n#define {header_name.upper()}_H\n\n#include <stdint.h>\n\n")
        f.write(f"// Each array element is a full BMP file (header + pixels)\n")
        f.write(f"const uint8_t {header_name}[{n_states}][{max_len}] = {{\n")
        for arr in bmp_arrays:
            arr_len = len(arr)
            f.write("    { ")
            for j, b in enumerate(arr):
                f.write(f"0x{b:02X}, ")
                if (j+1) % 16 == 0: f.write("\n      ")
            # pad to max_len
            if arr_len < max_len:
                f.write(", " + ", ".join(["0x00"]*(max_len-arr_len)))
            f.write("},\n")
        f.write("};\n\n#endif\n")
    print(f"✅ C header generated: {header_file} ({n_states} BMPs included)")



if __name__ == "__main__":
    generate_volume_bmp_header(step=5, width=130, height=32, transpose=1)
