#include "Window.hpp"
#include "Camera.hpp"
#include "PhysicsWorld.hpp"
#include "ecs/systems/PlayerInputSystem.hpp"
#include "ecs/Components.hpp"

#include <btBulletDynamicsCommon.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cmath>

namespace ecs
{
    namespace
    {
        // The system needs to convert raw GLFW / Emscripten mouse callbacks
        // into component mutations. Both callback paths run outside any
        // registry context, so we stash the active player here when the
        // scene registers one. Only one player is supported at a time.
        Registry *s_callbackRegistry = nullptr;
        Entity s_callbackPlayer = NullEntity;

        bool isPlayerOnGround(PhysicsWorld *physics, btRigidBody *rb)
        {
            btVector3 start = rb->getWorldTransform().getOrigin();
            btVector3 end = start - btVector3(0, 1.05f, 0);
            btCollisionWorld::ClosestRayResultCallback cb(start, end);
            physics->rayTest(start, end, cb);
            return cb.hasHit();
        }
    }

    void playerInputSystem(Registry &r, Window *window, PhysicsWorld *physics, float deltaTime)
    {
        s_callbackRegistry = &r;

        auto view = r.view<PlayerController, Physics>();
        for (auto e : view)
        {
            auto &pc = view.get<PlayerController>(e);
            auto &p = view.get<Physics>(e);
            if (!p.body || !pc.camera || !window)
                continue;

            bool tab = window->isKeyPressed(GLFW_KEY_TAB);
            if (tab && !pc.lastTabPressed)
            {
                bool disabled = window->toggleCursor();
                if (!disabled)
                    pc.firstMouse = true;
            }
            pc.lastTabPressed = tab;

            if (!window->isCursorDisabled())
                continue;

            // Apply accumulated mouse delta (web pointer-lock path)
            const float pointerLockMultiplier = 5.0f;
            if (std::fabs(pc.pendingMouseDx) > 0.0001f || std::fabs(pc.pendingMouseDy) > 0.0001f)
                pc.camera->look(pc.pendingMouseDx * pointerLockMultiplier, -pc.pendingMouseDy * pointerLockMultiplier);
            pc.pendingMouseDx = 0.0f;
            pc.pendingMouseDy = 0.0f;

            float cameraSpeed = pc.movementSpeed * deltaTime;
            btVector3 velocity = p.body->getLinearVelocity();
            btVector3 forwardDir = btVector3(pc.camera->getFront().x, 0, pc.camera->getFront().z).normalized();
            btVector3 rightDir = btVector3(pc.camera->getRight().x, 0, pc.camera->getRight().z).normalized();

            bool onGround = physics ? isPlayerOnGround(physics, p.body.get()) : false;
            float adjusted = onGround ? cameraSpeed : cameraSpeed * 0.1f;

            if (window->isKeyPressed(GLFW_KEY_W)) velocity += forwardDir * adjusted;
            if (window->isKeyPressed(GLFW_KEY_S)) velocity -= forwardDir * adjusted;
            if (window->isKeyPressed(GLFW_KEY_A)) velocity -= rightDir * adjusted;
            if (window->isKeyPressed(GLFW_KEY_D)) velocity += rightDir * adjusted;
            if (window->isKeyPressed(GLFW_KEY_SPACE) && onGround) velocity.setY(10.0f);

            p.body->activate(true);
            float maxSpeed = 10.0f;
            if (window->isKeyPressed(GLFW_KEY_LEFT_SHIFT)) maxSpeed *= 2;
            if (velocity.length() > maxSpeed)
                velocity = velocity.normalized() * maxSpeed;
            p.body->setLinearVelocity(velocity);

            btTransform trans;
            p.body->getMotionState()->getWorldTransform(trans);
            btVector3 pos = trans.getOrigin();
            pc.camera->setPosition(glm::vec3(pos.getX(), pos.getY() + 1.0f, pos.getZ()));
        }
    }

    void playerMouseCallback(Registry &r, Entity player, double xpos, double ypos)
    {
        auto *pc = r.try_get<PlayerController>(player);
        if (!pc || !pc->camera)
            return;
        if (pc->firstMouse)
        {
            pc->lastX = static_cast<float>(xpos);
            pc->lastY = static_cast<float>(ypos);
            pc->firstMouse = false;
        }
        float xoffset = static_cast<float>(xpos) - pc->lastX;
        float yoffset = pc->lastY - static_cast<float>(ypos);
        pc->lastX = static_cast<float>(xpos);
        pc->lastY = static_cast<float>(ypos);
        const float pointerLockMultiplier = 5.0f;
        pc->camera->look(xoffset * pointerLockMultiplier, yoffset * pointerLockMultiplier);
    }

    void playerMouseDelta(Registry &r, Entity player, float dx, float dy)
    {
        auto *pc = r.try_get<PlayerController>(player);
        if (!pc)
            return;
        pc->pendingMouseDx += dx;
        pc->pendingMouseDy += dy;
    }

    void playerResetInputState(Registry &r, Entity player)
    {
        auto *pc = r.try_get<PlayerController>(player);
        if (!pc)
            return;
        pc->firstMouse = true;
        pc->lastTabPressed = false;
    }

    // GLFW user-pointer callback hook: we set the window user pointer to a
    // sentinel and look up the active player via the static context.
    void playerGlfwMouseCallback(GLFWwindow *window, double xpos, double ypos)
    {
        if (!s_callbackRegistry || s_callbackPlayer == NullEntity)
            return;
        int mode = glfwGetInputMode(window, GLFW_CURSOR);
        if (mode != GLFW_CURSOR_DISABLED)
            return;
        playerMouseCallback(*s_callbackRegistry, s_callbackPlayer, xpos, ypos);
    }

    namespace detail
    {
        void setActivePlayer(Registry *r, Entity e)
        {
            s_callbackRegistry = r;
            s_callbackPlayer = e;
        }

        Entity activePlayer() { return s_callbackPlayer; }
        Registry *activePlayerRegistry() { return s_callbackRegistry; }
    }
}
