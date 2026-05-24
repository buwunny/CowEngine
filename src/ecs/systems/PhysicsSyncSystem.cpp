#include "ecs/systems/PhysicsSyncSystem.hpp"
#include "ecs/Components.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace ecs
{
    void physicsSyncSystem(Registry &r)
    {
        auto view = r.view<Transform, Physics>();
        for (auto e : view)
        {
            auto &t = view.get<Transform>(e);
            auto &p = view.get<Physics>(e);
            if (!p.body || !p.body->getMotionState())
                continue;

            btTransform trans;
            p.body->getMotionState()->getWorldTransform(trans);
            btScalar matrix[16];
            trans.getOpenGLMatrix(matrix);
            glm::mat4 phys = glm::make_mat4(matrix); // pure T*R, no scale

            t.position = glm::dvec3(phys[3][0], phys[3][1], phys[3][2]);

            float ry = glm::degrees(glm::atan(-phys[0][2], glm::sqrt(phys[1][2] * phys[1][2] + phys[2][2] * phys[2][2])));
            float rx = glm::degrees(glm::atan(phys[1][2], phys[2][2]));
            float rz = glm::degrees(glm::atan(phys[0][1], phys[0][0]));

            auto norm180 = [](float a) -> float
            {
                while (a > 180.f) a -= 360.f;
                while (a < -180.f) a += 360.f;
                return a;
            };
            float cRy = (ry >= 0.f ? 180.f : -180.f) - ry;
            float cRx = norm180(rx + 180.f);
            float cRz = norm180(rz + 180.f);
            float prx = static_cast<float>(t.rotation.x);
            float pry = static_cast<float>(t.rotation.y);
            float prz = static_cast<float>(t.rotation.z);
            float d1 = std::abs(norm180(rx - prx)) + std::abs(norm180(ry - pry)) + std::abs(norm180(rz - prz));
            float d2 = std::abs(norm180(cRx - prx)) + std::abs(norm180(cRy - pry)) + std::abs(norm180(cRz - prz));
            if (d2 < d1) { rx = cRx; ry = cRy; rz = cRz; }
            t.rotation = glm::dvec3(rx, ry, rz);

            t.model = glm::scale(phys, t.localScale);
        }
    }
}
