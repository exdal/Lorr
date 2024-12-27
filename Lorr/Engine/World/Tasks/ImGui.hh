#pragma once

#include "Engine/Asset/Asset.hh"

#include <imgui.h>

namespace lr::Tasks {
inline auto imgui_build_font_atlas(AssetManager &asset_man) -> ls::pair<std::vector<u8>, glm::ivec2> {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    auto roboto_path = (asset_man.asset_root_path(AssetType::Font) / "Roboto-Regular.ttf").string();
    auto fa_solid_900_path = (asset_man.asset_root_path(AssetType::Font) / "fa-solid-900.ttf").string();

    ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
    ImFontConfig font_config;
    font_config.GlyphMinAdvanceX = 16.0f;
    font_config.MergeMode = true;
    font_config.PixelSnapH = true;

    imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
    imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);

    imgui.Fonts->Build();

    u8 *font_data = nullptr;
    i32 font_width, font_height;
    imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);

    auto bytes_size = font_width * font_height * 4;
    auto bytes = std::vector<u8>(bytes_size);
    std::memcpy(bytes.data(), font_data, bytes_size);
    IM_FREE(font_data);

    return { std::move(bytes), { font_width, font_height } };
}

}  // namespace lr::Tasks
