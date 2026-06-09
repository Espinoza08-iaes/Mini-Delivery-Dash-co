#ifndef MAINMENU_H
#define MAINMENU_H

struct GLFWwindow;

class MainMenu
{
public:
    MainMenu(GLFWwindow* window, int screenWidth, int screenHeight);
    ~MainMenu();

    enum class Result { Play, Quit, None, HowToPlay };

    Result Show(bool showPause = false);

private:
    GLFWwindow* window;
    int width, height;

    // OpenGL resources for colored quads
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    unsigned int hudProgram = 0;

    // OpenGL resources for triangle arrows
    unsigned int arrowVAO = 0;
    unsigned int arrowVBO = 0;

    // OpenGL resources for text rendering
    unsigned int textVAO = 0;
    unsigned int textVBO = 0;
    unsigned int textProgram = 0;

    // Title texture loaded from PNG
    unsigned int titleTexture = 0;
    int titleTexWidth = 0;
    int titleTexHeight = 0;

    // Setup functions
    void SetupGraphics();
    void SetupTextRendering();
    void LoadTitleTexture();

    // Drawing primitives
    void RenderQuad(float x, float y, float w, float h, unsigned int texture);
    void RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
    void RenderTriangle(float x, float y, float size, bool pointingRight, float r, float g, float b, float a = 1.0f);
    void RenderText(const char* text, float x, float y, float r, float g, float b, float scale = 1.5f);
    void RenderTextCentered(const char* text, float centerX, float y, float r, float g, float b, float scale = 1.5f);
    void RenderTitleImage(float centerX, float y, float displayWidth, float displayHeight);

    // Utility
    bool PointInRect(float px, float py, float rx, float ry, float rw, float rh);

    // Sub-menus
    void ShowHowToPlay();
};

#endif