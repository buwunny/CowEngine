#include "platform/ImGuiLayer.hpp"
#include "platform/Window.hpp"
#include <imgui.h>
#include <cstdio>

ImFont *ImGuiLayer::fontH1 = nullptr;
ImFont *ImGuiLayer::fontH2 = nullptr;
ImFont *ImGuiLayer::fontH3 = nullptr;

namespace
{
    // Try the absolute ASSET_ROOT path first (native only), then the relative CWD path.
    // On Emscripten, ASSET_ROOT is the native build machine's path and does not exist
    // in the WASM virtual filesystem, so we skip it and rely on the preloaded relative path.
    ImFont *loadFont(const char *relPath, float size)
    {
        ImGuiIO &io = ImGui::GetIO();
#if defined(ASSET_ROOT) && !defined(__EMSCRIPTEN__)
        {
            char buf[512];
            std::snprintf(buf, sizeof(buf), "%s/%s", ASSET_ROOT, relPath);
            ImFont *f = io.Fonts->AddFontFromFileTTF(buf, size);
            if (f)
                return f;
        }
#endif
        return io.Fonts->AddFontFromFileTTF(relPath, size);
    }
}
#if defined(__EMSCRIPTEN__)
#include "platform/imgui_impl_emscripten.h"
#include "imgui_impl_opengl3.h"
#else
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#endif

struct ImGuiLayer::Impl
{
    Window *window;
#if defined(__EMSCRIPTEN__)
    // nothing else needed
#else
    GLFWwindow *glfwWindow;
#endif
};

ImGuiLayer::ImGuiLayer(Window *window) : impl(new Impl())
{
    impl->window = window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Load JetBrains Mono. The first font added becomes the default UI font.
    const char *reg = "engine_assets/fonts/JetBrainsMono-2.304/fonts/ttf/JetBrainsMono-Regular.ttf";
    const char *bold = "engine_assets/fonts/JetBrainsMono-2.304/fonts/ttf/JetBrainsMono-Bold.ttf";
    const char *semi = "engine_assets/fonts/JetBrainsMono-2.304/fonts/ttf/JetBrainsMono-SemiBold.ttf";
    loadFont(reg, 20.0f);           // index 0 → default UI font
    fontH1 = loadFont(bold, 28.0f); // index 1 → H1 headings
    fontH2 = loadFont(bold, 24.0f); // index 2 → H2 headings
    fontH3 = loadFont(semi, 18.0f); // index 3 → H3 headings

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ScrollbarRounding = 12.0f;
    style.ScrollbarSize = 14.0f;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.14f, 0.95f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.24f, 0.20f, 0.32f, 0.95f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.32f, 0.26f, 0.40f, 0.95f);
    colors[ImGuiCol_Header] = ImVec4(0.33f, 0.29f, 0.46f, 0.88f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.40f, 0.60f, 0.92f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.52f, 0.46f, 0.68f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.47f, 0.62f, 0.84f, 0.88f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.56f, 0.72f, 0.92f, 0.95f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.54f, 0.78f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.18f, 0.27f, 0.90f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.27f, 0.40f, 0.95f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.32f, 0.48f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.20f, 0.30f, 0.85f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.35f, 0.55f, 0.95f);
    colors[ImGuiCol_TabActive] = ImVec4(0.32f, 0.28f, 0.46f, 0.98f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.56f, 0.88f, 0.85f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.82f, 0.68f, 0.96f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.86f, 0.76f, 0.98f, 1.00f);
#if defined(__EMSCRIPTEN__)
    ImGui_ImplEmscripten_Init();
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    impl->glfwWindow = window->getWindow();
    ImGui_ImplGlfw_InitForOpenGL(impl->glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
#endif
}

ImGuiLayer::~ImGuiLayer()
{
#if defined(__EMSCRIPTEN__)
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplEmscripten_Shutdown();
#else
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
#endif
    ImGui::DestroyContext();
}

void ImGuiLayer::newFrame()
{
#if defined(__EMSCRIPTEN__)
    ImGui_ImplEmscripten_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
#else
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
#endif
    ImGui::NewFrame();
}

void ImGuiLayer::render()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
