#ifndef PROCEDURAL_MESHES_H
#define PROCEDURAL_MESHES_H

#include "../../engine/graphics/Mesh.h"

Mesh CreateOceanMesh();

Mesh CreateGroundMesh();

Mesh CreateOriginMarker();

Mesh CreateSkySphereMesh(
    const char* texturePath,
    int sectors = 32,
    int stacks = 16,
    float radius = 3000.0f
);

#endif