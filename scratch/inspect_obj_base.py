import os

obj_path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\models\mclaren\source\McLaren F1 1993 By Alex.Ka\McLaren F1 1993 by Alex.Ka..obj"

def find_base_material():
    if not os.path.exists(obj_path):
        print(f"Error: OBJ file not found at {obj_path}")
        return
        
    vertices = []
    current_material = None
    material_vertex_y_ranges = {}
    
    print("Parsing OBJ to associate vertices with materials...")
    with open(obj_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("v "):
                parts = line.split()
                if len(parts) >= 4:
                    try:
                        x = float(parts[1])
                        y = float(parts[2])
                        z = float(parts[3])
                        vertices.append((x, y, z))
                    except ValueError:
                        pass
            elif line.startswith("usemtl "):
                current_material = line.strip().split(maxsplit=1)[1]
                if current_material not in material_vertex_y_ranges:
                    material_vertex_y_ranges[current_material] = []
            elif line.startswith("f "):
                # Face references vertex indices (1-based)
                if current_material:
                    parts = line.strip().split()[1:]
                    for part in parts:
                        v_idx_str = part.split("/")[0]
                        if v_idx_str:
                            try:
                                v_idx = int(v_idx_str) - 1
                                if 0 <= v_idx < len(vertices):
                                    material_vertex_y_ranges[current_material].append(vertices[v_idx])
                            except ValueError:
                                pass

    print("\nAnalysis of each material's bounding box and vertex count:")
    for mat, v_list in material_vertex_y_ranges.items():
        if not v_list:
            continue
        xs = [v[0] for v in v_list]
        ys = [v[1] for v in v_list]
        zs = [v[2] for v in v_list]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        min_z, max_z = min(zs), max(zs)
        width = max_x - min_x
        height = max_y - min_y
        length = max_z - min_z
        print(f"Material '{mat}':")
        print(f"  Vertices count: {len(v_list)}")
        print(f"  Y range: [{min_y:.4f}, {max_y:.4f}] (Height: {height:.4f})")
        print(f"  X range: [{min_x:.4f}, {max_x:.4f}] (Width: {width:.4f})")
        print(f"  Z range: [{min_z:.4f}, {max_z:.4f}] (Length: {length:.4f})")
        
        # Check if it matches the flat wide showcase base under the car
        if height < 0.05 and width > 8.0 and length > 8.0:
            print(f"  >>> DING! Material '{mat}' is a flat, wide plane at the bottom! <<<")

if __name__ == "__main__":
    find_base_material()
