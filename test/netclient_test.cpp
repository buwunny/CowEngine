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

int main()
{
    PhysicsWorld physics;
    Scene scene;
    Camera cam(glm::vec3(0, 3, 10), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

    // A dynamic scene body (like Cube 2/3): NetClient should claim it — remove it
    // from the physics world and follow the server. Its netId == Identity.id.
    ecs::Entity sceneCube = scene.spawnCube(1, glm::translate(glm::mat4(1.0f), glm::vec3(5, 1, 5)),
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
