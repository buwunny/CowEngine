#include "Scene.hpp"
#include <iostream>
#include "meshes/AssetManager.hpp"
#include "script/CowScript.hpp"
#include "script/ScriptHost.hpp"

// define static current scene pointer
Scene *Scene::s_current = nullptr;

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

EM_JS(void, saveToLocalStorage, (const char *data), {
    localStorage.setItem('cowengine_save', UTF8ToString(data));
});

EM_JS(void, em_local_storage_set, (const char *key, const char *data), {
    try { localStorage.setItem(UTF8ToString(key), UTF8ToString(data)); }
    catch (e) { console.warn('localStorage.setItem failed for', UTF8ToString(key), e); }
});

// Walk both asset localStorage keys and write each entry into the in-memory
// filesystem via FS.writeFile. Doing the entire restore in JS avoids
// round-tripping strings through _malloc/stringToUTF8 (which are not always
// exposed depending on Emscripten settings).
EM_JS(void, em_restore_assets_to_fs, (), {
    function restoreKey(key) {
        var raw = null;
        try { raw = localStorage.getItem(key); } catch (e) { return; }
        if (raw == null) return;
        var obj;
        try { obj = JSON.parse(raw); }
        catch (e) { console.warn('cowengine: bad JSON in', key, e); return; }
        if (!obj || typeof obj !== 'object') return;
        for (var p in obj) {
            if (!obj.hasOwnProperty(p)) continue;
            var v = obj[p];
            if (typeof v !== 'string') continue;
            var abs = (p.charAt(0) === "/") ? p : ("/" + p);
            var parts = abs.split("/");
            var dir = "";
            for (var i = 1; i < parts.length - 1; ++i) {
                dir += "/" + parts[i];
                try { FS.mkdir(dir); } catch (e) { /* exists */ }
            }
            try { FS.writeFile(abs, v); }
            catch (e) { console.error("cowengine: FS.writeFile failed for", abs, e); }
        }
    }
    restoreKey('cowengine_scripts');
    restoreKey('cowengine_models');
});
#endif

using json = nlohmann::json;

void Scene::populateDefault()
{
    // Create some cubes
    objects.push_back(std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f));
    objects.push_back(std::make_unique<Cube>(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f)), glm::vec4(0.0f, 0.5f, 0.5f, 1.0f), 1.0f));
    objects.push_back(std::make_unique<Cube>(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 6.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.0f, 1.0f), 1.0f));
    objects.push_back(std::make_unique<Cube>(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 9.0f, 0.0f)), glm::vec4(0.5f, 0.0f, 0.5f, 1.0f), 1.0f));

    // Add some room/floor objects
    objects.push_back(std::make_unique<Plane>(1000, 1000, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)), glm::vec4(0.60f, 0.60f, 0.60f, 1.0f), 0.0f));

    // Basic room walls and additional static objects
    objects.push_back(std::make_unique<Plane>(50, 45, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 25.0f, 27.5f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.80f, 0.90f, 0.95f, 1.0f), 0.0f));
    objects.push_back(std::make_unique<Plane>(50, 45, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 25.0f, -27.5f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.80f, 0.90f, 0.95f, 1.0f), 0.0f));
    objects.push_back(std::make_unique<Plane>(40, 10, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(50.0f, 30.0f, 0.0f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.85f, 0.85f, 0.90f, 1.0f), 0.0f));
    objects.push_back(std::make_unique<Plane>(50, 100, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, 25.0f, 0.0f)), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec4(0.80f, 0.90f, 0.95f, 1.0f), 0.0f));
    objects.push_back(std::make_unique<Plane>(100, 50, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 25.0f, 50.0f)), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec4(0.75f, 0.90f, 0.80f, 1.0f), 0.0f));
    objects.push_back(std::make_unique<Plane>(100, 50, glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 25.0f, -50.0f)), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec4(0.75f, 0.90f, 0.80f, 1.0f), 0.0f));

    objects.push_back(std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f));
    objects.push_back(std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f));
    objects.push_back(std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f));
}

bool Scene::loadFromJSON(const std::string &path)
{
    namespace fs = std::filesystem;

    // Resolve candidate paths: try given path, ASSET_ROOT + path, relative parents
    std::vector<fs::path> candidates;
    candidates.emplace_back(path);
#ifdef ASSET_ROOT
    candidates.emplace_back(fs::path(ASSET_ROOT) / path);
#endif
    candidates.emplace_back(fs::path("./") / path);
    candidates.emplace_back(fs::path("../") / path);
    candidates.emplace_back(fs::path("../../") / path);

    fs::path chosen;
    for (auto &c : candidates)
    {
        if (fs::exists(c))
        {
            chosen = c;
            break;
        }
    }

    if (chosen.empty())
    {
        std::cerr << "Scene: JSON file not found: " << path << " (tried several candidate locations)" << std::endl;
        return false;
    }

    std::ifstream in(chosen);
    if (!in)
    {
        std::cerr << "Scene: failed to open scene JSON: " << chosen << std::endl;
        return false;
    }

    json j;
    try
    {
        in >> j;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Scene: JSON parse error: " << e.what() << std::endl;
        return false;
    }

    // If we have a physics world recorded, remove rigid bodies for current objects (but keep player)
    if (physicsWorld)
    {
        for (auto &o : objects)
        {
            if (o->getRigidBody())
                physicsWorld->removeRigidBody(o->getRigidBody());
        }
    }

    // Clear current objects but preserve player across reloads
    objects.clear();

    // Parse objects array
    if (j.contains("objects") && j["objects"].is_array())
    {
        for (const auto &obj : j["objects"])
        {
            try
            {
                std::string type = obj.value("type", "StaticObject");

                // Model matrix can be provided directly as a flat 16-array (compatibility),
                // or as user-friendly transform fields: position, rotation (deg), scale.
                glm::mat4 model = glm::mat4(1.0f);
                glm::vec3 pos = glm::vec3(0.0f);
                glm::vec3 rot = glm::vec3(0.0f);
                glm::vec3 scl = glm::vec3(1.0f);
                if (obj.contains("model") && obj["model"].is_array() && obj["model"].size() == 16)
                {
                    glm::mat4 m;
                    for (int i = 0; i < 16; ++i)
                        m[i / 4][i % 4] = obj["model"][i].get<float>();
                    model = m;
                }
                else
                {
                    auto getVec3 = [&](const json &j, const std::string &key, glm::vec3 def) -> glm::vec3
                    {
                        if (j.contains(key) && j[key].is_array())
                        {
                            auto &a = j[key];
                            glm::vec3 v = def;
                            for (size_t ii = 0; ii < std::min<size_t>(3, a.size()); ++ii)
                                (&v.x)[ii] = a[ii].get<float>();
                            return v;
                        }
                        return def;
                    };

                    pos = getVec3(obj, "position", glm::vec3(0.0f));
                    rot = getVec3(obj, "rotation", glm::vec3(0.0f)); // degrees: pitch,x ; yaw,y ; roll,z
                    scl = getVec3(obj, "scale", glm::vec3(1.0f));

                    model = glm::translate(glm::mat4(1.0f), pos);
                    // Rz*Ry*Rx order — matches ImGuizmo and setTransform
                    model = glm::rotate(model, glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f));
                    model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1.0f, 0.0f, 0.0f));
                    model = glm::scale(model, scl);
                }

                glm::vec4 color = glm::vec4(1.0f);
                if (obj.contains("color"))
                {
                    if (obj["color"].is_array())
                    {
                        auto &c = obj["color"];
                        for (size_t k = 0; k < std::min<size_t>(4, c.size()); ++k)
                            (&color.r)[k] = c[k].get<float>();
                    }
                    else if (obj["color"].is_string())
                    {
                        // Parse hex string like "#RRGGBB" or "#RRGGBBAA"
                        std::string s = obj["color"].get<std::string>();
                        if (!s.empty() && s[0] == '#')
                        {
                            try
                            {
                                std::string hex = s.substr(1);
                                unsigned int v = std::stoul(hex, nullptr, 16);
                                if (hex.size() == 6)
                                {
                                    unsigned int r = (v >> 16) & 0xFF;
                                    unsigned int g = (v >> 8) & 0xFF;
                                    unsigned int b = v & 0xFF;
                                    color = glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
                                }
                                else if (hex.size() == 8)
                                {
                                    unsigned int r = (v >> 24) & 0xFF;
                                    unsigned int g = (v >> 16) & 0xFF;
                                    unsigned int b = (v >> 8) & 0xFF;
                                    unsigned int a = v & 0xFF;
                                    color = glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
                                }
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                }

                float mass = obj.value("mass", 0.0f);
                std::string name = obj.value("name", "");
                std::string scriptPath = obj.value("script", "");

                std::unique_ptr<Object> newObject;
                if (type == "Plane")
                {
                    float length = obj.value("length", 100.0f);
                    float width = obj.value("width", 100.0f);
                    newObject = std::make_unique<Plane>(length, width, model, color, mass);
                }
                else if (type == "Cube")
                {
                    int size = obj.value("size", 1);
                    newObject = std::make_unique<Cube>(size, model, color, mass);
                }
                else // StaticObject (mesh)
                {
                    std::string meshPath = obj.value("mesh", "");
                    std::string meshName = obj.value("mesh_name", meshPath);

                    auto &am = AssetManager::instance();
                    std::shared_ptr<Mesh> mesh = nullptr;
                    if (!meshPath.empty())
                        mesh = am.loadStaticMeshFromOBJ(meshPath, meshName);

                    if (mesh)
                    {
                        const auto &verts = mesh->getVertices();
                        const auto &inds = mesh->getIndices();
                        int stride = mesh->getFloatsPerVertex();
                        newObject = std::make_unique<StaticObject>(mesh, verts.data(), verts.size() / stride, inds.data(), inds.size(), stride, model, color, mass);
                    }
                }

                if (newObject)
                {
                    newObject->setInitialModel(model);
                    newObject->setTransform(pos, rot, scl);
                    newObject->setColor(color);
                    newObject->setMass(mass);
                    if (!name.empty())
                        newObject->setName(name);
                    if (!scriptPath.empty())
                        newObject->setScriptPath(scriptPath);
                    if (type != "Plane" && type != "Cube")
                    {
                        std::string meshPath = obj.value("mesh", "");
                        if (!meshPath.empty())
                            newObject->setMeshPath(meshPath);
                    }
                    objects.push_back(std::move(newObject));
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Scene: error parsing object: " << e.what() << std::endl;
            }
        }
    }

    // After constructing objects, add their rigid bodies to the physics world if we have one
    if (physicsWorld)
    {
        for (auto &o : objects)
        {
            if (o->getRigidBody())
                physicsWorld->addRigidBody(o->getRigidBody());
        }
        // Reset player input state so edge-detect keys are consistent after reload
        if (player)
            player->resetInputState();
    }

    // Player block (optional)
    if (j.contains("player") && j["player"].is_object())
    {
        try
        {
            glm::mat4 pm = glm::mat4(1.0f);
            if (j["player"].contains("model") && j["player"]["model"].is_array() && j["player"]["model"].size() == 16)
            {
                for (int i = 0; i < 16; ++i)
                    pm[i / 4][i % 4] = j["player"]["model"][i].get<float>();
            }
            // Create player with default camera pointer null; caller should register camera and window
            player = nullptr; // player will be created by main via scene.addPlayer or left null
        }
        catch (...)
        {
        }
    }

    // remember resolved path for hot-reload
    scenePath = chosen.string();
    lastWriteTime = fs::last_write_time(chosen);
    return true;
}

bool Scene::loadFromString(const std::string &jsonData)
{
    // Load from string by writing to a temporary file and reusing existing logic
    // (could be optimized by refactoring loadFromJSON to separate file reading and parsing)
    std::string tempPath = "scene.json";
    std::ofstream out(tempPath);
    if (!out)
        return false;
    out << jsonData;
    out.close();
    bool result = loadFromJSON(tempPath);
    std::filesystem::remove(tempPath);
    return result;
}

bool Scene::saveToJSON(const std::string &path)
{
    json j;
    j["objects"] = json::array();
    for (auto &o : objects)
    {
        json obj;
        // Try dynamic casts
        if (dynamic_cast<Plane *>(o.get()))
        {
            obj["type"] = "Plane";
            obj["length"] = dynamic_cast<Plane *>(o.get())->getLength();
            obj["width"] = dynamic_cast<Plane *>(o.get())->getWidth();
        }
        else if (dynamic_cast<Cube *>(o.get()))
        {
            obj["type"] = "Cube";
            obj["size"] = dynamic_cast<Cube *>(o.get())->getSize();
        }
        else
        {
            obj["type"] = "StaticObject";
            if (!o->getMeshPath().empty())
                obj["mesh"] = o->getMeshPath();
        }
        glm::vec3 scale, translation, rotation;
        o->getTransform(translation, rotation, scale);
        obj["position"] = {translation.x, translation.y, translation.z};
        obj["rotation"] = {rotation.x, rotation.y, rotation.z};
        obj["scale"] = {scale.x, scale.y, scale.z};

        obj["color"] = o->getColorString();
        obj["name"] = o->getName();
        obj["mass"] = o->getMass();
        if (!o->getScriptPath().empty())
            obj["script"] = o->getScriptPath();

        j["objects"].push_back(obj);
    }

    if (!scenePath.empty())
        j["scenePath"] = scenePath;

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path outPath(path);
    if (outPath.has_parent_path())
        fs::create_directories(outPath.parent_path(), ec);
    std::ofstream out(outPath);
    if (!out)
        return false;
    out << j.dump(4);

// Also save to localStorage for web builds
#ifdef __EMSCRIPTEN__
    saveToLocalStorage(j.dump().c_str());
    snapshotScriptsToLocalStorage();
    snapshotModelsToLocalStorage();
#endif
    return true;
}

namespace
{
#ifdef __EMSCRIPTEN__
    // Walk `root` for files with `ext` and produce a JSON object mapping
    // "root/<rel/path>" -> file contents. Used for the localStorage mirror.
    std::string snapshotDirectory(const std::string &root, const std::string &ext)
    {
        namespace fs = std::filesystem;
        json out = json::object();
        std::error_code ec;
        fs::path rootPath(root);
        if (!fs::exists(rootPath, ec))
            return "";
        for (auto it = fs::recursive_directory_iterator(rootPath, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            const fs::directory_entry &entry = *it;
            if (!entry.is_regular_file(ec))
                continue;
            if (entry.path().extension() != ext)
                continue;
            std::ifstream in(entry.path(), std::ios::binary);
            if (!in)
                continue;
            std::stringstream ss;
            ss << in.rdbuf();
            fs::path rel = fs::relative(entry.path(), rootPath, ec);
            if (ec)
                continue;
            out[root + "/" + rel.generic_string()] = ss.str();
        }
        return out.dump();
    }

#endif
}

void Scene::snapshotScriptsToLocalStorage()
{
#ifdef __EMSCRIPTEN__
    std::string blob = snapshotDirectory("scripts", ".cow");
    if (!blob.empty())
        em_local_storage_set("cowengine_scripts", blob.c_str());
#endif
}

void Scene::snapshotModelsToLocalStorage()
{
#ifdef __EMSCRIPTEN__
    std::string blob = snapshotDirectory("models", ".obj");
    if (!blob.empty())
        em_local_storage_set("cowengine_models", blob.c_str());
#endif
}

void Scene::restoreAssetsFromLocalStorage()
{
#ifdef __EMSCRIPTEN__
    em_restore_assets_to_fs();
#endif
}

void Scene::checkReload()
{
    if (scenePath.empty())
        return;
    namespace fs = std::filesystem;
    fs::path p(scenePath);
    if (!fs::exists(p))
        return;
    auto t = fs::last_write_time(p);
    if (t != lastWriteTime)
    {
        auto now = std::chrono::steady_clock::now();
        if ((now - lastAutoReloadTime) < reloadDebounce)
        {
            // debounce: skip reload if triggered too recently
            return;
        }
        lastAutoReloadTime = now;
        loadFromJSON(scenePath);
    }
}

void Scene::forceReload()
{
    if (scenePath.empty())
        return;
    namespace fs = std::filesystem;
    fs::path p(scenePath);
    if (!fs::exists(p))
        return;
    std::cout << "Scene: force reload requested for " << scenePath << std::endl;
    loadFromJSON(scenePath);
    // update auto-reload timestamp to avoid immediate auto-reloads
    lastAutoReloadTime = std::chrono::steady_clock::now();
}

void Scene::addPlayer(std::unique_ptr<Player> pl, Window *window, PhysicsWorld &physics)
{
    if (!pl)
        return;
    player = std::move(pl);
    // register input callbacks
    if (window && player)
    {
#ifndef __EMSCRIPTEN__
        glfwSetWindowUserPointer(window->getWindow(), player.get());
        glfwSetCursorPosCallback(window->getWindow(), Player::mouse_callback);
#else
        // On web builds, forward mouse events from the Window wrapper to the player
        Window::setEmscriptenPlayer(player.get());
#endif
    }
    // add rigid body to physics
    if (player->getRigidBody())
        physics.addRigidBody(player->getRigidBody());
    // remember physics world for later reload cleanup
    physicsWorld = &physics;

    // Register this scene as the current active scene for runtime spawns
    Scene::s_current = this;
}

void Scene::addObject(std::unique_ptr<Object> obj)
{
    if (!obj)
        return;
    // If a physics world is present, register the rigid body
    if (physicsWorld && obj->getRigidBody())
        physicsWorld->addRigidBody(obj->getRigidBody());
    objects.push_back(std::move(obj));
}

Object *Scene::raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance)
{
    if (!Scene::s_current || !Scene::s_current->physicsWorld)
        return nullptr;

    btVector3 from(origin.x, origin.y, origin.z);
    btVector3 to = from + btVector3(direction.x, direction.y, direction.z) * maxDistance;

    btCollisionWorld::ClosestRayResultCallback rayCallback(from, to);
    Scene::s_current->physicsWorld->rayTest(from, to, rayCallback);

    if (rayCallback.hasHit())
    {
        const btCollisionObject *colObj = rayCallback.m_collisionObject;
        // Convert from btCollisionObject to our Object class using the user pointer
        if (colObj && colObj->getUserPointer())
        {
            hoveredObject = static_cast<Object *>(colObj->getUserPointer());
            return hoveredObject;
        }
    }
    hoveredObject = nullptr;
    return hoveredObject;
}

void Scene::render(Window &window, Shader &shader)
{
    for (auto &obj : objects)
    {
        if (obj.get() == selectedObject)
        {
            obj->setSelected(true);
            obj->setHovered(false);
        }
        else if (obj.get() == hoveredObject)
        {
            obj->setSelected(false);
            obj->setHovered(true);
        }
        else
        {
            obj->setSelected(false);
            obj->setHovered(false);
        }
        obj->render(window, shader);
    }
}

void Scene::renderTransparent(Window &window, Shader &shader)
{
    for (auto &obj : objects)
    {
        obj->renderTransparent(window, shader);
    }
}

void Scene::renderFill(Window &window, Shader &shader)
{
    for (auto &obj : objects)
    {
        obj->renderFill(window, shader);
    }
}

int Scene::loadScripts(ScriptHost &host)
{
    int count = 0;
    auto attach = [&](Object *o)
    {
        if (!o)
            return;
        const std::string &path = o->getScriptPath();
        if (path.empty() || o->getScript())
            return;
        std::string foundPath;
        std::string source = cowscript::readScriptFile(path, &foundPath);
        if (source.empty())
        {
            std::cerr << "Scene: failed to read script '" << path << "'" << std::endl;
            return;
        }
        auto script = std::make_shared<cowscript::Script>();
        host.bindBuiltins(*script);
        std::string err = script->compile(source);
        if (!err.empty())
        {
            std::cerr << "Scene: script compile error in '" << path << "': " << err << std::endl;
            return;
        }
        o->setScript(script);
        ++count;
    };
    if (player)
        attach(player.get());
    for (auto &o : objects)
        attach(o.get());
    return count;
}

void Scene::resetScripts()
{
    for (auto &o : objects)
        o->setScript(nullptr);
    if (player)
        player->setScript(nullptr);
}

void Scene::startScripts(ScriptHost &host)
{
    auto runStart = [&](Object *o)
    {
        if (!o || !o->getScript())
            return;
        host.setSelf(o);
        std::string err = o->getScript()->callEvent("start", {});
        if (!err.empty())
            std::cerr << "Scene: '" << o->getScriptPath() << "' on start: " << err << std::endl;
    };
    if (player)
        runStart(player.get());
    for (auto &o : objects)
        runStart(o.get());
    host.setSelf(nullptr);
}

void Scene::updateScripts(ScriptHost &host, float dt)
{
    std::vector<cowscript::Value> args;
    args.push_back(cowscript::Value::makeNumber(dt));
    auto runUpdate = [&](Object *o)
    {
        if (!o || !o->getScript())
            return;
        host.setSelf(o);
        std::string err = o->getScript()->callEvent("update", args);
        if (!err.empty())
            std::cerr << "Scene: '" << o->getScriptPath() << "' on update: " << err << std::endl;
    };
    if (player)
        runUpdate(player.get());
    for (auto &o : objects)
        runUpdate(o.get());
    host.setSelf(nullptr);
}