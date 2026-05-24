#ifndef ECS_SCRIPT_SYSTEM_HPP
#define ECS_SCRIPT_SYSTEM_HPP

#include "ecs/Entity.hpp"

#include <string>

class ScriptHost;

namespace ecs
{
    // Compile + attach scripts to entities whose Identity has a non-empty
    // scriptPath but no Script component yet. Returns the number of new
    // scripts attached.
    int loadScripts(Registry &r, ScriptHost &host);

    // Drop every Script component, forcing a recompile on the next
    // loadScripts call.
    void resetScripts(Registry &r);

    // Invoke `on start` / `on update(dt)` on every entity with a Script.
    void startScripts(Registry &r, ScriptHost &host);
    void updateScripts(Registry &r, ScriptHost &host, float dt);
}

#endif // ECS_SCRIPT_SYSTEM_HPP
