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

class ScriptHost;

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
    bool loadFromString(const std::string &jsonData);
    bool saveToJSON(const std::string &path);
    bool saveToMemory(std::string &outData);
    void checkReload();
    void forceReload();

    // Mirror script (.cow) and model (.obj) files from the in-memory filesystem
    // into localStorage so they persist across web page reloads. Snapshotting
    // models is opt-in because OBJ files can be large.
    // No-op on non-web builds.
    static void snapshotScriptsToLocalStorage();
    static void snapshotModelsToLocalStorage();

    // Restore scripts and models from localStorage back into the in-memory
    // filesystem. Call before loading scenes/scripts. No-op on non-web builds.
    static void restoreAssetsFromLocalStorage();

    Object *raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance);

    void addPlayer(std::unique_ptr<Player> player, Window *window, PhysicsWorld &physics);
    Player *getPlayer() { return player.get(); }
    void removePlayer()
    {
        if (player)
        {
            if (player->getRigidBody())
                this->physicsWorld->removeRigidBody(player->getRigidBody());
            player.reset();
        }
    }
    void addObject(std::unique_ptr<Object> obj);
    size_t getObjectCount() const { return objects.size(); }
    Object *getObjectByIndex(size_t index)
    {
        if (index >= objects.size())
            return nullptr;
        return objects[index].get();
    }
    void deleteObject(Object *obj)
    {
        if (!obj)
            return;
        // Remove from physics world if applicable
        if (physicsWorld && obj->getRigidBody())
            physicsWorld->removeRigidBody(obj->getRigidBody());
        // Remove from objects vector
        objects.erase(std::remove_if(objects.begin(), objects.end(),
                                     [obj](const std::unique_ptr<Object> &o)
                                     { return o.get() == obj; }),
                      objects.end());
        // Clear selection if this was the selected object
        if (selectedObject == obj)
            selectedObject = nullptr;
        if (hoveredObject == obj)
            hoveredObject = nullptr;
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

    // Compile and attach scripts to every object that has a scriptPath. Already-compiled
    // scripts are left as-is. Returns the number of scripts attached.
    int loadScripts(ScriptHost &host);

    // Reset script state (drop compiled scripts) and force a recompile on next loadScripts.
    void resetScripts();

    // Invoke `on start` on every script. Should be called once when entering testing mode.
    void startScripts(ScriptHost &host);

    // Invoke `on update(dt)` on every script. Should be called per testing-mode tick.
    void updateScripts(ScriptHost &host, float dt);

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
