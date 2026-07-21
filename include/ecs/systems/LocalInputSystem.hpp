#ifndef ECS_LOCAL_INPUT_SYSTEM_HPP
#define ECS_LOCAL_INPUT_SYSTEM_HPP

#include "ecs/Entity.hpp"

#include <string_view>

class Window;

namespace ecs
{
    // Fill the local player's PlayerInput for this tick from the Window: the
    // pressed-key bitmask plus the current camera yaw/pitch, and bump the input
    // sequence. Client-only — the headless server fills PlayerInput from the
    // network instead. Operates on entities with both PlayerInput and
    // LocalPlayer.
    void localInputSystem(Registry &r, Window *window);

    // Whether a named key (see ecs::kInputKeyNames) is held on the local
    // keyboard. Used as ScriptHost's fallback for entities without PlayerInput.
    // Client-only.
    bool localKeyPressed(Window *window, std::string_view name);
}

#endif // ECS_LOCAL_INPUT_SYSTEM_HPP
