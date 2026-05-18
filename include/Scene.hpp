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
#include <string>

#include <nlohmann/json.hpp>

class Scene
{
public:
    Scene() = default;
    ~Scene() = default;

    void populateDefault();
    bool loadFromJSON(const std::string &path);
    bool saveToJSON(const std::string &path);
    bool saveToMemory(std::string &outData);
    void checkReload();
    void forceReload();

    Object *raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance);

    void addPlayer(std::unique_ptr<Player> player, Window *window, PhysicsWorld &physics);
    Player *getPlayer() { return player.get(); }
    void addObject(std::unique_ptr<Object> obj);
    size_t getObjectCount() const { return objects.size(); }
    Object *getObjectByIndex(size_t index)
    {
        if (index >= objects.size())
            return nullptr;
        return objects[index].get();
    }
    void setSelectedObject(Object *obj) { selectedObject = obj; }
    Object *getSelectedObject() const { return selectedObject; }
    const std::string &getScenePath() const { return scenePath; }

    // Global accessor for the active scene (set when a scene registers a player)
    static Scene *getCurrent() { return s_current; }

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

    void render(Window &window, Shader &shader);    

    void renderTransparent(Window &window, Shader &shader);

    void renderFill(Window &window, Shader &shader);

private:
    std::vector<std::unique_ptr<Object>> objects;
    std::unique_ptr<Player> player;
    std::string scenePath;
    std::filesystem::file_time_type lastWriteTime;
    PhysicsWorld *physicsWorld = nullptr;
    std::chrono::steady_clock::time_point lastAutoReloadTime = std::chrono::steady_clock::time_point::min();
    std::chrono::milliseconds reloadDebounce{500};
    static Scene *s_current;
    Object *selectedObject = nullptr;
    Object *hoveredObject = nullptr;
};

#endif // SCENE_HPP
