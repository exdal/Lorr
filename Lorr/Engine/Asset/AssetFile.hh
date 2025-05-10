#pragma once

#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
enum class AssetType : u32 {
    None = 0,
    Root = 0,
    Shader,
    Model,
    Texture,
    Material,
    Font,
    Scene,
    Directory, // Internal use
};

// List of file extensions supported by Engine.
enum class AssetFileType : u32 {
    None = 0,
    Binary,
    Meta,
    GLB,
    GLTF,
    PNG,
    JPEG,
    JSON,
    KTX2,
};

enum class AssetFileFlags : u64 {
    None = 0,
};
consteval void enable_bitmask(AssetFileFlags);

struct TextureAssetFileHeader {
    vuk::Extent3D extent = {};
    vuk::Format format = vuk::Format::eUndefined;
};

struct AssetFileHeader {
    c8 magic[4] = { 'L', 'O', 'R', 'R' };
    u16 version = 1;
    AssetFileFlags flags = AssetFileFlags::None;
    AssetType type = AssetType::None;
    union {
        TextureAssetFileHeader texture_header = {};
    };
};

} // namespace lr
