#include "EditorApp.hh"

#include "Engine/OS/File.hh"
#include "Engine/Util/JsonWriter.hh"

#include <simdjson.h>
namespace sj = simdjson;

namespace lr {
auto EditorApp::load_editor_data(this EditorApp &self) -> void {
    ZoneScoped;

    auto data_file_path = self.asset_man.asset_root_path(AssetType::Root) / "editor.json";
    if (!fs::exists(data_file_path)) {
        self.save_editor_data();
        return;
    }

    File file(data_file_path, FileAccess::Read);
    if (!file) {
        // Its not that important
        return;
    }

    auto json = sj::padded_string(file.size);
    file.read(json.data(), file.size);

    sj::ondemand::parser parser;
    auto doc = parser.iterate(json);
    if (doc.error()) {
        LOG_ERROR("Failed to parse editor data file! {}", sj::error_message(doc.error()));
        return;
    }

    auto recent_projects = doc["recent_projects"].get_array();
    for (auto project_info_json : recent_projects) {
        auto project_path = project_info_json.get_string();
        if (project_path.error()) {
            continue;
        }

        self.recent_projects.insert(project_path.value());
    }
}

auto EditorApp::save_editor_data(this EditorApp &self) -> void {
    ZoneScoped;

    auto data_file_path = self.asset_man.asset_root_path(AssetType::Root) / "editor.json";
    JsonWriter json;
    json.begin_obj();
    json["recent_projects"].begin_array();
    for (const auto &v : self.recent_projects) {
        const auto &path_str = v.string();
        json << path_str;
    }

    json.end_array();
    json.end_obj();

    auto json_str = json.stream.view();
    File file(data_file_path, FileAccess::Write);
    file.write(json_str.data(), json_str.length());
    file.close();
}

auto EditorApp::new_project(this EditorApp &self, const fs::path &root_dir, const std::string &name) -> bool {
    ZoneScoped;

    if (!fs::is_directory(root_dir)) {
        LOG_ERROR("New projects must be inside a directory.");
        return false;
    }

    // root_dir = /path/to/my_projects
    // proj_root_dir = root_dir / <PROJECT_NAME>
    // proj_file = proj_root_dir / world.lrproj
    const auto &proj_root_dir = root_dir / name;
    const auto &proj_file_path = proj_root_dir / "world.lrproj";

    // Project Name
    // - Audio
    // - Models
    // - Scenes
    // - Shaders
    // - Prefabs
    // world.lrproj

    // Fresh project, the dev has freedom to change file structure
    // but we prepare initial structure to ease out some stuff
    if (!fs::exists(proj_root_dir)) {
        // /<root_dir>/<name>/[dir_tree*]
        // Create project root directory first
        std::error_code err;
        fs::create_directories(proj_root_dir, err);
        if (err) {
            LOG_ERROR("Failed to create directory '{}'! {}", proj_root_dir, err.message());
            return false;
        }

        // clang-format off
        constexpr static std::string_view dir_tree[] = {
            "Assets",
            "Assets/Audio",
            "Assets/Scenes",
            "Assets/Shaders",
            "Assets/Models",
            "Assets/Textures",
        };
        // clang-format on
        for (const auto &dir : dir_tree) {
            fs::create_directories(proj_root_dir / dir);
        }
    }

    JsonWriter json;
    json.begin_obj();
    json["name"] = name;
    json.end_obj();

    File file(proj_file_path, FileAccess::Write);
    if (!file) {
        LOG_ERROR("Failed to open file {}!", proj_file_path);
        return false;
    }

    auto json_str = json.stream.view();
    file.write(json_str.data(), json_str.length());
    file.close();

    self.active_project = Project{ .root_dir = proj_root_dir };
    self.recent_projects.insert(proj_root_dir);
    self.save_editor_data();

    return true;
}

auto EditorApp::open_project(this EditorApp &self, const fs::path &path) -> bool {
    ZoneScoped;

    const auto &proj_root_dir = path.parent_path();
    self.active_project = Project{ .root_dir = proj_root_dir };
    self.recent_projects.insert(proj_root_dir);
    self.save_editor_data();

    return true;
}

auto EditorApp::save_project(this EditorApp &self) -> void {
    ZoneScoped;

    if (self.active_scene_uuid) {
        auto *scene_asset = self.asset_man.get_asset(self.active_scene_uuid.value());
        self.asset_man.export_asset(self.active_scene_uuid.value(), scene_asset->path);
    }
}

bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    self.load_editor_data();
    self.layout.init();

    return true;
}

bool EditorApp::update(this EditorApp &, f64) {
    ZoneScoped;

    return true;
}

auto EditorApp::render(this EditorApp &self, vuk::Format format, vuk::Extent3D extent) -> bool {
    ZoneScoped;

    self.layout.render(format, extent);

    return true;
}

auto EditorApp::shutdown(this EditorApp &self) -> void {
    ZoneScoped;

    self.layout.destroy();
}

}  // namespace lr
