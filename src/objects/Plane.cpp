#include "objects/Plane.hpp"
#include <string>

Plane::Plane(float length, float width, glm::mat4 model, glm::vec4 color, float mass) : length(length), width(width)
{
    setName(std::string("Plane ") + std::to_string(getId()));
    mesh = std::make_shared<PlaneMesh>(length, width, length / 5.0f, width / 5.0f);
    collisionShape = std::make_unique<btBoxShape>(btVector3(length / 2.0f, 0.01f, width / 2.0f));
    // record scale and apply to collision shape before computing inertia
    this->setInitialModel(model);
    btVector3 localInertia(0, 0, 0);
    if (mass != 0.0f)
    {
        collisionShape->calculateLocalInertia(mass, localInertia);
    }
    btTransform transform;
    transform.setFromOpenGLMatrix(glm::value_ptr(this->modelNoScale));
    motionState = std::make_unique<btDefaultMotionState>(transform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), collisionShape.get(), localInertia);
    rigidBody.reset(new btRigidBody(rbInfo));

    rigidBody->setFriction(1.0f);
    rigidBody->setUserPointer(this);
    this->color = color;
}