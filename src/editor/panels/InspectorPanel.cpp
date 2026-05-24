#include "editor/panels/InspectorPanel.hpp"

#include "Scene.hpp"
#include "PhysicsWorld.hpp"
#include "ecs/Components.hpp"
#include "ecs/ComponentOps.hpp"
#include "ecs/Factories.hpp"
#include "meshes/AssetManager.hpp"
#include "meshes/StaticMesh.hpp"

#include <cstdio>
#include <filesystem>
#include <imgui.h>

namespace editor
{
    // ---------------------------------------------------------------------------
    // Per-component drawers. Each returns true to keep the component, false if
    // the user clicked the section's Remove button (currently always true — the
    // remove path runs through the InspectorEntry::remove function pointer below).
    // ---------------------------------------------------------------------------
    namespace
    {
        bool drawCollapsingWithRemove(const char *label, bool &requestRemove)
        {
            ImGui::PushID(label);
            bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
            float btnW = ImGui::GetFrameHeight();
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW - 4.0f);
            if (ImGui::SmallButton("X"))
                requestRemove = true;
            ImGui::PopID();
            return open;
        }

        bool drawIdentity(Context &ctx, ecs::Entity e)
        {
            auto *ident = ctx.scene->registry().try_get<ecs::Identity>(e);
            if (!ident)
                return true;
            char nameBuffer[256] = {};
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", ident->name.c_str());
            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                ident->name = nameBuffer;
            ImGui::Text("ID: %d", ident->id);
            return true;
        }

        bool drawTransform(Context &ctx, ecs::Entity)
        {
            if (ImGui::DragFloat3("Position", &ctx.selection.position.x, 0.1f))
                ctx.applySelectionTransform();
            if (ImGui::DragFloat3("Rotation", &ctx.selection.rotation.x, 0.5f))
                ctx.applySelectionTransform();
            if (ImGui::DragFloat3("Scale", &ctx.selection.scale.x, 0.05f, 0.001f, 1000.0f))
                ctx.applySelectionTransform();
            return true;
        }

        bool drawRenderable(Context &ctx, ecs::Entity e)
        {
            auto *rd = ctx.scene->registry().try_get<ecs::Renderable>(e);
            if (!rd)
                return true;
            if (ImGui::ColorEdit4("Color", &ctx.selection.color.r))
                ctx.applySelectionColor();

            auto *sm = ctx.scene->registry().try_get<ecs::ShapeMarker>(e);
            if (sm && sm->kind == ecs::ShapeKind::Cube)
                ImGui::Text("Mesh: Cube (size %d)", sm->cubeSize);
            else if (sm && sm->kind == ecs::ShapeKind::Plane)
                ImGui::Text("Mesh: Plane (%.1f x %.1f)", sm->planeLength, sm->planeWidth);
            else if (sm && sm->kind == ecs::ShapeKind::Static)
            {
                ImGui::Text("Mesh: StaticMesh");
                if (auto *ident = ctx.scene->registry().try_get<ecs::Identity>(e))
                {
                    char meshBuf[256] = {};
                    std::snprintf(meshBuf, sizeof(meshBuf), "%s", ident->meshPath.c_str());
                    if (ImGui::InputText("Path", meshBuf, sizeof(meshBuf)))
                        ident->meshPath = meshBuf;
                    ImGui::TextDisabled("Reload the scene to apply path changes.");
                }
            }

            ImGui::Text("Line width:");
            ImGui::SameLine();
            float lw = static_cast<float>(rd->lineWidth);
            if (ImGui::DragFloat("##linewidth", &lw, 0.1f, 0.1f, 10.0f))
                rd->lineWidth = lw;
            return true;
        }

        bool drawPhysics(Context &ctx, ecs::Entity e)
        {
            auto *p = ctx.scene->registry().try_get<ecs::Physics>(e);
            if (!p || !p->body)
                return true;
            float mass = 0.0f;
            if (p->body->getInvMass() > 0.0f)
                mass = 1.0f / p->body->getInvMass();
            if (ImGui::InputFloat("Mass", &mass, 0.1f, 1.0f))
                ecs::setMass(*p, mass);
            btVector3 v = p->body->getLinearVelocity();
            ImGui::Text("Velocity: %.2f %.2f %.2f", v.getX(), v.getY(), v.getZ());
            ImGui::Checkbox("Show Collider", &ctx.showColliders);
            return true;
        }

        bool drawScript(Context &ctx, ecs::Entity e)
        {
            auto *ident = ctx.scene->registry().try_get<ecs::Identity>(e);
            if (!ident)
                return true;
            char scriptBuf[256] = {};
            std::snprintf(scriptBuf, sizeof(scriptBuf), "%s", ident->scriptPath.c_str());
            if (ImGui::InputText("Path", scriptBuf, sizeof(scriptBuf)))
            {
                ident->scriptPath = scriptBuf;
                ctx.scene->registry().remove<ecs::ScriptComponent>(e);
            }
            ImGui::SameLine();
            if (ImGui::Button("Edit"))
            {
                if (ident->scriptPath.empty())
                    ctx.addLog("Set a script path first (e.g. scripts/spin.cow).",
                               ImVec4(0.9f, 0.7f, 0.4f, 1.0f));
                else
                    ctx.openScriptInCodeEditor(ident->scriptPath);
            }
            if (ctx.scene->registry().all_of<ecs::ScriptComponent>(e))
                ImGui::TextDisabled("Compiled");
            else
                ImGui::TextDisabled("Not compiled");
            return true;
        }

        struct InspectorEntry
        {
            const char *name;
            bool (*has)(Scene &, ecs::Entity);
            bool (*draw)(Context &, ecs::Entity);
            void (*remove)(Context &, ecs::Entity);
            bool removable;
        };

        const InspectorEntry kEntries[] = {
            {"Identity",
             [](Scene &s, ecs::Entity e)
             { return s.registry().all_of<ecs::Identity>(e); },
             &drawIdentity,
             [](Context &, ecs::Entity) {},
             false},
            {"Transform",
             [](Scene &s, ecs::Entity e)
             { return s.registry().all_of<ecs::Transform>(e); },
             &drawTransform,
             [](Context &, ecs::Entity) {},
             false},
            {"Renderable (Mesh)",
             [](Scene &s, ecs::Entity e)
             { return s.registry().all_of<ecs::Renderable>(e); },
             &drawRenderable,
             [](Context &c, ecs::Entity e)
             { ecs::removeRenderable(c.scene->registry(), e); },
             true},
            {"Physics (Collider + RigidBody)",
             [](Scene &s, ecs::Entity e)
             { return s.registry().all_of<ecs::Physics>(e); },
             &drawPhysics,
             [](Context &c, ecs::Entity e)
             { ecs::removePhysics(c.scene->registry(), e, c.scene->physicsWorld()); },
             true},
            {"Script",
             [](Scene &s, ecs::Entity e)
             {
                 auto *id = s.registry().try_get<ecs::Identity>(e);
                 return (id && !id->scriptPath.empty()) || s.registry().all_of<ecs::ScriptComponent>(e);
             },
             &drawScript,
             [](Context &c, ecs::Entity e)
             { ecs::removeScript(c.scene->registry(), e); },
             true},
        };

        void drawAddComponentPopup(Context &ctx)
        {
            if (ImGui::Button("+ Add Component"))
                ImGui::OpenPopup("##AddComponent");

            if (!ImGui::BeginPopup("##AddComponent"))
                return;

            auto &reg = ctx.scene->registry();
            ecs::Entity e = ctx.selection.entity;

            if (!reg.all_of<ecs::Renderable>(e))
            {
                if (ImGui::BeginMenu("Renderable (Mesh)"))
                {
                    if (ImGui::MenuItem("Cube"))
                        ecs::addRenderableCube(reg, e);
                    if (ImGui::MenuItem("Plane"))
                        ecs::addRenderablePlane(reg, e);
                    if (ImGui::BeginMenu("Static mesh from file"))
                    {
                        if (ctx.fileBrowserModels.empty())
                            ImGui::TextDisabled("(no .obj files found — refresh Files panel)");
                        for (const auto &path : ctx.fileBrowserModels)
                        {
                            if (ImGui::MenuItem(path.c_str()))
                            {
                                auto &am = AssetManager::instance();
                                std::string key = std::filesystem::path(path).stem().string();
                                auto mesh = am.loadStaticMeshFromOBJ(path, key);
                                if (mesh)
                                    ecs::addRenderableFromMesh(reg, e, mesh, path);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
            }

            if (!reg.all_of<ecs::Physics>(e))
            {
                if (ImGui::BeginMenu("Physics (Collider + RigidBody)"))
                {
                    if (ImGui::MenuItem("Box collider"))
                        ecs::addBoxCollider(reg, e, ctx.scene->physicsWorld());
                    if (ImGui::MenuItem("Sphere collider"))
                        ecs::addSphereCollider(reg, e, ctx.scene->physicsWorld());
                    if (ImGui::MenuItem("Capsule collider"))
                        ecs::addCapsuleCollider(reg, e, ctx.scene->physicsWorld());
                    if (ImGui::MenuItem("Convex hull from current mesh", nullptr, false,
                                        reg.all_of<ecs::Renderable>(e)))
                        ecs::addConvexHullColliderFromRenderable(reg, e, ctx.scene->physicsWorld());
                    ImGui::EndMenu();
                }
            }

            auto *ident = reg.try_get<ecs::Identity>(e);
            bool hasScript = ident && !ident->scriptPath.empty();
            if (!hasScript && ImGui::MenuItem("Script"))
                ecs::addScript(reg, e);

            ImGui::EndPopup();
        }
    }

    void InspectorPanel::draw(Context &ctx)
    {
        ImGui::Begin("Inspector", &ctx.showInspector);

        Scene *scene = ctx.scene;
        if (!scene)
        {
            ImGui::TextUnformatted("No scene loaded.");
            ImGui::End();
            return;
        }

        if (ctx.selection.entity == ecs::NullEntity || !scene->registry().valid(ctx.selection.entity))
        {
            ImGui::TextUnformatted("Select an object to inspect.");
            ImGui::End();
            return;
        }

        if (!ctx.selection.hasCache)
            ctx.refreshSelectionCache();

        auto &reg = scene->registry();
        auto *sm = reg.try_get<ecs::ShapeMarker>(ctx.selection.entity);

        const char *typeStr = "Entity";
        if (sm)
        {
            switch (sm->kind)
            {
            case ecs::ShapeKind::Cube:
                typeStr = "Cube";
                break;
            case ecs::ShapeKind::Plane:
                typeStr = "Plane";
                break;
            case ecs::ShapeKind::Static:
                typeStr = "StaticObject";
                break;
            case ecs::ShapeKind::Player:
                typeStr = "Player";
                break;
            }
        }
        ImGui::Text("Type: %s", typeStr);
        ImGui::Separator();

        for (const auto &entry : kEntries)
        {
            if (!entry.has(*scene, ctx.selection.entity))
                continue;
            bool requestRemove = false;
            bool open;
            if (entry.removable)
                open = drawCollapsingWithRemove(entry.name, requestRemove);
            else
            {
                ImGui::PushID(entry.name);
                open = ImGui::CollapsingHeader(entry.name, ImGuiTreeNodeFlags_DefaultOpen);
                ImGui::PopID();
            }
            if (open)
            {
                ImGui::PushID(entry.name);
                entry.draw(ctx, ctx.selection.entity);
                ImGui::PopID();
            }
            if (requestRemove && entry.removable)
                entry.remove(ctx, ctx.selection.entity);
        }

        ImGui::Separator();
        drawAddComponentPopup(ctx);

        ImGui::Separator();
        if (ImGui::Button("Delete Entity"))
        {
            ecs::Entity e = ctx.selection.entity;
            ctx.clearSelection();
            if (e == scene->getPlayerEntity())
                scene->removePlayer();
            else
                scene->destroyEntity(e);
        }

        ImGui::End();
    }
}
