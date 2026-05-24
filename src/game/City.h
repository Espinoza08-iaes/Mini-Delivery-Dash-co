#ifndef CITY_H
#define CITY_H

#include <string>
#include <glm/glm.hpp>

#include "../engine/Model.h"

class City
{
public:
    // modelPath: ruta al .gltf de la ciudad
    // scale:     escala del modelo (ajusta tamaño visual)
    // yOffset:   desplazamiento vertical (evita que flote o se entierre)
    City(const std::string &modelPath, float scale = 5.0f, float yOffset = -0.50f, float xOffset = 0.0f, float zOffset = 0.0f);

    

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
};

#endif
