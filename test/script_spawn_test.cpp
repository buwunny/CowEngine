// Headless test for script-driven spawning: where objects land, and how long they
// live. No GL context, no sockets — just a Scene, a PhysicsWorld and the script
// systems. Verifies:
//   * the real scripts/shoot_cow.cow puts a cow's *visible centre* on the camera's
//     view axis (cow.obj's origin is 0.89 units off its own centre, which used to
//     land the shot ~2.5 degrees beside the crosshair)
//   * spawn_cow's optional scale argument sizes the collision shape to match
//   * attach_script compiles and attaches a .cow to a spawned object
//   * on start() fires for a script attached after startScripts() has run
//   * several objects can be alive at once, each despawning on its own clock
//     (the regression: shoot_cow used to destroy the previous cow on every shot)
//   * scripts that spawn/attach/destroy during updateScripts() don't corrupt the
//     iteration over the ScriptComponent pool

#include "core/Scene.hpp"
#include "core/PhysicsWorld.hpp"
#include "core/Camera.hpp"
#include "ecs/Components.hpp"
#include "ecs/Factories.hpp"
#include "ecs/InputKeys.hpp"
#include "ecs/systems/ScriptSystem.hpp"
#include "meshes/AssetManager.hpp"
#include "script/ScriptHost.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <btBulletDynamicsCommon.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while(0)

namespace
{
    // Count the live cows, i.e. everything the spawner produced (the driver
    // entity itself carries no mesh).
    int liveCows(Scene &scene)
    {
        int n = 0;
        for (auto e : scene.registry().view<ecs::Identity>())
            if (scene.registry().get<ecs::Identity>(e).meshPath.find("cow") != std::string::npos)
                ++n;
        return n;
    }

    // Advance the sim by one frame at `dt`, exactly as GameServer::tick does.
    void step(Scene &scene, PhysicsWorld &phys, ScriptHost &host, double &t, float dt)
    {
        phys.stepSimulation(dt, 1);
        t += dt;
        host.setTime(t);
        host.setDelta(dt);
        scene.updateScripts(host, dt);
    }
}

// Fires the real scripts/shoot_cow.cow from a known eye position and facing, and
// checks where the cow actually ends up on screen.
static void testShotIsCentred()
{
    PhysicsWorld physics;
    Scene scene;
    scene.populateDefault();
    scene.addRigidBodiesToWorld(physics);

    ScriptHost host;
    host.setContext(&scene, nullptr);

    const glm::vec3 eye(3.0f, 12.0f, -7.0f);
    Camera camera(eye, glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    camera.setLook(37.0f, -11.0f); // an arbitrary off-axis pose, not a lucky one
    camera.setPosition(eye);

    // shoot_cow.cow alone: player_movement.cow would move the camera off `eye`.
    ecs::Entity player = ecs::createPlayer(scene.registry(), &physics, &camera,
                                           glm::translate(glm::mat4(1.0f), eye));
    scene.registry().get<ecs::Identity>(player).scriptPaths = {"scripts/shoot_cow.cow"};
    auto &input = scene.registry().emplace<ecs::PlayerInput>(player);

    host.setTime(0.0);
    host.setDelta(0.0);
    scene.loadScripts(host);
    scene.startScripts(host);

    input.keys = 1ull << ecs::inputKeyBit("c"); // hold C: one shot on the edge
    host.setTime(0.1);
    host.setDelta(1.0f / 60.0f);
    scene.updateScripts(host, 1.0f / 60.0f);

    ecs::Entity cow = ecs::NullEntity;
    for (auto e : scene.registry().view<ecs::Identity>())
        if (scene.registry().get<ecs::Identity>(e).meshPath.find("cow") != std::string::npos)
            cow = e;
    CHECK(cow != ecs::NullEntity);
    if (cow == ecs::NullEntity)
        return;

    const auto &t = scene.registry().get<ecs::Transform>(cow);
    const auto &rd = scene.registry().get<ecs::Renderable>(cow);
    CHECK(std::fabs(t.scale.x - 0.1) < 1e-5); // the scale argument took effect

    // Where the player sees the cow: its model origin plus the mesh's own
    // off-centre bounding box, scaled the same way the renderer scales it.
    glm::vec3 visible = glm::vec3(t.position) + glm::vec3(t.scale) * rd.mesh->getLocalCenter();
    glm::vec3 toCow = visible - eye;
    glm::vec3 front = camera.getFront();

    float distance = glm::length(toCow);
    float offAxis = glm::degrees(std::acos(glm::clamp(glm::dot(glm::normalize(toCow), front), -1.0f, 1.0f)));
    printf("  shot lands %.3f m ahead, %.4f deg off the view axis\n", distance, offAxis);
    CHECK(offAxis < 0.01f);                      // dead centre (was ~2.55 deg)
    CHECK(std::fabs(distance - 2.0f) < 0.01f);   // at the script's `muzzle` distance

    // The collision hull is sized for the cow the player can see, not the
    // full-size model — the scale reaches Bullet at spawn, not a frame later.
    const auto &phys = scene.registry().get<ecs::Physics>(cow);
    btVector3 lo, hi;
    phys.shape->getAabb(btTransform::getIdentity(), lo, hi);
    printf("  collider extent = %.3f m (cow.obj is 10.44 m at scale 1)\n", hi.x() - lo.x());
    CHECK(hi.x() - lo.x() < 1.2f);
}

int main()
{
    testShotIsCentred();

    PhysicsWorld physics;
    Scene scene;
    scene.populateDefault();
    scene.addRigidBodiesToWorld(physics);

    ScriptHost host;
    host.setContext(&scene, nullptr);

    // A driver entity standing in for the player: it fires one cow per call to
    // its `shoot` flag, the same way shoot_cow.cow does on a key edge.
    ecs::Entity driver = scene.createEmpty("Driver", glm::mat4(1.0f));
    auto &ident = scene.registry().get<ecs::Identity>(driver);
    ident.scriptPaths = {"scripts/test_shooter.cow"};

    // Written next to the scripts the engine searches so readScriptFile finds it.
    {
        FILE *f = std::fopen("scripts/test_shooter.cow", "w");
        CHECK(f != nullptr);
        if (!f)
            return 1;
        std::fputs(
            "let fired = 0\n"
            "let next_shot = 0\n"
            "on update(dt) {\n"
            "    if (fired < 3 and time() >= next_shot) {\n"
            "        let cow = spawn_cow(0, 5, 0)\n"
            "        attach_script(cow, \"scripts/despawn_after.cow\")\n"
            "        fired = fired + 1\n"
            "        next_shot = time() + 1\n"
            "    }\n"
            "}\n", f);
        std::fclose(f);
    }

    double t = 0.0;
    host.setTime(t);
    host.setDelta(0.0);
    scene.loadScripts(host);
    scene.startScripts(host);

    const float dt = 0.1f;

    // 0.0s: first shot. 1.0s: second. 2.0s: third. despawn_after.cow gives each
    // cow 4 seconds, so all three are airborne together at t≈2.5 — the whole
    // point of moving the lifetime onto the cow.
    for (int i = 0; i < 25; ++i)
        step(scene, physics, host, t, dt);
    printf("  t=%.1f live cows = %d (expect 3 coexisting)\n", t, liveCows(scene));
    CHECK(liveCows(scene) == 3);

    // Each cow got its own compiled script and its own on start().
    {
        int scripted = 0;
        for (auto e : scene.registry().view<ecs::ScriptComponent>())
        {
            const auto &sc = scene.registry().get<ecs::ScriptComponent>(e);
            for (const auto &inst : sc.scripts)
                if (inst.path == "scripts/despawn_after.cow")
                {
                    CHECK(inst.started); // start() ran even though it was attached late
                    ++scripted;
                }
        }
        printf("  cows carrying despawn_after.cow = %d\n", scripted);
        CHECK(scripted == 3);
        // Recorded on Identity too, so a scene save round-trips the attachment.
        for (auto e : scene.registry().view<ecs::Identity>())
        {
            const auto &id = scene.registry().get<ecs::Identity>(e);
            if (id.meshPath.find("cow") != std::string::npos)
                CHECK(id.scriptPaths.size() == 1 && id.scriptPaths[0] == "scripts/despawn_after.cow");
        }
    }

    // Past the first cow's 4s lifetime but not the third's: they expire in the
    // order they were fired, one at a time, rather than all at once.
    for (int i = 0; i < 20; ++i) // t -> 4.5
        step(scene, physics, host, t, dt);
    printf("  t=%.1f live cows = %d (expect 2)\n", t, liveCows(scene));
    CHECK(liveCows(scene) == 2);

    for (int i = 0; i < 10; ++i) // t -> 5.5
        step(scene, physics, host, t, dt);
    printf("  t=%.1f live cows = %d (expect 1)\n", t, liveCows(scene));
    CHECK(liveCows(scene) == 1);

    for (int i = 0; i < 10; ++i) // t -> 6.5
        step(scene, physics, host, t, dt);
    printf("  t=%.1f live cows = %d (expect 0)\n", t, liveCows(scene));
    CHECK(liveCows(scene) == 0);

    // The driver survived its cows destroying themselves mid-iteration.
    CHECK(scene.registry().valid(driver));
    CHECK(scene.registry().all_of<ecs::ScriptComponent>(driver));

    std::remove("scripts/test_shooter.cow");
    printf(failures ? "FAILURES: %d\n" : "ALL PASS\n", failures);
    return failures ? 1 : 0;
}
