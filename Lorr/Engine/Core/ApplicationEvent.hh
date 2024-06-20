#pragma once

#include "Engine/OS/Key.hh"

namespace lr {
union ApplicationEventData {
    u8 data = 0;

    // ::WindowResize
    struct {
        glm::uvec2 window_size;
    };

    // ::MousePosition
    struct {
        glm::vec2 mouse_pos;
    };

    // ::MouseState
    struct {
        Key mouse_key;
        KeyState mouse_key_state;
        KeyMod mouse_key_mod;
    };

    // ::MouseScroll
    struct {
        glm::vec2 mouse_scroll_offset;
    };

    // ::KeyboardState
    struct {
        Key key;
        KeyState key_state;
        KeyMod key_mods;
        i32 key_scancode;
    };

    // ::InputChar
    struct {
        c32 input_char;
    };
};

enum class ApplicationEvent : u32 {
    WindowResize,
    MousePosition,
    MouseState,
    MouseScroll,
    KeyboardState,
    InputChar,

    Invalid = ~0_u32,
};
}  // namespace lr
