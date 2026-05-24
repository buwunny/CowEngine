#include "ecs/systems/ScriptSystem.hpp"
#include "ecs/Components.hpp"
#include "script/CowScript.hpp"
#include "script/ScriptHost.hpp"

#include <iostream>

namespace ecs
{
    int loadScripts(Registry &r, ScriptHost &host)
    {
        int count = 0;
        auto view = r.view<Identity>();
        for (auto e : view)
        {
            auto &ident = view.get<Identity>(e);
            if (ident.scriptPath.empty())
                continue;
            if (r.all_of<ScriptComponent>(e) && r.get<ScriptComponent>(e).script)
                continue;

            std::string foundPath;
            std::string source = cowscript::readScriptFile(ident.scriptPath, &foundPath);
            if (source.empty())
            {
                std::cerr << "ScriptSystem: failed to read '" << ident.scriptPath << "'" << std::endl;
                continue;
            }
            auto script = std::make_shared<cowscript::Script>();
            host.bindBuiltins(*script);
            std::string err = script->compile(source);
            if (!err.empty())
            {
                std::cerr << "ScriptSystem: compile error in '" << ident.scriptPath << "': " << err << std::endl;
                continue;
            }
            r.emplace_or_replace<ScriptComponent>(e, ScriptComponent{script});
            ++count;
        }
        return count;
    }

    void resetScripts(Registry &r)
    {
        r.clear<ScriptComponent>();
    }

    void startScripts(Registry &r, ScriptHost &host)
    {
        auto view = r.view<ScriptComponent, Identity>();
        for (auto e : view)
        {
            auto &sc = view.get<ScriptComponent>(e);
            if (!sc.script)
                continue;
            host.setSelf(e);
            std::string err = sc.script->callEvent("start", {});
            if (!err.empty())
                std::cerr << "ScriptSystem: '" << view.get<Identity>(e).scriptPath << "' on start: " << err << std::endl;
        }
        host.setSelf(NullEntity);
    }

    void updateScripts(Registry &r, ScriptHost &host, float dt)
    {
        std::vector<cowscript::Value> args;
        args.push_back(cowscript::Value::makeNumber(dt));
        auto view = r.view<ScriptComponent, Identity>();
        for (auto e : view)
        {
            auto &sc = view.get<ScriptComponent>(e);
            if (!sc.script)
                continue;
            host.setSelf(e);
            std::string err = sc.script->callEvent("update", args);
            if (!err.empty())
                std::cerr << "ScriptSystem: '" << view.get<Identity>(e).scriptPath << "' on update: " << err << std::endl;
        }
        host.setSelf(NullEntity);
    }
}
