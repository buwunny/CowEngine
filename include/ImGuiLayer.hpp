#ifndef IMGUI_LAYER_HPP
#define IMGUI_LAYER_HPP
#include <memory>

struct Window;

class ImGuiLayer
{
public:
    ImGuiLayer(Window *window);
    ~ImGuiLayer();

    void newFrame();
    void render();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // IMGUI_LAYER_HPP