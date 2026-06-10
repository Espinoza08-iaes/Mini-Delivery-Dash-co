#ifndef ENTITY_H
#define ENTITY_H

#include <string>

#include "../../core/Transform.h"

struct Entity
{
    std::string name;
    std::string modelPath;
    Transform transform;
};

#endif
