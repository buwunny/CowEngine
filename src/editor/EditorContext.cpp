#include "editor/EditorContext.hpp"

#include "core/Scene.hpp"
#include "core/PhysicsWorld.hpp"
#include "app/CodeEditor.hpp"
#include "ecs/Components.hpp"
#include "ecs/ComponentOps.hpp"
#include "ecs/Factories.hpp"

#include <cstdlib>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, em_save_vfx, (const char *data), {
    try { localStorage.setItem('cowengine_vfx', UTF8ToString(data)); }
    catch (e) { console.warn('cowengine: vfx save failed', e); }
});

EM_JS(char *, em_load_vfx, (), {
    try
    {
        var raw = localStorage.getItem('cowengine_vfx');
        if (!raw) return 0;
        var lengthBytes = lengthBytesUTF8(raw) + 1;
        var ptr = _malloc(lengthBytes);
        stringToUTF8(raw, ptr, lengthBytes);
        return ptr;
    }
    catch (e) { return 0; }
});
#endif

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

    using json = nlohmann::json;

    std::string Context::serializeVfxToJson() const
    {
        json j;
        j["skyEnabled"] = vfx.skyEnabled;
        j["skyTop"] = {vfx.skyTop.x, vfx.skyTop.y, vfx.skyTop.z};
        j["skyMid"] = {vfx.skyMid.x, vfx.skyMid.y, vfx.skyMid.z};
        j["skyBottom"] = {vfx.skyBottom.x, vfx.skyBottom.y, vfx.skyBottom.z};
        j["sunPos"] = {vfx.sunPos.x, vfx.sunPos.y};
        j["sunRadius"] = vfx.sunRadius;
        j["sunColor"] = {vfx.sunColor.x, vfx.sunColor.y, vfx.sunColor.z};
        j["sunEnabled"] = vfx.sunEnabled;
        j["sunStripes"] = vfx.sunStripes;
        j["sunWorldAnchored"] = vfx.sunWorldAnchored;
        j["sunAzimuth"] = vfx.sunAzimuth;
        j["sunElevation"] = vfx.sunElevation;
        j["gridEnabled"] = vfx.gridEnabled;
        j["gridColor"] = {vfx.gridColor.x, vfx.gridColor.y, vfx.gridColor.z};
        j["gridScale"] = vfx.gridScale;
        j["gridFade"] = vfx.gridFade;
        j["gridLineWidth"] = vfx.gridLineWidth;
        j["horizonY"] = vfx.horizonY;
        j["wireframeFill"] = vfx.wireframeFill;
        j["fogEnabled"] = vfx.fogEnabled;
        j["fogColor"] = {vfx.fogColor.x, vfx.fogColor.y, vfx.fogColor.z};
        j["fogStart"] = vfx.fogStart;
        j["fogEnd"] = vfx.fogEnd;
        j["neonEnabled"] = vfx.neonEnabled;
        j["neonIntensity"] = vfx.neonIntensity;
        j["bloomEnabled"] = vfx.bloomEnabled;
        j["bloomThreshold"] = vfx.bloomThreshold;
        j["bloomIntensity"] = vfx.bloomIntensity;
        j["bloomRadius"] = vfx.bloomRadius;
        j["bloomIterations"] = vfx.bloomIterations;
        j["scanlinesEnabled"] = vfx.scanlinesEnabled;
        j["scanlineStrength"] = vfx.scanlineStrength;
        return j.dump();
    }

    bool Context::loadVfxFromJson(const std::string &text)
    {
        json j;
        try { j = json::parse(text); }
        catch (...) { return false; }
        auto getF = [&](const char *k, float &dst) { if (j.contains(k) && j[k].is_number()) dst = j[k].get<float>(); };
        auto getI = [&](const char *k, int &dst) { if (j.contains(k) && j[k].is_number()) dst = j[k].get<int>(); };
        auto getB = [&](const char *k, bool &dst) { if (j.contains(k) && j[k].is_boolean()) dst = j[k].get<bool>(); };
        auto getV3 = [&](const char *k, glm::vec3 &dst) {
            if (j.contains(k) && j[k].is_array() && j[k].size() == 3)
                dst = glm::vec3(j[k][0].get<float>(), j[k][1].get<float>(), j[k][2].get<float>());
        };
        auto getV2 = [&](const char *k, glm::vec2 &dst) {
            if (j.contains(k) && j[k].is_array() && j[k].size() == 2)
                dst = glm::vec2(j[k][0].get<float>(), j[k][1].get<float>());
        };
        getB("skyEnabled", vfx.skyEnabled);
        getV3("skyTop", vfx.skyTop);
        getV3("skyMid", vfx.skyMid);
        getV3("skyBottom", vfx.skyBottom);
        getV2("sunPos", vfx.sunPos);
        getF("sunRadius", vfx.sunRadius);
        getV3("sunColor", vfx.sunColor);
        getB("sunEnabled", vfx.sunEnabled);
        getI("sunStripes", vfx.sunStripes);
        getB("sunWorldAnchored", vfx.sunWorldAnchored);
        getF("sunAzimuth", vfx.sunAzimuth);
        getF("sunElevation", vfx.sunElevation);
        getB("gridEnabled", vfx.gridEnabled);
        getV3("gridColor", vfx.gridColor);
        getF("gridScale", vfx.gridScale);
        getF("gridFade", vfx.gridFade);
        getF("gridLineWidth", vfx.gridLineWidth);
        getF("horizonY", vfx.horizonY);
        getB("wireframeFill", vfx.wireframeFill);
        getB("fogEnabled", vfx.fogEnabled);
        getV3("fogColor", vfx.fogColor);
        getF("fogStart", vfx.fogStart);
        getF("fogEnd", vfx.fogEnd);
        getB("neonEnabled", vfx.neonEnabled);
        getF("neonIntensity", vfx.neonIntensity);
        getB("bloomEnabled", vfx.bloomEnabled);
        getF("bloomThreshold", vfx.bloomThreshold);
        getF("bloomIntensity", vfx.bloomIntensity);
        getF("bloomRadius", vfx.bloomRadius);
        getI("bloomIterations", vfx.bloomIterations);
        getB("scanlinesEnabled", vfx.scanlinesEnabled);
        getF("scanlineStrength", vfx.scanlineStrength);
        return true;
    }

    void Context::saveVfxToLocalStorage()
    {
#ifdef __EMSCRIPTEN__
        // Cheap dirty-skip: only re-write to localStorage when the serialized
        // payload changes, so dragging a slider doesn't write per-frame churn.
        static std::string lastWritten;
        std::string s = serializeVfxToJson();
        if (s != lastWritten)
        {
            em_save_vfx(s.c_str());
            lastWritten = s;
        }
#endif
    }

    void Context::loadVfxFromLocalStorage()
    {
#ifdef __EMSCRIPTEN__
        char *raw = em_load_vfx();
        if (!raw)
            return;
        loadVfxFromJson(raw);
        free(raw);
#endif
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
