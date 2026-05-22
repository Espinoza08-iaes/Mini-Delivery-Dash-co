import struct
import os

path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\Modelos3d\textures\McLAREN_F1.png"
if os.path.exists(path):
    with open(path, "rb") as f:
        sig = f.read(8)
        print("Signature:", sig)
        # Read IHDR chunk
        length_bytes = f.read(4)
        chunk_type = f.read(4)
        if chunk_type == b"IHDR":
            w, h, depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", f.read(13))
            print(f"Width: {w}")
            print(f"Height: {h}")
            print(f"Bit depth: {depth}")
            print(f"Color type: {color_type} (6=RGBA, 2=RGB)")
            
            # Read next few chunks to see if it is a normal PNG
            f.seek(8 + 8 + 13 + 4) # Skip header, IHDR chunk length/type/data, and CRC
            for _ in range(5):
                len_bytes = f.read(4)
                if not len_bytes:
                    break
                length = struct.unpack(">I", len_bytes)[0]
                ctype = f.read(4)
                print(f"Chunk: {ctype.decode('ascii', errors='ignore')}, Length: {length}")
                f.seek(length + 4, 1) # Skip chunk data and CRC
else:
    print("File not found")
