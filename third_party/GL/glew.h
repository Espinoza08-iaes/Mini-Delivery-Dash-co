#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

static int glewExperimental;

static inline int glewInit(void)
{
    return gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) ? 0 : 1;
}

#define GLEW_OK 0

#ifdef __cplusplus
}
#endif