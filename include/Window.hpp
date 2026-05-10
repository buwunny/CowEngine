#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

class Window
{
public:
    Window(int width, int height, const char *title);
    ~Window();

    GLFWwindow *getWindow() { return window; };
    bool shouldClose() { return glfwWindowShouldClose(window); };
    void update();
    void close() { glfwSetWindowShouldClose(window, true); };
    bool isKeyPressed(int key) { return glfwGetKey(window, key) == GLFW_PRESS; };
    void setPolygonMode(unsigned int mode) { glPolygonMode(GL_FRONT_AND_BACK, mode); };
    void setLineWidth(float width) { glLineWidth(width); };
    bool toggleCursor();
    void setCursorDisabled(bool disabled);
    bool isCursorDisabled() const { return cursorDisabled; };
    static void framebuffer_size_callback(GLFWwindow *window, int width, int height);

private:
    GLFWwindow *window;
    bool cursorDisabled = true;
};
#endif // WINDOW_HPP