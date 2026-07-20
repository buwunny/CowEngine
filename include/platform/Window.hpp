#ifndef WINDOW_HPP
#define WINDOW_HPP

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#if defined(__EMSCRIPTEN__)
#ifndef GL_FILL
#define GL_FILL 0
#endif
#ifndef GL_LINE
#define GL_LINE 1
#endif
#endif
#include <GLFW/glfw3.h>
#include <iostream>

class Window
{
public:
    Window(int width, int height, const char *title);
    ~Window();

    GLFWwindow *getWindow();
    bool shouldClose();
    void update();
    void close() { glfwSetWindowShouldClose(window, true); };
#if defined(__EMSCRIPTEN__)
    void setPolygonMode(unsigned int /*mode*/) {};
#else
    void setPolygonMode(unsigned int mode) { glPolygonMode(GL_FRONT_AND_BACK, mode); };
#endif
    void setLineWidth(float width) { glLineWidth(width); };
    bool toggleCursor();
    void setCursorDisabled(bool disabled);
    bool isKeyPressed(int key);
    bool isCursorDisabled() const { return cursorDisabled; };
    static void framebuffer_size_callback(GLFWwindow *window, int width, int height);
    using MouseDeltaCallback = void (*)(void *user, float dx, float dy);
    // Register a mouse-delta callback for web builds (pointer-lock movementX/Y).
    // No-op on desktop. Pass nullptr to clear.
    static void setEmscriptenMouseDeltaCallback(MouseDeltaCallback cb, void *user);

private:
    GLFWwindow *window;
    bool cursorDisabled = true;
};
// Helpers to query mouse sensitivity and device pixel ratio for a given GLFW window
double getMouseSensitivityFor(GLFWwindow *window);
float getDevicePixelRatioFor(GLFWwindow *window);
#endif // WINDOW_HPP