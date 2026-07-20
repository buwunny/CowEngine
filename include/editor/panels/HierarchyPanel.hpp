#ifndef EDITOR_HIERARCHY_PANEL_HPP
#define EDITOR_HIERARCHY_PANEL_HPP

#include <string>
#include <vector>
#include <imgui.h>
#include <glm/glm.hpp>

#include "editor/EditorContext.hpp"
#include "ecs/Components.hpp"

namespace editor
{
    class HierarchyPanel
    {
    public:
        void draw(Context &ctx);

        // Exposed so other panels (Inspector "Delete Entity", keybind handlers,
        // etc.) can copy/duplicate without re-implementing the snapshot.
        void copyEntityToClipboard(Context &ctx, ecs::Entity e);
        ecs::Entity pasteEntityFromClipboard(Context &ctx,
                                             const glm::vec3 &positionOffset = glm::vec3(1.5f, 0.f, 0.f));
        ecs::Entity duplicateEntity(Context &ctx, ecs::Entity e);

    private:
        // Snapshot of an entity captured by "Copy" in the hierarchy. Paste/duplicate
        // re-spawns through the same factory used by the original kind, then
        // re-applies transform/color/script. Player entities are not copied.
        struct EntityClipboard
        {
            bool valid = false;
            bool hasShapeMarker = false;
            ecs::ShapeKind kind = ecs::ShapeKind::Static;
            int cubeSize = 1;
            float planeLength = 10.f;
            float planeWidth = 10.f;
            std::string name;
            std::string meshPath;
            std::vector<std::string> scriptPaths;
            glm::vec3 position{0.f};
            glm::vec3 rotation{0.f};
            glm::vec3 scale{1.f};
            glm::vec4 color{1.f};
            bool hasRenderable = false;
            bool hasPhysics = false;
            float mass = 0.f;
        };

        EntityClipboard entityClipboard;
        ImGuiTextFilter hierarchyFilter;
    };
}

#endif // EDITOR_HIERARCHY_PANEL_HPP
