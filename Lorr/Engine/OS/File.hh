#pragma once

#include "Engine/Core/Handle.hh"

namespace lr {
struct DirectoryWatcher : Handle<DirectoryWatcher> {
    static auto create(const fs::path &directory) -> ls::option<DirectoryWatcher>;
    auto destroy() -> void;
};
}  // namespace lr
