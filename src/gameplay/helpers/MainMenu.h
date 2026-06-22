#ifndef MAINMENU_H
#define MAINMENU_H

#include <vector>
#include <string>

struct GLFWwindow;
struct GameSettings;
class AudioEngine;

class MainMenu
{
public:
    MainMenu(GLFWwindow* window, int screenWidth, int screenHeight);
    ~MainMenu();

    enum class Result { Play, Quit, None, HowToPlay, Shop, Settings, SettingsChanged, Credits };

    Result Show(bool pause = false, GameSettings* settings = nullptr, AudioEngine* audio = nullptr);
    void SetPauseBackground(unsigned int texture);
    void RenderLoading(float progress, const char* message);
    unsigned int GetCapturedBackground() const { return capturedBg; }

private:
    GLFWwindow* window;
    int width, height;

    unsigned int capturedBg = 0;

    // OpenGL resources for colored quads
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    unsigned int hudProgram = 0;

    // OpenGL resources for text rendering
    unsigned int textVAO = 0;
    unsigned int textVBO = 0;
    unsigned int textProgram = 0;

    // Background texture
    unsigned int backgroundTexture = 0;
    int bgTexWidth = 0, bgTexHeight = 0;

    // Pause textures
    unsigned int pauseDecorTexture = 0;
    int pauseDecorTexWidth = 0, pauseDecorTexHeight = 0;
    unsigned int pauseBackgroundTexture = 0;

    // Button textures + alpha for pixel-perfect hit detection
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

    unsigned int settingsTexture = 0;
    int settingsTexWidth = 0, settingsTexHeight = 0;
    std::vector<unsigned char> settingsAlpha;

    unsigned int creditsTexture = 0;
    int creditsTexWidth = 0, creditsTexHeight = 0;
    std::vector<unsigned char> creditsAlpha;

    unsigned int exitTexture = 0;
    int exitTexWidth = 0, exitTexHeight = 0;
    std::vector<unsigned char> exitAlpha;

    unsigned int mainMenuTexture = 0;
    int mainMenuTexWidth = 0, mainMenuTexHeight = 0;
    std::vector<unsigned char> mainMenuAlpha;

    // Help icon (reuses howToPlay texture)
    unsigned int helpIconTexture = 0;
    int helpIconTexWidth = 0, helpIconTexHeight = 0;
    std::vector<unsigned char> helpIconAlpha;

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
    void RenderButtonImageFixed(float cx, float cy, float w, float h, unsigned int tex);
    void RenderBackgroundImage(float fbW, float fbH, float timeVal = 0.0f);
    void RenderClouds(float fbW, float fbH, float timeVal);

    // Utility
    bool PointInRect(float px, float py, float rx, float ry, float rw, float rh);

    // Sub-menus
    void ShowHowToPlay(unsigned int bgTex = 0);
    Result ShowSettings(unsigned int bgTex, GameSettings* settings, AudioEngine* audio);
    void ShowCredits(unsigned int bgTex);
};

#endif