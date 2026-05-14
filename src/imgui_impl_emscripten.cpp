// Emscripten platform backend for Dear ImGui
#include "imgui_impl_emscripten.h"
#include <imgui.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#include <cstring>
#include <cctype>

static float getDevicePixelRatio()
{
    // emscripten_get_device_pixel_ratio() returns a double
    return static_cast<float>(emscripten_get_device_pixel_ratio());
}

static EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!e)
        return EM_FALSE;
    // Compute canvas bounding rect and convert client coordinates to canvas-local CSS pixels
    double left = EM_ASM_DOUBLE({
        var c = Module['canvas'] || document.querySelector('#canvas');
        if (!c)
            return 0.0;
        return c.getBoundingClientRect().left;
    });
    double top = EM_ASM_DOUBLE({
        var c = Module['canvas'] || document.querySelector('#canvas');
        if (!c)
            return 0.0;
        return c.getBoundingClientRect().top;
    });
    float x = (float)((double)e->clientX - left);
    float y = (float)((double)e->clientY - top);
    io.AddMousePosEvent(x, y);
    return EM_TRUE;
}

static EM_BOOL mousedown_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!e)
        return EM_FALSE;
    int button = e->button;
    if (button >= 0 && button < 3)
        io.AddMouseButtonEvent(button, true);
    return EM_TRUE;
}

static EM_BOOL mouseup_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!e)
        return EM_FALSE;
    int button = e->button;
    if (button >= 0 && button < 3)
        io.AddMouseButtonEvent(button, false);
    return EM_TRUE;
}

static EM_BOOL wheel_callback(int eventType, const EmscriptenWheelEvent *e, void *userData)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!e)
        return EM_FALSE;
    io.AddMouseWheelEvent((float)e->deltaX, (float)e->deltaY);
    return EM_TRUE;
}

static ImGuiKey mapKeyStringToImGuiKey(const char *key)
{
    if (!key || key[0] == '\0')
        return ImGuiKey_None;
    if (std::strcmp(key, "Tab") == 0)
        return ImGuiKey_Tab;
    if (std::strcmp(key, "ArrowLeft") == 0)
        return ImGuiKey_LeftArrow;
    if (std::strcmp(key, "ArrowRight") == 0)
        return ImGuiKey_RightArrow;
    if (std::strcmp(key, "ArrowUp") == 0)
        return ImGuiKey_UpArrow;
    if (std::strcmp(key, "ArrowDown") == 0)
        return ImGuiKey_DownArrow;
    if (std::strcmp(key, "PageUp") == 0)
        return ImGuiKey_PageUp;
    if (std::strcmp(key, "PageDown") == 0)
        return ImGuiKey_PageDown;
    if (std::strcmp(key, "Home") == 0)
        return ImGuiKey_Home;
    if (std::strcmp(key, "End") == 0)
        return ImGuiKey_End;
    if (std::strcmp(key, "Insert") == 0)
        return ImGuiKey_Insert;
    if (std::strcmp(key, "Delete") == 0)
        return ImGuiKey_Delete;
    if (std::strcmp(key, "Backspace") == 0)
        return ImGuiKey_Backspace;
    if (std::strcmp(key, "Enter") == 0 || std::strcmp(key, "Return") == 0)
        return ImGuiKey_Enter;
    if (std::strcmp(key, "Escape") == 0)
        return ImGuiKey_Escape;
    if (std::strcmp(key, " ") == 0 || std::strcmp(key, "Space") == 0)
        return ImGuiKey_Space;
    // Single character keys (letters)
    if (key[1] == '\0')
    {
        char c = key[0];
        if (std::isalpha((unsigned char)c))
        {
            int idx = std::toupper((unsigned char)c) - 'A';
            return static_cast<ImGuiKey>(ImGuiKey_A + idx);
        }
    }
    return ImGuiKey_None;
}

static EM_BOOL keydown_callback(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!e)
        return EM_FALSE;
    if (e->key[0] != '\0' && !(e->ctrlKey || e->metaKey))
        io.AddInputCharactersUTF8(e->key);
    // Fallback to legacy API: update modifier flags on io
    io.KeyShift = e->shiftKey;
    io.KeyCtrl = e->ctrlKey;
    io.KeyAlt = e->altKey;
    io.KeySuper = e->metaKey;
    return EM_TRUE;
}

static EM_BOOL keyup_callback(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!e)
        return EM_FALSE;
    io.KeyShift = e->shiftKey;
    io.KeyCtrl = e->ctrlKey;
    io.KeyAlt = e->altKey;
    io.KeySuper = e->metaKey;
    return EM_TRUE;
}

void ImGui_ImplEmscripten_Init()
{
    ImGuiIO &io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_emscripten";
    emscripten_set_mousemove_callback("#canvas", NULL, EM_TRUE, mouse_callback);
    emscripten_set_mousedown_callback("#canvas", NULL, EM_TRUE, mousedown_callback);
    emscripten_set_mouseup_callback("#canvas", NULL, EM_TRUE, mouseup_callback);
    emscripten_set_wheel_callback("#canvas", NULL, EM_TRUE, wheel_callback);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, keydown_callback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, keyup_callback);
}

void ImGui_ImplEmscripten_Shutdown()
{
    // No persistent handles to clean up in this minimal backend
}

void ImGui_ImplEmscripten_NewFrame()
{
    ImGuiIO &io = ImGui::GetIO();
    int w, h;
    emscripten_get_canvas_element_size("canvas", &w, &h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    float dpr = getDevicePixelRatio();
    io.DisplayFramebufferScale = ImVec2(dpr, dpr);
}

#endif