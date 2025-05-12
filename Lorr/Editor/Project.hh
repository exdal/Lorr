#pragma once

#include "Engine/Asset/UUID.hh"

#include <flecs.h>

namespace led {
// Contents of lrproj file.
struct ProjectFileInfo {
    std::string name = {};
};

enum class ActiveTool : u32 {
    Cursor = 0,
    TerrainBrush,
    DecalBrush,
    MaterialEditor,
    World,
};

struct Project {
    fs::path root_dir = {};
    std::string name = {};
    flecs::entity selected_entity = {};
    ActiveTool active_tool = ActiveTool::Cursor;
    lr::UUID active_scene_uuid = lr::UUID(nullptr);

    Project(fs::path root_path_, const ProjectFileInfo &info_);
    Project(fs::path root_path_, std::string name_);
    ~Project();

    auto set_active_scene(this Project &, const lr::UUID &scene_uuid) -> bool;
};
} // namespace led
