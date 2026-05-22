import os
import zlib
import struct

path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\Modelos3d\textures\McLAREN_F1.png"
if os.path.exists(path):
    with open(path, "rb") as f:
        f.seek(8)
        idat_data = b""
        w, h = 0, 0
        while True:
            len_bytes = f.read(4)
            if not len_bytes:
                break
            length = struct.unpack(">I", len_bytes)[0]
            chunk_type = f.read(4)
            if chunk_type == b"IHDR":
                w, h = struct.unpack(">II", f.read(8))
                f.seek(length - 8 + 4, 1)
            elif chunk_type == b"IDAT":
                idat_data += f.read(length)
                f.seek(4, 1)
            elif chunk_type == b"IEND":
                break
            else:
                f.seek(length + 4, 1)
        
        decompressed = zlib.decompress(idat_data)
        row_size = w * 4 + 1
        
        # Collect a sample of RGB values where alpha is 0
        samples = []
        for r in range(0, h, 20):
            row_idx = r * row_size
            row_data = decompressed[row_idx + 1:row_idx + row_size]
            for c in range(0, w, 20):
                r_val = row_data[c*4]
                g_val = row_data[c*4 + 1]
                b_val = row_data[c*4 + 2]
                a_val = row_data[c*4 + 3]
                if a_val == 0:
                    samples.append((r_val, g_val, b_val))
                if len(samples) >= 30:
                    break
            if len(samples) >= 30:
                break
        
        print("Sample RGB values where Alpha == 0:")
        for s in samples:
            print(s)
