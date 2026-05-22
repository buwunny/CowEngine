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
#include <cmath>
#include <memory>
#include <string>

namespace cowscript { class Script; }

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
            glm::mat4 phys = glm::make_mat4(matrix); // pure T*R, no scale, from Bullet

            // Keep stored transform in sync so scripts can read current position/rotation
            translation = glm::dvec3(phys[3][0], phys[3][1], phys[3][2]);
            // Extract Euler angles using the same Rz*Ry*Rx convention as setTransform/ImGuizmo.
            // Bullet gives a pure rotation matrix so columns are unit-length.
            float ry = glm::degrees(glm::atan(-phys[0][2], glm::sqrt(phys[1][2]*phys[1][2] + phys[2][2]*phys[2][2])));
            float rx = glm::degrees(glm::atan(phys[1][2], phys[2][2]));
            float rz = glm::degrees(glm::atan(phys[0][1], phys[0][0]));
            // For Rz*Ry*Rx, every rotation has a complementary representation:
            //   (X, Y, Z) ↔ (X+180°, 180°-Y, Z+180°)
            // Pick whichever is closer to the previously stored angles so the
            // sequence stays continuous and doesn't flip at the ±90° singularity.
            auto norm180 = [](float a) -> float {
                while (a > 180.f) a -= 360.f;
                while (a < -180.f) a += 360.f;
                return a;
            };
            float cRy = (ry >= 0.f ? 180.f : -180.f) - ry;
            float cRx = norm180(rx + 180.f);
            float cRz = norm180(rz + 180.f);
            float prx = static_cast<float>(rotation.x);
            float pry = static_cast<float>(rotation.y);
            float prz = static_cast<float>(rotation.z);
            float d1 = std::abs(norm180(rx - prx)) + std::abs(norm180(ry - pry)) + std::abs(norm180(rz - prz));
            float d2 = std::abs(norm180(cRx - prx)) + std::abs(norm180(cRy - pry)) + std::abs(norm180(cRz - prz));
            if (d2 < d1) { rx = cRx; ry = cRy; rz = cRz; }
            rotation = glm::dvec3(rx, ry, rz);

            // Apply stored local scale for rendering
            glm::mat4 modelMatrix = glm::scale(phys, localScale);
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
        // Extract local scale from column lengths (correct for TRS matrices: col j has length scale[j])
        glm::vec3 sx;
        sx.x = glm::length(glm::vec3(m[0][0], m[0][1], m[0][2]));
        sx.y = glm::length(glm::vec3(m[1][0], m[1][1], m[1][2]));
        sx.z = glm::length(glm::vec3(m[2][0], m[2][1], m[2][2]));
        localScale = sx;
        this->model = m;
        // Build a model matrix without scale by normalizing each column
        modelNoScale = m;
        if (sx.x != 0.0f)
        {
            modelNoScale[0][0] /= sx.x;
            modelNoScale[0][1] /= sx.x;
            modelNoScale[0][2] /= sx.x;
        }
        if (sx.y != 0.0f)
        {
            modelNoScale[1][0] /= sx.y;
            modelNoScale[1][1] /= sx.y;
            modelNoScale[1][2] /= sx.y;
        }
        if (sx.z != 0.0f)
        {
            modelNoScale[2][0] /= sx.z;
            modelNoScale[2][1] /= sx.z;
            modelNoScale[2][2] /= sx.z;
        }

        // Apply local scaling to collision shape if present (also needed for web builds)
        if (collisionShape)
        {
            collisionShape->setLocalScaling(btVector3(localScale.x, localScale.y, localScale.z));
        }
        translation = glm::vec3(m[3][0], m[3][1], m[3][2]);
        scale = localScale;

        // Extract Euler angles for R = Rz*Ry*Rx convention (matches ImGuizmo decompose).
        // Normalize each column to get the pure rotation matrix, then read off angles:
        //   col0 = [cos(Z)*cos(Y),  sin(Z)*cos(Y), -sin(Y)]
        //   col1[2] = cos(Y)*sin(X),  col2[2] = cos(Y)*cos(X)
        glm::vec3 col0 = (sx.x > 0.f) ? glm::vec3(m[0][0], m[0][1], m[0][2]) / sx.x : glm::vec3(1, 0, 0);
        glm::vec3 col1 = (sx.y > 0.f) ? glm::vec3(m[1][0], m[1][1], m[1][2]) / sx.y : glm::vec3(0, 1, 0);
        glm::vec3 col2 = (sx.z > 0.f) ? glm::vec3(m[2][0], m[2][1], m[2][2]) / sx.z : glm::vec3(0, 0, 1);
        // Same atan2 formula as ImGuizmo::DecomposeMatrixToComponents — full-range, no singularity at ±90°
        rotation.y = glm::degrees(glm::atan(-col0[2], glm::sqrt(col1[2]*col1[2] + col2[2]*col2[2])));
        rotation.x = glm::degrees(glm::atan(col1[2], col2[2]));
        rotation.z = glm::degrees(glm::atan(col0[1], col0[0]));
    };
    btRigidBody *getRigidBody() { return rigidBody.get(); };
    void setRigidBody(btRigidBody *rigidBody) { this->rigidBody.reset(rigidBody); };
    btCollisionShape *getCollisionShape() { return collisionShape.get(); };
    void setCollisionShape(btCollisionShape *shape) { this->collisionShape.reset(shape); };

    int getId() const { return id; }
    const std::string &getName() const { return name; }
    void setName(const std::string &value) { name = value; }
    const std::string &getScriptPath() const { return scriptPath; }
    void setScriptPath(const std::string &p) { scriptPath = p; }
    const std::string &getMeshPath() const { return meshPath; }
    void setMeshPath(const std::string &p) { meshPath = p; }
    const std::shared_ptr<cowscript::Script> &getScript() const { return script; }
    void setScript(std::shared_ptr<cowscript::Script> s) { script = std::move(s); }
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
    bool hovered = false;
    int id = 0;
    std::string name;
    std::string scriptPath;
    std::string meshPath;
    std::shared_ptr<cowscript::Script> script;
};

#endif // OBJECT_HPP