#ifndef VBO_CLASS_H
#define VBO_CLASS_H

#include <glad/glad.h>
#include <vector>

#include "../../core/Vertex.h"

class VBO
{
public:
    GLuint ID;

    VBO(const std::vector<Vertex> &vertices)
    {
        glGenBuffers(1, &ID);
        glBindBuffer(GL_ARRAY_BUFFER, ID);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    }

    void Bind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, ID);
    }

    void Unbind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void Delete()
    {
        glDeleteBuffers(1, &ID);
    }
};

#endif
