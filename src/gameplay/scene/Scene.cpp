#include "Scene.h"

void Scene::AddEntity(const Entity& entity)
{
    entities.push_back(entity);
}

void Scene::Clear()
{
    entities.clear();
}

const std::vector<Entity>& Scene::GetEntities() const
{
    return entities;
}
