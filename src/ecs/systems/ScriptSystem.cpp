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
            if (ident.scriptPaths.empty())
                continue;
            // Already compiled? Skip. The editor removes the component to force a reload.
            if (r.all_of<ScriptComponent>(e) && !r.get<ScriptComponent>(e).scripts.empty())
                continue;

            std::vector<ScriptInstance> scripts;
            for (const auto &path : ident.scriptPaths)
            {
                if (path.empty())
                    continue;
                std::string foundPath;
                std::string source = cowscript::readScriptFile(path, &foundPath);
                if (source.empty())
                {
                    std::cerr << "ScriptSystem: failed to read '" << path << "'" << std::endl;
                    continue;
                }
                auto script = std::make_shared<cowscript::Script>();
                host.bindBuiltins(*script);
                std::string err = script->compile(source);
                if (!err.empty())
                {
                    std::cerr << "ScriptSystem: compile error in '" << path << "': " << err << std::endl;
                    continue;
                }
                scripts.push_back(ScriptInstance{path, std::move(script)});
                ++count;
            }
            if (!scripts.empty())
                r.emplace_or_replace<ScriptComponent>(e, ScriptComponent{std::move(scripts)});
        }
        return count;
    }

    void resetScripts(Registry &r)
    {
        r.clear<ScriptComponent>();
    }

    void startScripts(Registry &r, ScriptHost &host)
    {
        auto view = r.view<ScriptComponent>();
        for (auto e : view)
        {
            auto &sc = view.get<ScriptComponent>(e);
            host.setSelf(e);
            for (auto &inst : sc.scripts)
            {
                if (!inst.script)
                    continue;
                std::string err = inst.script->callEvent("start", {});
                if (!err.empty())
                    std::cerr << "ScriptSystem: '" << inst.path << "' on start: " << err << std::endl;
            }
        }
        host.setSelf(NullEntity);
    }

    void updateScripts(Registry &r, ScriptHost &host, float dt)
    {
        std::vector<cowscript::Value> args;
        args.push_back(cowscript::Value::makeNumber(dt));
        auto view = r.view<ScriptComponent>();
        for (auto e : view)
        {
            auto &sc = view.get<ScriptComponent>(e);
            host.setSelf(e);
            for (auto &inst : sc.scripts)
            {
                if (!inst.script)
                    continue;
                std::string err = inst.script->callEvent("update", args);
                if (!err.empty())
                    std::cerr << "ScriptSystem: '" << inst.path << "' on update: " << err << std::endl;
            }
        }
        host.setSelf(NullEntity);
    }
}
