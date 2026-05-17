#include "objects/Object.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>

namespace
{
    int g_nextObjectId = 1;
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
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
    m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, scale);

    this->translation = glm::dvec3(pos);
    this->rotation = glm::dvec3(rotDeg);
    this->scale = glm::dvec3(scale);

    setInitialModel(m);

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
