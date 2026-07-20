#ifndef ECS_COMPONENTS_HPP
#define ECS_COMPONENTS_HPP

#include "meshes/Mesh.hpp"

#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>

#include <memory>
#include <string>
#include <vector>

class Camera;

namespace ecs
{
    // Position in world space, Euler degrees (Rz*Ry*Rx convention — matches
    // ImGuizmo), and per-axis local scale. Cached model + modelNoScale are
    // rebuilt by Transform::setTRS() and by PhysicsSyncSystem each frame.
    struct Transform
    {
        glm::dvec3 position{0.0};
        glm::dvec3 rotation{0.0}; // degrees
        glm::dvec3 scale{1.0};
        glm::vec3 localScale{1.0f};
        glm::mat4 model{1.0f};
        glm::mat4 modelNoScale{1.0f};
    };

    struct Renderable
    {
        std::shared_ptr<Mesh> mesh;
        glm::vec4 color{1.0f};
        double lineWidth = 1.0;
        bool wireframe = false;
    };

    // Bullet-owned resources for a single body. Ownership is RAII: removing
    // the component (or destroying the entity) destroys the rigid body.
    // Removal from the dynamics world must happen *before* destruction —
    // Scene::destroyEntity handles that.
    struct Physics
    {
        std::unique_ptr<btRigidBody> body;
        std::unique_ptr<btCollisionShape> shape;
        std::unique_ptr<btMotionState> motion;
        double mass = 0.0;
    };

    struct Identity
    {
        int id = 0;
        std::string name;
        // Zero or more .cow scripts driving this entity. Every script's
        // `on start`/`on update` runs, in order, sharing the same `self`.
        std::vector<std::string> scriptPaths;
        std::string meshPath;
    };

    // Stable type marker for serialization + editor display. The marker
    // also captures the per-shape parameters we need to round-trip
    // (length/width for planes, size for cubes).
    enum class ShapeKind
    {
        Cube,
        Plane,
        Static,
        Player,
    };

    struct ShapeMarker
    {
        ShapeKind kind = ShapeKind::Static;
        int cubeSize = 0;       // Cube only
        float planeLength = 0.f; // Plane only
        float planeWidth = 0.f;  // Plane only
    };

    namespace cowscript_fwd { class Script; }
}

// Forward-declare the script type without pulling the header into every TU
// that includes Components.hpp.
namespace cowscript { class Script; }

namespace ecs
{
    // One compiled script plus the path it came from (kept for diagnostics).
    struct ScriptInstance
    {
        std::string path;
        std::shared_ptr<cowscript::Script> script;
    };

    // Holds every compiled script attached to an entity, parallel to (but
    // possibly shorter than, if some failed to compile) Identity::scriptPaths.
    struct ScriptComponent
    {
        std::vector<ScriptInstance> scripts;
    };

    // Drives WSAD/jump/mouse-look on the entity that has one. The body's
    // collision shape is a btCapsuleShape; rotation is locked in factory.
    struct PlayerController
    {
        Camera *camera = nullptr;
        float movementSpeed = 100.0f;
        float mass = 10.0f;
        float lastX = 0.f;
        float lastY = 0.f;
        bool firstMouse = true;
        bool lastTabPressed = false;
        float pendingMouseDx = 0.f;
        float pendingMouseDy = 0.f;
    };

    // Tags. Stored as empty components so views can filter on them.
    struct Selected
    {
    };
    struct Hovered
    {
    };
    struct PlayerTag
    {
    };
}

#endif // ECS_COMPONENTS_HPP
