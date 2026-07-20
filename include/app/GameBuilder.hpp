#ifndef GAME_BUILDER_HPP
#define GAME_BUILDER_HPP

#include <string>
#include <vector>
#include <functional>

class Scene;

// GameBuilder packages the currently-loaded scene + scripts together with the
// embedded game runtime template into a redistributable artifact:
//   - Native: writes a self-contained folder to a user-chosen directory
//   - Web:    builds an in-memory zip and triggers a browser download
//
// The "embedded template" is a frozen copy of the game executable + bundled
// engine assets produced at editor build time (see EmbeddedTemplate.hpp).
class GameBuilder
{
public:
    enum class Target
    {
        Linux,
        Windows,
        Web,
    };

    struct Result
    {
        bool ok = false;
        std::string message;   // human-readable message (path on success, error on failure)
    };

    // True if the host platform can produce builds for this target without
    // additional toolchains. (e.g. on Linux: Linux + Web available; Windows
    // disabled.)  Web editor: only Web is producible.
    static bool isTargetAvailable(Target t);

    // Display label for a target ("Linux Game...", "Web Game (.zip)...", etc.)
    static const char *targetLabel(Target t);

    // Top-level entry: dispatch to native folder-picker or web zip-download
    // path based on target. Logger is invoked for progress lines.
    static Result build(Target t, Scene *scene,
                        const std::function<void(const std::string &)> &log);
};

#endif // GAME_BUILDER_HPP
