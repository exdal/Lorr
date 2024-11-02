#pragma once

#include "Engine/OS/Key.hh"

namespace lr {
struct ApplicationEventData {
    glm::uvec2 size = {};
    glm::vec2 position = {};

    Key key = LR_KEY_UNKNOWN;
    KeyState key_state = KeyState::Up;
    KeyMod key_mods = KeyMod::None;
    i32 key_scancode = ~0_i32;

    c32 input_char = ~0_u32;
    std::vector<fs::path> paths = {};
};

enum class ApplicationEvent : u32 {
    WindowResize = 0,
    MousePosition,
    MouseState,
    MouseScroll,
    KeyboardState,
    InputChar,
    Drop,
    Quit,

    Invalid = ~0_u32,
};
}  // namespace lr
