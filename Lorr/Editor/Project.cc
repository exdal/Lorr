#include "Editor/Project.hh"

#include "Engine/Asset/Asset.hh"
#include "Engine/Core/App.hh"

namespace led {
Project::Project(fs::path root_path_, const ProjectFileInfo &info_) : root_dir(std::move(root_path_)), name(info_.name) {}
Project::Project(fs::path root_path_, std::string name_) : root_dir(std::move(root_path_)), name(std::move(name_)) {}
Project::~Project() {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    if (active_scene_uuid && asset_man.get_asset(active_scene_uuid)) {
        asset_man.unload_scene(active_scene_uuid);
    }

    // app.scene_renderer.cleanup();
}

auto Project::set_active_scene(this Project &self, const lr::UUID &scene_uuid) -> bool {
    ZoneScoped;

    auto &asset_man = lr::App::mod<lr::AssetManager>();

    if (self.active_scene_uuid) {
        asset_man.unload_scene(self.active_scene_uuid);
    }

    if (!asset_man.load_scene(scene_uuid)) {
        return false;
    }

    // app.scene_renderer.cleanup();
    self.selected_entity = {};
    self.active_scene_uuid = scene_uuid;

    return true;
}

} // namespace led
