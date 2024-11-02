#include "Engine/OS/File.hh"

#include "Engine/OS/OS.hh"

namespace lr {
template<>
struct Handle<DirectoryWatcher>::Impl {
    FileDescriptor file_descriptor = FileDescriptor::Invalid;
};

auto DirectoryWatcher::create(const fs::path &directory) -> ls::option<DirectoryWatcher> {
    ZoneScoped;

    auto impl = new Impl;
    impl->file_descriptor = os_file_watcher_init();

    return DirectoryWatcher(impl);
}

auto DirectoryWatcher::destroy() -> void {
    ZoneScoped;
}

}  // namespace lr
