#ifndef ECS_PLAYER_INPUT_SYSTEM_HPP
#define ECS_PLAYER_INPUT_SYSTEM_HPP

#include "ecs/Entity.hpp"

struct GLFWwindow;
class Window;
class PhysicsWorld;

namespace ecs
{
    // Drives mouse-look and the cursor (Tab) toggle for the entity carrying a
    // PlayerController. WASD/jump/camera-follow are script-driven now (see
    // scripts/player_movement.cow); this system only handles the mouse-look
    // plumbing that CowScript can't express.
    void playerInputSystem(Registry &r, Window *window, PhysicsWorld *physics, float deltaTime);

    // Mouse callback / mouse-delta accumulation, scoped to the active
    // player entity. Picked up by Scene during addPlayer().
    void playerMouseCallback(Registry &r, Entity player, double xpos, double ypos);
    void playerMouseDelta(Registry &r, Entity player, float dx, float dy);
    void playerResetInputState(Registry &r, Entity player);

    // GLFW user-pointer callback hook for desktop builds. The Window's
    // user pointer is set to point at an `entt::entity` we update when
    // addPlayer runs.
    void playerGlfwMouseCallback(GLFWwindow *window, double xpos, double ypos);

    namespace detail
    {
        // Register the active player entity for the callback paths above.
        // Called by Scene::addPlayer / removePlayer.
        void setActivePlayer(Registry *r, Entity e);
        Entity activePlayer();
        Registry *activePlayerRegistry();
    }
}

#endif // ECS_PLAYER_INPUT_SYSTEM_HPP
