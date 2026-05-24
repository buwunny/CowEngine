#ifndef ECS_PLAYER_INPUT_SYSTEM_HPP
#define ECS_PLAYER_INPUT_SYSTEM_HPP

#include "ecs/Entity.hpp"

struct GLFWwindow;
class Window;
class PhysicsWorld;

namespace ecs
{
    // Drives WSAD/jump/mouse-look for every entity carrying a
    // PlayerController. Mirrors the original Player::processInput.
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
