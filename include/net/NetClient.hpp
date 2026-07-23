#ifndef NET_NET_CLIENT_HPP
#define NET_NET_CLIENT_HPP

#include "ecs/Entity.hpp"
#include "net/ITransport.hpp"
#include "net/Protocol.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

class Scene;

// Client-side netcode. Sits on top of an ITransport and a Scene and drives the
// multiplayer view:
//   * sends ClientHello once connected, learns its own netId from ServerWelcome;
//   * sends one InputCommand per fixed tick (from the local player's PlayerInput);
//   * the local player keeps running the normal client-side prediction and is
//     only corrected when it drifts far from the server (snap on large error);
//   * EVERY other replicated body — remote players, dynamic scene objects, and
//     server-spawned objects — is driven by the server. On construction the
//     client stops locally simulating dynamic scene bodies (removes them from
//     physics + drops their scripts) and instead interpolates them ~interpDelay
//     behind real time from snapshots. Remote players and spawned objects get a
//     render-only proxy created on demand.
//
// Transport-agnostic (WebTransport, WebSocket, or a mock in tests); no rendering.
namespace net
{
    class NetClient
    {
    public:
        // `playerName` is what this client asks to be called; the server
        // sanitises it and hands it back out to the other players. Empty means
        // "server, pick one for me".
        NetClient(ITransport *transport, Scene *scene, ecs::Entity localPlayer,
                  std::string playerName = {});
        ~NetClient();

        // Call once per fixed sim tick, after the local prediction step.
        void update(float dt);

        bool joined() const { return joined_; }
        uint32_t myNetId() const { return myNetId_; }
        // Total replicated bodies the client is following (scene objects +
        // spawned objects + remote player avatars).
        size_t replicatedCount() const { return reps_.size(); }

        void setInterpDelay(double sec) { interpDelay_ = sec; }
        void setSnapThreshold(float dist) { snapThreshold_ = dist; }
        void setHardSnapThreshold(float dist) { hardSnapThreshold_ = dist; }

    private:
        struct Sample
        {
            double t;
            glm::vec3 pos;
            glm::quat rot;
        };
        struct Rep
        {
            ecs::Entity entity = ecs::NullEntity;
            glm::vec3 scale{1.0f};
            bool ownsEntity = false;    // true for proxies/avatars we created (destroy on removal)
            bool placed = true;         // has been positioned by a snapshot at least once
            bool wantsCollider = false; // attach a kinematic collider on first placement
            std::deque<Sample> buf;
        };

        void claimSceneObjects();
        void sendHelloIfNeeded();
        void processIncoming();
        void sendInput();
        void updateReplicated();
        void applyLocalReconcile(const EntityState &s);
        Rep *ensurePlayerAvatar(uint32_t netId);
        // Put `name` on the avatar's floating label. Called from PlayerJoin,
        // which may arrive after a snapshot has already created the avatar.
        void setAvatarName(uint32_t netId, const std::string &name);
        void onSpawn(const SpawnEntity &s);
        void removeRep(uint32_t netId);

        ITransport *transport_;
        Scene *scene_;
        ecs::Entity localPlayer_;
        std::string playerName_;

        bool sentHello_ = false;
        bool joined_ = false;
        uint32_t myNetId_ = 0;
        uint32_t inputSeq_ = 0;

        double clientTime_ = 0.0;
        double interpDelay_ = 0.1;
        float snapThreshold_ = 1.5f;      // below this error: trust prediction, no correction
        float hardSnapThreshold_ = 5.0f;  // above this: teleport (respawn / big desync)
        float correctionRate_ = 0.25f;    // fraction of the error nudged out per snapshot
        float lastDt_ = 1.0f / 60.0f;

        std::unordered_map<uint32_t, Rep> reps_; // keyed by netId
    };
}

#endif // NET_NET_CLIENT_HPP
