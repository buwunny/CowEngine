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
        // Apply any stored local scale after physics transform so visual scale matches JSON
        modelMatrix = glm::scale(modelMatrix, localScale);
        this->setModel(modelMatrix);
    };
    glm::mat4 getModel() { return model; };
    glm::vec4 getColor() { return color; };
    std::shared_ptr<Mesh> getMesh() { return mesh; };
    void setMesh(std::shared_ptr<Mesh> m) { mesh = m; };
    void setModel(glm::mat4 model) { this->model = model; };
    void setInitialModel(const glm::mat4 &m)
    {
        // Extract local scale from the provided model matrix
        glm::vec3 sx;
        sx.x = glm::length(glm::vec3(m[0][0], m[1][0], m[2][0]));
        sx.y = glm::length(glm::vec3(m[0][1], m[1][1], m[2][1]));
        sx.z = glm::length(glm::vec3(m[0][2], m[1][2], m[2][2]));
        localScale = sx;
        this->model = m;
        // build a model matrix without scale for physics transforms
        modelNoScale = m;
        if (sx.x != 0.0f)
        {
            modelNoScale[0][0] /= sx.x;
            modelNoScale[1][0] /= sx.x;
            modelNoScale[2][0] /= sx.x;
        }
        if (sx.y != 0.0f)
        {
            modelNoScale[0][1] /= sx.y;
            modelNoScale[1][1] /= sx.y;
            modelNoScale[2][1] /= sx.y;
        }
        if (sx.z != 0.0f)
        {
            modelNoScale[0][2] /= sx.z;
            modelNoScale[1][2] /= sx.z;
            modelNoScale[2][2] /= sx.z;
        }

        // Apply local scaling to collision shape if present
        if (collisionShape)
        {
            collisionShape->setLocalScaling(btVector3(localScale.x, localScale.y, localScale.z));
        }
    };
    btRigidBody *getRigidBody() { return rigidBody.get(); };
    void setRigidBody(btRigidBody *rigidBody) { this->rigidBody.reset(rigidBody); };
    btCollisionShape *getCollisionShape() { return collisionShape.get(); };
    void setCollisionShape(btCollisionShape *shape) { this->collisionShape.reset(shape); };

protected:
    std::unique_ptr<btRigidBody> rigidBody;
    std::unique_ptr<btCollisionShape> collisionShape;
    std::unique_ptr<btMotionState> motionState;
    std::shared_ptr<Mesh> mesh;
    glm::vec4 color;
    glm::mat4 model;
    glm::mat4 modelNoScale;
    glm::vec3 localScale = glm::vec3(1.0f);
    bool wireframe;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

#endif // OBJECT_HPP