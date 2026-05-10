#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "../meshes/Mesh.hpp"
#include "../Window.hpp"
#include "../Shader.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <btBulletDynamicsCommon.h>
#include <memory>

class Object
{
public:
    virtual ~Object() {}
    virtual void render(Window &window, Shader &shader) = 0;
    virtual void renderTransparent(Window &window, Shader &shader) = 0;
    virtual void renderFill(Window &window, Shader &shader) = 0;
    void update()
    {
        btTransform trans;
        this->getRigidBody()->getMotionState()->getWorldTransform(trans);
        btScalar matrix[16];
        trans.getOpenGLMatrix(matrix);
        glm::mat4 modelMatrix = glm::make_mat4(matrix);
        this->setModel(modelMatrix);
    };
    glm::mat4 getModel() { return model; };
    glm::vec4 getColor() { return color; };
    Mesh *getMesh() { return mesh.get(); };
    void setMesh(Mesh *m) { mesh.reset(m); };
    void setModel(glm::mat4 model) { this->model = model; };
    btRigidBody *getRigidBody() { return rigidBody.get(); };
    void setRigidBody(btRigidBody *rigidBody) { this->rigidBody.reset(rigidBody); };
    btCollisionShape *getCollisionShape() { return collisionShape.get(); };
    void setCollisionShape(btCollisionShape *shape) { this->collisionShape.reset(shape); };

protected:
    std::unique_ptr<btRigidBody> rigidBody;
    std::unique_ptr<btCollisionShape> collisionShape;
    std::unique_ptr<btMotionState> motionState;
    std::unique_ptr<Mesh> mesh;
    glm::vec4 color;
    glm::mat4 model;
    bool wireframe;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

#endif // OBJECT_HPP