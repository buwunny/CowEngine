#include "ecs/systems/LocalInputSystem.hpp"
#include "ecs/Components.hpp"
#include "ecs/InputKeys.hpp"
#include "platform/Window.hpp"
#include "core/Camera.hpp"

#include <GLFW/glfw3.h>

namespace ecs
{
    namespace
    {
        // Bit index (ecs::kInputKeyNames order) -> GLFW key code. GLFW lives on
        // the client only, which is why this mapping is here rather than in the
        // shared InputKeys.hpp. Window::isKeyPressed works on both desktop and
        // web (emscripten fills the same key state).
        int glfwForBit(int bit)
        {
            static const int table[] = {
                GLFW_KEY_A, GLFW_KEY_B, GLFW_KEY_C, GLFW_KEY_D, GLFW_KEY_E, GLFW_KEY_F,
                GLFW_KEY_G, GLFW_KEY_H, GLFW_KEY_I, GLFW_KEY_J, GLFW_KEY_K, GLFW_KEY_L,
                GLFW_KEY_M, GLFW_KEY_N, GLFW_KEY_O, GLFW_KEY_P, GLFW_KEY_Q, GLFW_KEY_R,
                GLFW_KEY_S, GLFW_KEY_T, GLFW_KEY_U, GLFW_KEY_V, GLFW_KEY_W, GLFW_KEY_X,
                GLFW_KEY_Y, GLFW_KEY_Z,
                GLFW_KEY_SPACE, GLFW_KEY_ENTER, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_LEFT_CONTROL,
                GLFW_KEY_LEFT_ALT, GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT,
                GLFW_KEY_RIGHT, GLFW_KEY_ESCAPE, GLFW_KEY_TAB};
            static_assert(sizeof(table) / sizeof(table[0]) == kInputKeyCount,
                          "glfwForBit table must match kInputKeyNames");
            if (bit < 0 || bit >= kInputKeyCount)
                return -1;
            return table[bit];
        }
    }

    void localInputSystem(Registry &r, Window *window)
    {
        if (!window)
            return;
        auto view = r.view<PlayerInput, LocalPlayer>();
        for (auto e : view)
        {
            auto &in = view.get<PlayerInput>(e);

            uint64_t keys = 0;
            for (int bit = 0; bit < kInputKeyCount; ++bit)
            {
                if (window->isKeyPressed(glfwForBit(bit)))
                    keys |= (static_cast<uint64_t>(1) << bit);
            }
            in.keys = keys;

            if (auto *pc = r.try_get<PlayerController>(e); pc && pc->camera)
            {
                in.lookYaw = pc->camera->getYaw();
                in.lookPitch = pc->camera->getPitch();
            }
            ++in.sequence;
        }
    }

    bool localKeyPressed(Window *window, std::string_view name)
    {
        if (!window)
            return false;
        int bit = inputKeyBit(name);
        if (bit < 0)
            return false;
        int code = glfwForBit(bit);
        return code >= 0 && window->isKeyPressed(code);
    }
}
