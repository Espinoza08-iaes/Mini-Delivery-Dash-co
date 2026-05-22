#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

#include "Shader.h"

class Camera
{
public:
    glm::vec3 Position;
    glm::vec3 Orientation = glm::vec3(0.0f, -0.2f, -1.0f);
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);

    int width;
    int height;

    bool firstClick = true;
    float speed = 0.2f;
    float sensitivity = 100.0f;

    Camera(int width, int height, glm::vec3 position)
        : Position(position), width(width), height(height)
    {
    }

    void updateMatrix(float FOVdeg, float nearPlane, float farPlane)
    {
        glm::mat4 view = glm::lookAt(Position, Position + Orientation, Up);
        glm::mat4 projection = glm::perspective(glm::radians(FOVdeg), static_cast<float>(width) / height, nearPlane, farPlane);
        cameraMatrix = projection * view;
    }

    void Matrix(Shader& shader, const char* uniform)
    {
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, uniform), 1, GL_FALSE, glm::value_ptr(cameraMatrix));
    }

    void Inputs(GLFWwindow* window)
    {
        float crntSpeed = speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        {
            crntSpeed = speed * 4.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            Position += crntSpeed * Orientation;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            Position -= crntSpeed * Orientation;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            Position -= glm::normalize(glm::cross(Orientation, Up)) * crntSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            Position += glm::normalize(glm::cross(Orientation, Up)) * crntSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            Position += crntSpeed * Up;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        {
            Position -= crntSpeed * Up;
        }

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

            if (firstClick)
            {
                glfwSetCursorPos(window, width / 2, height / 2);
                firstClick = false;
            }

            double mouseX;
            double mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);

            float rotX = sensitivity * static_cast<float>(mouseY - height / 2) / height;
            float rotY = sensitivity * static_cast<float>(mouseX - width / 2) / width;

            glm::vec3 right = glm::normalize(glm::cross(Orientation, Up));
            Orientation = glm::rotate(Orientation, glm::radians(-rotX), right);
            Orientation = glm::rotate(Orientation, glm::radians(-rotY), Up);

            glfwSetCursorPos(window, width / 2, height / 2);
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            firstClick = true;
        }
    }

private:
    glm::mat4 cameraMatrix = glm::mat4(1.0f);
};

#endif
