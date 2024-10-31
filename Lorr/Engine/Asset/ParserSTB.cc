#include "ParserSTB.hh"

#include "Engine/OS/OS.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace lr {

auto STBImageInfo::parse(const fs::path &path) -> ls::option<STBImageInfo> {
    ZoneScoped;

    File file(path, FileAccess::Read);
    if (!file) {
        return ls::nullopt;
    }

    auto file_data = file.whole_data();
    return STBImageInfo::parse({ file_data.get(), file.size });
}

auto STBImageInfo::parse(ls::span<u8> data) -> ls::option<STBImageInfo> {
    i32 width, height, channel_count;
    u8 *parsed_data = stbi_load_from_memory(data.data(), static_cast<i32>(data.size()), &width, &height, &channel_count, 4);
    if (!parsed_data) {
        return ls::nullopt;
    }

    u64 upload_size = width * height * 4;
    STBImageInfo image = {};
    image.format = vk::Format::R8G8B8A8_SRGB;
    image.extent = { static_cast<u32>(width), static_cast<u32>(height) };
    image.data.resize(upload_size);
    std::memcpy(&image.data[0], parsed_data, upload_size);

    stbi_image_free(parsed_data);

    return image;
}

}  // namespace lr
