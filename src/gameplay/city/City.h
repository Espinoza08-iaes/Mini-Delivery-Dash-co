#ifndef CITY_H
#define CITY_H

#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "../../engine/rendering/Model.h"
#include "CityPhysics.h"

class City
{
public:
    // modelPath: ruta al .gltf de la ciudad
    // scale:     escala del modelo (ajusta tamaño visual)
    // yOffset:   desplazamiento vertical (evita que flote o se entierre)
    // autoAlign: si se activa, ajusta la ciudad para que el punto más bajo esté en y=0
    City(const std::string &modelPath, float scale = 5.0f, float yOffset = -0.50f, float xOffset = 0.0f, float zOffset = 0.0f, bool autoAlign = false);

    // Computes the height of the street/terrain at (x, z) snapping to the closest vertical surface within threshold
    float GetHeightAt(float x, float z, float currentY, bool* outFound = nullptr, float snapDownMax = 8.0f, float snapUpMax = 0.12f) const;

    // Samples the best matching ground surface and its normal at a world position.
    bool GetGroundSample(const glm::vec3& worldPos, float currentY, game::GroundSample& outSample, float snapDownMax = 8.0f, float snapUpMax = 0.12f) const;

    // Returns the safest nearby road spawn point for the car.
    glm::vec3 GetBestRoadSpawn(const glm::vec3& preferred, float maxDistance = 1000.0f) const;

    // World-space bounds of the loaded city model.
    glm::vec3 GetWorldMinBounds() const;
    glm::vec3 GetWorldMaxBounds() const;

    // Checks if the car is colliding with any static obstacle meshes (buildings, barriers, props)
    bool CheckCollision(const glm::vec3& pos, float radius) const;

    // Dibuja la ciudad en el shader/camara actuales
    void Draw(Shader &shader, Camera &camera);

    // Devuelve la matriz mundo de la ciudad (escala + traslacion Y)
    glm::mat4 GetMatrix() const;

    // Accesores para que Game.cpp pueda usarlos en el world clamping
    float GetScale() const { return mScale; }
    float GetYOffset() const { return mYOffset; }
    float GetXOffset() const { return mXOffset; }
    float GetZOffset() const { return mZOffset; }

    std::vector<glm::vec3> GetStreetLampPositions(float spacing = 18.0f) const;
    std::vector<glm::vec3> GetStreetLampPositionsFromFile(const char* filePath) const;
    game::CityPhysics& GetPhysics() { return mPhysics; }
    const game::CityPhysics& GetPhysics() const { return mPhysics; }

private:
    void BuildVisualGapFillMesh();

    Model mModel;
    float mScale;
    float mYOffset;
    float mXOffset;
    float mZOffset;
    game::CityPhysics mPhysics;
    std::unique_ptr<Mesh> mVisualGapFillMesh;
};

#endif
