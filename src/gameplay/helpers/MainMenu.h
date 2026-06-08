#ifndef MAINMENU_H
#define MAINMENU_H

struct GLFWwindow;

class MainMenu
{
public:
    MainMenu(GLFWwindow* window, int screenWidth, int screenHeight);
    ~MainMenu();

    enum class Result { Play, Quit, None };
    Result Show();

private:
    GLFWwindow* window;
    int width, height;

    unsigned int backgroundTex = 0;
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    unsigned int hudProgram = 0;

    void LoadTextures();
    void SetupGraphics();
    void RenderQuad(float x, float y, float w, float h, unsigned int texture);
    void RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b);
    bool PointInRect(float px, float py, float rx, float ry, float rw, float rh);
};

#endif