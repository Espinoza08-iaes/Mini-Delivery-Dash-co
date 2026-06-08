#include "MainMenu.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cstring>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../../../third_party/stb/stb_easy_font.h"

// ------------------------------------------------------------------
// Compilación local de shaders
// ------------------------------------------------------------------
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

// ------------------------------------------------------------------
// Conversión de quads (stb_easy_font) a triángulos
// ------------------------------------------------------------------
static void convertQuadsToTriangles(float* quads, int numQuads, float* triangles)
{
    for (int i = 0; i < numQuads; i++)
    {
        float* quad = quads + i * 16;

        triangles[i * 24 + 0]  = quad[0];
        triangles[i * 24 + 1]  = quad[1];
        triangles[i * 24 + 2]  = quad[2];
        triangles[i * 24 + 3]  = quad[3];

        triangles[i * 24 + 4]  = quad[4];
        triangles[i * 24 + 5]  = quad[5];
        triangles[i * 24 + 6]  = quad[6];
        triangles[i * 24 + 7]  = quad[7];

        triangles[i * 24 + 8]  = quad[8];
        triangles[i * 24 + 9]  = quad[9];
        triangles[i * 24 + 10] = quad[10];
        triangles[i * 24 + 11] = quad[11];

        triangles[i * 24 + 12] = quad[0];
        triangles[i * 24 + 13] = quad[1];
        triangles[i * 24 + 14] = quad[2];
        triangles[i * 24 + 15] = quad[3];

        triangles[i * 24 + 16] = quad[8];
        triangles[i * 24 + 17] = quad[9];
        triangles[i * 24 + 18] = quad[10];
        triangles[i * 24 + 19] = quad[11];

        triangles[i * 24 + 20] = quad[12];
        triangles[i * 24 + 21] = quad[13];
        triangles[i * 24 + 22] = quad[14];
        triangles[i * 24 + 23] = quad[15];
    }
}

// ==================================================================
// MainMenu - Constructor / Destructor
// ==================================================================
MainMenu::MainMenu(GLFWwindow* win, int screenWidth, int screenHeight)
    : window(win), width(screenWidth), height(screenHeight),
      quadVAO(0), quadVBO(0), hudProgram(0),
      textVAO(0), textVBO(0), textProgram(0)
{
    SetupGraphics();
    SetupTextRendering();
}

MainMenu::~MainMenu()
{
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    if (hudProgram)  glDeleteProgram(hudProgram);
    if (textProgram) glDeleteProgram(textProgram);
}

// ------------------------------------------------------------------
void MainMenu::SetupGraphics()
{
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

// ------------------------------------------------------------------
void MainMenu::SetupTextRendering()
{
    const char* textVertSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        uniform mat4 projection;
        uniform mat4 model;
        void main() {
            gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
        }
    )";
    const char* textFragSrc = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";
    textProgram = compileShader(textVertSrc, textFragSrc);

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
}

// ------------------------------------------------------------------
void MainMenu::RenderText(const char* text, float x, float y, float r, float g, float b)
{
    static float quadBuffer[60000];
    int numQuads = stb_easy_font_print(0, 0, (char*)text, NULL, quadBuffer, sizeof(quadBuffer));
    if (numQuads <= 0) return;

    static float triBuffer[90000];
    convertQuadsToTriangles(quadBuffer, numQuads, triBuffer);

    glUseProgram(textProgram);

    glm::mat4 proj = glm::ortho(0.0f, (float)width, (float)height, 0.0f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(1.5f, 1.5f, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(textProgram, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform4f(glGetUniformLocation(textProgram, "uColor"), r, g, b, 1.0f);

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, numQuads * 6 * 4 * sizeof(float), triBuffer, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glDrawArrays(GL_TRIANGLES, 0, numQuads * 6);
    glBindVertexArray(0);
}

// ------------------------------------------------------------------
MainMenu::Result MainMenu::Show(bool showPause)
{
    Result choice = Result::None;

    // Ignorar primer frame de ESC si es pausa
    if (showPause)
    {
        glfwPollEvents();
    }

    while (!glfwWindowShouldClose(window) && choice == Result::None)
    {
        glfwPollEvents();

        // Si es pausa, ESC también cierra el menú
        if (showPause)
        {
            static bool escWasPressed = true;
            bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escPressed && !escWasPressed)
            {
                choice = Result::Play; // Play = continuar jugando
            }
            escWasPressed = escPressed;
        }

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        float bw = 300, bh = 70;
        float button1X = (width - bw) * 0.5f;
        float button1Y = 280;
        float button2X = (width - bw) * 0.5f;
        float button2Y = 380;

        bool overButton1 = PointInRect((float)mx, (float)my, button1X, button1Y, bw, bh);
        bool overButton2 = PointInRect((float)mx, (float)my, button2X, button2Y, bw, bh);

        static bool wasPressed = false;
        if (leftPressed && !wasPressed)
        {
            if (overButton1)  choice = Result::Play;       // Play / Resume
            if (overButton2)  choice = Result::Quit;       // Exit / Main Menu
        }
        wasPressed = leftPressed;

        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        // Fondo azul oscuro (igual en ambos modos)
        RenderColoredQuad(0, 0, (float)width, (float)height, 0.05f, 0.05f, 0.2f);

        // Botón 1: PLAY o RESUME
        float r1 = overButton1 ? 0.3f : 0.2f;
        float g1 = overButton1 ? 0.8f : 0.6f;
        float b1 = overButton1 ? 0.3f : 0.2f;
        RenderColoredQuad(button1X, button1Y, bw, bh, r1, g1, b1);
        RenderText(showPause ? "RESUME" : "PLAY", button1X + (showPause ? 70 : 95), button1Y + 22, 1.0f, 1.0f, 1.0f);

        // Botón 2: EXIT o MAIN MENU
        float r2 = overButton2 ? 0.9f : 0.6f;
        float g2 = overButton2 ? 0.2f : 0.1f;
        float b2 = overButton2 ? 0.2f : 0.1f;
        RenderColoredQuad(button2X, button2Y, bw, bh, r2, g2, b2);
        RenderText(showPause ? "MAIN MENU" : "EXIT", button2X + (showPause ? 55 : 95), button2Y + 22, 1.0f, 1.0f, 1.0f);

        glfwSwapBuffers(window);
    }
    return choice;
}

// ------------------------------------------------------------------
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

// ------------------------------------------------------------------
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

// ------------------------------------------------------------------
bool MainMenu::PointInRect(float px, float py, float rx, float ry, float rw, float rh)
{
    return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}