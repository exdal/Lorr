#pragma once

#include <imgui.h>  // IWYU pragma: export

namespace lr::Graphics
{
struct ImguiRenderer
{
    void init();
    void destroy();

    void new_frame(f32 width, f32 height);
    void end_frame();
};
}  // namespace lr::Graphics