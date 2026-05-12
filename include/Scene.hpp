#ifndef SCENE_HPP
#define SCENE_HPP

#include "objects/Object.hpp"
#include "objects/Cube.hpp"
#include "objects/Plane.hpp"
#include "objects/StaticObject.hpp"
#include "objects/Player.hpp"
#include "rooms/Room.hpp"
#include "meshes/AssetManager.hpp"
#include "PhysicsWorld.hpp"

#include <vector>
#include <memory>
#include <filesystem>
#include <chrono>

#include <nlohmann/json.hpp>

class Scene
{
public:
    Scene() = default;
    ~Scene() = default;

    void populateDefault();
    bool loadFromJSON(const std::string &path);
    bool saveToJSON(const std::string &path);
    void checkReload();
    void forceReload();

    void addPlayer(std::unique_ptr<Player> player, Window *window, PhysicsWorld &physics);
    Player *getPlayer() { return player.get(); }
    void addObject(std::unique_ptr<Object> obj);

    void addRigidBodiesToWorld(PhysicsWorld &physics)
    {
        for (auto &obj : objects)
            physics.addRigidBody(obj->getRigidBody());
        physicsWorld = &physics;
    }

    void update()
    {
        for (auto &obj : objects)
            obj->update();
        if (player)
            player->update();
    }

    void render(Window &window, Shader &shader)
    {
        for (auto &obj : objects)
            obj->render(window, shader);
    }

    void renderTransparent(Window &window, Shader &shader)
    {
        for (auto &obj : objects)
            obj->renderTransparent(window, shader);
    }

    void renderFill(Window &window, Shader &shader)
    {
        for (auto &obj : objects)
            obj->renderFill(window, shader);
    }

private:
    std::vector<std::unique_ptr<Object>> objects;
    std::unique_ptr<Player> player;
    std::string scenePath;
    std::filesystem::file_time_type lastWriteTime;
    PhysicsWorld *physicsWorld = nullptr;
    std::chrono::steady_clock::time_point lastAutoReloadTime = std::chrono::steady_clock::time_point::min();
    std::chrono::milliseconds reloadDebounce{500};
};

#endif // SCENE_HPP
