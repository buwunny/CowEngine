#ifndef CAMERA_HPP
#define CAMERA_HPP

#if defined(ENGINE_HEADLESS)
#include "platform/GLHeadless.hpp"
#elif defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    Camera(glm::vec3 position, glm::vec3 front, glm::vec3 up);
    ~Camera();

    glm::vec3 getPosition() { return position; };
    glm::vec3 getFront() { return front; };
    glm::vec3 getUp() { return up; };
    glm::vec3 getRight() const;
    float getYaw() { return yaw; };
    float getPitch() { return pitch; };

    void setPosition(glm::vec3 position) { this->position = position; };
    void setFront(glm::vec3 front) { this->front = front; };
    void setUp(glm::vec3 up) { this->up = up; };

    void look(float xoffset, float yoffset);
    // Set absolute yaw/pitch (degrees) and rebuild the facing vector. Used by
    // the server to drive each player's camera from InputCommand look angles,
    // and by client reconciliation to restore a camera to a known orientation.
    void setLook(float yawDeg, float pitchDeg);

private:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    float yaw;
    float pitch;
    float sensitivity = 0.1f;
};
#endif // CAMERA_HPP