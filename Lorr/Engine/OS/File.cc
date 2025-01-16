#include "Engine/OS/File.hh"

namespace lr {
File::File(const fs::path &path, FileAccess access) {
    auto file_handle = os::file_open(path, access);
    if (!file_handle.has_value()) {
        this->result = file_handle.error();
        return;
    }

    this->handle = file_handle.value();
    this->size = os::file_size(this->handle.value()).value_or(0);
}

auto File::write(const void *data, usize data_size) -> u64 {
    ZoneScoped;

    return os::file_write(this->handle.value(), data, data_size);
}

auto File::read(void *data, usize data_size) -> u64 {
    ZoneScoped;

    return os::file_read(this->handle.value(), data, data_size);
}

auto File::seek(i64 offset) -> void {
    ZoneScoped;

    os::file_seek(this->handle.value(), offset);
}

auto File::close() -> void {
    ZoneScoped;

    if (this->handle.has_value()) {
        os::file_close(this->handle.value());
        this->handle.reset();
    }
}

auto File::to_bytes(const fs::path &path) -> std::vector<u8> {
    ZoneScoped;

    File file(path, FileAccess::Read);
    if (!file) {
        return {};
    }

    auto contents = std::vector<u8>();
    // intentionally reserve+resize to insert null termination
    contents.reserve(file.size + 1);
    contents.resize(file.size);

    file.read(contents.data(), file.size);

    return contents;
}

auto File::to_string(const fs::path &path) -> std::string {
    ZoneScoped;

    File file(path, FileAccess::Read);
    if (!file) {
        return {};
    }

    std::string str;
    str.resize(file.size);
    file.read(str.data(), file.size);

    return str;
}

auto File::to_stdout(std::string_view str) -> void {
    ZoneScoped;

    os::file_stdout(str);
}

auto File::to_stderr(std::string_view str) -> void {
    ZoneScoped;

    os::file_stderr(str);
}

auto File::open_dialog(std::string_view title, FileDialogFlag flags) -> ls::option<fs::path> {
    ZoneScoped;

    return os::file_dialog(title, flags);
}

FileWatcher::FileWatcher() {
    ZoneScoped;

    this->listener = os::file_watcher_init();
    this->event_storage.resize(os::file_watcher_buffer_size());
}

auto FileWatcher::watch_dir(const fs::path &path) -> FileDescriptor {
    ZoneScoped;

    auto watching_dir = os::file_watcher_add(this->listener, path);
    if (!watching_dir.has_value()) {
        LOG_ERROR("Cannot watch '{}'.", path);
        return FileDescriptor::Invalid;
    }

    return this->watching_dirs.emplace(watching_dir.value(), path).first->first;
}

auto FileWatcher::remove_dir(FileDescriptor watch_descriptor) -> void {
    ZoneScoped;

    os::file_watcher_remove(this->listener, watch_descriptor);
    this->watching_dirs.erase(watch_descriptor);
}

auto FileWatcher::peek() -> ls::option<FileEvent> {
    ZoneScoped;

    if (this->avail_storage_size == 0) {
        this->avail_storage_size =
            os::file_watcher_read(this->listener, this->event_storage.data(), this->event_storage.size()).value_or(0);
    }

    if (this->storage_offset < this->avail_storage_size) {
        return os::file_watcher_peek(this->event_storage.data(), this->storage_offset);
    }

    this->avail_storage_size = 0;
    this->storage_offset = 0;

    return ls::nullopt;
}

auto FileWatcher::get_path(FileDescriptor descriptor) -> const fs::path & {
    ZoneScoped;

    return this->watching_dirs[descriptor];
}

auto FileWatcher::close() -> void {
    ZoneScoped;

    for (const auto &v : this->watching_dirs) {
        os::file_watcher_remove(this->listener, v.first);
    }
}

}  // namespace lr
