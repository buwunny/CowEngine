#include "InputHandler.hpp"

InputHandler::InputHandler(Camera *camera)
{
    movementSpeed = 2.5f;
    firstMouse = true;
    this->camera = camera;
}

InputHandler::~InputHandler()
{
}

void InputHandler::processInput(Window *window, float deltaTime)
{
    glm::vec3 cameraPos = camera->getPosition();
    glm::vec3 cameraFront = camera->getFront();
    glm::vec3 cameraUp = camera->getUp();
    float cameraSpeed = movementSpeed * deltaTime;
    if (window->isKeyPressed(GLFW_KEY_LEFT_SHIFT))
        cameraSpeed *= 2;
    if (window->isKeyPressed(GLFW_KEY_W))
        cameraPos += cameraSpeed * cameraFront;
    if (window->isKeyPressed(GLFW_KEY_S))
        cameraPos -= cameraSpeed * cameraFront;
    if (window->isKeyPressed(GLFW_KEY_A))
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (window->isKeyPressed(GLFW_KEY_D))
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (window->isKeyPressed(GLFW_KEY_SPACE))
        cameraPos += cameraSpeed * cameraUp;
    if (window->isKeyPressed(GLFW_KEY_LEFT_CONTROL))
        cameraPos -= cameraSpeed * cameraUp;
    camera->setPosition(cameraPos);
}

void InputHandler::processMouse(GLFWwindow *window, float xpos, float ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    // Apply device-pixel scaling and global sensitivity
    float dpr = getDevicePixelRatioFor(window);
    double sens = getMouseSensitivityFor(window);
    xoffset = xoffset * dpr * static_cast<float>(sens);
    yoffset = yoffset * dpr * static_cast<float>(sens);

    this->camera->look(xoffset, yoffset);
}

void InputHandler::processMouseDelta(float dx, float dy)
{
    this->camera->look(dx, dy);
}

void InputHandler::mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    InputHandler *inputHandler = static_cast<InputHandler *>(glfwGetWindowUserPointer(window));
    if (inputHandler)
    {
        inputHandler->processMouse(window, static_cast<float>(xpos), static_cast<float>(ypos));
    }
}
