import json
import os

gltf_path = r"c:\Universidad\QuintoSemestre\Programacion Grafica\Lab8\res\models\hongkong_modificado\scene.gltf"

def search_translated_nodes():
    if not os.path.exists(gltf_path):
        print(f"Error: {gltf_path} does not exist.")
        return
        
    print(f"Reading {gltf_path}...")
    with open(gltf_path, "r", encoding="utf-8") as f:
        data = json.load(f)
        
    nodes = data.get("nodes", [])
    
    print("\n--- SAMPLE NODES WITH TRANSLATION OR MATRIX ---")
    count = 0
    for idx, node in enumerate(nodes):
        name = node.get("name", "")
        translation = node.get("translation", None)
        matrix = node.get("matrix", None)
        if translation is not None or matrix is not None:
            print(f"Node {idx}: Name='{name}', Translation={translation}, Matrix={matrix[:4] if matrix else 'N/A'}")
            count += 1
            if count >= 100:
                break

if __name__ == "__main__":
    search_translated_nodes()
