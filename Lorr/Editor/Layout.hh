#pragma once

namespace lr {
enum class EditorTheme {
    Dark,
};

struct EditorLayout {
    void setup_theme(this EditorLayout &, EditorTheme theme);
    void draw_base(this EditorLayout &);
};
}  // namespace lr
