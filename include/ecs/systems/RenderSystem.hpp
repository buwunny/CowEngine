#ifndef ECS_RENDER_SYSTEM_HPP
#define ECS_RENDER_SYSTEM_HPP

#include "ecs/Entity.hpp"

class Window;
class Shader;

namespace ecs
{
    // Draw every entity that has Transform + Renderable. Mirrors the old
    // Object::render path: filled pass with selection/hover tint, then a
    // wireframe overlay in the entity's color.
    void renderSystem(Registry &r, Window &window, Shader &shader);

    // When false, renderSystem skips the opaque black fill pass and only draws
    // wireframes — letting the vaporwave sky/grid show through every object.
    // Defaults to true to preserve the editor's classic look.
    void setWireframeFillEnabled(bool enabled);
    bool isWireframeFillEnabled();

    // Transparent / fill-only variants of the same iteration, matching the
    // old Scene::renderTransparent / renderFill entry points.
    void renderTransparentSystem(Registry &r, Window &window, Shader &shader);
    void renderFillSystem(Registry &r, Window &window, Shader &shader);
}

#endif // ECS_RENDER_SYSTEM_HPP
