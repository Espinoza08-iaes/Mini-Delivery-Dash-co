#include "MainMenu.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "../../../third_party/stb/stb_image.h"

// Compilación local de shaders (sin depender de tu clase Shader)
static unsigned int compileShader(const char* vertexSrc, const char* fragmentSrc)
{
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexSrc, NULL);
    glCompileShader(vs);
    int success;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(vs, 512, NULL, info);
        std::cerr << "Vertex shader failed: " << info << std::endl;
    }

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentSrc, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(fs, 512, NULL, info);
        std::cerr << "Fragment shader failed: " << info << std::endl;
    }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

MainMenu::MainMenu(GLFWwindow* win, int screenWidth, int screenHeight)
    : window(win), width(screenWidth), height(screenHeight),
      backgroundTex(0), quadVAO(0), quadVBO(0), hudProgram(0)
{
    LoadTextures();
    SetupGraphics();
}

MainMenu::~MainMenu()
{
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteTextures(1, &backgroundTex);
    if (hudProgram) glDeleteProgram(hudProgram);
}

void MainMenu::LoadTextures()
{
    int w, h, comp;
    unsigned char* data = stbi_load("res/textures/menu_bg.png", &w, &h, &comp, 4);
    if (!data)
    {
        std::cerr << "[MainMenu] Failed to load menu_bg.png, usando color sólido" << std::endl;
        backgroundTex = 0;
        return;
    }
    glGenTextures(1, &backgroundTex);
    glBindTexture(GL_TEXTURE_2D, backgroundTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
}

void MainMenu::SetupGraphics()
{
    // Shader para rectángulos con textura
    const char* vertSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        uniform mat4 projection;
        uniform mat4 model;
        void main() {
            gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";
    const char* fragSrc = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D uTexture;
        uniform int uUseSolidColor;
        uniform vec4 uSolidColor;
        void main() {
            if (uUseSolidColor == 1)
                FragColor = uSolidColor;
            else
                FragColor = texture(uTexture, TexCoord);
        }
    )";
    hudProgram = compileShader(vertSrc, fragSrc);

    float quad[] = {
        // pos      // tex
        0.f, 0.f,   0.f, 1.f,
        0.f, 1.f,   0.f, 0.f,
        1.f, 1.f,   1.f, 0.f,

        0.f, 0.f,   0.f, 1.f,
        1.f, 1.f,   1.f, 0.f,
        1.f, 0.f,   1.f, 1.f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

MainMenu::Result MainMenu::Show()
{
    glm::mat4 proj = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    glUseProgram(hudProgram);
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "projection"), 1, GL_FALSE, &proj[0][0]);

    Result choice = Result::None;

    while (!glfwWindowShouldClose(window) && choice == Result::None)
    {
        glfwPollEvents();

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        // Dimensiones y posición de los botones (centrados en ventana 800x800)
        float bw = 300, bh = 70;
        float playX = (width - bw) * 0.5f;
        float playY = 280;
        float exitX = (width - bw) * 0.5f;
        float exitY = 380;

        bool overPlay = PointInRect((float)mx, (float)my, playX, playY, bw, bh);
        bool overExit = PointInRect((float)mx, (float)my, exitX, exitY, bw, bh);

        static bool wasPressed = false;
        if (leftPressed && !wasPressed)
        {
            if (overPlay)  choice = Result::Play;
            if (overExit)  choice = Result::Quit;
        }
        wasPressed = leftPressed;

        // --- RENDERIZADO ---
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        // Fondo (si hay textura, la usa; si no, color gris oscuro)
        if (backgroundTex != 0)
            RenderQuad(0, 0, (float)width, (float)height, backgroundTex);
        else
            RenderColoredQuad(0, 0, (float)width, (float)height, 0.1f, 0.1f, 0.15f);

        // Botón JUGAR (verde, más claro si está hover)
        float playR = overPlay ? 0.3f : 0.2f;
        float playG = overPlay ? 0.8f : 0.6f;
        float playB = overPlay ? 0.3f : 0.2f;
        RenderColoredQuad(playX, playY, bw, bh, playR, playG, playB);

        // Botón SALIR (rojo, más claro si está hover)
        float exitR = overExit ? 0.9f : 0.6f;
        float exitG = overExit ? 0.2f : 0.1f;
        float exitB = overExit ? 0.2f : 0.1f;
        RenderColoredQuad(exitX, exitY, bw, bh, exitR, exitG, exitB);

        glfwSwapBuffers(window);
    }
    return choice;
}

void MainMenu::RenderQuad(float x, float y, float w, float h, unsigned int texture)
{
    glUseProgram(hudProgram);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uUseSolidColor"), 0);
    glUniform1i(glGetUniformLocation(hudProgram, "uTexture"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void MainMenu::RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b)
{
    glUseProgram(hudProgram);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uUseSolidColor"), 1);
    glUniform4f(glGetUniformLocation(hudProgram, "uSolidColor"), r, g, b, 1.0f);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool MainMenu::PointInRect(float px, float py, float rx, float ry, float rw, float rh)
{
    return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}