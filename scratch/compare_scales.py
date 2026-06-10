import json
import os

car_path = r"res/models/mclaren/source/McLaren F1 1993 By Alex.Ka/McLaren F1 1993 by Alex.Ka..obj"
city_path = r"res/models/hongkong_modificado/scene.gltf"

def get_obj_bbox(path):
    if not os.path.exists(path):
        return None
    min_x = min_y = min_z = float('inf')
    max_x = max_y = max_z = float('-inf')
    with open(path, "r") as f:
        for line in f:
            if line.startswith("v "):
                parts = line.split()
                if len(parts) >= 4:
                    try:
                        x = float(parts[1])
                        y = float(parts[2])
                        z = float(parts[3])
                        min_x = min(min_x, x)
                        min_y = min(min_y, y)
                        min_z = min(min_z, z)
                        max_x = max(max_x, x)
                        max_y = max(max_y, y)
                        max_z = max(max_z, z)
                    except ValueError:
                        pass
    return (min_x, min_y, min_z), (max_x, max_y, max_z)

def get_gltf_bbox(path):
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    
    # We want to find the min and max values of POSITION accessors.
    # In gltf, meshes have primitives with attributes -> POSITION.
    # POSITION points to an accessor, which has 'min' and 'max' properties representing its bounds.
    min_x = min_y = min_z = float('inf')
    max_x = max_y = max_z = float('-inf')
    
    accessors = data.get('accessors', [])
    meshes = data.get('meshes', [])
    for mesh in meshes:
        for primitive in mesh.get('primitives', []):
            attrs = primitive.get('attributes', {})
            if 'POSITION' in attrs:
                acc_idx = attrs['POSITION']
                if acc_idx < len(accessors):
                    acc = accessors[acc_idx]
                    if 'min' in acc and 'max' in acc:
                        m_min = acc['min']
                        m_max = acc['max']
                        if len(m_min) >= 3 and len(m_max) >= 3:
                            min_x = min(min_x, m_min[0])
                            min_y = min(min_y, m_min[1])
                            min_z = min(min_z, m_min[2])
                            max_x = max(max_x, m_max[0])
                            max_y = max(max_y, m_max[1])
                            max_z = max(max_z, m_max[2])
                            
    return (min_x, min_y, min_z), (max_x, max_y, max_z)

def main():
    print("--- Calculating Bounding Boxes ---")
    car_bounds = get_obj_bbox(car_path)
    if car_bounds:
        (c_min, c_max) = car_bounds
        c_size = (c_max[0]-c_min[0], c_max[1]-c_min[1], c_max[2]-c_min[2])
        print(f"Car Model:")
        print(f"  Min: {c_min}")
        print(f"  Max: {c_max}")
        print(f"  Size (Width, Height, Length): {c_size}")
    else:
        print("Car model not found.")
        
    city_bounds = get_gltf_bbox(city_path)
    if city_bounds:
        (ct_min, ct_max) = city_bounds
        ct_size = (ct_max[0]-ct_min[0], ct_max[1]-ct_min[1], ct_max[2]-ct_min[2])
        print(f"City Model (unscaled):")
        print(f"  Min: {ct_min}")
        print(f"  Max: {ct_max}")
        print(f"  Size (Width, Height, Length): {ct_size}")
    else:
        print("City model not found.")

if __name__ == "__main__":
    main()
