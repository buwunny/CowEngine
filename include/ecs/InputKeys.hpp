#ifndef ECS_INPUT_KEYS_HPP
#define ECS_INPUT_KEYS_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

// Network-stable list of the keys a .cow script may query through `key(name)`.
// The index into kInputKeyNames is the bit position used in PlayerInput::keys,
// so this ordering is part of the wire format — only ever append, never
// reorder. Deliberately free of GLFW so it compiles into the headless server;
// the bit -> GLFW code mapping lives in the client-only LocalInputSystem.
namespace ecs
{
    inline constexpr const char *kInputKeyNames[] = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
        "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "space", "enter", "shift", "ctrl", "alt",
        "up", "down", "left", "right", "escape", "tab"};

    inline constexpr int kInputKeyCount =
        static_cast<int>(sizeof(kInputKeyNames) / sizeof(kInputKeyNames[0]));
    static_assert(kInputKeyCount <= 64, "PlayerInput::keys is a 64-bit mask");

    // Case-insensitive key-name -> bit index, or -1 if the name is not a
    // recognised input key. Allocation-free so it is cheap to call per script.
    inline int inputKeyBit(std::string_view name)
    {
        for (int i = 0; i < kInputKeyCount; ++i)
        {
            std::string_view k = kInputKeyNames[i];
            if (k.size() != name.size())
                continue;
            bool eq = true;
            for (size_t j = 0; j < k.size(); ++j)
            {
                char c = name[j];
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
                if (c != k[j])
                {
                    eq = false;
                    break;
                }
            }
            if (eq)
                return i;
        }
        return -1;
    }
}

#endif // ECS_INPUT_KEYS_HPP
