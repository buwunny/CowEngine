#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "meshes/Mesh.hpp"
#include "Window.hpp"
#include "Shader.hpp"

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <btBulletDynamicsCommon.h>
#include <memory>
#include <string>

class Object
{
public:
    Object();
    virtual ~Object() {}
    virtual const char *getTypeName() const = 0;
    virtual void render(Window &window, Shader &shader);
    virtual void renderTransparent(Window &window, Shader &shader);
    virtual void renderFill(Window &window, Shader &shader);
    void update()
    {
        btTransform trans;
        if (this->getRigidBody())
        {
            this->getRigidBody()->getMotionState()->getWorldTransform(trans);
            btScalar matrix[16];
            trans.getOpenGLMatrix(matrix);
            glm::mat4 modelMatrix = glm::make_mat4(matrix);
            // Apply any stored local scale after physics transform so visual scale matches JSON
            modelMatrix = glm::scale(modelMatrix, localScale);
            this->setModel(modelMatrix);
        }
    };
    glm::mat4 getModel() { return model; };
    glm::vec4 getColor() { return color; };
    std::string getColorString() const
    {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
                 static_cast<int>(color.r * 255),
                 static_cast<int>(color.g * 255),
                 static_cast<int>(color.b * 255),
                 static_cast<int>(color.a * 255));
        return std::string(buffer);
    }
    void setColor(const glm::vec4 &value) { color = value; };
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

        // Apply local scaling to collision shape if present (also needed for web builds)
        if (collisionShape)
        {
            collisionShape->setLocalScaling(btVector3(localScale.x, localScale.y, localScale.z));
        }
        // Set transfrom, rotation, scale from the model matrix for use in JSON export
        translation = glm::vec3(m[3][0], m[3][1], m[3][2]);
        // Extract rotation in degrees (assuming no shearing)
        rotation.x = glm::degrees(glm::atan(m[2][1], m[2][2])) * (m[2][2] < 0 ? -1.0f : 1.0f);   // pitch
        rotation.y = glm::degrees(glm::atan(m[2][0], glm::length(glm::vec2(m[0][0], m[1][0])))); // yaw
        rotation.z = glm::degrees(glm::atan(m[1][0], m[0][0])) * (m[0][0] < 0 ? -1.0f : 1.0f);   // roll
        scale = localScale;
    };
    btRigidBody *getRigidBody() { return rigidBody.get(); };
    void setRigidBody(btRigidBody *rigidBody) { this->rigidBody.reset(rigidBody); };
    btCollisionShape *getCollisionShape() { return collisionShape.get(); };
    void setCollisionShape(btCollisionShape *shape) { this->collisionShape.reset(shape); };

    int getId() const { return id; }
    const std::string &getName() const { return name; }
    void setName(const std::string &value) { name = value; }
    void getTransform(glm::vec3 &pos, glm::vec3 &rotDeg, glm::vec3 &scale) const;
    void setTransform(const glm::vec3 &pos, const glm::vec3 &rotDeg, const glm::vec3 &scale);
    double getMass() const
    {
        return this->mass;
    }
    void setMass(double mass)
    {
        this->mass = mass;
        if (rigidBody)
        {
            btVector3 inertia;
            collisionShape->calculateLocalInertia(mass, inertia);
            rigidBody->setMassProps(mass, inertia);
        }
    }
    void setLineWidth(double width)
    {
        this->lineWidth = width;
    }
    double getLineWidth() const
    {
        return this->lineWidth;
    }
    void setSelected(bool selected)
    {
        this->selected = selected;
    }
    bool isSelected() const
    {
        return this->selected;
    }
    void setHovered(bool hovered)
    {
        this->hovered = hovered;
    }
    bool isHovered() const
    {
        return this->hovered;
    }

protected:
    std::unique_ptr<btRigidBody> rigidBody;
    std::unique_ptr<btCollisionShape> collisionShape;
    std::unique_ptr<btMotionState> motionState;
    std::shared_ptr<Mesh> mesh;
    glm::dvec3 translation = glm::dvec3(0.0f);
    glm::dvec3 rotation = glm::dvec3(0.0f);
    glm::dvec3 scale = glm::dvec3(1.0f);
    double mass = 0.0;
    double lineWidth = 1.0;
    glm::vec4 color;
    glm::mat4 model;
    glm::mat4 modelNoScale;
    glm::vec3 localScale = glm::vec3(1.0f);
    bool wireframe;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    bool selected = false;
    ;
    bool hovered = false;
    int id = 0;
    std::string name;
};

#endif // OBJECT_HPP