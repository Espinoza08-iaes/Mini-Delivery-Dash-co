import struct
import os

path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\Modelos3d\source\McLaren F1 1993 By Alex.Ka\1 McLaren F1 1993  By Alex.Ka..png"
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
else:
    print("File not found")
