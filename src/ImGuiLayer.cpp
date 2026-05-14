#include "ImGuiLayer.hpp"
#include "Window.hpp"
#include <imgui.h>
#if defined(__EMSCRIPTEN__)
#include "imgui_impl_emscripten.h"
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
    ImGui::StyleColorsDark();
    ImGuiIO &io = ImGui::GetIO();
    // Note: docking/viewports may not be available in this ImGui version.
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
