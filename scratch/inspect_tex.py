import os
from PIL import Image

path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\Modelos3d\textures\McLAREN_F1.png"
if os.path.exists(path):
    print("File exists!")
    img = Image.open(path)
    print(f"Format: {img.format}")
    print(f"Mode: {img.mode}")
    print(f"Size: {img.size}")
    # Get bands
    print(f"Bands: {img.getbands()}")
    
    # Check if there is transparency or if it's dark
    # Let's crop a small part or get average pixel values
    pixels = list(img.resize((10, 10)).getdata())
    print("Sample pixels (10x10 resized):")
    for p in pixels[:10]:
        print(p)
else:
    print("File does NOT exist!")
