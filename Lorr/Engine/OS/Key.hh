#pragma once

namespace lr {
enum class KeyScancode : u32 {
    eA = 4,
};
enum Key : u32 {
    LR_KEY_UNKNOWN = ~0_u32,
    LR_KEY_MOUSE_1 = 0,
    LR_KEY_MOUSE_2 = 1,
    LR_KEY_MOUSE_3 = 2,
    LR_KEY_MOUSE_4 = 3,
    LR_KEY_MOUSE_5 = 4,
    LR_KEY_MOUSE_6 = 5,
    LR_KEY_MOUSE_7 = 6,
    LR_KEY_MOUSE_8 = 7,
    LR_KEY_MOUSE_LEFT = LR_KEY_MOUSE_1,
    LR_KEY_MOUSE_RIGHT = LR_KEY_MOUSE_2,
    LR_KEY_MOUSE_MIDDLE = LR_KEY_MOUSE_3,

    LR_KEY_SPACE = 32,
    LR_KEY_AND = 38,    // &
    LR_KEY_QUOTE = 39,  // '
    LR_KEY_COMMA = 44,  // ,
    LR_KEY_MINUS = 45,  // -
    LR_KEY_DOT = 46,    // .
    LR_KEY_SLASH = 47,  // /

    LR_KEY_0 = 48,
    LR_KEY_1 = 49,
    LR_KEY_2 = 50,
    LR_KEY_3 = 51,
    LR_KEY_4 = 52,
    LR_KEY_5 = 53,
    LR_KEY_6 = 54,
    LR_KEY_7 = 55,
    LR_KEY_8 = 56,
    LR_KEY_9 = 57,

    LR_KEY_SEMICOLON = 59,  // ;
    LR_KEY_EQUAL = 61,      // =

    LR_KEY_A = 65,
    LR_KEY_B = 66,
    LR_KEY_C = 67,
    LR_KEY_D = 68,
    LR_KEY_E = 69,
    LR_KEY_F = 70,
    LR_KEY_G = 71,
    LR_KEY_H = 72,
    LR_KEY_I = 73,
    LR_KEY_J = 74,
    LR_KEY_K = 75,
    LR_KEY_L = 76,
    LR_KEY_M = 77,
    LR_KEY_N = 78,
    LR_KEY_O = 79,
    LR_KEY_P = 80,
    LR_KEY_Q = 81,
    LR_KEY_R = 82,
    LR_KEY_S = 83,
    LR_KEY_T = 84,
    LR_KEY_U = 85,
    LR_KEY_V = 86,
    LR_KEY_W = 87,
    LR_KEY_X = 88,
    LR_KEY_Y = 89,
    LR_KEY_Z = 90,

    LR_KEY_LBRACKET = 91,      // [
    LR_KEY_BACKSLASH = 92,     /* \ */
    LR_KEY_RBRACKET = 93,      // ]
    LR_KEY_GRAVE_ACCENT = 96,  // `
    LR_KEY_WORLD_1 = 161,
    LR_KEY_WORLD_2 = 162,

    LR_KEY_ESC = 256,
    LR_KEY_ENTER = 257,
    LR_KEY_TAB = 258,
    LR_KEY_BACKSPACE = 259,
    LR_KEY_INSERT = 260,
    LR_KEY_DELETE = 261,
    LR_KEY_RIGHT = 262,
    LR_KEY_LEFT = 263,
    LR_KEY_DOWN = 264,
    LR_KEY_UP = 265,
    LR_KEY_PAGE_UP = 266,
    LR_KEY_PAGE_DOWN = 267,
    LR_KEY_HOME = 268,
    LR_KEY_END = 269,
    LR_KEY_CAPS_LOCK = 280,
    LR_KEY_SCROLL_LOCK = 281,
    LR_KEY_NUM_LOCK = 282,
    LR_KEY_PRINT_SCREEN = 283,
    LR_KEY_PAUSE = 284,
    LR_KEY_F1 = 290,
    LR_KEY_F2 = 291,
    LR_KEY_F3 = 292,
    LR_KEY_F4 = 293,
    LR_KEY_F5 = 294,
    LR_KEY_F6 = 295,
    LR_KEY_F7 = 296,
    LR_KEY_F8 = 297,
    LR_KEY_F9 = 298,
    LR_KEY_F10 = 299,
    LR_KEY_F11 = 300,
    LR_KEY_F12 = 301,
    LR_KEY_F13 = 302,
    LR_KEY_F14 = 303,
    LR_KEY_F15 = 304,
    LR_KEY_F16 = 305,
    LR_KEY_F17 = 306,
    LR_KEY_F18 = 307,
    LR_KEY_F19 = 308,
    LR_KEY_F20 = 309,
    LR_KEY_F21 = 310,
    LR_KEY_F22 = 311,
    LR_KEY_F23 = 312,
    LR_KEY_F24 = 313,
    LR_KEY_F25 = 314,

    LR_KEY_KP_0 = 320,
    LR_KEY_KP_1 = 321,
    LR_KEY_KP_2 = 322,
    LR_KEY_KP_3 = 323,
    LR_KEY_KP_4 = 324,
    LR_KEY_KP_5 = 325,
    LR_KEY_KP_6 = 326,
    LR_KEY_KP_7 = 327,
    LR_KEY_KP_8 = 328,
    LR_KEY_KP_9 = 329,

    LR_KEY_KP_DECIMAL = 330,
    LR_KEY_KP_DIVIDE = 331,
    LR_KEY_KP_MULTIPLY = 332,
    LR_KEY_KP_SUBTRACT = 333,
    LR_KEY_KP_ADD = 334,
    LR_KEY_KP_ENTER = 335,
    LR_KEY_KP_EQUAL = 336,

    LR_KEY_LSHIFT = 340,
    LR_KEY_LCONTROL = 341,
    LR_KEY_LALT = 342,
    LR_KEY_LSUPER = 343,
    LR_KEY_RSHIFT = 344,
    LR_KEY_RCONTROL = 345,
    LR_KEY_RALT = 346,
    LR_KEY_RSUPER = 347,
    LR_KEY_MENU = 348,
};

enum class KeyState : u32 {
    Up = 0,
    Down,
    Repeat,
};

enum class KeyMod : u32 {
    None = 0,
    LShift = 1 << 0,
    RShift = 1 << 1,
    LControl = 1 << 2,
    RControl = 1 << 3,
    LAlt = 1 << 4,
    RAlt = 1 << 5,
    LSuper = 1 << 6,
    RSuper = 1 << 7,
    NumLock = 1 << 8,
    CapsLock = 1 << 9,
    AltGr = 1 << 10,
    ScrollLock = 1 << 11,
};

}  // namespace lr
