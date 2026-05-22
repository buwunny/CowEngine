#include "script/ScriptHost.hpp"

#include "Scene.hpp"
#include "Window.hpp"
#include "Camera.hpp"
#include "objects/Object.hpp"
#include "objects/Cube.hpp"
#include "objects/Plane.hpp"
#include "objects/Player.hpp"
#include "objects/StaticObject.hpp"
#include "meshes/AssetManager.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
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
            {"a", GLFW_KEY_A},
            {"b", GLFW_KEY_B},
            {"c", GLFW_KEY_C},
            {"d", GLFW_KEY_D},
            {"e", GLFW_KEY_E},
            {"f", GLFW_KEY_F},
            {"g", GLFW_KEY_G},
            {"h", GLFW_KEY_H},
            {"i", GLFW_KEY_I},
            {"j", GLFW_KEY_J},
            {"k", GLFW_KEY_K},
            {"l", GLFW_KEY_L},
            {"m", GLFW_KEY_M},
            {"n", GLFW_KEY_N},
            {"o", GLFW_KEY_O},
            {"p", GLFW_KEY_P},
            {"q", GLFW_KEY_Q},
            {"r", GLFW_KEY_R},
            {"s", GLFW_KEY_S},
            {"t", GLFW_KEY_T},
            {"u", GLFW_KEY_U},
            {"v", GLFW_KEY_V},
            {"w", GLFW_KEY_W},
            {"x", GLFW_KEY_X},
            {"y", GLFW_KEY_Y},
            {"z", GLFW_KEY_Z},
            {"space", GLFW_KEY_SPACE},
            {"enter", GLFW_KEY_ENTER},
            {"shift", GLFW_KEY_LEFT_SHIFT},
            {"ctrl", GLFW_KEY_LEFT_CONTROL},
            {"alt", GLFW_KEY_LEFT_ALT},
            {"up", GLFW_KEY_UP},
            {"down", GLFW_KEY_DOWN},
            {"left", GLFW_KEY_LEFT},
            {"right", GLFW_KEY_RIGHT},
            {"escape", GLFW_KEY_ESCAPE},
        };
        auto it = map.find(n);
        return it == map.end() ? -1 : it->second;
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
    script.setBuiltin("print", [this](const std::vector<Value> &a)
                      { return builtinPrint(a); });
    script.setBuiltin("time", [this](const std::vector<Value> &a)
                      { return builtinTime(a); });
    script.setBuiltin("dt", [this](const std::vector<Value> &a)
                      { return builtinDt(a); });
    script.setBuiltin("key", [this](const std::vector<Value> &a)
                      { return builtinKey(a); });

    script.setBuiltin("sin", [](const std::vector<Value> &a)
                      { return Value::makeNumber(std::sin(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("cos", [](const std::vector<Value> &a)
                      { return Value::makeNumber(std::cos(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("tan", [](const std::vector<Value> &a)
                      { return Value::makeNumber(std::tan(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("sqrt", [](const std::vector<Value> &a)
                      { return Value::makeNumber(std::sqrt(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("abs", [](const std::vector<Value> &a)
                      { return Value::makeNumber(std::fabs(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("floor", [](const std::vector<Value> &a)
                      { return Value::makeNumber(std::floor(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("ceil", [](const std::vector<Value> &a)
                      { return Value::makeNumber(std::ceil(a.empty() ? 0.0 : a[0].toNumber())); });
    script.setBuiltin("random", [](const std::vector<Value> &)
                      { return Value::makeNumber(static_cast<double>(rand()) / static_cast<double>(RAND_MAX)); });

    script.setBuiltin("self_x", [this](const std::vector<Value> &)
                      { return builtinSelfPos(0); });
    script.setBuiltin("self_y", [this](const std::vector<Value> &)
                      { return builtinSelfPos(1); });
    script.setBuiltin("self_z", [this](const std::vector<Value> &)
                      { return builtinSelfPos(2); });
    script.setBuiltin("self_rx", [this](const std::vector<Value> &)
                      { return builtinSelfRot(0); });
    script.setBuiltin("self_ry", [this](const std::vector<Value> &)
                      { return builtinSelfRot(1); });
    script.setBuiltin("self_rz", [this](const std::vector<Value> &)
                      { return builtinSelfRot(2); });
    script.setBuiltin("self_sx", [this](const std::vector<Value> &)
                      { return builtinSelfScale(0); });
    script.setBuiltin("self_sy", [this](const std::vector<Value> &)
                      { return builtinSelfScale(1); });
    script.setBuiltin("self_sz", [this](const std::vector<Value> &)
                      { return builtinSelfScale(2); });
    script.setBuiltin("self_set_pos", [this](const std::vector<Value> &a)
                      { return builtinSelfSetPos(a); });
    script.setBuiltin("self_set_rot", [this](const std::vector<Value> &a)
                      { return builtinSelfSetRot(a); });
    script.setBuiltin("self_set_scale", [this](const std::vector<Value> &a)
                      { return builtinSelfSetScaleFn(a); });
    script.setBuiltin("self_set_color", [this](const std::vector<Value> &a)
                      { return builtinSelfSetColor(a); });
    script.setBuiltin("self_apply_impulse", [this](const std::vector<Value> &a)
                      { return builtinSelfApplyImpulse(a); });
    script.setBuiltin("self_apply_force", [this](const std::vector<Value> &a)
                      { return builtinSelfApplyForce(a); });
    script.setBuiltin("self_set_velocity", [this](const std::vector<Value> &a)
                      { return builtinSelfSetVelocity(a); });

    script.setBuiltin("spawn_cube", [this](const std::vector<Value> &a)
                      { return builtinSpawn(a, "cube"); });
    script.setBuiltin("spawn_cow", [this](const std::vector<Value> &a)
                      { return builtinSpawn(a, "cow"); });
    script.setBuiltin("spawn_plane", [this](const std::vector<Value> &a)
                      { return builtinSpawn(a, "plane"); });

    // Component handles. With no args they target `self`; with an Object handle
    // they target that object. `camera()` returns the active player camera.
    script.setBuiltin("self", [this](const std::vector<Value> &a)
                      { return builtinSelfHandle(a); });
    script.setBuiltin("transform", [this](const std::vector<Value> &a)
                      { return builtinTransform(a); });
    script.setBuiltin("rigidbody", [this](const std::vector<Value> &a)
                      { return builtinRigidbody(a); });
    script.setBuiltin("transform_of", [this](const std::vector<Value> &a)
                      { return builtinTransform(a); });
    script.setBuiltin("rigidbody_of", [this](const std::vector<Value> &a)
                      { return builtinRigidbody(a); });
    script.setBuiltin("camera", [this](const std::vector<Value> &a)
                      { return builtinCamera(a); });

    script.setPropertyGetter([this](const Value &t, const std::string &p)
                             { return getProperty(t, p); });
    script.setPropertySetter([this](const Value &t, const std::string &p, const Value &v)
                             { setProperty(t, p, v); });
}

Value ScriptHost::builtinPrint(const std::vector<Value> &args)
{
    std::string line;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i)
            line.push_back(' ');
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

Value ScriptHost::builtinSelfPos(int axis)
{
    if (!selfObj)
        return Value::makeNumber(0.0);
    glm::vec3 p, r, s;
    selfObj->getTransform(p, r, s);
    return Value::makeNumber(axis == 0 ? p.x : axis == 1 ? p.y
                                                         : p.z);
}
Value ScriptHost::builtinSelfRot(int axis)
{
    if (!selfObj)
        return Value::makeNumber(0.0);
    glm::vec3 p, r, s;
    selfObj->getTransform(p, r, s);
    return Value::makeNumber(axis == 0 ? r.x : axis == 1 ? r.y
                                                         : r.z);
}
Value ScriptHost::builtinSelfScale(int axis)
{
    if (!selfObj)
        return Value::makeNumber(0.0);
    glm::vec3 p, r, s;
    selfObj->getTransform(p, r, s);
    return Value::makeNumber(axis == 0 ? s.x : axis == 1 ? s.y
                                                         : s.z);
}

static glm::vec3 vec3FromArgs(const std::vector<Value> &args, glm::vec3 def)
{
    glm::vec3 v = def;
    if (args.size() > 0)
        v.x = static_cast<float>(args[0].toNumber());
    if (args.size() > 1)
        v.y = static_cast<float>(args[1].toNumber());
    if (args.size() > 2)
        v.z = static_cast<float>(args[2].toNumber());
    return v;
}

Value ScriptHost::builtinSelfSetPos(const std::vector<Value> &args)
{
    if (!selfObj)
        return Value::makeNull();
    glm::vec3 p, r, s;
    selfObj->getTransform(p, r, s);
    glm::vec3 np = vec3FromArgs(args, p);
    selfObj->setTransform(np, r, s);
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetRot(const std::vector<Value> &args)
{
    if (!selfObj)
        return Value::makeNull();
    glm::vec3 p, r, s;
    selfObj->getTransform(p, r, s);
    glm::vec3 nr = vec3FromArgs(args, r);
    selfObj->setTransform(p, nr, s);
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetScaleFn(const std::vector<Value> &args)
{
    if (!selfObj)
        return Value::makeNull();
    glm::vec3 p, r, s;
    selfObj->getTransform(p, r, s);
    glm::vec3 ns = vec3FromArgs(args, s);
    selfObj->setTransform(p, r, ns);
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetColor(const std::vector<Value> &args)
{
    if (!selfObj)
        return Value::makeNull();
    glm::vec4 c = selfObj->getColor();
    if (args.size() > 0)
        c.r = static_cast<float>(args[0].toNumber());
    if (args.size() > 1)
        c.g = static_cast<float>(args[1].toNumber());
    if (args.size() > 2)
        c.b = static_cast<float>(args[2].toNumber());
    if (args.size() > 3)
        c.a = static_cast<float>(args[3].toNumber());
    selfObj->setColor(c);
    return Value::makeNull();
}

Value ScriptHost::builtinSelfApplyImpulse(const std::vector<Value> &args)
{
    if (!selfObj || !selfObj->getRigidBody())
        return Value::makeNull();
    glm::vec3 v = vec3FromArgs(args, glm::vec3(0.0f));
    selfObj->getRigidBody()->activate(true);
    selfObj->getRigidBody()->applyCentralImpulse(btVector3(v.x, v.y, v.z));
    return Value::makeNull();
}

Value ScriptHost::builtinSelfApplyForce(const std::vector<Value> &args)
{
    if (!selfObj || !selfObj->getRigidBody())
        return Value::makeNull();
    glm::vec3 v = vec3FromArgs(args, glm::vec3(0.0f));
    selfObj->getRigidBody()->activate(true);
    selfObj->getRigidBody()->applyCentralForce(btVector3(v.x, v.y, v.z));
    return Value::makeNull();
}

Value ScriptHost::builtinSelfSetVelocity(const std::vector<Value> &args)
{
    if (!selfObj || !selfObj->getRigidBody())
        return Value::makeNull();
    glm::vec3 v = vec3FromArgs(args, glm::vec3(0.0f));
    selfObj->getRigidBody()->activate(true);
    selfObj->getRigidBody()->setLinearVelocity(btVector3(v.x, v.y, v.z));
    return Value::makeNull();
}

Value ScriptHost::builtinSpawn(const std::vector<Value> &args, const std::string &kind)
{
    if (!sceneRef)
        return Value::makeNull();

    glm::vec3 pos = vec3FromArgs(args, glm::vec3(0.0f, 5.0f, 0.0f));
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float g = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float b = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    glm::vec4 color(r, g, b, 1.0f);

    std::unique_ptr<Object> spawned;
    if (kind == "cube")
    {
        spawned = std::make_unique<Cube>(1, model, color, 1.0f);
    }
    else if (kind == "plane")
    {
        spawned = std::make_unique<Plane>(10.0f, 10.0f, model, color, 0.0f);
    }
    else if (kind == "cow")
    {
        auto &am = AssetManager::instance();
        auto mesh = am.loadStaticMeshFromOBJ("models/cow.obj", "cow");
        if (!mesh)
            mesh = am.loadStaticMeshFromArrays("cow", mesh.get()->getVertices().data(), mesh.get()->getVertexCount(),
                                               mesh.get()->getIndices().data(), mesh.get()->getIndexCount(), mesh.get()->getFloatsPerVertex());
        if (mesh)
        {
            const auto &verts = mesh->getVertices();
            const auto &inds = mesh->getIndices();
            int stride = mesh->getFloatsPerVertex();
            spawned = std::make_unique<StaticObject>(
                mesh, verts.data(), verts.size() / stride, inds.data(), inds.size(), stride,
                model, color, 1.0f);
        }
    }

    if (!spawned)
        return Value::makeNull();
    Object *raw = spawned.get();
    sceneRef->addObject(std::move(spawned));
    return Value::makeHandle("object", raw);
}

Value ScriptHost::builtinSelfHandle(const std::vector<Value> &)
{
    if (!selfObj)
        return Value::makeNull();
    return Value::makeHandle("object", selfObj);
}

static Object *objectFromHandleArg(Object *fallback, const std::vector<Value> &args)
{
    if (args.empty())
        return fallback;
    const Value &v = args[0];
    if (v.type == Value::Handle && (v.str == "object" || v.str == "transform" || v.str == "rigidbody"))
        return static_cast<Object *>(v.handle);
    return fallback;
}

Value ScriptHost::builtinTransform(const std::vector<Value> &args)
{
    Object *o = objectFromHandleArg(selfObj, args);
    if (!o)
        return Value::makeNull();
    return Value::makeHandle("transform", o);
}

Value ScriptHost::builtinRigidbody(const std::vector<Value> &args)
{
    Object *o = objectFromHandleArg(selfObj, args);
    if (!o || !o->getRigidBody())
        return Value::makeNull();
    return Value::makeHandle("rigidbody", o);
}

Value ScriptHost::builtinCamera(const std::vector<Value> &)
{
    if (!sceneRef || !sceneRef->getPlayer())
        return Value::makeNull();
    Camera *cam = sceneRef->getPlayer()->getCamera();
    if (!cam)
        return Value::makeNull();
    return Value::makeHandle("camera", cam);
}

// ---------------------------------------------------------------------
// Property dispatch
// ---------------------------------------------------------------------

namespace
{
    Value transformGet(Object *o, const std::string &prop)
    {
        glm::vec3 p, r, s;
        o->getTransform(p, r, s);
        if (prop == "x")
            return Value::makeNumber(p.x);
        if (prop == "y")
            return Value::makeNumber(p.y);
        if (prop == "z")
            return Value::makeNumber(p.z);
        if (prop == "rx")
            return Value::makeNumber(r.x);
        if (prop == "ry")
            return Value::makeNumber(r.y);
        if (prop == "rz")
            return Value::makeNumber(r.z);
        if (prop == "sx")
            return Value::makeNumber(s.x);
        if (prop == "sy")
            return Value::makeNumber(s.y);
        if (prop == "sz")
            return Value::makeNumber(s.z);
        throw std::runtime_error("transform has no property '" + prop + "'");
    }

    void transformSet(Object *o, const std::string &prop, const Value &v)
    {
        glm::vec3 p, r, s;
        o->getTransform(p, r, s);
        float f = static_cast<float>(v.toNumber());
        if (prop == "x")
            p.x = f;
        else if (prop == "y")
            p.y = f;
        else if (prop == "z")
            p.z = f;
        else if (prop == "rx")
            r.x = f;
        else if (prop == "ry")
            r.y = f;
        else if (prop == "rz")
            r.z = f;
        else if (prop == "sx")
            s.x = f;
        else if (prop == "sy")
            s.y = f;
        else if (prop == "sz")
            s.z = f;
        else
            throw std::runtime_error("transform has no settable property '" + prop + "'");
        o->setTransform(p, r, s);
    }

    Value rigidbodyGet(Object *o, const std::string &prop)
    {
        btRigidBody *rb = o->getRigidBody();
        if (!rb)
            throw std::runtime_error("object has no rigidbody");
        const btVector3 &lv = rb->getLinearVelocity();
        if (prop == "vx")
            return Value::makeNumber(lv.x());
        if (prop == "vy")
            return Value::makeNumber(lv.y());
        if (prop == "vz")
            return Value::makeNumber(lv.z());
        if (prop == "mass")
            return Value::makeNumber(o->getMass());
        throw std::runtime_error("rigidbody has no property '" + prop + "'");
    }

    void rigidbodySet(Object *o, const std::string &prop, const Value &v)
    {
        btRigidBody *rb = o->getRigidBody();
        if (!rb)
            throw std::runtime_error("object has no rigidbody");
        float f = static_cast<float>(v.toNumber());
        btVector3 lv = rb->getLinearVelocity();
        if (prop == "vx")
            lv.setX(f);
        else if (prop == "vy")
            lv.setY(f);
        else if (prop == "vz")
            lv.setZ(f);
        else
            throw std::runtime_error("rigidbody has no settable property '" + prop + "'");
        rb->activate(true);
        rb->setLinearVelocity(lv);
    }

    Value cameraGet(Camera *c, const std::string &prop)
    {
        glm::vec3 p = c->getPosition();
        glm::vec3 f = c->getFront();
        glm::vec3 u = c->getUp();
        if (prop == "x")
            return Value::makeNumber(p.x);
        if (prop == "y")
            return Value::makeNumber(p.y);
        if (prop == "z")
            return Value::makeNumber(p.z);
        if (prop == "fx")
            return Value::makeNumber(f.x);
        if (prop == "fy")
            return Value::makeNumber(f.y);
        if (prop == "fz")
            return Value::makeNumber(f.z);
        if (prop == "ux")
            return Value::makeNumber(u.x);
        if (prop == "uy")
            return Value::makeNumber(u.y);
        if (prop == "uz")
            return Value::makeNumber(u.z);
        if (prop == "yaw")
            return Value::makeNumber(c->getYaw());
        if (prop == "pitch")
            return Value::makeNumber(c->getPitch());
        throw std::runtime_error("camera has no property '" + prop + "'");
    }

    void cameraSet(Camera *c, const std::string &prop, const Value &v)
    {
        glm::vec3 p = c->getPosition();
        float f = static_cast<float>(v.toNumber());
        if (prop == "x")
            p.x = f;
        else if (prop == "y")
            p.y = f;
        else if (prop == "z")
            p.z = f;
        else
            throw std::runtime_error("camera has no settable property '" + prop + "'");
        c->setPosition(p);
    }
}

Value ScriptHost::getProperty(const Value &target, const std::string &prop)
{
    if (target.type != Value::Handle || !target.handle)
        throw std::runtime_error("cannot read '." + prop + "' on a non-handle value");
    const std::string &kind = target.str;
    if (kind == "transform")
        return transformGet(static_cast<Object *>(target.handle), prop);
    if (kind == "rigidbody")
        return rigidbodyGet(static_cast<Object *>(target.handle), prop);
    if (kind == "camera")
        return cameraGet(static_cast<Camera *>(target.handle), prop);
    if (kind == "object")
    {
        Object *o = static_cast<Object *>(target.handle);
        if (prop == "transform")
            return Value::makeHandle("transform", o);
        if (prop == "rigidbody")
        {
            if (!o->getRigidBody())
                throw std::runtime_error("object has no rigidbody");
            return Value::makeHandle("rigidbody", o);
        }
        if (prop == "name")
            return Value::makeString(o->getName());
        // Fall through to transform-style shortcuts so `obj.x` works too.
        return transformGet(o, prop);
    }
    throw std::runtime_error("unknown handle kind '" + kind + "'");
}

void ScriptHost::setProperty(const Value &target, const std::string &prop, const Value &value)
{
    if (target.type != Value::Handle || !target.handle)
        throw std::runtime_error("cannot write '." + prop + "' on a non-handle value");
    const std::string &kind = target.str;
    if (kind == "transform")
    {
        transformSet(static_cast<Object *>(target.handle), prop, value);
        return;
    }
    if (kind == "rigidbody")
    {
        rigidbodySet(static_cast<Object *>(target.handle), prop, value);
        return;
    }
    if (kind == "camera")
    {
        cameraSet(static_cast<Camera *>(target.handle), prop, value);
        return;
    }
    if (kind == "object")
    {
        transformSet(static_cast<Object *>(target.handle), prop, value);
        return;
    }
    throw std::runtime_error("unknown handle kind '" + kind + "'");
}
