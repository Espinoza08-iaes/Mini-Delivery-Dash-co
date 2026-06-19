#ifndef MAINMENU_H
#define MAINMENU_H

#include <vector>

struct GLFWwindow;

class MainMenu
{
public:
    MainMenu(GLFWwindow* window, int screenWidth, int screenHeight);
    ~MainMenu();

    enum class Result { Play, Quit, None, HowToPlay, Shop };

    Result Show(bool showPause = false);
    void SetPauseBackground(unsigned int texture);
    void RenderLoading(float progress, const char* message);

private:
    GLFWwindow* window;
    int width, height;

    // OpenGL resources for colored quads
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    unsigned int hudProgram = 0;

    // OpenGL resources for text rendering (used in HowToPlay)
    unsigned int textVAO = 0;
    unsigned int textVBO = 0;
    unsigned int textProgram = 0;

    // Background texture (main menu and HowToPlay)
    unsigned int backgroundTexture = 0;
    int bgTexWidth = 0;
    int bgTexHeight = 0;
    unsigned int pauseBackgroundTexture = 0;

    // Button textures + alpha for pixel‑perfect hit detection
    unsigned int playTexture = 0;
    int playTexWidth = 0, playTexHeight = 0;
    std::vector<unsigned char> playAlpha;

    unsigned int resumeTexture = 0;
    int resumeTexWidth = 0, resumeTexHeight = 0;
    std::vector<unsigned char> resumeAlpha;

    unsigned int howToPlayTexture = 0;
    int howToPlayTexWidth = 0, howToPlayTexHeight = 0;
    std::vector<unsigned char> howToPlayAlpha;

    unsigned int shopTexture = 0;
    int shopTexWidth = 0, shopTexHeight = 0;
    std::vector<unsigned char> shopAlpha;

    unsigned int exitTexture = 0;
    int exitTexWidth = 0, exitTexHeight = 0;
    std::vector<unsigned char> exitAlpha;

    unsigned int mainMenuTexture = 0;
    int mainMenuTexWidth = 0, mainMenuTexHeight = 0;
    std::vector<unsigned char> mainMenuAlpha;

    // Setup
    void SetupGraphics();
    void SetupTextRendering();
    void LoadAllTextures();
    unsigned int LoadTextureFile(const char* path, int& outW, int& outH, std::vector<unsigned char>& outAlpha);

    // Drawing
    void RenderQuad(float x, float y, float w, float h, unsigned int texture);
    void RenderQuadSubset(float x, float y, float w, float h, unsigned int texture, float sx, float sy, float sw, float sh, float sheenVal = -10.0f);
    void RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
    void RenderText(const char* text, float x, float y, float r, float g, float b, float scale = 1.5f);
    void RenderTextCentered(const char* text, float cx, float y, float r, float g, float b, float scale = 1.5f);
    void RenderButtonImage(float cx, float cy, float maxW, unsigned int tex, int tw, int th);
    void RenderBackgroundImage(float fbW, float fbH, float timeVal = 0.0f);
    void RenderClouds(float fbW, float fbH, float timeVal);

    // Utility
    bool PointInRect(float px, float py, float rx, float ry, float rw, float rh);

    // Sub‑menu
    void ShowHowToPlay();
};

#endif