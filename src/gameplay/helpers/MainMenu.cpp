#include "MainMenu.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../../../third_party/stb/stb_easy_font.h"
#include "../../../third_party/stb/stb_image.h"

// ======================================================================
// SHADER COMPILATION
// ======================================================================
static unsigned int compileShader(const char* vsSrc, const char* fsSrc)
{
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, NULL); glCompileShader(vs);
    int ok; glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { char info[512]; glGetShaderInfoLog(vs, 512, NULL, info); std::cerr << "VS: " << info << std::endl; }

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, NULL); glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { char info[512]; glGetShaderInfoLog(fs, 512, NULL, info); std::cerr << "FS: " << info << std::endl; }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// ======================================================================
// QUAD TO TRIANGLES
// ======================================================================
static void convertQuadsToTrianglesClean(float* quads, int num, float* tris)
{
    for (int i = 0; i < num; i++)
    {
        float* q = quads + i * 16;
        memcpy(tris + i * 24 + 0,  q,      12 * sizeof(float));
        memcpy(tris + i * 24 + 12, q,       4 * sizeof(float));
        memcpy(tris + i * 24 + 16, q + 8,   4 * sizeof(float));
        memcpy(tris + i * 24 + 20, q + 12,  4 * sizeof(float));
    }
}

static float GetTextWidth(const char* text, float scale)
{
    float buf[60000];
    int n = stb_easy_font_print(0, 0, (char*)text, NULL, buf, sizeof(buf));
    if (n <= 0) return 0;
    float mn = buf[0], mx = buf[0];
    for (int i = 0; i < n; i++)
    {
        float* q = buf + i * 16;
        for (int j = 0; j < 4; j++) { float x = q[j * 4]; if (x < mn) mn = x; if (x > mx) mx = x; }
    }
    return (mx - mn) * scale;
}

static bool IsMouseOverButtonPixel(float mx, float my, float cx, float cy,
                                   float dw, float dh,
                                   const std::vector<unsigned char>& alpha,
                                   int tw, int th)
{
    if (alpha.empty() || tw <= 0 || th <= 0) return false;
    float L = cx - dw * 0.5f, T = cy - dh * 0.5f;
    if (mx < L || mx > L + dw || my < T || my > T + dh) return false;
    float u = (mx - L) / dw, v = (my - T) / dh;
    int tx = (int)(u * tw), ty = (int)(v * th);
    if (tx < 0) tx = 0; if (tx >= tw) tx = tw - 1;
    if (ty < 0) ty = 0; if (ty >= th) ty = th - 1;
    return alpha[ty * tw + tx] > 128;
}

// ======================================================================
// CONSTRUCTOR / DESTRUCTOR
// ======================================================================
MainMenu::MainMenu(GLFWwindow* w, int sw, int sh)
    : window(w), width(sw), height(sh),
      quadVAO(0), quadVBO(0), hudProgram(0),
      textVAO(0), textVBO(0), textProgram(0),
      backgroundTexture(0), bgTexWidth(0), bgTexHeight(0),
      playTexture(0), playTexWidth(0), playTexHeight(0),
      resumeTexture(0), resumeTexWidth(0), resumeTexHeight(0),
      howToPlayTexture(0), howToPlayTexWidth(0), howToPlayTexHeight(0),
      exitTexture(0), exitTexWidth(0), exitTexHeight(0),
      mainMenuTexture(0), mainMenuTexWidth(0), mainMenuTexHeight(0),
      shopTexture(0), shopTexWidth(0), shopTexHeight(0),
      settingsTexture(0), settingsTexWidth(0), settingsTexHeight(0),
      helpIconTexture(0), helpIconTexWidth(0), helpIconTexHeight(0),
      pauseBackgroundTexture(0)
{
    SetupGraphics();
    SetupTextRendering();
    LoadAllTextures();
}

MainMenu::~MainMenu()
{
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &textVAO); glDeleteBuffers(1, &textVBO);
    if (backgroundTexture) glDeleteTextures(1, &backgroundTexture);
    if (pauseDecorTexture) glDeleteTextures(1, &pauseDecorTexture);
    if (playTexture)       glDeleteTextures(1, &playTexture);
    if (resumeTexture)     glDeleteTextures(1, &resumeTexture);
    if (howToPlayTexture)  glDeleteTextures(1, &howToPlayTexture);
    if (exitTexture)       glDeleteTextures(1, &exitTexture);
    if (mainMenuTexture)   glDeleteTextures(1, &mainMenuTexture);
    if (shopTexture)       glDeleteTextures(1, &shopTexture);
    if (settingsTexture)   glDeleteTextures(1, &settingsTexture);
    if (helpIconTexture && helpIconTexture != howToPlayTexture) glDeleteTextures(1, &helpIconTexture);
    if (hudProgram)        glDeleteProgram(hudProgram);
    if (textProgram)       glDeleteProgram(textProgram);
}

void MainMenu::SetPauseBackground(unsigned int texture)
{
    pauseBackgroundTexture = texture;
}

// ======================================================================
// SETUP
// ======================================================================
void MainMenu::SetupGraphics()
{
    const char* vs = R"(#version 330 core
        layout(location=0) in vec2 aPos; layout(location=1) in vec2 aTex;
        out vec2 Tex; uniform mat4 proj, model;
        void main(){ gl_Position = proj * model * vec4(aPos,0,1); Tex = aTex; })";
    const char* fs = R"(#version 330 core
        out vec4 frag; in vec2 Tex;
        uniform sampler2D uTex; uniform int uSolid; uniform vec4 uColor;
        uniform vec4 uTexCoordTransform;
        uniform float uSheen;
        uniform float uFlickerTime;
        uniform int uCloudLayer;
        uniform float uCloudTime;
        void main(){ 
            if (uCloudLayer == 1) {
                float speed = uCloudTime * 0.035;
                float n1 = sin(Tex.x * 4.0 - speed) * cos(Tex.y * 3.0 + speed * 0.4);
                float n2 = cos(Tex.x * 8.0 + speed * 0.8) * sin(Tex.y * 5.0 - speed * 0.6);
                float n3 = sin(Tex.x * 16.0 - speed * 1.5) * cos(Tex.y * 10.0 + speed * 0.9);
                float density = (n1 * 0.5 + n2 * 0.3 + n3 * 0.2) * 0.5 + 0.5;
                density *= smoothstep(0.0, 0.3, Tex.y) * smoothstep(1.0, 0.7, Tex.y);
                vec3 colClouds = mix(vec3(0.9, 0.45, 0.2), vec3(0.18, 0.12, 0.22), Tex.y);
                frag = vec4(colClouds, density * 0.38);
                return;
            }
            vec2 uv = Tex * uTexCoordTransform.xy + uTexCoordTransform.zw;
            vec4 texColor = uSolid==1 ? uColor : texture(uTex,uv); 
            if (uSolid == 0 && uSheen > -5.0) {
                float linePos = Tex.x + Tex.y;
                float dist = abs(linePos - uSheen);
                float width = 0.15;
                float factor = smoothstep(width, 0.0, dist);
                texColor.rgb += vec3(factor * 0.25);
            }
            if (uSolid == 0 && uFlickerTime > 0.0) {
                if (uv.y < 0.45 && texColor.r > 0.48 && texColor.g > 0.38 && texColor.b < 0.35) {
                    float freq = 2.0 + sin(uv.x * 200.0) * 1.0;
                    float phase = uv.x * 500.0 + uv.y * 300.0;
                    float noise = sin(uFlickerTime * freq + phase);
                    float flicker = 0.75 + 0.35 * noise;
                    if (flicker < 0.5) flicker = 0.2;
                    else if (flicker > 0.95) flicker = 1.25;
                    texColor.rgb *= flicker;
                }
            }
            frag = texColor;
        })";
    hudProgram = compileShader(vs, fs);

    float quad[] = { 0,0,0,1, 0,1,0,0, 1,1,1,0, 0,0,0,1, 1,1,1,0, 1,0,1,1 };
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);
}

void MainMenu::SetupTextRendering()
{
    const char* vs = R"(#version 330 core
        layout(location=0) in vec2 aPos; uniform mat4 proj, model;
        void main(){ gl_Position = proj * model * vec4(aPos,0,1); })";
    const char* fs = R"(#version 330 core
        out vec4 frag; uniform vec4 uColor; void main(){ frag = uColor; })";
    textProgram = compileShader(vs, fs);
    glGenVertexArrays(1, &textVAO); glGenBuffers(1, &textVBO);
}

// ======================================================================
// TEXTURE LOADING
// ======================================================================
unsigned int MainMenu::LoadTextureFile(const char* path, int& w, int& h, std::vector<unsigned char>& alpha)
{
    int comp;
    unsigned char* data = stbi_load(path, &w, &h, &comp, 4);
    if (!data) { std::cerr << "[MainMenu] Failed: " << path << std::endl; return 0; }
    int total = w * h;
    alpha.resize(total);
    for (int i = 0; i < total; i++) alpha[i] = data[i * 4 + 3];
    unsigned int tex;
    glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);
    std::cout << "[MainMenu] Loaded: " << path << " (" << w << "x" << h << ")" << std::endl;
    return tex;
}

void MainMenu::LoadAllTextures()
{
    stbi_set_flip_vertically_on_load(true);
    std::vector<unsigned char> dummy;
    backgroundTexture = LoadTextureFile("res/textures/BackGround_STB.png", bgTexWidth, bgTexHeight, dummy);
    pauseDecorTexture = LoadTextureFile("res/textures/PauseMenu_STB.png", pauseDecorTexWidth, pauseDecorTexHeight, dummy);
    playTexture       = LoadTextureFile("res/textures/Play_STB.png", playTexWidth, playTexHeight, playAlpha);
    resumeTexture     = LoadTextureFile("res/textures/Resume_STB.png", resumeTexWidth, resumeTexHeight, resumeAlpha);
    howToPlayTexture  = LoadTextureFile("res/textures/HowToPlay_STB.png", howToPlayTexWidth, howToPlayTexHeight, howToPlayAlpha);
    helpIconTexture   = howToPlayTexture;
    helpIconTexWidth  = howToPlayTexWidth;
    helpIconTexHeight = howToPlayTexHeight;
    helpIconAlpha     = howToPlayAlpha;
    settingsTexture   = LoadTextureFile("res/textures/Settings_STB.png", settingsTexWidth, settingsTexHeight, settingsAlpha);
    exitTexture       = LoadTextureFile("res/textures/Exit_STB.png", exitTexWidth, exitTexHeight, exitAlpha);
    mainMenuTexture   = LoadTextureFile("res/textures/BackToMenu_STB.png", mainMenuTexWidth, mainMenuTexHeight, mainMenuAlpha);
    shopTexture       = LoadTextureFile("res/textures/Shop_STB.png", shopTexWidth, shopTexHeight, shopAlpha);
}

// ======================================================================
// RENDERING
// ======================================================================
void MainMenu::RenderText(const char* text, float x, float y, float r, float g, float b, float scale)
{
    static float qb[60000]; int n = stb_easy_font_print(0, 0, (char*)text, NULL, qb, sizeof(qb));
    if (n <= 0) return;
    static float tb[90000]; convertQuadsToTrianglesClean(qb, n, tb);
    glUseProgram(textProgram);
    int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(x, y, 0));
    model = glm::scale(model, glm::vec3(scale, scale, 1));
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "proj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform4f(glGetUniformLocation(textProgram, "uColor"), r, g, b, 1);
    glBindVertexArray(textVAO); glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, n * 6 * 16, tb, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, n * 6); glBindVertexArray(0);
}

void MainMenu::RenderTextCentered(const char* text, float cx, float y, float r, float g, float b, float s)
{
    float w = GetTextWidth(text, s);
    RenderText(text, cx - w * 0.5f, y, r, g, b, s);
}

void MainMenu::RenderButtonImage(float cx, float cy, float maxW, unsigned int tex, int tw, int th)
{
    if (!tex || tw <= 0 || th <= 0) return;
    float asp = (float)tw / th;
    float dw = maxW, dh = maxW / asp;
    RenderQuad(cx - dw * 0.5f, cy - dh * 0.5f, dw, dh, tex);
}

void MainMenu::RenderButtonImageFixed(float cx, float cy, float w, float h, unsigned int tex)
{
    if (!tex) return;
    RenderQuad(cx - w * 0.5f, cy - h * 0.5f, w, h, tex);
}

void MainMenu::RenderBackgroundImage(float fbW, float fbH, float timeVal)
{
    if (!backgroundTexture || bgTexWidth <= 0) { RenderColoredQuad(0, 0, fbW, fbH, 0, 0, 0); return; }
    glUseProgram(hudProgram);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(0, 0, 0));
    model = glm::scale(model, glm::vec3(fbW, fbH, 1));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 0);
    glUniform1i(glGetUniformLocation(hudProgram, "uTex"), 0);
    glUniform4f(glGetUniformLocation(hudProgram, "uTexCoordTransform"), 1.0f, 1.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uSheen"), -10.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uFlickerTime"), timeVal);
    glUniform1i(glGetUniformLocation(hudProgram, "uCloudLayer"), 0);
    glUniform1f(glGetUniformLocation(hudProgram, "uCloudTime"), 0.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, backgroundTexture);
    glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
}

void MainMenu::RenderClouds(float fbW, float fbH, float timeVal)
{
    glUseProgram(hudProgram);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(0, 0, 0));
    model = glm::scale(model, glm::vec3(fbW, fbH * 0.5f, 1));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 0);
    glUniform1i(glGetUniformLocation(hudProgram, "uTex"), 0);
    glUniform4f(glGetUniformLocation(hudProgram, "uTexCoordTransform"), 1.0f, 1.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uSheen"), -10.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uFlickerTime"), 0.0f);
    glUniform1i(glGetUniformLocation(hudProgram, "uCloudLayer"), 1);
    glUniform1f(glGetUniformLocation(hudProgram, "uCloudTime"), timeVal);
    glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
    glUniform1i(glGetUniformLocation(hudProgram, "uCloudLayer"), 0);
}

void MainMenu::RenderQuad(float x, float y, float w, float h, unsigned int tex)
{
    glUseProgram(hudProgram);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(x, y, 0));
    model = glm::scale(model, glm::vec3(w, h, 1));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 0);
    glUniform1i(glGetUniformLocation(hudProgram, "uTex"), 0);
    glUniform4f(glGetUniformLocation(hudProgram, "uTexCoordTransform"), 1.0f, 1.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uSheen"), -10.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uFlickerTime"), 0.0f);
    glUniform1i(glGetUniformLocation(hudProgram, "uCloudLayer"), 0);
    glUniform1f(glGetUniformLocation(hudProgram, "uCloudTime"), 0.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
}

void MainMenu::RenderQuadSubset(float x, float y, float w, float h, unsigned int tex, float sx, float sy, float sw, float sh, float sheenVal)
{
    glUseProgram(hudProgram);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(x, y, 0));
    model = glm::scale(model, glm::vec3(w, h, 1));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 0);
    glUniform1i(glGetUniformLocation(hudProgram, "uTex"), 0);
    glUniform4f(glGetUniformLocation(hudProgram, "uTexCoordTransform"), sw, sh, sx, sy);
    glUniform1f(glGetUniformLocation(hudProgram, "uSheen"), sheenVal);
    glUniform1f(glGetUniformLocation(hudProgram, "uFlickerTime"), 0.0f);
    glUniform1i(glGetUniformLocation(hudProgram, "uCloudLayer"), 0);
    glUniform1f(glGetUniformLocation(hudProgram, "uCloudTime"), 0.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
}

void MainMenu::RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glUseProgram(hudProgram);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(x, y, 0));
    model = glm::scale(model, glm::vec3(w, h, 1));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 1);
    glUniform4f(glGetUniformLocation(hudProgram, "uColor"), r, g, b, a);
    glUniform4f(glGetUniformLocation(hudProgram, "uTexCoordTransform"), 1.0f, 1.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uSheen"), -10.0f);
    glUniform1f(glGetUniformLocation(hudProgram, "uFlickerTime"), 0.0f);
    glUniform1i(glGetUniformLocation(hudProgram, "uCloudLayer"), 0);
    glUniform1f(glGetUniformLocation(hudProgram, "uCloudTime"), 0.0f);
    glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool MainMenu::PointInRect(float px, float py, float rx, float ry, float rw, float rh)
{
    return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}

// ======================================================================
// HOW TO PLAY
// ======================================================================
void MainMenu::ShowHowToPlay(unsigned int bgTex)
{
    double startTime = glfwGetTime();
    double lastT = startTime;
    float fadeAlpha = 1.0f;
    float fadeOutAlpha = 0.0f;
    bool isFadingOut = false;
    float fadeDuration = 0.3f;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH); glViewport(0, 0, fbW, fbH);
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        int wW, wH; glfwGetWindowSize(window, &wW, &wH);
        mx *= (float)fbW / wW; my *= (float)fbH / wH;
        bool click = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        double currentT = glfwGetTime();
        float dt = (float)(currentT - lastT);
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastT = currentT;

        if (!isFadingOut)
        {
            if (fadeAlpha > 0.0f) { fadeAlpha -= dt / fadeDuration; if (fadeAlpha < 0.0f) fadeAlpha = 0.0f; }
        }
        else
        {
            fadeOutAlpha += dt / fadeDuration;
            if (fadeOutAlpha >= 1.0f) return;
        }

        float bw = 200, bh = 50;
        float bx = (fbW - bw) * 0.5f, by = fbH - 120.f;
        float bob = sin((float)currentT * 2.5f) * 5.0f;
        float drawYBase = by + bob;

        bool over = PointInRect((float)mx, (float)my, bx, drawYBase, bw, bh);
        static bool was = false;
        if (click && !was && over && !isFadingOut) isFadingOut = true;
        was = click;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !isFadingOut) isFadingOut = true;

        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
        glUseProgram(hudProgram);
        glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);

        if (bgTex) {
            RenderQuad(0, 0, (float)fbW, (float)fbH, bgTex);
        } else {
            RenderBackgroundImage((float)fbW, (float)fbH);
        }
        RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, 0.30f);

        float pw = fbW * 0.75f, ph = fbH * 0.60f;
        float px = (fbW - pw) * 0.5f, py = (fbH - ph) * 0.5f;
        RenderColoredQuad(px, py, pw, ph, 0.05f, 0.05f, 0.05f, 0.95f);
        float bd = 2, bc = 0.25f;
        RenderColoredQuad(px - bd, py - bd, pw + bd * 2, bd, bc, bc, bc);
        RenderColoredQuad(px - bd, py + ph, pw + bd * 2, bd, bc, bc, bc);
        RenderColoredQuad(px - bd, py, bd, ph, bc, bc, bc);
        RenderColoredQuad(px + pw, py, bd, ph, bc, bc, bc);

        RenderTextCentered("HOW TO PLAY", px + pw * 0.5f, py + 30, 0.85f, 0.85f, 0.85f, 1.8f);
        RenderColoredQuad(px + 40, py + 70, pw - 80, 1, 0.3f, 0.3f, 0.3f);

        const char* lines[] = {
            "W / S       - Accelerate / Brake", "A / D       - Steer Left / Right",
            "Z           - Jump", "R           - Respawn", "L           - Toggle Headlights",
            "SHIFT       - Nitro Boost", "ESC         - Pause Menu", "RIGHT CLICK - Orbit Camera",
        };
        for (int i = 0; i < 8; i++)
            RenderText(lines[i], px + 60, py + 105 + i * 38, 0.65f, 0.65f, 0.65f, 1.3f);

        float s = 1.0f, yOff = 0.0f;
        if (over) { if (click) { s = 0.92f; yOff = 4.0f; } else { s = 1.08f; } }

        float drawW = bw * s, drawH = bh * s;
        float drawX = bx + (bw - drawW) * 0.5f, drawY = drawYBase + (bh - drawH) * 0.5f + yOff;

        RenderColoredQuad(drawX, drawY, drawW, drawH, 0.1f, 0.1f, 0.1f);
        float bt = 1.2f * s, bcol = 0.3f;
        RenderColoredQuad(drawX, drawY, drawW, bt, bcol, bcol, bcol);
        RenderColoredQuad(drawX, drawY + drawH - bt, drawW, bt, bcol, bcol, bcol);
        RenderColoredQuad(drawX, drawY, bt, drawH, bcol, bcol, bcol);
        RenderColoredQuad(drawX + drawW - bt, drawY, bt, drawH, bcol, bcol, bcol);
        RenderTextCentered("BACK", drawX + drawW * 0.5f, drawY + (drawH - 18.0f * s) * 0.5f, 0.75f, 0.75f, 0.75f, 1.3f * s);

        float overlayAlpha = isFadingOut ? fadeOutAlpha : fadeAlpha;
        if (overlayAlpha > 0.0f) RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, overlayAlpha);

        glfwSwapBuffers(window);
    }
}

// ======================================================================
// MAIN MENU / PAUSE
// ======================================================================
MainMenu::Result MainMenu::Show(bool pause)
{
    Result choice = Result::None;
    bool escWas = false;
    if (pause) escWas = true;

    double startTime = glfwGetTime();
    double lastT = startTime;
    float fadeAlpha = 1.0f;
    float fadeOutAlpha = 0.0f;
    bool isFadingOut = false;
    Result pendingChoice = Result::None;
    float fadeDuration = 0.5f;

    // Pre-assign texture arrays once before the loop
    const std::vector<unsigned char>* al[4];
    unsigned int texID[4]; int tw[4], th[4];
    if (pause) {
        texID[0]=resumeTexture;    tw[0]=resumeTexWidth;     th[0]=resumeTexHeight;     al[0]=&resumeAlpha;
        texID[1]=shopTexture;      tw[1]=shopTexWidth;       th[1]=shopTexHeight;       al[1]=&shopAlpha;
        texID[2]=settingsTexture;  tw[2]=settingsTexWidth;   th[2]=settingsTexHeight;   al[2]=&settingsAlpha;
        texID[3]=mainMenuTexture;  tw[3]=mainMenuTexWidth;   th[3]=mainMenuTexHeight;   al[3]=&mainMenuAlpha;
    } else {
        texID[0]=playTexture;      tw[0]=playTexWidth;       th[0]=playTexHeight;       al[0]=&playAlpha;
        texID[1]=shopTexture;      tw[1]=shopTexWidth;       th[1]=shopTexHeight;       al[1]=&shopAlpha;
        texID[2]=settingsTexture;  tw[2]=settingsTexWidth;   th[2]=settingsTexHeight;   al[2]=&settingsAlpha;
        texID[3]=exitTexture;      tw[3]=exitTexWidth;       th[3]=exitTexHeight;       al[3]=&exitAlpha;
    }

    while (!glfwWindowShouldClose(window) && choice == Result::None)
    {
        glfwPollEvents();
        int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH); glViewport(0, 0, fbW, fbH);
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        int wW, wH; glfwGetWindowSize(window, &wW, &wH);
        mx *= (float)fbW / wW; my *= (float)fbH / wH;
        bool click = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        double currentT = glfwGetTime();
        float dt = (float)(currentT - lastT);
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;
        lastT = currentT;

        if (!isFadingOut)
        {
            if (fadeAlpha > 0.0f) { fadeAlpha -= dt / fadeDuration; if (fadeAlpha < 0.0f) fadeAlpha = 0.0f; }
        }
        else
        {
            fadeOutAlpha += dt / fadeDuration;
            if (fadeOutAlpha >= 1.0f) { fadeOutAlpha = 1.0f; choice = pendingChoice; }
        }

        if (pause && !isFadingOut) {
            bool esc = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (esc && !escWas) { pendingChoice = Result::Play; isFadingOut = true; }
            escWas = esc;
        }

        float ref = std::min((float)fbW, (float)fbH);
        float btnMaxW = std::min(std::max(ref * 0.3f, 200.f), 400.f);

        float btnW = btnMaxW;
        float btnH = btnMaxW / 3.2f;

        float dw[4], dh[4];
        if (pause) {
            float uniformW = btnW * 1.1f;
            dw[0] = uniformW; dh[0] = btnH * 1.0f;
            dw[1] = uniformW; dh[1] = btnH * 1.0f;
            dw[2] = uniformW; dh[2] = btnH * 1.0f;
            dw[3] = uniformW; dh[3] = btnH * 1.0f;
        } else {
            dw[0] = btnW * 1.0f;  dh[0] = btnH * 1.0f;
            dw[1] = btnW * 1.15f; dh[1] = btnH * 1.0f;
            dw[2] = btnW * 1.15f; dh[2] = btnH * 1.0f;
            dw[3] = btnW * 0.85f; dh[3] = btnH * 1.0f;
        }

        float sp = 10.0f;
        float totalH = dh[0] + dh[1] + dh[2] + dh[3] + sp * 3;
        float startY = (fbH - totalH) * 0.5f;

        float cy[4] = {
            startY + dh[0] * 0.5f,
            startY + dh[0] + sp + dh[1] * 0.5f,
            startY + dh[0] + sp + dh[1] + sp + dh[2] * 0.5f,
            startY + dh[0] + sp + dh[1] + sp + dh[2] + sp + dh[3] * 0.5f
        };
        float cx = fbW * 0.13f;

        bool over[4];
        for (int i = 0; i < 4; i++)
            over[i] = IsMouseOverButtonPixel((float)mx, (float)my, cx, cy[i], dw[i], dh[i], *al[i], tw[i], th[i]);

        // "?" help icon — glued to top-right corner regardless of size
        float helpSize = 230.0f;

        float helpDisplayW = helpSize;
        float helpDisplayH = helpSize;
        if (helpIconTexWidth > 0 && helpIconTexHeight > 0)
        {
            float helpAsp = (float)helpIconTexWidth / (float)helpIconTexHeight;
            helpDisplayH = helpSize / helpAsp;
        }

        float helpRight = fbW - 20.0f;
        float helpTop = 85.0f;
        float helpCx = helpRight - helpDisplayW * 0.5f;
        float helpCy = helpTop + helpDisplayH * 0.5f;

        bool helpOver = !pause && IsMouseOverButtonPixel((float)mx, (float)my, helpCx, helpCy, helpDisplayW, helpDisplayH, helpIconAlpha, helpIconTexWidth, helpIconTexHeight);

        static bool wasCl = false;
        if (click && !wasCl && !isFadingOut) {
            if (over[0])      { pendingChoice = Result::Play;      isFadingOut = true; }
            else if (over[1]) { choice = Result::Shop; click = false; }
            else if (over[2]) { pendingChoice = Result::Settings;  isFadingOut = true; }
            else if (over[3]) { pendingChoice = Result::Quit;      isFadingOut = true; }
            else if (helpOver) { choice = Result::HowToPlay; click = false; }
        }
        wasCl = click;

        // RENDER
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
        glUseProgram(hudProgram);
        glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);

        if (pause) {
            if (pauseBackgroundTexture != 0) RenderQuad(0, 0, (float)fbW, (float)fbH, pauseBackgroundTexture);
            RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, 0.75f);
            if (pauseDecorTexture != 0) RenderQuad(0, 0, (float)fbW, (float)fbH, pauseDecorTexture);
        }
        else {
            RenderBackgroundImage((float)fbW, (float)fbH, (float)currentT);
            RenderClouds((float)fbW, (float)fbH, (float)currentT);
        }

        for (int i = 0; i < 4; i++)
        {
            float s = 1.0f, yOff = 0.0f;
            if (over[i]) { if (click) { s = 0.92f; yOff = 4.0f; } else { s = 1.08f; } }
            RenderButtonImageFixed(cx, cy[i] + yOff, dw[i] * s, dh[i] * s, texID[i]);
        }

        if (!pause && helpIconTexture)
        {
            float hs = 1.0f;
            if (helpOver) hs = click ? 0.92f : 1.08f;
            RenderQuad(helpCx - helpDisplayW * 0.5f * hs, helpCy - helpDisplayH * 0.5f * hs,
                    helpDisplayW * hs, helpDisplayH * hs, helpIconTexture);
        }

        float overlayAlpha = isFadingOut ? fadeOutAlpha : fadeAlpha;
        if (overlayAlpha > 0.0f) RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, overlayAlpha);

        if (choice == Result::Shop || choice == Result::HowToPlay) {
            // Capture the back buffer and create a BLURRED version for the fade effect
            unsigned char* pixels = new unsigned char[fbW * fbH * 3];
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, fbW, fbH, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            
            // Downsample to 1/6th resolution with averaging for blur effect
            int smallW = fbW / 6;
            int smallH = fbH / 6;
            if (smallW < 1) smallW = 1;
            if (smallH < 1) smallH = 1;
            unsigned char* blurred = new unsigned char[smallW * smallH * 3];
            for (int y = 0; y < smallH; y++) {
                for (int x = 0; x < smallW; x++) {
                    int r = 0, g = 0, b = 0, count = 0;
                    for (int dy = 0; dy < 6; dy++) {
                        for (int dx = 0; dx < 6; dx++) {
                            int srcX = x * 6 + dx;
                            int srcY = y * 6 + dy;
                            if (srcX < fbW && srcY < fbH) {
                                int idx = (srcY * fbW + srcX) * 3;
                                r += pixels[idx];
                                g += pixels[idx + 1];
                                b += pixels[idx + 2];
                                count++;
                            }
                        }
                    }
                    int dstIdx = (y * smallW + x) * 3;
                    blurred[dstIdx]     = (unsigned char)(r / count);
                    blurred[dstIdx + 1] = (unsigned char)(g / count);
                    blurred[dstIdx + 2] = (unsigned char)(b / count);
                }
            }
            delete[] pixels;
            
            glGenTextures(1, &capturedBg);
            glBindTexture(GL_TEXTURE_2D, capturedBg);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, smallW, smallH, 0, GL_RGB, GL_UNSIGNED_BYTE, blurred);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            delete[] blurred;
            
            std::cout << "[CAPTURE] Blurred texture " << smallW << "x" << smallH << " texID=" << capturedBg << std::endl;
            
            if (choice == Result::HowToPlay) {
                ShowHowToPlay(capturedBg);
                glDeleteTextures(1, &capturedBg);
                capturedBg = 0;
                choice = Result::None; // continue main menu loop
            }
        }

        glfwSwapBuffers(window);
    }
    return choice;
}

void MainMenu::RenderLoading(float progress, const char* message)
{
    static float displayedProgress = 0.0f;
    static double animStartTime = glfwGetTime();
    float catchUpSpeed = 3.5f;
    displayedProgress += (progress - displayedProgress) * catchUpSpeed * 0.016f;
    if (displayedProgress < progress) displayedProgress = progress;
    if (displayedProgress > 1.0f) displayedProgress = 1.0f;

    float p = displayedProgress;
    double time = glfwGetTime();
    int fbW = width, fbH = height;

    glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glUseProgram(hudProgram);
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);

    float particleAlpha = 0.12f;
    for (int i = 0; i < 30; i++)
    {
        float seed = (float)i * 137.508f;
        float phase = sin((float)time * 0.7f + seed) * 0.5f + 0.5f;
        float px = fmod(seed * 0.618f, 1.0f) * fbW;
        float py = fmod(seed * 0.382f + (float)time * 0.03f, 1.0f) * fbH;
        float size = 2.0f + phase * 3.0f;
        float alpha = particleAlpha * (0.3f + phase * 0.7f);
        RenderColoredQuad(px, py, size, size, 1.0f, 1.0f, 1.0f, alpha);
    }

    float pw = 580.0f, ph = 185.0f;
    float centerX = fbW * 0.5f, centerY = fbH * 0.5f;
    float px = centerX - pw * 0.5f, py = centerY - ph * 0.5f;

    float glowSize = 8.0f;
    RenderColoredQuad(px - glowSize, py - glowSize, pw + glowSize * 2, ph + glowSize * 2, 0.25f, 0.25f, 0.30f, 0.5f);
    RenderColoredQuad(px, py, pw, ph, 0.08f, 0.08f, 0.10f, 0.92f);

    float accentHeight = 2.5f;
    RenderColoredQuad(px, py, pw * p, accentHeight, 0.75f, 0.75f, 0.82f, 0.9f);
    RenderColoredQuad(px + pw * p, py, pw * (1.0f - p), accentHeight, 0.15f, 0.15f, 0.18f, 0.4f);

    float borderThick = 1.2f, cornerLen = 40.0f, cornerAlpha = 0.6f, borderAlpha = 0.25f;
    RenderColoredQuad(px, py, pw, borderThick, 1.0f, 1.0f, 1.0f, borderAlpha);
    RenderColoredQuad(px, py + ph - borderThick, pw, borderThick, 1.0f, 1.0f, 1.0f, borderAlpha);
    RenderColoredQuad(px, py, borderThick, ph, 1.0f, 1.0f, 1.0f, borderAlpha);
    RenderColoredQuad(px + pw - borderThick, py, borderThick, ph, 1.0f, 1.0f, 1.0f, borderAlpha);

    RenderColoredQuad(px, py, cornerLen, borderThick * 1.5f, 1.0f, 1.0f, 1.0f, cornerAlpha);
    RenderColoredQuad(px, py, borderThick * 1.5f, cornerLen, 1.0f, 1.0f, 1.0f, cornerAlpha);
    RenderColoredQuad(px + pw - cornerLen, py, cornerLen, borderThick * 1.5f, 1.0f, 1.0f, 1.0f, cornerAlpha);
    RenderColoredQuad(px + pw - borderThick * 1.5f, py, borderThick * 1.5f, cornerLen, 1.0f, 1.0f, 1.0f, cornerAlpha);
    RenderColoredQuad(px, py + ph - borderThick * 1.5f, cornerLen, borderThick * 1.5f, 1.0f, 1.0f, 1.0f, cornerAlpha);
    RenderColoredQuad(px, py + ph - cornerLen, borderThick * 1.5f, cornerLen, 1.0f, 1.0f, 1.0f, cornerAlpha);
    RenderColoredQuad(px + pw - cornerLen, py + ph - borderThick * 1.5f, cornerLen, borderThick * 1.5f, 1.0f, 1.0f, 1.0f, cornerAlpha);
    RenderColoredQuad(px + pw - borderThick * 1.5f, py + ph - cornerLen, borderThick * 1.5f, cornerLen, 1.0f, 1.0f, 1.0f, cornerAlpha);

    float dotY = py + 42.0f;
    for (int i = 0; i < 3; i++)
    {
        float dotPhase = (float)time * 3.0f + i * 2.094f;
        float dotAlpha = 0.3f + sin(dotPhase) * 0.4f;
        float dotX = centerX + (i - 1) * 16.0f;
        RenderColoredQuad(dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f, 1.0f, 1.0f, 1.0f, dotAlpha);
    }

    float msgY = py + 62.0f;
    RenderTextCentered(message, centerX, msgY, 0.82f, 0.82f, 0.85f, 1.25f);

    float barW = pw * 0.72f, barH = 5.0f;
    float barX = centerX - barW * 0.5f, barY = py + 100.0f;
    RenderColoredQuad(barX, barY, barW, barH, 0.18f, 0.18f, 0.20f, 1.0f);

    float fillW = barW * p;
    if (fillW > 0.0f)
    {
        RenderColoredQuad(barX, barY, fillW, barH, 0.85f, 0.85f, 0.88f, 1.0f);
        float edgeW = std::min(3.0f, fillW);
        RenderColoredQuad(barX + fillW - edgeW, barY, edgeW, barH, 1.0f, 1.0f, 1.0f, 0.9f);
        RenderColoredQuad(barX, barY + barH, fillW, 2.0f, 0.65f, 0.65f, 0.70f, 0.25f);
    }

    float pctPulse = 1.0f + sin((float)time * 2.5f) * 0.02f;
    char pct[16]; sprintf(pct, "%d%%", (int)(p * 100.0f));
    float pctY = barY + 28.0f;
    RenderTextCentered(pct, centerX, pctY, 0.55f, 0.55f, 0.58f, 1.05f * pctPulse);

    float sepY = pctY + 24.0f, sepW = pw * 0.3f;
    RenderColoredQuad(centerX - sepW * 0.5f, sepY, sepW, 0.8f, 0.35f, 0.35f, 0.38f, 0.35f);

    float orbX = barX + fillW, orbY = barY + barH * 0.5f, orbRadius = 8.0f;
    int orbSegments = 16;
    for (int i = 0; i < orbSegments; i++)
    {
        float angle1 = (float)i / orbSegments * 6.28318f;
        float angle2 = (float)(i + 1) / orbSegments * 6.28318f;
        float r1 = orbRadius * 0.85f, r2 = orbRadius;
        float cx1 = orbX + cos(angle1) * r1, cy1 = orbY + sin(angle1) * r1;
        float segAlpha = 0.7f + sin((float)time * 4.0f + (float)i) * 0.3f;
        RenderColoredQuad(cx1 - 1.5f, cy1 - 1.5f, 3.0f, 3.0f, 1.0f, 1.0f, 1.0f, segAlpha * 0.6f);
    }
    RenderColoredQuad(orbX - 3.0f, orbY - 3.0f, 6.0f, 6.0f, 1.0f, 1.0f, 1.0f, 0.9f);

    float ringAlpha = 0.25f + sin((float)time * 3.0f) * 0.1f;
    for (int i = 0; i < 12; i++)
    {
        float angle = (float)i / 12.0f * 6.28318f;
        float rx = orbX + cos(angle) * orbRadius * 1.6f, ry = orbY + sin(angle) * orbRadius * 1.6f;
        RenderColoredQuad(rx - 2.0f, ry - 2.0f, 4.0f, 4.0f, 1.0f, 1.0f, 1.0f, ringAlpha);
    }

    glfwSwapBuffers(window);
}