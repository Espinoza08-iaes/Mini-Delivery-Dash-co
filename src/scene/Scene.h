#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>

#include "../entities/Entity.h"

class Scene
{
public:
    void AddEntity(const Entity& entity);
    void Clear();

    const std::vector<Entity>& GetEntities() const;

private:
    std::vector<Entity> entities;
};

#endif
