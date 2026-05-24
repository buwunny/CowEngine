#include "editor/panels/StatsPanel.hpp"

#include "Scene.hpp"

#include <cstddef>
#include <imgui.h>

namespace editor
{
    void StatsPanel::draw(Context &ctx, float deltaSeconds, float fps)
    {
        ImGui::Begin("Stats", &ctx.showStats);
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame: %.2f ms", deltaSeconds * 1000.0f);
        if (ctx.scene)
        {
            size_t count = 0;
            ctx.scene->forEachEntity([&](ecs::Entity)
                                     { ++count; });
            ImGui::Text("Entities: %zu", count);
            ImGui::Text("Has Player: %s", ctx.scene->hasPlayer() ? "Yes" : "No");
        }
        ImGui::End();
    }
}
