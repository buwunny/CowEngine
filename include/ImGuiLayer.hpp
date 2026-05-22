#ifndef IMGUI_LAYER_HPP
#define IMGUI_LAYER_HPP
#include <memory>
#include <imgui.h>

struct Window;

class ImGuiLayer
{
public:
    ImGuiLayer(Window *window);
    ~ImGuiLayer();

    void newFrame();
    void render();

    // Heading fonts loaded at startup (null = use default font).
    static ImFont *fontH1;
    static ImFont *fontH2;
    static ImFont *fontH3;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // IMGUI_LAYER_HPP