#include "platform/Window.hpp"
#include "core/Camera.hpp"
#include "core/PhysicsWorld.hpp"
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
    }

    // Feeds the player entity mouse-look and the cursor (Tab) toggle only.
    // WASD/jump/camera-follow now live in the entity's .cow script (see
    // scripts/player_movement.cow) so gameplay movement is fully scriptable;
    // this system just handles the mouse-look plumbing the script language
    // can't express. `physics`/`deltaTime` are retained for call-site
    // compatibility but no longer used here.
    void playerInputSystem(Registry &r, Window *window, PhysicsWorld *physics, float deltaTime)
    {
        (void)physics;
        (void)deltaTime;
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

            // Apply accumulated mouse delta (web pointer-lock path).
            const float pointerLockMultiplier = 5.0f;
            if (std::fabs(pc.pendingMouseDx) > 0.0001f || std::fabs(pc.pendingMouseDy) > 0.0001f)
                pc.camera->look(pc.pendingMouseDx * pointerLockMultiplier, -pc.pendingMouseDy * pointerLockMultiplier);
            pc.pendingMouseDx = 0.0f;
            pc.pendingMouseDy = 0.0f;
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
