#include "script/ScriptHost.hpp"

#include "core/Scene.hpp"
#include "platform/Window.hpp"
#include "core/Camera.hpp"
#include "core/PhysicsWorld.hpp"
#include "ecs/Components.hpp"
#include "ecs/Factories.hpp"
#include "meshes/AssetManager.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>

#include <GLFW/glfw3.h>

using cowscript::Value;

namespace
{
    int resolveGlfwKey(const std::string &name)
    {
        std::string n;
        n.reserve(name.size());
        for (char c : name)
            n.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

        static const std::unordered_map<std::string, int> map = {
            {"a", GLFW_KEY_A}, {"b", GLFW_KEY_B}, {"c", GLFW_KEY_C}, {"d", GLFW_KEY_D},
            {"e", GLFW_KEY_E}, {"f", GLFW_KEY_F}, {"g", GLFW_KEY_G}, {"h", GLFW_KEY_H},
            {"i", GLFW_KEY_I}, {"j", GLFW_KEY_J}, {"k", GLFW_KEY_K}, {"l", GLFW_KEY_L},
            {"m", GLFW_KEY_M}, {"n", GLFW_KEY_N}, {"o", GLFW_KEY_O}, {"p", GLFW_KEY_P},
            {"q", GLFW_KEY_Q}, {"r", GLFW_KEY_R}, {"s", GLFW_KEY_S}, {"t", GLFW_KEY_T},
            {"u", GLFW_KEY_U}, {"v", GLFW_KEY_V}, {"w", GLFW_KEY_W}, {"x", GLFW_KEY_X},
            {"y", GLFW_KEY_Y}, {"z", GLFW_KEY_Z},
            {"space", GLFW_KEY_SPACE}, {"enter", GLFW_KEY_ENTER},
            {"shift", GLFW_KEY_LEFT_SHIFT}, {"ctrl", GLFW_KEY_LEFT_CONTROL},
            {"alt", GLFW_KEY_LEFT_ALT}, {"up", GLFW_KEY_UP}, {"down", GLFW_KEY_DOWN},
            {"left", GLFW_KEY_LEFT}, {"right", GLFW_KEY_RIGHT}, {"escape", GLFW_KEY_ESCAPE},
        };
        auto it = map.find(n);
        return it == map.end() ? -1 : it->second;
    }

    // Entity handle ↔ Value::handle packing. Reuses the same scheme as the
    // Bullet user-pointer so the handle is portable between systems.
    Value entityHandle(const std::string &kind, ecs::Entity e)
    {
        if (e == ecs::NullEntity)
            return Value::makeNull();
        return Value::makeHandle(kind, ecs::toUserPointer(e));
    }

    ecs::Entity entityFromHandle(const Value &v)
    {
        if (v.type != Value::Handle)
            return ecs::NullEntity;
        return ecs::fromUserPointer(v.handle);
    }
}

ScriptHost::ScriptHost() = default;

void ScriptHost::log(const std::string &line)
{
    if (logger)
        logger(line);
}

void ScriptHost::bindBuiltins(cowscript::Script &script)
{
    script.setBuiltin("print", [this](const std::vector<Value> &a) { return builtinPrint(a); });
    script.setBuiltin("time", [this](const std::vector<Value> &a) { return builtinTime(a); });
    script.setBuiltin("dt", [this](const std::vector<Value> &a) { return builtinDt(a); });
    script.setBuiltin("key", [this](const std::vector<Value> &a) { return builtinKey(a); });

    script.setBuiltin("sin", [](const std::vector<Value> &a) { return Value::makeNumber(std::sin(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("cos", [](const std::vector<Value> &a) { return Value::makeNumber(std::cos(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("tan", [](const std::vector<Value> &a) { return Value::makeNumber(std::tan(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("sqrt", [](const std::vector<Value> &a) { return Value::makeNumber(std::sqrt(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("abs", [](const std::vector<Value> &a) { return Value::makeNumber(std::fabs(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("floor", [](const std::vector<Value> &a) { return Value::makeNumber(std::floor(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("ceil", [](const std::vector<Value> &a) { return Value::makeNumber(std::ceil(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("random", [](const std::vector<Value> &) { return Value::makeNumber(static_cast<double>(rand()) / static_cast<double>(RAND_MAX)); });

    script.setBuiltin("self_x", [this](const std::vector<Value> &) { return builtinSelfPos(0); });
    script.setBuiltin("self_y", [this](const std::vector<Value> &) { return builtinSelfPos(1); });
    script.setBuiltin("self_z", [this](const std::vector<Value> &) { return builtinSelfPos(2); });
    script.setBuiltin("self_rx", [this](const std::vector<Value> &) { return builtinSelfRot(0); });
    script.setBuiltin("self_ry", [this](const std::vector<Value> &) { return builtinSelfRot(1); });
    script.setBuiltin("self_rz", [this](const std::vector<Value> &) { return builtinSelfRot(2); });
    script.setBuiltin("self_sx", [this](const std::vector<Value> &) { return builtinSelfScale(0); });
    script.setBuiltin("self_sy", [this](const std::vector<Value> &) { return builtinSelfScale(1); });
    script.setBuiltin("self_sz", [this](const std::vector<Value> &) { return builtinSelfScale(2); });
    script.setBuiltin("self_set_pos", [this](const std::vector<Value> &a) { return builtinSelfSetPos(a); });
    script.setBuiltin("self_set_rot", [this](const std::vector<Value> &a) { return builtinSelfSetRot(a); });
    script.setBuiltin("self_set_scale", [this](const std::vector<Value> &a) { return builtinSelfSetScaleFn(a); });
    script.setBuiltin("self_set_color", [this](const std::vector<Value> &a) { return builtinSelfSetColor(a); });
    script.setBuiltin("self_apply_impulse", [this](const std::vector<Value> &a) { return builtinSelfApplyImpulse(a); });
    script.setBuiltin("self_apply_force", [this](const std::vector<Value> &a) { return builtinSelfApplyForce(a); });
    script.setBuiltin("self_set_velocity", [this](const std::vector<Value> &a) { return builtinSelfSetVelocity(a); });
    script.setBuiltin("self_on_ground", [this](const std::vector<Value> &a) { return builtinSelfOnGround(a); });

    script.setBuiltin("spawn_cube", [this](const std::vector<Value> &a) { return builtinSpawn(a, "cube"); });
    script.setBuiltin("spawn_cow", [this](const std::vector<Value> &a) { return builtinSpawn(a, "cow"); });
    script.setBuiltin("spawn_plane", [this](const std::vector<Value> &a) { return builtinSpawn(a, "plane"); });

    script.setBuiltin("self", [this](const std::vector<Value> &a) { return builtinSelfHandle(a); });
    script.setBuiltin("transform", [this](const std::vector<Value> &a) { return builtinTransform(a); });
    script.setBuiltin("rigidbody", [this](const std::vector<Value> &a) { return builtinRigidbody(a); });
    script.setBuiltin("transform_of", [this](const std::vector<Value> &a) { return builtinTransform(a); });
    script.setBuiltin("rigidbody_of", [this](const std::vector<Value> &a) { return builtinRigidbody(a); });
    script.setBuiltin("camera", [this](const std::vector<Value> &a) { return builtinCamera(a); });

    script.setPropertyGetter([this](const Value &t, const std::string &p) { return getProperty(t, p); });
    script.setPropertySetter([this](const Value &t, const std::string &p, const Value &v) { setProperty(t, p, v); });
}

Value ScriptHost::builtinPrint(const std::vector<Value> &args)
{
    std::string line;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i) line.push_back(' ');
        line += args[i].toString();
    }
    log(line);
    return Value::makeNull();
}

Value ScriptHost::builtinTime(const std::vector<Value> &) { return Value::makeNumber(timeSeconds); }
Value ScriptHost::builtinDt(const std::vector<Value> &) { return Value::makeNumber(lastDelta); }

Value ScriptHost::builtinKey(const std::vector<Value> &args)
{
    if (args.empty() || !windowRef)
        return Value::makeBool(false);
    int code = resolveGlfwKey(args[0].toString());
    if (code < 0)
        return Value::makeBool(false);
    return Value::makeBool(windowRef->isKeyPressed(code));
}

namespace
{
    ecs::Transform *getSelfTransform(Scene *scene, ecs::Entity self)
    {
        if (!scene || self == ecs::NullEntity)
            return nullptr;
        return scene->registry().try_get<ecs::Transform>(self);
    }
}

Value ScriptHost::builtinSelfPos(int axis)
{
    auto *t = getSelfTransform(sceneRef, selfEntity);
    if (!t) return Value::makeNumber(0.0);
    double v = axis == 0 ? t->position.x : axis == 1 ? t->position.y : t->position.z;
    return Value::makeNumber(v);
}
Value ScriptHost::builtinSelfRot(int axis)
{
    auto *t = getSelfTransform(sceneRef, selfEntity);
    if (!t) return Value::makeNumber(0.0);
    double v = axis == 0 ? t->rotation.x : axis == 1 ? t->rotation.y : t->rotation.z;
    return Value::makeNumber(v);
}
Value ScriptHost::builtinSelfScale(int axis)
{
    auto *t = getSelfTransform(sceneRef, selfEntity);
    if (!t) return Value::makeNumber(0.0);
    double v = axis == 0 ? t->scale.x : axis == 1 ? t->scale.y : t->scale.z;
    return Value::makeNumber(v);
}

static glm::vec3 vec3FromArgs(const std::vector<Value> &args, glm::vec3 def)
{
    glm::vec3 v = def;
    if (args.size() > 0) v.x = static_cast<float>(args[0].toNumber());
    if (args.size() > 1) v.y = static_cast<float>(args[1].toNumber());
    if (args.size() > 2) v.z = static_cast<float>(args[2].toNumber());
    return v;
}

Value ScriptHost::builtinSelfSetPos(const std::vector<Value> &args)
{
    auto *t = getSelfTransform(sceneRef, selfEntity);
    if (!t) return Value::makeNull();
    glm::vec3 np = vec3FromArgs(args, glm::vec3(t->position));
    ecs::applyTransform(sceneRef->registry(), selfEntity, np, glm::vec3(t->rotation), glm::vec3(t->scale));
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetRot(const std::vector<Value> &args)
{
    auto *t = getSelfTransform(sceneRef, selfEntity);
    if (!t) return Value::makeNull();
    glm::vec3 nr = vec3FromArgs(args, glm::vec3(t->rotation));
    ecs::applyTransform(sceneRef->registry(), selfEntity, glm::vec3(t->position), nr, glm::vec3(t->scale));
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetScaleFn(const std::vector<Value> &args)
{
    auto *t = getSelfTransform(sceneRef, selfEntity);
    if (!t) return Value::makeNull();
    glm::vec3 ns = vec3FromArgs(args, glm::vec3(t->scale));
    ecs::applyTransform(sceneRef->registry(), selfEntity, glm::vec3(t->position), glm::vec3(t->rotation), ns);
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetColor(const std::vector<Value> &args)
{
    if (!sceneRef || selfEntity == ecs::NullEntity) return Value::makeNull();
    auto *rd = sceneRef->registry().try_get<ecs::Renderable>(selfEntity);
    if (!rd) return Value::makeNull();
    glm::vec4 c = rd->color;
    if (args.size() > 0) c.r = static_cast<float>(args[0].toNumber());
    if (args.size() > 1) c.g = static_cast<float>(args[1].toNumber());
    if (args.size() > 2) c.b = static_cast<float>(args[2].toNumber());
    if (args.size() > 3) c.a = static_cast<float>(args[3].toNumber());
    rd->color = c;
    return Value::makeNull();
}

Value ScriptHost::builtinSelfApplyImpulse(const std::vector<Value> &args)
{
    if (!sceneRef || selfEntity == ecs::NullEntity) return Value::makeNull();
    auto *p = sceneRef->registry().try_get<ecs::Physics>(selfEntity);
    if (!p || !p->body) return Value::makeNull();
    glm::vec3 v = vec3FromArgs(args, glm::vec3(0.0f));
    p->body->activate(true);
    p->body->applyCentralImpulse(btVector3(v.x, v.y, v.z));
    return Value::makeNull();
}

Value ScriptHost::builtinSelfApplyForce(const std::vector<Value> &args)
{
    if (!sceneRef || selfEntity == ecs::NullEntity) return Value::makeNull();
    auto *p = sceneRef->registry().try_get<ecs::Physics>(selfEntity);
    if (!p || !p->body) return Value::makeNull();
    glm::vec3 v = vec3FromArgs(args, glm::vec3(0.0f));
    p->body->activate(true);
    p->body->applyCentralForce(btVector3(v.x, v.y, v.z));
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetVelocity(const std::vector<Value> &args)
{
    if (!sceneRef || selfEntity == ecs::NullEntity) return Value::makeNull();
    auto *p = sceneRef->registry().try_get<ecs::Physics>(selfEntity);
    if (!p || !p->body) return Value::makeNull();
    glm::vec3 v = vec3FromArgs(args, glm::vec3(0.0f));
    p->body->activate(true);
    p->body->setLinearVelocity(btVector3(v.x, v.y, v.z));
    return Value::makeNull();
}

// Raycasts a short distance straight down from the body's centre so a
// controller script can tell whether it may jump. Mirrors the ground check
// the native player input system used before movement moved into script land.
Value ScriptHost::builtinSelfOnGround(const std::vector<Value> &)
{
    if (!sceneRef || selfEntity == ecs::NullEntity) return Value::makeBool(false);
    auto *p = sceneRef->registry().try_get<ecs::Physics>(selfEntity);
    if (!p || !p->body) return Value::makeBool(false);
    PhysicsWorld *phys = sceneRef->physicsWorld();
    if (!phys) return Value::makeBool(false);
    btVector3 start = p->body->getWorldTransform().getOrigin();
    btVector3 end = start - btVector3(0.0f, 1.05f, 0.0f);
    btCollisionWorld::ClosestRayResultCallback cb(start, end);
    phys->rayTest(start, end, cb);
    return Value::makeBool(cb.hasHit());
}

Value ScriptHost::builtinSpawn(const std::vector<Value> &args, const std::string &kind)
{
    if (!sceneRef) return Value::makeNull();
    glm::vec3 pos = vec3FromArgs(args, glm::vec3(0.0f, 5.0f, 0.0f));
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    float r = static_cast<float>(rand()) / RAND_MAX;
    float g = static_cast<float>(rand()) / RAND_MAX;
    float b = static_cast<float>(rand()) / RAND_MAX;
    glm::vec4 color(r, g, b, 1.0f);

    ecs::Entity e = ecs::NullEntity;
    if (kind == "cube")
        e = sceneRef->spawnCube(1, model, color, 1.0f);
    else if (kind == "plane")
        e = sceneRef->spawnPlane(10.0f, 10.0f, model, color, 0.0f);
    else if (kind == "cow")
        e = sceneRef->spawnStaticFromAsset("models/cow.obj", "cow", model, color, 1.0f);

    return entityHandle("object", e);
}

Value ScriptHost::builtinSelfHandle(const std::vector<Value> &)
{
    return entityHandle("object", selfEntity);
}

namespace
{
    ecs::Entity entityFromArgs(ecs::Entity fallback, const std::vector<Value> &args)
    {
        if (args.empty())
            return fallback;
        const Value &v = args[0];
        if (v.type == Value::Handle && (v.str == "object" || v.str == "transform" || v.str == "rigidbody"))
            return ecs::fromUserPointer(v.handle);
        return fallback;
    }
}

Value ScriptHost::builtinTransform(const std::vector<Value> &args)
{
    ecs::Entity e = entityFromArgs(selfEntity, args);
    if (!sceneRef || e == ecs::NullEntity || !sceneRef->registry().all_of<ecs::Transform>(e))
        return Value::makeNull();
    return entityHandle("transform", e);
}

Value ScriptHost::builtinRigidbody(const std::vector<Value> &args)
{
    ecs::Entity e = entityFromArgs(selfEntity, args);
    if (!sceneRef || e == ecs::NullEntity)
        return Value::makeNull();
    auto *p = sceneRef->registry().try_get<ecs::Physics>(e);
    if (!p || !p->body)
        return Value::makeNull();
    return entityHandle("rigidbody", e);
}

Value ScriptHost::builtinCamera(const std::vector<Value> &)
{
    if (!sceneRef || !sceneRef->hasPlayer())
        return Value::makeNull();
    auto *pc = sceneRef->registry().try_get<ecs::PlayerController>(sceneRef->getPlayerEntity());
    if (!pc || !pc->camera)
        return Value::makeNull();
    return Value::makeHandle("camera", pc->camera);
}

namespace
{
    Value transformGet(Scene *scene, ecs::Entity e, const std::string &prop)
    {
        auto *t = scene->registry().try_get<ecs::Transform>(e);
        if (!t) throw std::runtime_error("transform missing");
        if (prop == "x") return Value::makeNumber(t->position.x);
        if (prop == "y") return Value::makeNumber(t->position.y);
        if (prop == "z") return Value::makeNumber(t->position.z);
        if (prop == "rx") return Value::makeNumber(t->rotation.x);
        if (prop == "ry") return Value::makeNumber(t->rotation.y);
        if (prop == "rz") return Value::makeNumber(t->rotation.z);
        if (prop == "sx") return Value::makeNumber(t->scale.x);
        if (prop == "sy") return Value::makeNumber(t->scale.y);
        if (prop == "sz") return Value::makeNumber(t->scale.z);
        throw std::runtime_error("transform has no property '" + prop + "'");
    }

    void transformSet(Scene *scene, ecs::Entity e, const std::string &prop, const Value &v)
    {
        auto *t = scene->registry().try_get<ecs::Transform>(e);
        if (!t) throw std::runtime_error("transform missing");
        glm::vec3 p(t->position), r(t->rotation), s(t->scale);
        float f = static_cast<float>(v.toNumber());
        if (prop == "x") p.x = f;
        else if (prop == "y") p.y = f;
        else if (prop == "z") p.z = f;
        else if (prop == "rx") r.x = f;
        else if (prop == "ry") r.y = f;
        else if (prop == "rz") r.z = f;
        else if (prop == "sx") s.x = f;
        else if (prop == "sy") s.y = f;
        else if (prop == "sz") s.z = f;
        else throw std::runtime_error("transform has no settable property '" + prop + "'");
        ecs::applyTransform(scene->registry(), e, p, r, s);
    }

    Value rigidbodyGet(Scene *scene, ecs::Entity e, const std::string &prop)
    {
        auto *p = scene->registry().try_get<ecs::Physics>(e);
        if (!p || !p->body) throw std::runtime_error("object has no rigidbody");
        const btVector3 &lv = p->body->getLinearVelocity();
        if (prop == "vx") return Value::makeNumber(lv.x());
        if (prop == "vy") return Value::makeNumber(lv.y());
        if (prop == "vz") return Value::makeNumber(lv.z());
        // Live body position, post-physics-step (the Transform component is only
        // synced later in the frame, so this is fresher for camera-follow).
        const btVector3 &origin = p->body->getWorldTransform().getOrigin();
        if (prop == "px") return Value::makeNumber(origin.x());
        if (prop == "py") return Value::makeNumber(origin.y());
        if (prop == "pz") return Value::makeNumber(origin.z());
        if (prop == "mass") return Value::makeNumber(p->mass);
        throw std::runtime_error("rigidbody has no property '" + prop + "'");
    }

    void rigidbodySet(Scene *scene, ecs::Entity e, const std::string &prop, const Value &v)
    {
        auto *p = scene->registry().try_get<ecs::Physics>(e);
        if (!p || !p->body) throw std::runtime_error("object has no rigidbody");
        float f = static_cast<float>(v.toNumber());
        btVector3 lv = p->body->getLinearVelocity();
        if (prop == "vx") lv.setX(f);
        else if (prop == "vy") lv.setY(f);
        else if (prop == "vz") lv.setZ(f);
        else throw std::runtime_error("rigidbody has no settable property '" + prop + "'");
        p->body->activate(true);
        p->body->setLinearVelocity(lv);
    }

    Value cameraGet(Camera *c, const std::string &prop)
    {
        glm::vec3 p = c->getPosition();
        glm::vec3 f = c->getFront();
        glm::vec3 u = c->getUp();
        glm::vec3 rt = c->getRight();
        if (prop == "x") return Value::makeNumber(p.x);
        if (prop == "y") return Value::makeNumber(p.y);
        if (prop == "z") return Value::makeNumber(p.z);
        if (prop == "fx") return Value::makeNumber(f.x);
        if (prop == "fy") return Value::makeNumber(f.y);
        if (prop == "fz") return Value::makeNumber(f.z);
        if (prop == "ux") return Value::makeNumber(u.x);
        if (prop == "uy") return Value::makeNumber(u.y);
        if (prop == "uz") return Value::makeNumber(u.z);
        if (prop == "rx") return Value::makeNumber(rt.x);
        if (prop == "ry") return Value::makeNumber(rt.y);
        if (prop == "rz") return Value::makeNumber(rt.z);
        if (prop == "yaw") return Value::makeNumber(c->getYaw());
        if (prop == "pitch") return Value::makeNumber(c->getPitch());
        throw std::runtime_error("camera has no property '" + prop + "'");
    }

    void cameraSet(Camera *c, const std::string &prop, const Value &v)
    {
        glm::vec3 p = c->getPosition();
        float f = static_cast<float>(v.toNumber());
        if (prop == "x") p.x = f;
        else if (prop == "y") p.y = f;
        else if (prop == "z") p.z = f;
        else throw std::runtime_error("camera has no settable property '" + prop + "'");
        c->setPosition(p);
    }
}

Value ScriptHost::getProperty(const Value &target, const std::string &prop)
{
    if (target.type != Value::Handle)
        throw std::runtime_error("cannot read '." + prop + "' on a non-handle value");
    const std::string &kind = target.str;
    if (kind == "camera")
        return cameraGet(static_cast<Camera *>(target.handle), prop);

    ecs::Entity e = ecs::fromUserPointer(target.handle);
    if (!sceneRef || e == ecs::NullEntity)
        throw std::runtime_error("dangling handle");

    if (kind == "transform")
        return transformGet(sceneRef, e, prop);
    if (kind == "rigidbody")
        return rigidbodyGet(sceneRef, e, prop);
    if (kind == "object")
    {
        if (prop == "transform")
            return entityHandle("transform", e);
        if (prop == "rigidbody")
        {
            auto *p = sceneRef->registry().try_get<ecs::Physics>(e);
            if (!p || !p->body)
                throw std::runtime_error("object has no rigidbody");
            return entityHandle("rigidbody", e);
        }
        if (prop == "name")
        {
            auto *id = sceneRef->registry().try_get<ecs::Identity>(e);
            return Value::makeString(id ? id->name : std::string());
        }
        return transformGet(sceneRef, e, prop);
    }
    throw std::runtime_error("unknown handle kind '" + kind + "'");
}

void ScriptHost::setProperty(const Value &target, const std::string &prop, const Value &value)
{
    if (target.type != Value::Handle)
        throw std::runtime_error("cannot write '." + prop + "' on a non-handle value");
    const std::string &kind = target.str;
    if (kind == "camera")
    {
        cameraSet(static_cast<Camera *>(target.handle), prop, value);
        return;
    }

    ecs::Entity e = ecs::fromUserPointer(target.handle);
    if (!sceneRef || e == ecs::NullEntity)
        throw std::runtime_error("dangling handle");

    if (kind == "transform")
    {
        transformSet(sceneRef, e, prop, value);
        return;
    }
    if (kind == "rigidbody")
    {
        rigidbodySet(sceneRef, e, prop, value);
        return;
    }
    if (kind == "object")
    {
        transformSet(sceneRef, e, prop, value);
        return;
    }
    throw std::runtime_error("unknown handle kind '" + kind + "'");
}
