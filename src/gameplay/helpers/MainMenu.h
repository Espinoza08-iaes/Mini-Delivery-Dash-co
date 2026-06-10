#ifndef MAINMENU_H
#define MAINMENU_H

struct GLFWwindow;

class MainMenu
{
public:
    MainMenu(GLFWwindow* window, int screenWidth, int screenHeight);
    ~MainMenu();

    enum class Result { Play, Quit, None };

    Result Show(bool showPause = false);

private:
    GLFWwindow* window;
    int width, height;

    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    unsigned int hudProgram = 0;
    unsigned int textVAO = 0;
    unsigned int textVBO = 0;
    unsigned int textProgram = 0;

    void SetupGraphics();
    void SetupTextRendering();
    void RenderQuad(float x, float y, float w, float h, unsigned int texture);
    void RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b);
    void RenderText(const char* text, float x, float y, float r, float g, float b);
    bool PointInRect(float px, float py, float rx, float ry, float rw, float rh);
};

#endif