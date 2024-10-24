#pragma once

#include "Engine/World/Renderer.hh"
#include "Engine/World/Scene.hh"

namespace lr {
struct ProjectInfo {
    std::string project_name = {};
    fs::path project_root_path = {};
    fs::path scenes_path = {};
    fs::path models_path = {};
};

struct World {
    flecs::world ecs{};
    flecs::entity root = {};
    std::unique_ptr<WorldRenderer> renderer = {};
    std::vector<std::unique_ptr<Scene>> scenes = {};
    ls::option<SceneID> active_scene = ls::nullopt;
    ls::option<ProjectInfo> project_info = ls::nullopt;

    bool init(this World &);
    void shutdown(this World &);
    void begin_frame(this World &);
    void end_frame(this World &);

    // Project IO
    bool import_scene(this World &, Scene &scene, const fs::path &path);
    bool export_scene(this World &, Scene &scene, const fs::path &dir);
    bool import_project(this World &, const fs::path &path);
    bool export_project(this World &, std::string_view project_name, const fs::path &root_dir);

    // Active Project Utils
    bool unload_active_project(this World &);
    bool save_active_project(this World &);
    bool is_project_active() { return project_info.has_value(); }

    template<typename T>
    SceneID create_scene(this World &self, std::string_view name) {
        ZoneScoped;

        auto scene = std::make_unique<T>(std::string(name), self.ecs, self.root);
        usize scene_id = self.scenes.size();
        self.scenes.push_back(std::move(scene));

        return static_cast<SceneID>(scene_id);
    }

    template<typename T>
    SceneID create_scene_from_file(this World &self, const fs::path &path) {
        ZoneScoped;

        auto scene_id = self.create_scene<T>("unnamed scene");
        if (!self.import_scene(self.scene_at(scene_id), path)) {
            LOG_ERROR("Failed to parse scene!");
            return SceneID::Invalid;
        }

        return scene_id;
    }

    void set_active_scene(this World &, SceneID scene_id);
    Scene &scene_at(this World &self, SceneID scene_id) { return *self.scenes[static_cast<usize>(scene_id)]; }
};

}  // namespace lr
