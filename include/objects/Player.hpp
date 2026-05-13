#ifndef PLAYER_HPP
#define PLAYER_HPP

#include "../Window.hpp"
#include "../Camera.hpp"
#include "../InputHandler.hpp"
#include "Object.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>
#include "../PhysicsWorld.hpp"

class Player : public Object
{
public:
    Player(Camera *camera, glm::mat4 model = glm::mat4(1.0f));
    ~Player() = default;

    bool isOnGround(PhysicsWorld *physics);
    void processInput(Window *window, float deltaTime, PhysicsWorld *physics);
    void processMouse(GLFWwindow *window, double xpos, double ypos);
    void processMouseDelta(float dx, float dy);
    static void mouse_callback(GLFWwindow *window, double xpos, double ypos);
    void render(Window &window, Shader &shader) {};
    void renderTransparent(Window &window, Shader &shader) {};
    void renderFill(Window &window, Shader &shader) {};
    void resetInputState();
    Camera *getCamera() { return camera; }

private:
    Camera *camera;
    std::unique_ptr<InputHandler> inputHandler;
    float mass;
    float movementSpeed;
    float lastX, lastY;
    bool firstMouse;
    bool lastTabPressed;
    bool lastCPressed = false;
    float pendingMouseDx = 0.0f;
    float pendingMouseDy = 0.0f;
};
#endif // PLAYER_HPP