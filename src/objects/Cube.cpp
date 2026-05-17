#include "objects/Cube.hpp"
#include <cstdio>
#include <string>

Cube::Cube(int size, glm::mat4 model, glm::vec4 color, float mass) : size(size)
{
    setName(std::string("Cube ") + std::to_string(getId()));
    mesh = std::make_shared<CubeMesh>(size);
    collisionShape = std::make_unique<btBoxShape>(btVector3(size / 2.0f, size / 2.0f, size / 2.0f));
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
    // Ensure body is active and print debug info for web builds
    rigidBody->setActivationState(ACTIVE_TAG);

    this->color = color;
}

void Cube::render(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    // Draw filled geometry first with polygon offset to avoid z-fighting
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    mesh->render();
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Draw wireframe overlay
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(3.0f);
    shader.setFragmentColor(color);
    mesh->renderWireframe();
}

void Cube::renderTransparent(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_LINE);
    window.setLineWidth(3.0f);
    shader.setFragmentColor(color);
    mesh->renderWireframe();
}

void Cube::renderFill(Window &window, Shader &shader)
{
    shader.setModelMatrix(model);
    window.setPolygonMode(GL_FILL);
    shader.setFragmentColor(color);
    mesh->render();
}