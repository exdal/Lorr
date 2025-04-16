#include "Engine/Asset/ParserKTX2.hh"

#include <ktx.h>

namespace lr {
auto KTX2ImageInfo::parse(ls::span<u8> bytes) -> ls::option<KTX2ImageInfo> {
    ZoneScoped;

    ktxTexture2 *texture = nullptr;
    LS_DEFER(&) {
        if (texture) {
            ktxTexture_Destroy(ktxTexture(texture));
        }
    };

    auto result = ktxTexture2_CreateFromMemory(bytes.data(), bytes.size_bytes(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
    if (result != KTX_SUCCESS) {
        LOG_ERROR("Failed to parse KTX2 file, error code: {}.", static_cast<u32>(result));
        return ls::nullopt;
    }

    auto format = vuk::Format::eUndefined;
    if (ktxTexture2_NeedsTranscoding(texture)) {
        ktxTexture2_TranscodeBasis(texture, KTX_TTF_BC7_RGBA, KTX_TF_HIGH_QUALITY);
        format = vuk::Format::eBc7UnormBlock;
    } else {
        format = static_cast<vuk::Format>(texture->vkFormat);
    }

    auto level_count = texture->numLevels;

    auto info = KTX2ImageInfo{};
    info.format = format;
    info.base_extent = { .width = texture->baseWidth, .height = texture->baseHeight, .depth = texture->baseDepth };
    info.mip_level_count = level_count;
    info.per_level_offsets.resize(level_count);

    for (u32 level = 0; level < level_count; level++) {
        u64 offset = 0;
        auto offset_result = ktxTexture_GetImageOffset(ktxTexture(texture), level, 0, 0, &offset);
        if (offset_result != KTX_SUCCESS) {
            LOG_ERROR("Failed to get KTX2 offset.");
            return ls::nullopt;
        }

        auto *image_data = ktxTexture_GetData(ktxTexture(texture)) + offset;
        auto image_size = ktxTexture_GetImageSize(ktxTexture(texture), level);

        auto output_offset = static_cast<usize>(info.data.size());
        info.per_level_offsets[level] = output_offset;
        info.data.resize(info.data.size() + image_size);
        std::memcpy(info.data.data() + output_offset, image_data, image_size);
    }

    return info;
}

auto KTX2ImageInfo::parse_info(ls::span<u8> bytes) -> ls::option<KTX2ImageInfo> {
    ZoneScoped;

    ktxTexture2 *texture = nullptr;
    LS_DEFER(&) {
        if (texture) {
            ktxTexture_Destroy(ktxTexture(texture));
        }
    };

    auto result = ktxTexture2_CreateFromMemory(bytes.data(), bytes.size_bytes(), 0, &texture);
    if (result != KTX_SUCCESS) {
        LOG_ERROR("Failed to parse KTX2 file, error code: {}.", static_cast<u32>(result));
        return ls::nullopt;
    }

    auto format = vuk::Format::eUndefined;
    if (ktxTexture2_NeedsTranscoding(texture)) {
        ktxTexture2_TranscodeBasis(texture, KTX_TTF_BC7_RGBA, KTX_TF_HIGH_QUALITY);
        format = vuk::Format::eBc7UnormBlock;
    } else {
        format = static_cast<vuk::Format>(texture->vkFormat);
    }

    auto info = KTX2ImageInfo{};
    info.format = format;
    info.base_extent = { .width = texture->baseWidth, .height = texture->baseHeight, .depth = texture->baseDepth };
    info.mip_level_count = texture->numLevels;

    return info;
}

auto KTX2ImageInfo::encode(ls::span<u8> raw_pixels, vuk::Format format, vuk::Extent3D extent, u32 level_count, bool normal) -> std::vector<u8> {
    ZoneScoped;

    ktxTextureCreateInfo create_info = {
        .glInternalformat = 0,
        .vkFormat = static_cast<u32>(format),
        .pDfd = nullptr,
        .baseWidth = extent.width,
        .baseHeight = extent.height,
        .baseDepth = extent.depth,
        .numDimensions = static_cast<u32>(extent.depth > 1 ? 3 : 2),
        .numLevels = level_count,
        .numLayers = 1,
        .numFaces = 1,
        .isArray = KTX_FALSE,
        .generateMipmaps = KTX_FALSE,
    };

    ktxTexture2 *texture = nullptr;
    LS_DEFER(&) {
        if (texture) {
            ktxTexture_Destroy(ktxTexture(texture));
        }
    };

    auto result = ktxTexture2_Create(&create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    if (result != KTX_SUCCESS) {
        LOG_ERROR("Cannot create KTX2 texture! {}", static_cast<u32>(result));
        return {};
    }

    ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, 0, 0, raw_pixels.data(), raw_pixels.size_bytes());

    ktxBasisParams params = {};
    params.structSize = sizeof(ktxBasisParams);
    params.uastc = KTX_FALSE;
    params.verbose = KTX_FALSE;
    params.noSSE = KTX_FALSE;
    params.threadCount = 1;
    params.compressionLevel = 3;
    params.qualityLevel = 127;
    params.normalMap = normal;
    result = ktxTexture2_CompressBasisEx(texture, &params);
    if (result != KTX_SUCCESS) {
        LOG_ERROR("Cannot compress texture! {}", static_cast<u32>(result));
        return {};
    }

    auto output_pixels = std::vector<u8>(texture->dataSize);
    auto *output_pixels_data = output_pixels.data();
    u64 written_bytes = 0;
    result = ktxTexture_WriteToMemory(ktxTexture(texture), &output_pixels_data, &written_bytes);
    if (result != KTX_SUCCESS) {
        LOG_ERROR("Cannot write compressed texture into memory! {}", static_cast<u32>(result));
        return {};
    }

    return output_pixels;
}

} // namespace lr
