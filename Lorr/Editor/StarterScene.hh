#pragma once

#include "Engine/World/Scene.hh"

namespace lr {
// Dummy scene for Editor's "Load Project" UI.
// Better than displaying full black image to screen.
struct StarterScene : Scene {
    StarterScene(std::string_view name_, flecs::world &world_)
        : Scene(name_, world_) {};

    bool load(this StarterScene &);
    bool unload(this StarterScene &);
    void update(this StarterScene &);

    bool do_load() override { return load(); };
    bool do_unload() override { return unload(); };
    void do_update() override { update(); };
};

}  // namespace lr
