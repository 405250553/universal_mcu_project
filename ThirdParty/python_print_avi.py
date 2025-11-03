import struct


#========================================================================================================================

def le_to_int(b, signed=False):
    """小端轉整數"""
    return int.from_bytes(b, byteorder='little', signed=signed)
def read_ascii(data):
    """不可列印字元顯示為 ."""
    return ''.join(chr(b) if 32 <= b <= 126 else '.' for b in data)

#========================================================================================================================

def parse_list_hierarchy(data, start, length, depth=1):
    """遞迴解析 LIST 結構"""
    end = start + length
    offset = start
    indent = "  " * depth

    while offset + 8 <= end:
        chunk_id = data[offset:offset+4]
        chunk_size = struct.unpack("<I", data[offset+4:offset+8])[0]

        # 防止錯誤或越界
        if chunk_size <= 0 or offset + 8 + chunk_size > len(data):
            break

        if chunk_id == b'LIST':
            list_type = data[offset+8:offset+12]
            list_name = list_type.decode(errors="ignore")
            print(f"{indent}LIST '{list_name}' @ 0x{offset:08X}, size={chunk_size}")
            parse_list_hierarchy(data, offset + 12, chunk_size - 4, depth + 1)
            offset += 8 + chunk_size
        else:
            try:
                id_str = chunk_id.decode("ascii")
            except:
                id_str = str(chunk_id)
            print(f"{indent}{id_str:4} @ 0x{offset:08X}, size={chunk_size}")
            offset += 8 + chunk_size

def parse_avi_hierarchy(file_path):
    """AVI 主解析器"""
    with open(file_path, "rb") as f:
        data = f.read()

    # 基本檢查
    if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'AVI ':
        print("❌ Not a valid AVI file.")
        return

    riff_size = le_to_int(data[4:8])
    print("=== AVI Chunk Tree ===")
    print(f"RIFF 'AVI ' size={riff_size}")
    parse_list_hierarchy(data, 12, riff_size - 4)


#========================================================================================================================

def print_chunk(data, offset, fields, depth=0, title="Chunk"):
    """列印 chunk 的各欄位資訊"""
    indent = "  " * depth
    print(f"\n{indent}{title}\n")
    print(f"{indent}| Offset | Hex Data    | ASCII | Field Name          | Value |")
    print(f"{indent}| ------ | ----------- | ----- | ------------------- | ----- |")

    for size, field_name, unit in fields:
        hex_bytes = data[offset:offset+size]
        ascii_str = read_ascii(hex_bytes)

        # 判斷是否為文字型欄位
        if all(32 <= b <= 126 for b in hex_bytes) and (
            field_name.endswith("ID") or 
            field_name.endswith("Type") or 
            field_name in ("Format", "ListType", "biCompression")
        ):
            value = ascii_str
        else:
            # 特殊 signed 欄位
            signed_fields = ("biHeight", "Left", "Top", "Right", "Bottom")
            signed = field_name in signed_fields
            num = le_to_int(hex_bytes, signed=signed)
            value = f"{num} {unit}" if unit else num

        hex_str = ' '.join(f'{b:02X}' for b in hex_bytes)
        print(f"{indent}| 0x{offset:04X} | {hex_str:<11} | {ascii_str:<5} | {field_name:<19} | {value} |")
        offset += size

    print("\n\n")
    return offset


def parse_list(data, start, length, depth=0, chunk_map=None):
    """遞迴解析 LIST 結構"""
    end = start + length
    offset = start
    indent = "  " * depth

    while offset + 8 <= end:
        chunk_id = data[offset:offset+4]
        chunk_size = le_to_int(data[offset+4:offset+8])

        # 對齊補位 (AVI chunk 對齊到偶數)
        next_offset = offset + 8 + chunk_size
        if next_offset % 2 == 1:
            next_offset += 1

        # 安全性檢查
        if chunk_size <= 0 or next_offset > len(data):
            break

        if chunk_id == b'LIST':
            list_type = data[offset+8:offset+12].decode(errors="ignore")
            print(f"{indent}LIST '{list_type}' @ 0x{offset:08X}, size={chunk_size}")
            # 遞迴解析 LIST 內部內容
            parse_list(data, offset + 12, chunk_size - 4, depth + 1, chunk_map)
        elif chunk_id in chunk_map:
            if chunk_id in (b'00dc', b'01wb'):
                # 音訊/影像幀資料不展開
                print(f"{indent}Chunk '{chunk_id.decode()}' @ 0x{offset:08X}, size={chunk_size}")
            else:
                fields = chunk_map[chunk_id]
                print_chunk(data, offset, fields, depth, title=f"Chunk '{chunk_id.decode()}'")
        else:
            # 未知 chunk
            try:
                name = chunk_id.decode("ascii")
            except:
                name = str(chunk_id)
            #print(f"{indent}Unknown chunk '{name}' @ 0x{offset:08X}, size={chunk_size}")

        offset = next_offset


def parse_avi(file_path):
    with open(file_path, 'rb') as f:
        data = f.read()

    # ------------------------ RIFF ------------------------
    riff_chunk_fields = [
        (4, "ChunkID", ""),               # RIFF 標識碼，固定為 "RIFF"
        (4, "ChunkSize", "bytes"),        # RIFF 區塊大小，不含前8個byte
        (4, "Format", ""),                 # 格式，AVI 文件通常為 "AVI "
    ]

    # ------------------------ chunk ------------------------
    chunk_header = [
        (4, "ChunkID", ""),                 # Chunk 標識碼"
        (4, "Chunk Size", "bytes"),         # Chunk 區塊大小
    ]

    # ------------------------ avih ------------------------
    avih_fields = [
        (4, "MicroSecPerFrame", "μs"),     # 每幀所需微秒數
        (4, "MaxBytesPerSec", "bytes/sec"),# 最大傳輸率
        (4, "PaddingGranularity", "bytes"),# 對齊填充粒度
        (4, "Flags", ""),                   # 標誌位
        (4, "TotalFrames", "frames"),       # 總幀數
        (4, "InitialFrames", "frames"),     # 初始幀數
        (4, "Streams", ""),                 # 流數量
        (4, "SuggestedBufferSize", "bytes"),# 建議緩衝區大小
        (4, "Width", "pixels"),             # 視頻寬度
        (4, "Height", "pixels"),            # 視頻高度
    ]

    # ------------------------ strh ------------------------
    strh_fields = [
        (4, "fccType", ""),                 # 流類型，例如 "vids" (視頻) 或 "auds" (音頻)
        (4, "fccHandler", ""),              # 編碼器代碼，例如 "MJPG"
        (4, "Flags", ""),                   # 標誌位
        (4, "Reserved1", ""),               # 保留欄位
        (4, "InitialFrames", "frames"),     # 初始幀數
        (4, "Scale", ""),                   # 時間尺度
        (4, "Rate", "units/sec"),           # 每秒單位數 (Rate/Scale = fps)
        (4, "Start", "units"),              # 流開始時間
        (4, "Length", "units"),             # 流長度
        (4, "SuggestedBufferSize", "bytes"),# 建議緩衝區大小
        (4, "Quality", ""),                  # 質量
        (4, "SampleSize", "bytes"),         # 每個樣本大小
        (4, "Left", "pixels"),              # 顯示區左邊界
        (4, "Top", "pixels"),               # 顯示區上邊界
        (4, "Right", "pixels"),             # 顯示區右邊界
        (4, "Bottom", "pixels"),            # 顯示區下邊界
    ]

    # ------------------------ strf ------------------------
    strf_fields = [
        (4, "biSize", "bytes"),             # BITMAPINFOHEADER 結構大小
        (4, "biWidth", "pixels"),           # 圖像寬度
        (4, "biHeight", "pixels"),          # 圖像高度
        (2, "biPlanes", ""),                 # 色彩平面數，固定1
        (2, "biBitCount", "bits"),           # 每像素位數
        (4, "biCompression", ""),            # 壓縮類型，例如 "MJPG"
        (4, "biSizeImage", "bytes"),         # 圖像大小
        (4, "biXPelsPerMeter", "pixels/m"),  # 水平解析度
        (4, "biYPelsPerMeter", "pixels/m"),  # 垂直解析度
        (4, "biClrUsed", "colors"),          # 調色板使用顏色數
        (4, "biClrImportant", "colors"),     # 重要顏色數
    ]

    # chunk id → 欄位對應字典
    chunk_map = {
        b'RIFF': riff_chunk_fields,
        b'avih': chunk_header + avih_fields,
        b'strh': chunk_header + strh_fields,
        b'strf': chunk_header + strf_fields,
        b'JUNK': [(4, "JUNK ChunkID", ""), (4, "JUNK size", "bytes")],
        b'00dc': [(4, "00dc ChunkID", ""), (4, "Size", "bytes")],  # 視頻幀
        b'01wb': [(4, "01wb ChunkID", ""), (4, "Size", "bytes")],  # 音頻幀
    }

    # 檢查 AVI
    if len(data) < 12 or data[:4] != b'RIFF' or data[8:12] != b'AVI ':
        print("❌ Not a valid AVI file.")
        return

    print("=== AVI Chunk Tree ===")
    riff_size = le_to_int(data[4:8])
    print(f"RIFF 'AVI ' size={riff_size}")

    # 解析 LIST/RIFF 內部 chunk
    parse_list(data, 12, riff_size - 4, depth=0, chunk_map=chunk_map)

if __name__ == "__main__":
    #parse_avi_hierarchy("8_argb8888.avi")
    parse_avi("output/8_rotate90_rgb565.avi")
