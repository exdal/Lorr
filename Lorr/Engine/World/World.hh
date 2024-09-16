#pragma once

#include "Scene.hh"

namespace lr {
struct ProjectInfo {
    std::string_view project_name = {};
    std::string_view scenes_path = {};
    std::string_view models_path = {};
};

struct World {
    std::string name = "unnamed_world";
    flecs::world ecs{};
    std::vector<std::unique_ptr<Scene>> scenes = {};
    ls::option<SceneID> active_scene = ls::nullopt;
    ls::option<ProjectInfo> project_info = ls::nullopt;

    bool init(this World &);
    void shutdown(this World &);
    bool poll(this World &);

    bool import_scene(this World &, Scene &scene, const fs::path &path);
    bool export_scene(this World &, Scene &scene, const fs::path &dir);
    bool import_project(this World &, const fs::path &path);
    bool export_project(this World &, const fs::path &root_dir);

    template<typename T>
    SceneID create_scene(this World &self, std::string_view name) {
        ZoneScoped;

        auto scene = std::make_unique<T>(std::string(name), self.ecs);
        usize scene_id = self.scenes.size();
        self.scenes.push_back(std::move(scene));

        return static_cast<SceneID>(scene_id);
    }

    template<typename T>
    SceneID create_scene_from_file(this World &self, const fs::path &path) {
        ZoneScoped;

        auto scene_id = self.create_scene<T>("unnamed scene");
        if (!self.import_scene(scene_id, path)) {
            LR_LOG_ERROR("Failed to parse scene!");
            return SceneID::Invalid;
        }

        return scene_id;
    }

    void set_active_scene(this World &, SceneID scene_id);
    Scene &scene_at(this World &self, SceneID scene_id) { return *self.scenes[static_cast<usize>(scene_id)]; }
};

}  // namespace lr
