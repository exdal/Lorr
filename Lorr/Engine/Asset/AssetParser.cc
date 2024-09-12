#include "AssetParser.hh"

#include <glm/packing.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

template<>
struct fastgltf::ElementTraits<glm::vec4> : fastgltf::ElementTraitsBase<glm::vec4, AccessorType::Vec4, float> {};
template<>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, float> {};
template<>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<glm::vec2, AccessorType::Vec2, float> {};

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace lr {
std::unique_ptr<ImageAssetData> AssetParser::STB(u8 *data, usize data_size) {
    ZoneScoped;

    i32 width, height, channel_count;
    u8 *parsed_data = stbi_load_from_memory(data, static_cast<i32>(data_size), &width, &height, &channel_count, 4);
    if (!parsed_data) {
        return nullptr;
    }

    auto result = std::make_unique<ImageAssetData>();
    result->width = static_cast<u32>(width);
    result->height = static_cast<u32>(height);
    result->format = lr::Format::R8G8B8A8_UNORM;
    result->data = parsed_data;
    result->data_size = width * height * channel_count;
    result->deleter = [](u8 *v) { stbi_image_free(v); };

    return result;
}

std::unique_ptr<ModelAssetData> AssetParser::GLTF(const fs::path &path) {
    ZoneScoped;

    std::string path_str = path.string();
    LR_LOG_TRACE("Attempting to load GLTF file {}", path_str);

    auto gltf_buffer = fastgltf::GltfDataBuffer::FromPath(path);
    auto gltf_type = fastgltf::determineGltfFileType(gltf_buffer.get());
    if (gltf_type == fastgltf::GltfType::Invalid) {
        LR_LOG_ERROR("GLTF model type is invalid!");
        return nullptr;
    }

    auto extensions = fastgltf::Extensions::None;
    extensions |= fastgltf::Extensions::KHR_mesh_quantization;
    extensions |= fastgltf::Extensions::KHR_texture_transform;
    extensions |= fastgltf::Extensions::KHR_texture_basisu;
    extensions |= fastgltf::Extensions::KHR_lights_punctual;
    extensions |= fastgltf::Extensions::KHR_materials_specular;
    extensions |= fastgltf::Extensions::KHR_materials_ior;
    extensions |= fastgltf::Extensions::KHR_materials_iridescence;
    extensions |= fastgltf::Extensions::KHR_materials_volume;
    extensions |= fastgltf::Extensions::KHR_materials_transmission;
    extensions |= fastgltf::Extensions::KHR_materials_clearcoat;
    extensions |= fastgltf::Extensions::KHR_materials_emissive_strength;
    extensions |= fastgltf::Extensions::KHR_materials_sheen;
    extensions |= fastgltf::Extensions::KHR_materials_unlit;
    extensions |= fastgltf::Extensions::KHR_materials_anisotropy;
    extensions |= fastgltf::Extensions::EXT_meshopt_compression;
    extensions |= fastgltf::Extensions::EXT_texture_webp;
    extensions |= fastgltf::Extensions::MSFT_texture_dds;

    LR_LOG_TRACE("GLTF parsing start.");
    fastgltf::Parser parser(extensions);

    auto options = fastgltf::Options::None;
    options |= fastgltf::Options::DontRequireValidAssetMember;
    options |= fastgltf::Options::LoadExternalBuffers;
    options |= fastgltf::Options::LoadExternalImages;

    auto result = parser.loadGltf(gltf_buffer.get(), path.parent_path(), options);
    if (!result) {
        LR_LOG_ERROR("Failed to load GLTF! {}", fastgltf::getErrorMessage(result.error()));
        return nullptr;
    }

    fastgltf::Asset asset = std::move(result.get());
    auto model = std::make_unique<ModelAssetData>();

    ///////////////////////////////////////////////
    // Buffers
    ///////////////////////////////////////////////

    std::vector<ls::span<u8>> buffers = {};

    LR_LOG_TRACE("Parsing GLTF buffers...");

    // sources::Vector is not used for importing, ignore it
    for (const auto &v : asset.buffers) {
        std::visit(
            fastgltf::visitor{
                [](const auto &) {},
                [&](const fastgltf::sources::ByteView &view) {
                    // Embedded byte
                    buffers.emplace_back(ls::bit_cast<u8 *>(view.bytes.data()), view.bytes.size_bytes());
                },
                [&](const fastgltf::sources::Array &arr) {
                    // Embedded array
                    buffers.emplace_back(ls::bit_cast<u8 *>(arr.bytes.data()), arr.bytes.size_bytes());
                },
            },
            v.data);
    }

    LR_LOG_TRACE("{} total buffers.", buffers.size());

    ///////////////////////////////////////////////
    // Samplers
    ///////////////////////////////////////////////

    LR_LOG_TRACE("Parsing GLTF samplers...");

    auto gltf_filter_to_lr = [](fastgltf::Filter f) -> Filtering {
        switch (f) {
            case fastgltf::Filter::Nearest:
                return Filtering::Nearest;
            case fastgltf::Filter::Linear:
            default:
                return Filtering::Linear;
        }
    };

    auto gltf_address_mode_to_lr = [](fastgltf::Wrap w) -> TextureAddressMode {
        switch (w) {
            case fastgltf::Wrap::ClampToEdge:
                return TextureAddressMode::ClampToEdge;
            case fastgltf::Wrap::MirroredRepeat:
                return TextureAddressMode::MirroredRepeat;
            case fastgltf::Wrap::Repeat:
                return TextureAddressMode::Repeat;
        }
    };

    for (const auto &v : asset.samplers) {
        auto &sampler = model->samplers.emplace_back();
        sampler.mag_filter = gltf_filter_to_lr(v.magFilter.value_or(fastgltf::Filter::Linear));
        sampler.min_filter = gltf_filter_to_lr(v.minFilter.value_or(fastgltf::Filter::Linear));
        sampler.address_u = gltf_address_mode_to_lr(v.wrapS);
        sampler.address_v = gltf_address_mode_to_lr(v.wrapT);
    }

    LR_LOG_TRACE("{} total samplers.", model->samplers.size());

    ///////////////////////////////////////////////
    // Textures
    ///////////////////////////////////////////////

    auto mime_type_to_asset_type = [](fastgltf::MimeType m) -> AssetFileType {
        switch (m) {
            case fastgltf::MimeType::JPEG:
                return AssetFileType::JPEG;
            case fastgltf::MimeType::PNG:
                return AssetFileType::PNG;
            case fastgltf::MimeType::KTX2:
            case fastgltf::MimeType::DDS:
            case fastgltf::MimeType::None:
            case fastgltf::MimeType::GltfBuffer:
            case fastgltf::MimeType::OctetStream:
                return AssetFileType::None;
        }
    };

    LR_LOG_TRACE("Parsing GLTF textures...");

    for (const auto &v : asset.textures) {
        auto &texture = model->textures.emplace_back();
        auto &image = asset.images[v.imageIndex.value()];

        texture.name = v.name;
        if (auto sampler_index = v.samplerIndex; sampler_index.has_value()) {
            texture.sampler_index = sampler_index.value();
        }

        std::visit(
            fastgltf::visitor{
                [](const auto &) {},
                [&](const fastgltf::sources::ByteView &view) {
                    // Embedded buffer
                    auto sp = std::span(ls::bit_cast<u8 *>(view.bytes.data()), view.bytes.size_bytes());

                    texture.file_type = mime_type_to_asset_type(view.mimeType);
                    texture.data.insert(texture.data.begin(), sp.begin(), sp.end());
                },
                [&](const fastgltf::sources::BufferView &view) {
                    // Embedded buffer
                    auto &buffer_view = asset.bufferViews[view.bufferViewIndex];
                    auto &buffer = buffers[buffer_view.bufferIndex];
                    auto sp = std::span(ls::bit_cast<u8 *>(buffer.data() + buffer_view.byteOffset), buffer_view.byteLength);

                    texture.file_type = mime_type_to_asset_type(view.mimeType);
                    texture.data.insert(texture.data.begin(), sp.begin(), sp.end());
                },
                [&](const fastgltf::sources::Array &arr) {
                    // Embedded array
                    auto sp = std::span(ls::bit_cast<u8 *>(arr.bytes.data()), arr.bytes.size_bytes());

                    texture.file_type = mime_type_to_asset_type(arr.mimeType);
                    texture.data.insert(texture.data.begin(), sp.begin(), sp.end());
                },
            },
            image.data);
    }

    LR_LOG_TRACE("{} total textures.", model->textures.size());

    ///////////////////////////////////////////////
    // Materials
    ///////////////////////////////////////////////

    LR_LOG_TRACE("Parsing GLTF materials...");

    for (const auto &v : asset.materials) {
        auto &material = model->materials.emplace_back();
        auto &pbr = v.pbrData;

        material.albedo_color = {
            pbr.baseColorFactor[0],
            pbr.baseColorFactor[1],
            pbr.baseColorFactor[2],
            pbr.baseColorFactor[3],
        };
        material.metallic_factor = pbr.metallicFactor;
        material.roughness_factor = pbr.roughnessFactor;
        material.emissive_color = {
            v.emissiveFactor[0],
            v.emissiveFactor[1],
            v.emissiveFactor[2],
            v.emissiveStrength,
        };
        material.alpha_mode = static_cast<AlphaMode>(v.alphaMode);
        material.alpha_cutoff = v.alphaCutoff;

        if (auto &tex = pbr.baseColorTexture; tex.has_value()) {
            auto &texture = asset.textures[tex->textureIndex];
            material.albedo_image_data_index = texture.imageIndex.value();
        }

        if (auto &tex = v.normalTexture; tex.has_value()) {
            auto &texture = asset.textures[tex->textureIndex];
            material.normal_image_data_index = texture.imageIndex.value();
        }

        if (auto &tex = v.emissiveTexture; tex.has_value()) {
            auto &texture = asset.textures[tex->textureIndex];
            material.emissive_image_data_index = texture.imageIndex.value();
        }
    }

    LR_LOG_TRACE("{} total materials.", model->materials.size());

    LR_LOG_TRACE("Parsing GLTF meshes...");

    for (const auto &v : asset.meshes) {
        auto &mesh = model->meshes.emplace_back();
        mesh.name = v.name;

        for (const auto &k : v.primitives) {
            mesh.primitive_indices.push_back(model->primitives.size());

            auto &primitive = model->primitives.emplace_back();
            usize vertex_offset = model->vertices.size();
            usize index_offset = model->indices.size();

            primitive.vertex_offset = vertex_offset;
            primitive.index_offset = index_offset;

            // clang-format off
            auto &index_accessor = asset.accessors[k.indicesAccessor.value()];
            primitive.index_count = index_accessor.count;
            model->indices.resize(model->indices.size() + index_accessor.count);
            fastgltf::iterateAccessorWithIndex<u32>(asset, index_accessor,
                [&](u32 index, usize i) { 
                    model->indices[index_offset + i] = index;
                });

            if (auto attrib = k.findAttribute("POSITION");
                attrib != k.attributes.end()
            ) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                primitive.vertex_count = accessor.count;
                model->vertices.resize(model->vertices.size() + accessor.count);
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor,
                    [&](glm::vec3 pos, usize i) {
                        model->vertices[vertex_offset + i].position = pos;
                    });
            }

            if (auto attrib = k.findAttribute("NORMAL");
                attrib != k.attributes.end()
            ) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor,
                    [&](glm::vec3 normal, usize i) {
                        model->vertices[vertex_offset + i].normal = normal;
                    });
            }

            if (auto attrib = k.findAttribute("TEXCOORD_0");
                attrib != k.attributes.end()
            ) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor,
                    [&](glm::vec2 uv, usize i) {
                        model->vertices[vertex_offset + i].uv_x = uv.x;
                        model->vertices[vertex_offset + i].uv_y = uv.y;
                    });
            }

            if (auto attrib = k.findAttribute("COLOR");
                attrib != k.attributes.end()
            ) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, accessor,
                    [&](glm::vec4 color, usize i) {
                        model->vertices[vertex_offset + i].color = glm::packUnorm4x8(color);
                    });
            }
            // clang-format on

            if (k.materialIndex.has_value()) {
                primitive.material_index = k.materialIndex.value();
            }
        }
    }

    LR_LOG_TRACE("{} total meshes, {} total primitives.", model->meshes.size(), model->primitives.size());

    LR_LOG_TRACE("GLTF parsing end.");

    return model;
}

}  // namespace lr
