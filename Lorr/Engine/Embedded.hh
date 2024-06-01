#pragma once

namespace lr::embedded {
union _data_t {
    c8 as_c8 = 0;
    u8 as_u8;
};
static_assert(sizeof(_data_t) == 1);

namespace models {
}

namespace shaders {
    constexpr static _data_t lorr[] = {
#include <lorr.slang.h>
    };
    constexpr static std::span lorr_sp = { &lorr->as_u8, count_of(lorr) };
    constexpr static std::string_view lorr_str = { &lorr->as_c8, count_of(lorr) };

    constexpr static _data_t imgui[] = {
#include <imgui.slang.h>
    };
    constexpr static std::span imgui_sp = { &imgui->as_u8, count_of(imgui) };
    constexpr static std::string_view imgui_str = { &imgui->as_c8, count_of(imgui) };
}  // namespace shaders

}  // namespace lr::embedded
