#ifndef CITY_H
#define CITY_H

#include <string>
#include <glm/glm.hpp>

#include "../engine/Model.h"
#include "city_physics/CityPhysics.h"

class City
{
public:
    // modelPath: ruta al .gltf de la ciudad
    // scale:     escala del modelo (ajusta tamaño visual)
    // yOffset:   desplazamiento vertical (evita que flote o se entierre)
    City(const std::string &modelPath, float scale = 5.0f, float yOffset = -0.50f, float xOffset = 0.0f, float zOffset = 0.0f);

    // Computes the height of the street/terrain at (x, z) snapping to the closest vertical surface within threshold
    float GetHeightAt(float x, float z, float currentY, bool* outFound = nullptr) const;

    // Samples the best matching ground surface and its normal at a world position.
    bool GetGroundSample(const glm::vec3& worldPos, float currentY, game::GroundSample& outSample) const;

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

private:
    Model mModel;
    float mScale;
    float mYOffset;
    float mXOffset;
    float mZOffset;
    game::CityPhysics mPhysics;
};

#endif
