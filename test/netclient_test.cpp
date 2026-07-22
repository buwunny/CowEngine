// Headless integration test for the client netcode (NetClient), driven by a
// mock transport so it needs no server, sockets, or GL context. Verifies:
//   * ClientHello sent on connect; ServerWelcome learned; InputCommand streamed
//   * remote players become interpolated avatar entities (join)
//   * PlayerLeave removes the avatar
//   * the local player is snapped when it drifts far from the server state

#include "core/Scene.hpp"
#include "core/PhysicsWorld.hpp"
#include "core/Camera.hpp"
#include "ecs/Components.hpp"
#include "net/NetClient.hpp"
#include "net/ITransport.hpp"
#include "net/Protocol.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <btBulletDynamicsCommon.h>

#include <cstdio>
#include <deque>
#include <vector>
#include <string>

using namespace net;

static int failures = 0;
#define CHECK(c) do { if(!(c)){ printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while(0)

// Transport that records outgoing messages and replays injected inbound ones.
struct MockTransport : ITransport
{
    std::deque<Incoming> inbox;
    std::vector<Message> sent;
    bool up = true;

    void sendUnreliable(const uint8_t *d, size_t n) override { rec(d, n); }
    void sendReliable(const uint8_t *d, size_t n) override { rec(d, n); }
    void rec(const uint8_t *d, size_t n) { if (auto m = decode(d, n)) sent.push_back(*m); }
    bool poll(Incoming &out) override
    {
        if (inbox.empty()) return false;
        out = std::move(inbox.front());
        inbox.pop_front();
        return true;
    }
    TransportState state() const override
    {
        return up ? TransportState::Connected : TransportState::Disconnected;
    }
    void inject(const Message &m)
    {
        Incoming i;
        i.channel = channelFor(typeOf(m));
        i.bytes = encode(m);
        inbox.push_back(std::move(i));
    }
    template <class T> int countSent() const
    {
        int n = 0;
        for (auto &m : sent) if (std::holds_alternative<T>(m)) ++n;
        return n;
    }
};

// Find the remote-avatar entity (there is at most one in these tests) and read
// its world-space translation from Transform.model.
static bool remoteAvatarPos(Scene &scene, glm::vec3 &out)
{
    auto view = scene.registry().view<ecs::Identity, ecs::Transform>();
    for (auto e : view)
    {
        if (view.get<ecs::Identity>(e).name == "RemotePlayer")
        {
            out = glm::vec3(view.get<ecs::Transform>(e).model[3]);
            return true;
        }
    }
    return false;
}

static glm::vec3 entityPos(Scene &scene, ecs::Entity e)
{
    return glm::vec3(scene.registry().get<ecs::Transform>(e).model[3]);
}

static ecs::Entity findByName(Scene &scene, const char *name)
{
    auto view = scene.registry().view<ecs::Identity>();
    for (auto e : view)
        if (view.get<ecs::Identity>(e).name == name)
            return e;
    return ecs::NullEntity;
}

// A proxy hidden until its first snapshot has a zero model matrix ([3][3]==0);
// once placed it's a normal affine transform ([3][3]==1).
static bool isHidden(Scene &scene, ecs::Entity e)
{
    return scene.registry().get<ecs::Transform>(e).model[3][3] == 0.0f;
}

static const float DT = 1.0f / 60.0f;

// Reproduction against the real scenes/scene.json: after NetClient claims the
// dynamic (mass>0) scene objects, each must keep a kinematic collider (not a
// render-only ghost) and render at its authored scale.
static void checkRealSceneClaim()
{
    PhysicsWorld physics;
    Scene scene;
    Camera cam(glm::vec3(0, 3, 10), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    if (!scene.loadFromJSON("scenes/scene.json"))
    {
        printf("  (real-scene repro skipped: scenes/scene.json not found)\n");
        return;
    }
    scene.addRigidBodiesToWorld(physics);
    scene.addPlayer(&cam, glm::translate(glm::mat4(1.0f), glm::vec3(0, 3, 10)), nullptr, physics);
    ecs::Entity local = scene.getPlayerEntity();

    // Record the mass>0 scene objects and their authored scale BEFORE claiming.
    struct Obj { ecs::Entity e; uint32_t id; glm::vec3 scale; std::string name; };
    std::vector<Obj> dyn;
    {
        auto view = scene.registry().view<ecs::Physics, ecs::Identity, ecs::Transform>();
        for (auto e : view)
        {
            if (e == local) continue;
            if (view.get<ecs::Physics>(e).mass <= 0.0) continue;
            dyn.push_back({e, (uint32_t)view.get<ecs::Identity>(e).id,
                           glm::vec3(view.get<ecs::Transform>(e).scale),
                           view.get<ecs::Identity>(e).name});
        }
    }
    printf("  real scene: %zu dynamic (mass>0) objects to claim\n", dyn.size());

    MockTransport mock;
    NetClient nc(&mock, &scene, local);
    mock.inject(ServerWelcome{kPlayerNetIdBase, 0, 60});
    nc.update(DT);

    // Drive every claimed object with a snapshot at a fresh position.
    Snapshot s; s.serverTick = 1; s.ackSeq = 1;
    for (auto &o : dyn) { EntityState es; es.netId = o.id; es.pos = {0, 20, 0}; s.entities.push_back(es); }
    mock.inject(s);
    nc.update(DT);
    physics.stepSimulation(DT, 1);
    scene.syncFromPhysics();

    auto modelScale = [&](ecs::Entity e) {
        const glm::mat4 &m = scene.registry().get<ecs::Transform>(e).model;
        return glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])),
                         glm::length(glm::vec3(m[2])));
    };
    for (auto &o : dyn)
    {
        bool hasBody = false, kin = false;
        if (auto *p = scene.registry().try_get<ecs::Physics>(o.e))
        { hasBody = p->body != nullptr; kin = p->body && p->body->isKinematicObject(); }
        glm::vec3 rs = modelScale(o.e);
        bool scaleOk = glm::distance(rs, o.scale) < std::max(0.05f, 0.05f * glm::length(o.scale));
        printf("    %-16s id=%u collider=%d kinematic=%d authoredScale=(%.2f,%.2f,%.2f) render=(%.2f,%.2f,%.2f) %s\n",
               o.name.c_str(), o.id, hasBody, kin, o.scale.x, o.scale.y, o.scale.z,
               rs.x, rs.y, rs.z, (hasBody && kin && scaleOk) ? "" : "  <-- BAD");
        CHECK(hasBody);   // not a render-only ghost
        CHECK(kin);       // kinematic so physicsSyncSystem won't clobber it
        CHECK(scaleOk);   // rendered at authored scale
    }
}

int main()
{
    checkRealSceneClaim();

    PhysicsWorld physics;
    Scene scene;
    Camera cam(glm::vec3(0, 3, 10), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

    // A dynamic scene body (like Cube 2/3): NetClient should claim it — remove it
    // from the physics world and follow the server. Its netId == Identity.id.
    // Scale 5, like "Cube 2" in scenes/scene.json — exercises non-unit scale so a
    // render-scale regression (rendering at localScale vs net scale) would show.
    ecs::Entity sceneCube = scene.spawnCube(1, glm::translate(glm::mat4(1.0f), glm::vec3(5, 1, 5)) *
                                                   glm::scale(glm::mat4(1.0f), glm::vec3(5.0f)),
                                            glm::vec4(1, 0, 0, 1), 1.0f);
    uint32_t sceneCubeNet = scene.registry().get<ecs::Identity>(sceneCube).id;

    scene.addPlayer(&cam, glm::translate(glm::mat4(1.0f), glm::vec3(0, 3, 10)), nullptr, physics);
    ecs::Entity local = scene.getPlayerEntity();

    MockTransport mock;
    NetClient nc(&mock, &scene, local);
    nc.setInterpDelay(0.05);
    nc.setSnapThreshold(1.5f);

    // The scene cube is now claimed: script gone (it had none) and, crucially,
    // its rigid body was removed from the physics world so it won't be simulated.
    CHECK(nc.replicatedCount() == 1); // the scene cube

    const uint32_t MY = kPlayerNetIdBase;      // our player
    const uint32_t OTHER = kPlayerNetIdBase + 1; // another player
    const uint32_t SPAWN = kSpawnNetIdBase;     // a server-spawned cow/cube

    // 1) Hello on connect.
    nc.update(DT);
    CHECK(mock.countSent<ClientHello>() == 1);
    CHECK(!nc.joined());

    // 2) Welcome -> joined + our netId; subsequent ticks stream input.
    mock.inject(ServerWelcome{MY, 0, 60});
    nc.update(DT);
    CHECK(nc.joined());
    CHECK(nc.myNetId() == MY);
    CHECK(mock.countSent<InputCommand>() >= 1);

    // Helper: a snapshot carrying us, the scene cube, and one other player.
    auto snap = [&](uint32_t tick, glm::vec3 cubePos, glm::vec3 otherPos) {
        Snapshot s;
        s.serverTick = tick;
        s.ackSeq = 1;
        EntityState me; me.netId = MY; me.pos = {0, 3, 10}; // matches local: no snap
        EntityState cube; cube.netId = sceneCubeNet; cube.pos = cubePos;
        EntityState other; other.netId = OTHER; other.pos = otherPos;
        s.entities = {me, cube, other};
        mock.inject(s);
    };

    // 3) First snapshot: the other player becomes an avatar; the scene cube is
    // driven to the server position (not its local spawn at (5,1,5)).
    snap(100, glm::vec3(5, 8, 5), glm::vec3(0, 1, 0));
    nc.update(DT);
    CHECK(nc.replicatedCount() == 2); // scene cube + other-player avatar
    glm::vec3 rpos;
    CHECK(remoteAvatarPos(scene, rpos));
    glm::vec3 cpos = entityPos(scene, sceneCube);
    CHECK(std::fabs(cpos.y - 8.0f) < 0.01f); // cube follows server, not gravity/local
    printf("  scene cube y = %.2f (server-driven)\n", cpos.y);

    // The claimed scene cube must: (a) still carry a collider (kinematic body kept
    // in the world) so it isn't a render-only "ghost"; (b) be marked kinematic so
    // physicsSyncSystem skips it; (c) render at its NET scale (5), and keep that
    // scale after a physics step + syncFromPhysics render frame.
    {
        auto *cp = scene.registry().try_get<ecs::Physics>(sceneCube);
        CHECK(cp && cp->body);                              // collider present
        CHECK(cp && cp->body && cp->body->isKinematicObject());
        auto cubeScale = [&]() {
            const glm::mat4 &m = scene.registry().get<ecs::Transform>(sceneCube).model;
            return glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])),
                             glm::length(glm::vec3(m[2])));
        };
        glm::vec3 sc0 = cubeScale();
        physics.stepSimulation(DT, 1);
        scene.syncFromPhysics();
        glm::vec3 sc1 = cubeScale();
        printf("  claimed scene cube scale before/after frame = (%.2f,%.2f,%.2f)/(%.2f,%.2f,%.2f)\n",
               sc0.x, sc0.y, sc0.z, sc1.x, sc1.y, sc1.z);
        CHECK(std::fabs(sc0.x - 5.0f) < 0.05f);   // rendered at net scale, not 1
        CHECK(glm::distance(sc0, sc1) < 0.01f);   // survives a render frame
    }

    // 4) A server-spawned object (SpawnEntity) becomes a proxy and follows snaps.
    // Before its first snapshot it is hidden and has no collider (avoids an
    // origin flash and spawning a kinematic body on top of the player).
    mock.inject(SpawnEntity{SPAWN, SpawnKind::Cube, glm::vec3(0.1f), glm::vec4(1)});
    nc.update(DT);
    CHECK(nc.replicatedCount() == 3);
    ecs::Entity proxy = findByName(scene, "NetProxy");
    CHECK(proxy != ecs::NullEntity);
    CHECK(isHidden(scene, proxy));                                  // not yet placed
    CHECK(!scene.registry().any_of<ecs::Physics>(proxy));          // no collider yet
    {
        Snapshot s; s.serverTick = 101; s.ackSeq = 1;
        EntityState sp; sp.netId = SPAWN; sp.pos = {2, 2, 2};
        s.entities = {sp};
        mock.inject(s);
    }
    nc.update(DT);
    // Placed now: visible, positioned, and carrying a kinematic collider so the
    // predicted local player can't walk through it.
    CHECK(!isHidden(scene, proxy));
    CHECK(scene.registry().any_of<ecs::Physics>(proxy));
    {
        glm::vec3 pp = entityPos(scene, proxy);
        CHECK(glm::distance(pp, glm::vec3(2, 2, 2)) < 0.01f);
        printf("  spawned proxy placed at (%.1f,%.1f,%.1f) with collider\n", pp.x, pp.y, pp.z);
    }

    // The proxy was spawned with net scale 0.1. Its render model must reflect
    // that scale, and a syncFromPhysics() pass (which runs every render frame and
    // rebuilds Transform.model from the physics body at the entity's *local*
    // scale) must NOT clobber it back to scale 1 — that would leave a wrongly
    // scaled duplicate over the net-driven proxy ("ghost" bug).
    auto modelScale = [&](ecs::Entity e) {
        const glm::mat4 &m = scene.registry().get<ecs::Transform>(e).model;
        return glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])),
                         glm::length(glm::vec3(m[2])));
    };
    {
        glm::vec3 s0 = modelScale(proxy);
        scene.syncFromPhysics();
        glm::vec3 s1 = modelScale(proxy);
        printf("  proxy model scale before/after syncFromPhysics = "
               "(%.2f,%.2f,%.2f)/(%.2f,%.2f,%.2f)\n",
               s0.x, s0.y, s0.z, s1.x, s1.y, s1.z);
        CHECK(std::fabs(s0.x - 0.1f) < 0.01f);       // net scale applied
        CHECK(glm::distance(s0, s1) < 0.001f);       // survives a render frame
        glm::vec3 pp = entityPos(scene, proxy);
        CHECK(glm::distance(pp, glm::vec3(2, 2, 2)) < 0.01f); // position too
    }
    nc.update(DT);

    // 5) Interpolation: advance ~0.1s then move the other player to x=10; the
    // rendered avatar should sit strictly between the two samples.
    for (int i = 0; i < 6; ++i) nc.update(DT);
    snap(106, glm::vec3(5, 8, 5), glm::vec3(10, 1, 0));
    nc.update(DT);
    nc.update(DT);
    CHECK(remoteAvatarPos(scene, rpos));
    CHECK(rpos.x > 0.5f && rpos.x < 9.5f);
    printf("  interpolated other-player x = %.2f (between 0 and 10)\n", rpos.x);

    // 6) Local reconciliation: server says we're far away -> snap the body.
    {
        Snapshot s; s.serverTick = 110; s.ackSeq = 5;
        EntityState me; me.netId = MY; me.pos = {100, 3, 10};
        s.entities = {me};
        mock.inject(s);
        nc.update(DT);
        auto *p = scene.registry().try_get<ecs::Physics>(local);
        CHECK(p && p->body);
        glm::vec3 bp(p->body->getWorldTransform().getOrigin().x(),
                    p->body->getWorldTransform().getOrigin().y(),
                    p->body->getWorldTransform().getOrigin().z());
        CHECK(glm::distance(bp, glm::vec3(100, 3, 10)) < 0.5f);
        printf("  local body after reconcile = (%.1f,%.1f,%.1f)\n", bp.x, bp.y, bp.z);
    }

    // 7) Leave removes the avatar; Despawn removes the spawned proxy.
    mock.inject(PlayerLeave{OTHER});
    mock.inject(DespawnEntity{SPAWN});
    nc.update(DT);
    CHECK(nc.replicatedCount() == 1); // only the scene cube remains
    CHECK(!remoteAvatarPos(scene, rpos));

    if (failures == 0) printf("ALL PASS\n");
    else printf("%d FAILURES\n", failures);
    return failures ? 1 : 0;
}
