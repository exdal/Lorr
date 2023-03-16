//
// Created on Monday 21st November 2022 by exdal
//

#pragma once

namespace lr
{
    // They are keys, literal chars, don't confuse them with 'scancodes'
    enum Key : u32
    {
        LR_KEY_NONE = 0,
        LR_KEY_LMOUSE,
        LR_KEY_RMOUSE,
        LR_KEY_MMOUSE,

        // Mods
        LR_KEY_LSHIFT,
        LR_KEY_LCTRL,
        LR_KEY_LALT,
        LR_KEY_RSHIFT,
        LR_KEY_RCTRL,
        LR_KEY_RALT,

        LR_KEY_CAPS_LOCK,
        LR_KEY_TAB,
        LR_KEY_BACKSPACE,
        LR_KEY_ENTER,
        LR_KEY_ESCAPE,
        LR_KEY_SPACE,

        LR_KEY_ARROW_LEFT,
        LR_KEY_ARROW_UP,
        LR_KEY_ARROW_RIGHT,
        LR_KEY_ARROW_DOWN,

        LR_KEY_0,
        LR_KEY_1,
        LR_KEY_2,
        LR_KEY_3,
        LR_KEY_4,
        LR_KEY_5,
        LR_KEY_6,
        LR_KEY_7,
        LR_KEY_8,
        LR_KEY_9,

        LR_KEY_A,
        LR_KEY_B,
        LR_KEY_C,
        LR_KEY_D,
        LR_KEY_E,
        LR_KEY_F,
        LR_KEY_G,
        LR_KEY_H,
        LR_KEY_I,
        LR_KEY_J,
        LR_KEY_K,
        LR_KEY_L,
        LR_KEY_M,
        LR_KEY_N,
        LR_KEY_O,
        LR_KEY_P,
        LR_KEY_Q,
        LR_KEY_R,
        LR_KEY_S,
        LR_KEY_T,
        LR_KEY_U,
        LR_KEY_V,
        LR_KEY_W,
        LR_KEY_X,
        LR_KEY_Y,
        LR_KEY_Z,

        LR_KEY_SUPER,  // Windows key

        LR_KEY_NUM_0,
        LR_KEY_NUM_1,
        LR_KEY_NUM_2,
        LR_KEY_NUM_3,
        LR_KEY_NUM_4,
        LR_KEY_NUM_5,
        LR_KEY_NUM_6,
        LR_KEY_NUM_7,
        LR_KEY_NUM_8,
        LR_KEY_NUM_9,

        // Symbols
        LR_KEY_MULTIPLY,      // *
        LR_KEY_ADD,           // +
        LR_KEY_MINUS,         // -
        LR_KEY_DOT,           // .
        LR_KEY_SLASH,         // /
        LR_KEY_BACKSLASH,     /* \ */
        LR_KEY_AND,           // &
        LR_KEY_AT,            // @
        LR_KEY_COLON,         // :
        LR_KEY_COMMA,         // ,
        LR_KEY_VERTICAL_BAR,  // |
        LR_KEY_EQUALS,        // =
        LR_KEY_EXCLAM,        // !
        LR_KEY_LCURLYBRACK,   // {
        LR_KEY_RCURLYBRACK,   // }
        LR_KEY_LBRACK,        // [
        LR_KEY_RBRACK,        // ]
        LR_KEY_LPAREN,        // (
        LR_KEY_RPAREN,        // )
        LR_KEY_GREATER,       // >
        LR_KEY_LESS,          // <
        LR_KEY_PERCENT,       // %
        LR_KEY_POWER,         // ^
        LR_KEY_DQUOTE,        // "
        LR_KEY_QUOTE,         // '
        LR_KEY_GRAVE_ACCENT,  // `
        LR_KEY_DOLLAR,        // $
        LR_KEY_QUESTION,      // ?
        LR_KEY_UNDERSCORE,    // _
        LR_KEY_HASH,          // #

        // Functions
        LR_KEY_F1,
        LR_KEY_F2,
        LR_KEY_F3,
        LR_KEY_F4,
        LR_KEY_F5,
        LR_KEY_F6,
        LR_KEY_F7,
        LR_KEY_F8,
        LR_KEY_F9,
        LR_KEY_F10,
        LR_KEY_F12,
    };

    enum MouseState : u8
    {
        LR_MOUSE_STATE_DOWN,
        LR_MOUSE_STATE_UP,
        LR_MOUSE_STATE_DOUBLE_CLICK,
    };

    enum KeyState : u8
    {
        LR_KEY_STATE_DOWN,
        LR_KEY_STATE_UP,
    };

}  // namespace lr