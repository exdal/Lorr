#pragma once

#include <utility>

#include "Engine/World/Scene.hh"

namespace lr {
// Dummy scene for Editor's "Load Project" UI.
// Better than displaying full black image to screen.
struct DummyScene : Scene {
    DummyScene(std::string name_, flecs::world &world_)
        : Scene(std::move(name_), world_) {};

    bool load(this DummyScene &);
    bool unload(this DummyScene &);
    void update(this DummyScene &);

    bool do_load() override { return load(); };
    bool do_unload() override { return unload(); };
    void do_update() override { update(); };
};

}  // namespace lr
