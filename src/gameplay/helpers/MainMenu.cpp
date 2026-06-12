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
    if (playTexture)       glDeleteTextures(1, &playTexture);
    if (resumeTexture)     glDeleteTextures(1, &resumeTexture);
    if (howToPlayTexture)  glDeleteTextures(1, &howToPlayTexture);
    if (exitTexture)       glDeleteTextures(1, &exitTexture);
    if (mainMenuTexture)   glDeleteTextures(1, &mainMenuTexture);
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
                    if (flicker < 0.5) {
                        flicker = 0.2;
                    } else if (flicker > 0.95) {
                        flicker = 1.25;
                    }
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
    playTexture       = LoadTextureFile("res/textures/Play_STB.png", playTexWidth, playTexHeight, playAlpha);
    resumeTexture     = LoadTextureFile("res/textures/Resume_STB.png", resumeTexWidth, resumeTexHeight, resumeAlpha);
    howToPlayTexture  = LoadTextureFile("res/textures/HowToPlay_STB.png", howToPlayTexWidth, howToPlayTexHeight, howToPlayAlpha);
    exitTexture       = LoadTextureFile("res/textures/Exit_STB.png", exitTexWidth, exitTexHeight, exitAlpha);
    mainMenuTexture   = LoadTextureFile("res/textures/Exit_STB.png", mainMenuTexWidth, mainMenuTexHeight, mainMenuAlpha);
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
void MainMenu::ShowHowToPlay()
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
            if (fadeAlpha > 0.0f)
            {
                fadeAlpha -= dt / fadeDuration;
                if (fadeAlpha < 0.0f) fadeAlpha = 0.0f;
            }
        }
        else
        {
            fadeOutAlpha += dt / fadeDuration;
            if (fadeOutAlpha >= 1.0f)
            {
                return;
            }
        }

        float bw = 200, bh = 50;
        float bx = (fbW - bw) * 0.5f, by = fbH - 120.f;

        // Bobbing animation for BACK button
        float bob = sin((float)currentT * 2.5f) * 5.0f;
        float drawYBase = by + bob;

        bool over = PointInRect((float)mx, (float)my, bx, drawYBase, bw, bh);
        static bool was = false;
        if (click && !was && over && !isFadingOut) {
            isFadingOut = true;
        }
        was = click;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !isFadingOut) {
            isFadingOut = true;
        }

        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
        glUseProgram(hudProgram);
        glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);

        RenderBackgroundImage((float)fbW, (float)fbH);
        RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, 0.65f);

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

        float s = 1.0f;
        float yOff = 0.0f;
        if (over)
        {
            if (click)
            {
                s = 0.92f;
                yOff = 4.0f;
            }
            else
            {
                s = 1.08f;
            }
        }
        
        float drawW = bw * s;
        float drawH = bh * s;
        float drawX = bx + (bw - drawW) * 0.5f;
        float drawY = drawYBase + (bh - drawH) * 0.5f + yOff;

        RenderColoredQuad(drawX, drawY, drawW, drawH, 0.1f, 0.1f, 0.1f);
        float bt = 1.2f * s, bcol = 0.3f;
        RenderColoredQuad(drawX, drawY, drawW, bt, bcol, bcol, bcol);
        RenderColoredQuad(drawX, drawY + drawH - bt, drawW, bt, bcol, bcol, bcol);
        RenderColoredQuad(drawX, drawY, bt, drawH, bcol, bcol, bcol);
        RenderColoredQuad(drawX + drawW - bt, drawY, bt, drawH, bcol, bcol, bcol);
        RenderTextCentered("BACK", drawX + drawW * 0.5f, drawY + (drawH - 18.0f * s) * 0.5f, 0.75f, 0.75f, 0.75f, 1.3f * s);

        // Draw fade overlay
        float overlayAlpha = isFadingOut ? fadeOutAlpha : fadeAlpha;
        if (overlayAlpha > 0.0f)
        {
            RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, overlayAlpha);
        }

        glfwSwapBuffers(window);
    }
}

// ======================================================================
// MAIN MENU / PAUSE
// ======================================================================
MainMenu::Result MainMenu::Show(bool pause)
{
    Result choice = Result::None;
    static bool escWas = false;
    if (pause) escWas = true;

    double startTime = glfwGetTime();
    double lastT = startTime;
    float fadeAlpha = 1.0f;
    float fadeOutAlpha = 0.0f;
    bool isFadingOut = false;
    Result pendingChoice = Result::None;
    float fadeDuration = 0.5f;

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
            if (fadeAlpha > 0.0f)
            {
                fadeAlpha -= dt / fadeDuration;
                if (fadeAlpha < 0.0f) fadeAlpha = 0.0f;
            }
        }
        else
        {
            fadeOutAlpha += dt / fadeDuration;
            if (fadeOutAlpha >= 1.0f)
            {
                fadeOutAlpha = 1.0f;
                choice = pendingChoice;
            }
        }

        if (pause && !isFadingOut) {
            bool esc = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (esc && !escWas) { pendingChoice = Result::Play; isFadingOut = true; }
            escWas = esc;
        }

        float ref = std::min((float)fbW, (float)fbH);

        // All buttons same width
        float btnMaxW = std::min(std::max(ref * 0.30f, 200.f), 500.f);

        // Pick textures
        const std::vector<unsigned char>* al[3];
        unsigned int texID[3]; int tw[3], th[3];
        if (pause) {
            texID[0]=resumeTexture;   tw[0]=resumeTexWidth;    th[0]=resumeTexHeight;    al[0]=&resumeAlpha;
            texID[1]=howToPlayTexture; tw[1]=howToPlayTexWidth; th[1]=howToPlayTexHeight; al[1]=&howToPlayAlpha;
            texID[2]=mainMenuTexture;  tw[2]=mainMenuTexWidth;  th[2]=mainMenuTexHeight;  al[2]=&mainMenuAlpha;
        } else {
            texID[0]=playTexture;      tw[0]=playTexWidth;      th[0]=playTexHeight;      al[0]=&playAlpha;
            texID[1]=howToPlayTexture; tw[1]=howToPlayTexWidth; th[1]=howToPlayTexHeight; al[1]=&howToPlayAlpha;
            texID[2]=exitTexture;      tw[2]=exitTexWidth;      th[2]=exitTexHeight;      al[2]=&exitAlpha;
        }

        // Per-button size multipliers { widthScale, heightScale }
        float btnScale[3][2] = {
            { 1.20f, 1.20f },  // Top button    (PLAY / RESUME)
            { 1.30f, 1.30f },  // Middle button (HOW TO PLAY)
            { 1.03f, 1.03f },  // Bottom button (EXIT / MAIN MENU)
        };

        float dw[3], dh[3];
        for (int i = 0; i < 3; i++)
        {
            float ws = btnScale[i][0];
            float hs = btnScale[i][1];

            if (tw[i] > 0 && th[i] > 0)
            {
                float asp = (float)tw[i] / th[i];
                dw[i] = btnMaxW * ws;
                dh[i] = (btnMaxW / asp) * hs;
            }
            else
            {
                dw[i] = btnMaxW * ws;
                dh[i] = btnMaxW * 0.2f * hs;
            }
        }

        // Spacing
        float sp = 10.0f;

        // Total height of all 3 buttons + 2 gaps
        float totalH = dh[0] + dh[1] + dh[2] + sp * 2;

        // Center vertically
        float startY = (fbH - totalH) * 0.7f;

        // Button center Y positions
        // Order: 0=PLAY/RESUME (top), 1=HOW TO PLAY (middle), 2=EXIT/MAIN MENU (bottom)
        float cy[3] = {
            startY + dh[0] * 0.5f,                                          // Top button
            startY + dh[0] + sp + dh[1] * 0.5f,                             // Middle button
            startY + dh[0] + sp + dh[1] + sp + dh[2] * 0.5f                 // Bottom button
        };
        float cx = fbW * 0.5f;

        // Pixel-perfect hit detection (including bobbing)
        bool over[3];
        float bob[3];
        for (int i = 0; i < 3; i++)
        {
            bob[i] = sin((float)currentT * 2.5f + i * 1.2f) * 5.0f;
            over[i] = IsMouseOverButtonPixel((float)mx, (float)my, cx, cy[i] + bob[i], dw[i], dh[i], *al[i], tw[i], th[i]);
        }

        static bool wasCl = false;
        if (click && !wasCl && !isFadingOut) {
            if (over[0]) { pendingChoice = Result::Play; isFadingOut = true; }
            if (over[1]) { pendingChoice = Result::HowToPlay; isFadingOut = true; }
            if (over[2]) { pendingChoice = Result::Quit; isFadingOut = true; }
        }
        wasCl = click;

        // RENDER
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
        glUseProgram(hudProgram);
        glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);

        if (pause) {
            if (pauseBackgroundTexture != 0) {
                RenderQuad(0, 0, (float)fbW, (float)fbH, pauseBackgroundTexture);
            }
            RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, 0.75f);
        }
        else {
            RenderBackgroundImage((float)fbW, (float)fbH, (float)currentT);
            RenderClouds((float)fbW, (float)fbH, (float)currentT);

            // 1. Cover up the delivery box above the logo by cloning the adjacent sky gradient
            float coverW = fbW * 0.16f;
            float coverH = fbH * 0.15f;
            float coverX = fbW * 0.5f - coverW * 0.5f;
            float coverY = fbH * 0.03f;
            RenderQuadSubset(coverX, coverY, coverW, coverH, backgroundTexture, 0.2f, 0.80f, 0.16f, 0.15f);

            // 2. Animate a premium metallic sheen/shine sweep across the main logo plate
            float sheenCycle = fmod((float)currentT, 3.0f);
            float sheenVal = -10.0f;
            if (sheenCycle < 1.0f) {
                sheenVal = sheenCycle * 2.0f; // sweep from 0.0 to 2.0 in 1 second
            }
            float logoW = fbW * 0.26f;
            float logoH = fbH * 0.11f;
            float logoX = fbW * 0.5f - logoW * 0.5f;
            float logoY = fbH * 0.245f - logoH * 0.5f;
            RenderQuadSubset(logoX, logoY, logoW, logoH, backgroundTexture, 0.37f, 0.695f, 0.26f, 0.11f, sheenVal);
        }

        // Draw all 3 buttons
        for (int i = 0; i < 3; i++)
        {
            float s = 1.0f;
            float yOff = 0.0f;
            if (over[i])
            {
                if (click)
                {
                    s = 0.92f;
                    yOff = 4.0f;
                }
                else
                {
                    s = 1.08f;
                }
            }
            RenderButtonImage(cx, cy[i] + yOff + bob[i], dw[i] * s, texID[i], tw[i], th[i]);
        }

        // Draw fade overlay
        float overlayAlpha = isFadingOut ? fadeOutAlpha : fadeAlpha;
        if (overlayAlpha > 0.0f)
        {
            RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, overlayAlpha);
        }

        glfwSwapBuffers(window);
    }
    if (choice == Result::HowToPlay) { ShowHowToPlay(); return Show(pause); }
    return choice;
}

void MainMenu::RenderLoading(float progress, const char* message)
{
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int fbW = width, fbH = height;
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glUseProgram(hudProgram);
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);

    // Draw background dimmed to 70% black
    RenderBackgroundImage((float)fbW, (float)fbH);
    RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0, 0, 0, 0.7f);

    // Draw a nice retro loading panel in the center
    float pw = 600.0f;
    float ph = 170.0f;
    float px = (fbW - pw) * 0.5f;
    float py = (fbH - ph) * 0.5f;
    
    // Panel base (dark charcoal brown)
    RenderColoredQuad(px, py, pw, ph, 0.12f, 0.10f, 0.09f, 0.95f);
    
    // Double retro borders (matching the buttons!)
    float bcol1_r = 0.35f, bcol1_g = 0.28f, bcol1_b = 0.22f;
    float bcol2_r = 0.20f, bcol2_g = 0.16f, bcol2_b = 0.13f;
    
    // Outer border
    float ob = 3.0f;
    RenderColoredQuad(px - ob, py - ob, pw + ob*2, ob, bcol2_r, bcol2_g, bcol2_b);
    RenderColoredQuad(px - ob, py + ph, pw + ob*2, ob, bcol2_r, bcol2_g, bcol2_b);
    RenderColoredQuad(px - ob, py, ob, ph, bcol2_r, bcol2_g, bcol2_b);
    RenderColoredQuad(px + pw, py, ob, ph, bcol2_r, bcol2_g, bcol2_b);

    // Inner border
    float ib = 1.5f;
    float gap = 4.0f;
    RenderColoredQuad(px + gap, py + gap, pw - gap*2, ib, bcol1_r, bcol1_g, bcol1_b);
    RenderColoredQuad(px + gap, py + ph - gap - ib, pw - gap*2, ib, bcol1_r, bcol1_g, bcol1_b);
    RenderColoredQuad(px + gap, py + gap, ib, ph - gap*2, bcol1_r, bcol1_g, bcol1_b);
    RenderColoredQuad(px + pw - gap - ib, py + gap, ib, ph - gap*2, bcol1_r, bcol1_g, bcol1_b);

    // Message (Warm Gold/Bronze)
    RenderTextCentered(message, fbW * 0.5f, py + 30.0f, 0.8f, 0.65f, 0.45f, 1.4f);

    // Percentage text
    char pct[32];
    sprintf(pct, "%d%%", (int)(progress * 100.0f));
    RenderTextCentered(pct, fbW * 0.5f, py + 65.0f, 0.8f, 0.65f, 0.45f, 1.2f);

    // Segmented progress bar
    int maxSegments = 16;
    int filledSegments = (int)(progress * maxSegments);
    
    float barW = pw * 0.8f;
    float barH = 20.0f;
    float segmentGap = 4.0f;
    float totalGapsWidth = (maxSegments - 1) * segmentGap;
    float segmentW = (barW - totalGapsWidth) / maxSegments;
    
    float barX = px + (pw - barW) * 0.5f;
    float barY = py + 105.0f;
    
    // Draw segments
    for (int i = 0; i < maxSegments; i++)
    {
        float segX = barX + i * (segmentW + segmentGap);
        if (i < filledSegments)
        {
            // Filled segment: warm gold/bronze color (R=0.8f, G=0.55f, B=0.35f)
            RenderColoredQuad(segX, barY, segmentW, barH, 0.8f, 0.55f, 0.35f, 1.0f);
            
            // Sub-borders inside the segment to make it look 3D/beveled!
            RenderColoredQuad(segX, barY, segmentW, 1.5f, 0.95f, 0.75f, 0.55f, 1.0f); // top highlight
            RenderColoredQuad(segX, barY + barH - 1.5f, segmentW, 1.5f, 0.5f, 0.35f, 0.2f, 1.0f); // bottom shadow
        }
        else
        {
            // Empty segment: dark background with a very subtle gold/bronze border
            RenderColoredQuad(segX, barY, segmentW, barH, 0.08f, 0.06f, 0.05f, 1.0f);
            
            // Subtle border around empty segment
            float bt = 1.0f;
            RenderColoredQuad(segX, barY, segmentW, bt, 0.25f, 0.2f, 0.15f, 1.0f);
            RenderColoredQuad(segX, barY + barH - bt, segmentW, bt, 0.25f, 0.2f, 0.15f, 1.0f);
            RenderColoredQuad(segX, barY, bt, barH, 0.25f, 0.2f, 0.15f, 1.0f);
            RenderColoredQuad(segX + segmentW - bt, barY, bt, barH, 0.25f, 0.2f, 0.15f, 1.0f);
        }
    }

    glfwSwapBuffers(window);
}