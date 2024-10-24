#pragma once

#include "Engine/World/Scene.hh"

namespace lr {
struct RuntimeScene : Scene {
    RuntimeScene(std::string_view name_, flecs::world &world_, flecs::entity &root_)
        : Scene(name_, world_, root_) {};

    bool load(this RuntimeScene &);
    bool unload(this RuntimeScene &);
    void update(this RuntimeScene &);

    bool do_load() override { return load(); };
    bool do_unload() override { return unload(); };
    void do_update() override { update(); };
};
}  // namespace lr
