#ifndef INPUTHANDLER_HPP
#define INPUTHANDLER_HPP

#include "Window.hpp"
#include "Camera.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>

class InputHandler
{
public:
    InputHandler(Camera *camera);
    ~InputHandler();

    void processInput(Window *window, float deltaTime);
    void processMouse(GLFWwindow *window, float xpos, float ypos);
    void processMouseDelta(float dx, float dy);
    void resetFirstMouse();
    void setMovementSpeed(float speed) { movementSpeed = speed; }
    static void mouse_callback(GLFWwindow *window, double xpos, double ypos);

private:
    Camera *camera;
    float movementSpeed;
    float lastX, lastY;
    bool firstMouse;
    float pendingMouseDx = 0.0f;
    float pendingMouseDy = 0.0f;
};
#endif // INPUTHANDLER_HPP