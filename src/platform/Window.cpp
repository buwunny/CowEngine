#include "platform/Window.hpp"
#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#include <emscripten/emscripten.h>
#include <imgui.h>
#include <cstring>
#include <cctype>

EM_JS(double, js_get_mouse_sensitivity, (), {
    try
    {
        return Module.mouseSensitivity ? Module.mouseSensitivity : 1.0;
    }
    catch (e)
    {
        return 1.0;
    }
});

EM_JS(int, js_is_canvas_pointer_locked, (), {
    try
    {
        return (Module && Module.canvas && Object.is(document.pointerLockElement, Module.canvas)) ? 1 : 0;
    }
    catch (e)
    {
        return 0;
    }
});

static Window::MouseDeltaCallback s_mouseDeltaCallback = nullptr;
static void *s_mouseDeltaUser = nullptr;

static ImGuiKey glfwKeyToImGuiKey(int key)
{
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return static_cast<ImGuiKey>(ImGuiKey_A + (key - GLFW_KEY_A));
    switch (key)
    {
    case GLFW_KEY_SPACE:         return ImGuiKey_Space;
    case GLFW_KEY_ESCAPE:        return ImGuiKey_Escape;
    case GLFW_KEY_TAB:           return ImGuiKey_Tab;
    case GLFW_KEY_BACKSPACE:     return ImGuiKey_Backspace;
    case GLFW_KEY_LEFT_SHIFT:    return ImGuiKey_LeftShift;
    case GLFW_KEY_RIGHT_SHIFT:   return ImGuiKey_RightShift;
    case GLFW_KEY_LEFT_CONTROL:  return ImGuiKey_LeftCtrl;
    case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
    case GLFW_KEY_LEFT_ALT:      return ImGuiKey_LeftAlt;
    case GLFW_KEY_RIGHT_ALT:     return ImGuiKey_RightAlt;
    case GLFW_KEY_LEFT:          return ImGuiKey_LeftArrow;
    case GLFW_KEY_RIGHT:         return ImGuiKey_RightArrow;
    case GLFW_KEY_UP:            return ImGuiKey_UpArrow;
    case GLFW_KEY_DOWN:          return ImGuiKey_DownArrow;
    default:                     return ImGuiKey_None;
    }
}

static EM_BOOL em_mousemove_callback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    if (!e)
        return EM_FALSE;
    if (!s_mouseDeltaCallback)
        return EM_FALSE;
    if (!js_is_canvas_pointer_locked())
        return EM_FALSE;
    double dpr = emscripten_get_device_pixel_ratio();
    double jsSens = js_get_mouse_sensitivity();
    float dx = static_cast<float>(e->movementX * dpr * jsSens);
    float dy = static_cast<float>(e->movementY * dpr * jsSens);
    s_mouseDeltaCallback(s_mouseDeltaUser, dx, dy);
    return EM_TRUE;
}

static EM_BOOL em_on_mouse_down(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    if (!e)
        return EM_FALSE;

    if (e->button == 2)
    {
        bool isDown = (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN);

        // Manually inject the right-click event into ImGui's IO backend
        ImGuiIO &io = ImGui::GetIO();
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, isDown);

        return EM_TRUE; // Consume the event so it doesn't bubble up
    }
    return EM_FALSE; // Pass other buttons (like left-click) to GLFW's native handler
}

static EM_BOOL em_on_mouse_up(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
    if (!e)
        return EM_FALSE;

    if (e->button == 2)
    {
        // Manually inject the right-click release event into ImGui's IO backend
        ImGuiIO &io = ImGui::GetIO();
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);

        return EM_TRUE; // Consume the event so it doesn't bubble up
    }
    return EM_FALSE; // Pass other buttons (like left-click) to GLFW's native handler
}

#endif

// Provide cross-platform helpers declared in Window.hpp
double getMouseSensitivityFor(GLFWwindow *window)
{
#if defined(__EMSCRIPTEN__)
    return js_get_mouse_sensitivity();
#else
    (void)window;
    return 1.0;
#endif
}

float getDevicePixelRatioFor(GLFWwindow *window)
{
#if defined(__EMSCRIPTEN__)
    return static_cast<float>(emscripten_get_device_pixel_ratio());
#else
    if (!window)
        return 1.0f;
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    return xscale;
#endif
}

Window::Window(int width, int height, const char *title)
{
#if defined(__EMSCRIPTEN__)
    // On web, create a WebGL2 context for the default canvas and initialize basic GL state
    window = nullptr;
    cursorDisabled = true;
    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = false;
    attr.depth = true;
    attr.stencil = false;
    attr.antialias = true;
    attr.preserveDrawingBuffer = false;
    attr.majorVersion = 2; // Request WebGL2

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attr);
    if (ctx > 0)
    {
        emscripten_webgl_make_context_current(ctx);
        emscripten_set_mousedown_callback("#canvas", NULL, EM_TRUE, em_on_mouse_down);
        emscripten_set_mouseup_callback("#canvas", NULL, EM_TRUE, em_on_mouse_up);
        // Attach mousemove handler directly to our canvas element so pointer-lock deltas arrive
        emscripten_set_mousemove_callback("#canvas", NULL, EM_TRUE, em_mousemove_callback);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }
#else
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(-1);
    }
    // Request an OpenGL 3.3 core profile context for modern OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window)
    {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        exit(-1);
    }

    glfwMakeContextCurrent(window);

    // Initialize GL function loader after creating the OpenGL context (glad).
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        exit(-1);
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glEnable(GL_DEPTH_TEST);
#endif
}

Window::~Window()
{
#if defined(__EMSCRIPTEN__)
    // nothing to teardown for the web context here
#else
    glfwTerminate();
#endif
}

void Window::update()
{
#if defined(__EMSCRIPTEN__)
    glFlush();
#else
    glfwSwapBuffers(window);
    glfwPollEvents();
#endif
}

bool Window::toggleCursor()
{
    cursorDisabled = !cursorDisabled;
#if defined(__EMSCRIPTEN__)
    // On web, cursor visibility is tied to pointer lock; toggle pointer lock state
    if (js_is_canvas_pointer_locked())
    {
        emscripten_exit_pointerlock();
        return false;
    }
    else
    {
        emscripten_request_pointerlock("#canvas", EM_TRUE);
        return true;
    }
#else
    glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    return cursorDisabled;
#endif
}

void Window::setCursorDisabled(bool disabled)
{
    cursorDisabled = disabled;
#if !(defined(__EMSCRIPTEN__))
    glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
#else
    if (disabled)
    {
        emscripten_request_pointerlock("#canvas", EM_TRUE);
    }
    else
    {
        emscripten_exit_pointerlock();
    }
#endif
}

void Window::framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

bool Window::isKeyPressed(int key)
{
#if defined(__EMSCRIPTEN__)
    ImGuiKey imKey = glfwKeyToImGuiKey(key);
    if (imKey != ImGuiKey_None)
        return ImGui::IsKeyDown(imKey);
    return false;
#else
    return glfwGetKey(window, key) == GLFW_PRESS;
#endif
}

GLFWwindow *Window::getWindow()
{
    return window;
}

bool Window::shouldClose()
{
#if defined(__EMSCRIPTEN__)
    return false;
#else
    return glfwWindowShouldClose(window);
#endif
}

void Window::setEmscriptenMouseDeltaCallback(MouseDeltaCallback cb, void *user)
{
#if defined(__EMSCRIPTEN__)
    s_mouseDeltaCallback = cb;
    s_mouseDeltaUser = user;
#else
    (void)cb;
    (void)user;
#endif
}