import os

obj_path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\models\mclaren\source\McLaren F1 1993 By Alex.Ka\McLaren F1 1993 by Alex.Ka..obj"

def scan_materials():
    if not os.path.exists(obj_path):
        print(f"Error: OBJ file not found at {obj_path}")
        return
        
    materials = set()
    print("Reading OBJ file for material names...")
    with open(obj_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("usemtl "):
                mat_name = line.strip().split(maxsplit=1)[1]
                materials.add(mat_name)
                
    print("\nMaterials found in the OBJ file:")
    for mat in sorted(list(materials)):
        print(f"  - {mat}")

if __name__ == "__main__":
    scan_materials()
