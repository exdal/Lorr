#include "ParserSTB.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace lr {
auto STBImageInfo::parse(ls::span<u8> bytes) -> ls::option<STBImageInfo> {
    ZoneScoped;

    i32 width, height, channel_count;
    u8 *parsed_data = stbi_load_from_memory(bytes.data(), static_cast<i32>(bytes.size_bytes()), &width, &height, &channel_count, 4);
    if (!parsed_data) {
        return ls::nullopt;
    }

    u64 upload_size = width * height * 4;
    STBImageInfo image = {};
    image.format = vuk::Format::eR8G8B8A8Srgb;
    image.extent = { .width = static_cast<u32>(width), .height = static_cast<u32>(height), .depth = 1 };
    image.data.resize(upload_size);
    std::memcpy(&image.data[0], parsed_data, upload_size);

    stbi_image_free(parsed_data);

    return image;
}

auto STBImageInfo::parse_info(ls::span<u8> bytes) -> ls::option<STBImageInfo> {
    ZoneScoped;

    i32 width, height, channel_count;
    if (!stbi_info_from_memory(bytes.data(), static_cast<i32>(bytes.size_bytes()), &width, &height, &channel_count)) {
        return ls::nullopt;
    }

    STBImageInfo image = {};
    image.format = vuk::Format::eR8G8B8A8Srgb;
    image.extent = { .width = static_cast<u32>(width), .height = static_cast<u32>(height), .depth = 1 };

    return image;
}

}  // namespace lr
