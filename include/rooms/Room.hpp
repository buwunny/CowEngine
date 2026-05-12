#ifndef ROOM_HPP
#define ROOM_HPP

#include "../objects/Object.hpp"
#include "../Shader.hpp"
#include "../Camera.hpp"
#include "../Window.hpp"
#include "../PhysicsWorld.hpp"

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#endif
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class Room
{
public:
    void addRigidBodiesToWorld(PhysicsWorld &physics)
    {
        for (auto &object : objects)
        {
            physics.addRigidBody(object->getRigidBody());
        }
    };
    void update()
    {
        for (auto &object : objects)
        {
            object->update();
        }
    };
    void render(Window &window, Shader &shader)
    {
        for (auto &object : objects)
        {
            object->render(window, shader);
        }
    };
    void renderTransparent(Window &window, Shader &shader)
    {
        for (auto &object : objects)
        {
            object->renderTransparent(window, shader);
        }
    };
    void renderFill(Window &window, Shader &shader)
    {
        for (auto &object : objects)
        {
            object->renderFill(window, shader);
        }
    };

protected:
    std::vector<std::unique_ptr<Object>> objects;
};

#endif // ROOM_HPP