#include "objects/Object.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>

namespace
{
    int g_nextObjectId = 1;

    double wrapAngleDeg(double angle)
    {
        double a = std::fmod(angle, 360.0);
        if (a > 180.0)
            a -= 360.0;
        else if (a < -180.0)
            a += 360.0;
        return a;
    }

    glm::vec3 normalizeEuler(const glm::vec3 &deg)
    {
        return glm::vec3(
            static_cast<float>(wrapAngleDeg(deg.x)),
            static_cast<float>(wrapAngleDeg(deg.y)),
            static_cast<float>(wrapAngleDeg(deg.z)));
    }

    glm::vec3 chooseClosestEuler(const glm::vec3 &currentDeg, const glm::vec3 &incomingDeg)
    {
        // Two equivalent Euler triplets for ZYX (Rz*Ry*Rx): (x,y,z) and (x+180, 180-y, z+180)
        glm::vec3 a = normalizeEuler(incomingDeg);
        glm::vec3 b = normalizeEuler(glm::vec3(incomingDeg.x + 180.0f, 180.0f - incomingDeg.y, incomingDeg.z + 180.0f));

        glm::vec3 cur = normalizeEuler(currentDeg);
        float distA = std::abs(a.x - cur.x) + std::abs(a.y - cur.y) + std::abs(a.z - cur.z);
        float distB = std::abs(b.x - cur.x) + std::abs(b.y - cur.y) + std::abs(b.z - cur.z);
        return (distB < distA) ? b : a;
    }
}

Object::Object()
{
    id = g_nextObjectId++;
    name = "Object " + std::to_string(id);
}

void Object::getTransform(glm::vec3 &pos, glm::vec3 &rotDeg, glm::vec3 &scale) const
{
    pos = glm::vec3(this->translation);
    rotDeg = glm::vec3(this->rotation);
    scale = glm::vec3(this->scale);
}

void Object::setTransform(const glm::vec3 &pos, const glm::vec3 &rotDeg, const glm::vec3 &scale)
{
    // glm::vec3 stableRot = chooseClosestEuler(glm::vec3(this->rotation), rotDeg);
    // Rotation order Rz*Ry*Rx matches ImGuizmo's DecomposeMatrixToComponents convention
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
    m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::scale(m, scale);

    setInitialModel(m);
    // Override the extraction with the exact values we were given — avoids floating-point
    // round-trip errors and gimbal-lock ambiguity in setInitialModel's asin/atan path.
    this->translation = glm::dvec3(pos);
    this->rotation = glm::dvec3(rotDeg);
    this->scale = glm::dvec3(scale);

    if (rigidBody)
    {
        btTransform trans;
        trans.setFromOpenGLMatrix(glm::value_ptr(modelNoScale));
        rigidBody->setWorldTransform(trans);
        if (motionState)
            motionState->setWorldTransform(trans);
        rigidBody->activate(true);
    }
}

void Object::render(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    // Draw filled geometry with the plane's color
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    window.setPolygonMode(GL_FILL);

    // default to black
    glm::vec4 fillColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    // if selected dark pink, almost black
    if (selected)
    {
        fillColor = glm::vec4(0.10f, 0.10f, 0.14f, 0.95f);
    }
    if (hovered)
    {
        fillColor = glm::vec4(0.05f, 0.05f, 0.05f, 1.0f);
    }

    shader.setFragmentColor(fillColor);
    mesh->render();
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Wireframe overlay
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(lineWidth);
    shader.setFragmentColor(color);
    mesh->renderWireframe();
}

void Object::renderTransparent(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(lineWidth);
    shader.setFragmentColor(color);
    mesh->renderWireframe();
}

void Object::renderFill(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(color);
    mesh->render();
}