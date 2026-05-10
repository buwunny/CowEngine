#include "objects/Cube.hpp"

Cube::Cube(int size, glm::mat4 model, glm::vec4 color, float mass)
{
    mesh = std::make_unique<CubeMesh>(size);
    collisionShape = std::make_unique<btBoxShape>(btVector3(size / 2.0f, size / 2.0f, size / 2.0f));
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
    this->model = model;
    this->color = color;
}

void Cube::render(Window &window, Shader &shader)
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

void Cube::renderTransparent(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(3.0f);
    shader.setFragmentColor(color);
    mesh->render();
}

void Cube::renderFill(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(color);
    mesh->render();
}