#ifndef SCRIPT_HOST_HPP
#define SCRIPT_HOST_HPP

#include "script/CowScript.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Object;
class Scene;
class Window;

// ScriptHost binds engine state (current `self` object, key state, time, etc.) to
// the built-in functions that .cow scripts may call. One ScriptHost is reused across
// frames; before invoking a script the engine sets the current `self` object so that
// `self_*` builtins act on that object.
class ScriptHost
{
public:
    using LogFn = std::function<void(const std::string &)>;

    ScriptHost();

    // Sets the engine objects available to script builtins. May be called many times.
    void setContext(Scene *scene, Window *window) { sceneRef = scene; windowRef = window; }
    void setLogger(LogFn fn) { logger = std::move(fn); }

    // Configures a freshly-compiled Script with all engine builtins, bound to `this`.
    void bindBuiltins(cowscript::Script &script);

    // Switch the `self` target for the next script invocation.
    void setSelf(Object *o) { selfObj = o; }
    Object *self() const { return selfObj; }

    // Simulated time (seconds since testing mode began) for the `time()` builtin.
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

    cowscript::Value builtinSpawn(const std::vector<cowscript::Value> &args, const std::string &kind);

    // Component-style accessors. With no args these refer to `self`; with one
    // argument (an object handle), they refer to that object.
    cowscript::Value builtinTransform(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinRigidbody(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinSelfHandle(const std::vector<cowscript::Value> &args);
    cowscript::Value builtinCamera(const std::vector<cowscript::Value> &args);

    // Dispatched on `handle.prop` / `handle.prop = value` from any script.
    cowscript::Value getProperty(const cowscript::Value &target, const std::string &prop);
    void setProperty(const cowscript::Value &target, const std::string &prop, const cowscript::Value &value);

    Scene *sceneRef = nullptr;
    Window *windowRef = nullptr;
    Object *selfObj = nullptr;
    double timeSeconds = 0.0;
    double lastDelta = 0.0;
    LogFn logger;
};

#endif // SCRIPT_HOST_HPP
