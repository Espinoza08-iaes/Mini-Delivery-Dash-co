import json

try:
    with open("res/models/city_3d/scene.gltf", "r") as f:
        data = json.load(f)
    print("Materials in scene.gltf:")
    materials = data.get("materials", [])
    for idx, mat in enumerate(materials):
        print(f"Index {idx}: {mat.get('name')}")
except Exception as e:
    print("Error:", e)
