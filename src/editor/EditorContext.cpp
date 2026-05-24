#include "editor/EditorContext.hpp"

#include "Scene.hpp"
#include "PhysicsWorld.hpp"
#include "CodeEditor.hpp"
#include "ecs/Components.hpp"
#include "ecs/ComponentOps.hpp"
#include "ecs/Factories.hpp"

#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>

namespace editor
{
    void Context::addLog(const std::string &text, const ImVec4 &color)
    {
        consoleLines.push_back({text, color});
        scrollToBottom = true;
    }

    void Context::setSelection(ecs::Entity e)
    {
        if (selection.entity == e)
            return;
        if (scene)
            scene->setSelectedEntity(e);
        selection.entity = e;
        selection.hasCache = false;
    }

    void Context::refreshSelectionCache()
    {
        if (!scene || selection.entity == ecs::NullEntity)
            return;
        auto &reg = scene->registry();
        if (!reg.valid(selection.entity))
            return;
        if (auto *t = reg.try_get<ecs::Transform>(selection.entity))
        {
            selection.position = glm::vec3(t->position);
            selection.rotation = glm::vec3(t->rotation);
            selection.scale = glm::vec3(t->scale);
        }
        if (auto *rd = reg.try_get<ecs::Renderable>(selection.entity))
            selection.color = rd->color;
        selection.hasCache = true;
    }

    void Context::applySelectionTransform()
    {
        if (!scene || selection.entity == ecs::NullEntity)
            return;
        ecs::applyTransform(scene->registry(), selection.entity,
                            selection.position, selection.rotation, selection.scale);
        // Refresh Bullet's broadphase AABB so editor-mode raycasts find the new pose
        // (stepSimulation isn't called in editor mode).
        if (physics)
        {
            if (auto *p = scene->registry().try_get<ecs::Physics>(selection.entity); p && p->body)
                physics->updateSingleAabb(p->body.get());
        }
    }

    void Context::applySelectionColor()
    {
        if (!scene || selection.entity == ecs::NullEntity)
            return;
        if (auto *rd = scene->registry().try_get<ecs::Renderable>(selection.entity))
            rd->color = selection.color;
    }

    void Context::openScriptInCodeEditor(const std::string &path)
    {
        if (path.empty() || !codeEditor)
            return;
        codeEditor->openFile(path);
        requestedTab = CodeTab;
    }

    void addObjectToScene(Context &ctx, const std::string &type)
    {
        Scene *scene = ctx.scene;
        if (!scene)
            return;

        if (type == "cube")
        {
            scene->spawnCube(1,
                             glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                             glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
        }
        else if (type == "plane")
        {
            scene->spawnPlane(100, 100,
                              glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                              glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 0.0f);
        }
        else if (type == "cow")
        {
            scene->spawnStaticFromAsset("models/cow.obj", "cow",
                                        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                                        glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
        }
        else if (type == "tower")
        {
            ecs::Entity e = scene->spawnStaticFromAsset(
                "models/eiffel_tower.obj", "tower",
                glm::translate(glm::mat4(0.0f), glm::vec3(0.0f, 0.0f, 0.0f)),
                glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 0.0f);
            if (e != ecs::NullEntity)
                ecs::applyTransform(scene->registry(), e,
                                    glm::vec3(0.0f), glm::vec3(-90.0f, 0.0f, 0.0f), glm::vec3(0.1f));
        }
        scene->saveToJSON("scenes/scene.json");
    }

    void spawnStaticObjectFromMesh(Context &ctx, const std::string &meshPath)
    {
        Scene *scene = ctx.scene;
        if (!scene)
            return;
        std::string key = std::filesystem::path(meshPath).stem().string();
        ecs::Entity e = scene->spawnStaticFromAsset(meshPath, key,
                                                    glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
                                                    glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f);
        if (e == ecs::NullEntity)
        {
            ctx.addLog("Failed to load mesh: " + meshPath, ImVec4(0.95f, 0.5f, 0.5f, 1.0f));
            return;
        }
        scene->registry().get<ecs::Identity>(e).name = key;
        ctx.addLog("Spawned " + meshPath, ImVec4(0.7f, 0.95f, 0.7f, 1.0f));
    }
}
