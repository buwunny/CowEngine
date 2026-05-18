#include "Scene.hpp"
#include <iostream>
#include "meshes/AssetManager.hpp"
#include "../cow_mesh.hpp"

// define static current scene pointer
Scene *Scene::s_current = nullptr;

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void Scene::populateDefault()
{
    // Create some cubes
    objects.push_back(std::make_unique<Cube>(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 15.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), 10.0f));
    objects.push_back(std::make_unique<Cube>(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 0.0f)), glm::vec4(0.0f, 0.5f, 0.5f, 1.0f), 1.0f));
    objects.push_back(std::make_unique<Cube>(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 6.0f, 0.0f)), glm::vec4(0.5f, 0.5f, 0.0f, 1.0f), 1.0f));
    objects.push_back(std::make_unique<Cube>(2, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 9.0f, 0.0f)), glm::vec4(0.5f, 0.0f, 0.5f, 1.0f), 1.0f));

    // Shared cow mesh
    auto &assetManager = AssetManager::instance();
    auto cowMesh = assetManager.loadStaticMeshFromOBJ("models/cow.obj", "cow");
    if (!cowMesh)
        cowMesh = assetManager.loadStaticMeshFromArrays("cow", cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3);

    int numObjects = 50;
    for (int i = 4; i < numObjects; i++)
    {
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        float b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        objects.push_back(std::make_unique<StaticObject>(cowMesh, cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 10.0f, 5.0f)), glm::vec4(r, g, b, 1.0f), 1.0f));
    }

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

    // Additional scene setup could be added here
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
                    // Apply yaw (y), pitch (x), roll (z) in that order
                    model = glm::rotate(model, glm::radians(rot.y), glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::rotate(model, glm::radians(rot.x), glm::vec3(1.0f, 0.0f, 0.0f));
                    model = glm::rotate(model, glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f));
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
                    if (!mesh)
                    {
                        // fallback to cow embedded mesh
                        mesh = am.loadStaticMeshFromArrays("cow", cow_mesh_vertices, cow_mesh_vertex_count, cow_mesh_indices, cow_mesh_index_count, 3);
                    }

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
            // If mesh present, attempt to reference by name not available here; leave blank
        }
        glm::vec3 scale, translation, rotation;
        o->getTransform(translation, rotation, scale);
        obj["position"] = {translation.x, translation.y, translation.z};
        obj["rotation"] = {rotation.x, rotation.y, rotation.z};
        obj["scale"] = {scale.x, scale.y, scale.z};

        obj["color"] = o->getColorString();
        obj["name"] = o->getName();
        obj["mass"] = o->getMass();

        j["objects"].push_back(obj);
    }

    if (!scenePath.empty())
        j["scenePath"] = scenePath;

    std::ofstream out(path);
    if (!out)
        return false;
    out << j.dump(4);
    return true;
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
#ifndef EMSCRIPTEN
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
            return static_cast<Object *>(colObj->getUserPointer());
        }
    }
    return nullptr;
}
