#pragma once

#include "Engine/Asset/UUID.hh"

struct RuntimeModule {
    static constexpr auto MODULE_NAME = "Runtime";

    bool debugging = false;
    fs::path world_path = {};
    lr::UUID active_scene_uuid = lr::UUID(nullptr);

    RuntimeModule(fs::path world_path_) : world_path(std::move(world_path_)) {}
    auto init(this RuntimeModule &) -> bool;
    auto update(this RuntimeModule &, f64 delta_time) -> void;
    auto destroy(this RuntimeModule &) -> void;
};
