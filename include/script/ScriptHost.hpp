#ifndef SCRIPT_HOST_HPP
#define SCRIPT_HOST_HPP

#include "script/CowScript.hpp"
#include "ecs/Entity.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Scene;
class Window;

// ScriptHost binds engine state (current `self` entity, key state, time, etc.)
// to the built-in functions that .cow scripts may call. One ScriptHost is
// reused across frames; before invoking a script the engine sets the current
// `self` entity so that `self_*` builtins act on that entity.
class ScriptHost
{
public:
    using LogFn = std::function<void(const std::string &)>;
    // Fallback keyboard query for `key()` on entities that carry no PlayerInput
    // (e.g. non-player scripts like jump_on_space). Wired to the local Window on
    // the client; left unset on the headless server so those scripts see no
    // input there.
    using KeyQueryFn = std::function<bool(std::string_view)>;

    ScriptHost();

    void setContext(Scene *scene, Window *window) { sceneRef = scene; windowRef = window; }
    void setLogger(LogFn fn) { logger = std::move(fn); }
    void setGlobalKeyQuery(KeyQueryFn fn) { globalKeyQuery = std::move(fn); }

    void bindBuiltins(cowscript::Script &script);

    void setSelf(ecs::Entity e) { selfEntity = e; }
    ecs::Entity self() const { return selfEntity; }

    void setTime(double t) { timeSeconds = t; }
    void setDelta(double d) { lastDelta = d; }

    void log(const std::string &line);

private:
    cowscript::Value builtinPrint(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinTime(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinDt(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinKey(const std::vector<cowscript::Value> &args);

    cowscript::Value builtinSelfPos(int axis);
    cowscript::Value builtinSelfRot(int axis);
    cowscript::Value builtinSelfScale(int axis);
    cowscript::Value builtinSelfSetPos(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfSetRot(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfSetScaleFn(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfSetColor(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfApplyImpulse(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfApplyForce(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfSetVelocity(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfOnGround(const std::vector<cowscript::Value> &args);

    cowscript::Value builtinSpawn(const std::vector<cowscript::Value> &args, const std::string &kind);

    cowscript::Value builtinTransform(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinRigidbody(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfHandle(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinCamera(const std::vector<cowscript::Value> &args);

    cowscript::Value getProperty(const cowscript::Value &target, const std::string &prop);
    void setProperty(const cowscript::Value &target, const std::string &prop, const cowscript::Value &value);

    Scene *sceneRef = nullptr;
    Window *windowRef = nullptr;
    ecs::Entity selfEntity = ecs::NullEntity;
    double timeSeconds = 0.0;
    double lastDelta = 0.0;
    LogFn logger;
    KeyQueryFn globalKeyQuery;
};

#endif // SCRIPT_HOST_HPP
