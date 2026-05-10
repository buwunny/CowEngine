#include "objects/Plane.hpp"

Plane::Plane(float length, float width, glm::mat4 model, glm::vec4 color, float mass)
{
    mesh = std::make_unique<PlaneMesh>(length, width, length / 5.0f, width / 5.0f);
    collisionShape = std::make_unique<btBoxShape>(btVector3(length / 2.0f, 0.01f, width / 2.0f));
    btVector3 localInertia(0, 0, 0);
    if (mass != 0.0f)
    {
        collisionShape->calculateLocalInertia(mass, localInertia);
    }
    btTransform transform;
    transform.setFromOpenGLMatrix(glm::value_ptr(model));
    motionState = std::make_unique<btDefaultMotionState>(transform);
    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), collisionShape.get(), localInertia);
    rigidBody.reset(new btRigidBody(rbInfo));

    rigidBody->setFriction(1.0f);

    this->model = model;
    this->color = color;
}

void Plane::render(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    mesh->render();
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(3.0f);
    shader.setFragmentColor(color);
    mesh->render();
}

void Plane::renderTransparent(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(3.0f);
    shader.setFragmentColor(color);
    mesh->render();
}

void Plane::renderFill(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(color);
    mesh->render();
}