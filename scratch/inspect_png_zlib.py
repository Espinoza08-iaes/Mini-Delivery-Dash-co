import os
import zlib
import struct

path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\Modelos3d\textures\McLAREN_F1.png"
if os.path.exists(path):
    with open(path, "rb") as f:
        f.seek(8) # Skip signature
        idat_data = b""
        w, h = 0, 0
        while True:
            len_bytes = f.read(4)
            if not len_bytes:
                break
            length = struct.unpack(">I", len_bytes)[0]
            chunk_type = f.read(4)
            if chunk_type == b"IHDR":
                w, h, depth, color_type = struct.unpack(">IIBB", f.read(10)[:10])
                f.seek(length - 10 + 4, 1) # Skip rest and CRC
            elif chunk_type == b"IDAT":
                idat_data += f.read(length)
                f.seek(4, 1) # Skip CRC
            elif chunk_type == b"IEND":
                break
            else:
                f.seek(length + 4, 1) # Skip data and CRC
        
        print(f"IHDR: Width={w}, Height={h}, ColorType={color_type}")
        if color_type == 6: # RGBA
            print("Decompressing IDAT...")
            try:
                decompressed = zlib.decompress(idat_data)
                print(f"Decompressed length: {len(decompressed)}")
                # For an RGBA image, each row is h scanlines.
                # Each scanline starts with a filter byte, followed by w * 4 bytes.
                # Let's check some pixels in the middle (e.g., row h // 2)
                row_size = w * 4 + 1
                mid_row_idx = (h // 2) * row_size
                row_data = decompressed[mid_row_idx + 1:mid_row_idx + row_size]
                
                # Check alpha of first 20 pixels in this row
                alphas = [row_data[i*4 + 3] for i in range(min(w, 100))]
                print("First 100 alpha values in middle row:")
                print(alphas)
                
                # Let's count how many pixels have alpha < 255
                total_pixels = w * h
                semi_trans = 0
                fully_trans = 0
                opaque = 0
                
                # Let's sample every 100th pixel to be fast
                sample_step = 100
                total_samples = 0
                for r in range(0, h, 10):
                    row_idx = r * row_size
                    row_data = decompressed[row_idx + 1:row_idx + row_size]
                    for c in range(0, w, 10):
                        total_samples += 1
                        a = row_data[c*4 + 3]
                        if a == 0:
                            fully_trans += 1
                        elif a < 255:
                            semi_trans += 1
                        else:
                            opaque += 1
                
                print(f"Sampled {total_samples} pixels:")
                print(f"  Fully transparent (alpha=0): {fully_trans} ({fully_trans/total_samples*100:.2f}%)")
                print(f"  Semi-transparent (alpha<255): {semi_trans} ({semi_trans/total_samples*100:.2f}%)")
                print(f"  Opaque (alpha=255): {opaque} ({opaque/total_samples*100:.2f}%)")
            except Exception as e:
                print("Error decompressing:", e)
else:
    print("File not found")
