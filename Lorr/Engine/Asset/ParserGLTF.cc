#include "Engine/Asset/ParserGLTF.hh"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

template<>
struct fastgltf::ElementTraits<glm::vec4> : fastgltf::ElementTraitsBase<glm::vec4, AccessorType::Vec4, float> {};
template<>
struct fastgltf::ElementTraits<glm::vec3> : fastgltf::ElementTraitsBase<glm::vec3, AccessorType::Vec3, float> {};
template<>
struct fastgltf::ElementTraits<glm::vec2> : fastgltf::ElementTraitsBase<glm::vec2, AccessorType::Vec2, float> {};

namespace lr {
auto GLTFModelInfo::parse(const fs::path &path, bool load_resources, GLTFModelCallbacks callbacks) -> ls::option<GLTFModelInfo> {
    ZoneScoped;

    auto gltf_buffer = fastgltf::GltfDataBuffer::FromPath(path);
    auto gltf_type = fastgltf::determineGltfFileType(gltf_buffer.get());
    if (gltf_type == fastgltf::GltfType::Invalid) {
        LOG_ERROR("GLTF model type is invalid!");
        return ls::nullopt;
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

    fastgltf::Parser parser;

    auto options = fastgltf::Options::None;
    // options |= fastgltf::Options::DontRequireValidAssetMember;

    if (load_resources) {
        options |= fastgltf::Options::LoadExternalBuffers;
    }

    auto result = parser.loadGltf(gltf_buffer.get(), path.parent_path(), options);
    if (!result) {
        LOG_ERROR("Failed to load GLTF! {}", fastgltf::getErrorMessage(result.error()));
        return ls::nullopt;
    }

    fastgltf::Asset asset = std::move(result.get());
    GLTFModelInfo model = {};

    ///////////////////////////////////////////////
    // Buffers
    ///////////////////////////////////////////////

    std::vector<ls::span<u8>> buffers = {};

    // sources::Vector is not used for importing, ignore it
    for (const auto &v : asset.buffers) {
        if (!load_resources) {
            buffers.emplace_back();
            continue;
        }

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

    ///////////////////////////////////////////////
    // Samplers
    ///////////////////////////////////////////////

    auto gltf_filter_to_lr = [](fastgltf::Filter f) -> vuk::Filter {
        switch (f) {
            case fastgltf::Filter::Nearest:
                return vuk::Filter::eNearest;
            case fastgltf::Filter::Linear:
            default:
                return vuk::Filter::eLinear;
        }
    };

    auto gltf_address_mode_to_lr = [](fastgltf::Wrap w) -> vuk::SamplerAddressMode {
        switch (w) {
            case fastgltf::Wrap::ClampToEdge:
                return vuk::SamplerAddressMode::eClampToEdge;
            case fastgltf::Wrap::MirroredRepeat:
                return vuk::SamplerAddressMode::eMirroredRepeat;
            case fastgltf::Wrap::Repeat:
                return vuk::SamplerAddressMode::eRepeat;
        }
    };

    for (const auto &v : asset.samplers) {
        auto &sampler = model.samplers.emplace_back();
        sampler.mag_filter = gltf_filter_to_lr(v.magFilter.value_or(fastgltf::Filter::Linear));
        sampler.min_filter = gltf_filter_to_lr(v.minFilter.value_or(fastgltf::Filter::Linear));
        sampler.address_u = gltf_address_mode_to_lr(v.wrapS);
        sampler.address_v = gltf_address_mode_to_lr(v.wrapT);
    }

    ///////////////////////////////////////////////
    // Images
    ///////////////////////////////////////////////

    auto to_asset_file_type = [](fastgltf::MimeType mime) -> AssetFileType {
        switch (mime) {
            case fastgltf::MimeType::JPEG:
                return AssetFileType::JPEG;
            case fastgltf::MimeType::PNG:
                return AssetFileType::PNG;
            default:
                return AssetFileType::None;
        }
    };

    for (const auto &v : asset.images) {
        std::visit(
            fastgltf::visitor{
                [](const auto &) {},
                [&](const fastgltf::sources::ByteView &view) {
                    // Embedded buffer
                    std::vector<u8> pixels(view.bytes.size_bytes());
                    std::memcpy(pixels.data(), view.bytes.data(), view.bytes.size_bytes());

                    auto &image_info = model.images.emplace_back();
                    image_info.image_data.emplace<std::vector<u8>>(std::move(pixels));
                    image_info.file_type = to_asset_file_type(view.mimeType);
                },
                [&](const fastgltf::sources::BufferView &view) {
                    // Embedded buffer
                    auto &buffer_view = asset.bufferViews[view.bufferViewIndex];
                    auto &buffer = buffers[buffer_view.bufferIndex];

                    std::vector<u8> pixels(buffer_view.byteLength);
                    std::memcpy(pixels.data(), buffer.data() + buffer_view.byteOffset, buffer_view.byteLength);

                    auto &image_info = model.images.emplace_back();
                    image_info.image_data.emplace<std::vector<u8>>(std::move(pixels));
                    image_info.file_type = to_asset_file_type(view.mimeType);
                },
                [&](const fastgltf::sources::Array &arr) {
                    // Embedded array
                    std::vector<u8> pixels(arr.bytes.size_bytes());
                    std::memcpy(pixels.data(), arr.bytes.data(), arr.bytes.size_bytes());

                    auto &image_info = model.images.emplace_back();
                    image_info.image_data.emplace<std::vector<u8>>(std::move(pixels));
                    image_info.file_type = to_asset_file_type(arr.mimeType);
                },
                [&](const fastgltf::sources::URI &uri) {
                    // External file
                    const auto &image_file_path = path.parent_path() / uri.uri.fspath();

                    auto &image_info = model.images.emplace_back();
                    image_info.image_data.emplace<fs::path>(image_file_path);
                    image_info.file_type = to_asset_file_type(uri.mimeType);
                },
            },
            v.data);
    }

    ///////////////////////////////////////////////
    // Textures
    ///////////////////////////////////////////////

    for (const auto &v : asset.textures) {
        auto &texture = model.textures.emplace_back();
        if (v.samplerIndex.has_value()) {
            texture.sampler_index = v.samplerIndex.value();
        }
        if (v.imageIndex.has_value()) {
            texture.image_index = v.imageIndex.value();
        }
    }

    ///////////////////////////////////////////////
    // Materials
    ///////////////////////////////////////////////

    for (const auto &v : asset.materials) {
        auto &material = model.materials.emplace_back();
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
        // material.alpha_mode = static_cast<vk::AlphaMode>(v.alphaMode);
        material.alpha_cutoff = v.alphaCutoff;

        if (auto &tex = pbr.baseColorTexture; tex.has_value()) {
            material.albedo_texture_index = tex->textureIndex;
        }

        if (auto &tex = v.normalTexture; tex.has_value()) {
            material.normal_texture_index = tex->textureIndex;
        }

        if (auto &tex = v.emissiveTexture; tex.has_value()) {
            material.emissive_texture_index = tex->textureIndex;
        }
    }

    u32 total_vertex_count = 0;
    u32 total_index_count = 0;
    for (const auto &v : asset.meshes) {
        for (const auto &k : v.primitives) {
            auto &index_accessor = asset.accessors[k.indicesAccessor.value()];
            if (auto attrib = k.findAttribute("POSITION"); attrib != k.attributes.end()) {
                auto &vertex_accessor = asset.accessors[attrib->accessorIndex];
                total_vertex_count += vertex_accessor.count;
            }

            total_index_count += index_accessor.count;
        }
    }

    if (callbacks.on_buffer_sizes) {
        callbacks.on_buffer_sizes(callbacks.user_data, total_vertex_count, total_index_count);
    }

    if (load_resources) {
        u32 vertex_offset = 0;
        u32 index_offset = 0;
        for (const auto &v : asset.meshes) {
            auto &mesh = model.meshes.emplace_back();
            mesh.name = v.name;

            for (const auto &k : v.primitives) {
                mesh.primitive_indices.push_back(model.primitives.size());
                auto &primitive = model.primitives.emplace_back();
                auto &index_accessor = asset.accessors[k.indicesAccessor.value()];

                primitive.vertex_offset = vertex_offset;
                primitive.index_offset = index_offset;
                primitive.index_count = index_accessor.count;

                fastgltf::iterateAccessorWithIndex<u32>(asset, index_accessor, [&](u32 index, usize i) {  //
                    LS_EXPECT(callbacks.on_access_index);
                    callbacks.on_access_index(callbacks.user_data, index_offset + i, index);
                });

                if (auto attrib = k.findAttribute("POSITION"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    primitive.vertex_count = accessor.count;

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 pos, usize i) {  //
                        if (callbacks.on_access_position) {
                            callbacks.on_access_position(callbacks.user_data, vertex_offset + i, pos);
                        }
                    });
                }

                if (auto attrib = k.findAttribute("NORMAL"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 normal, usize i) {  //
                        if (callbacks.on_access_normal) {
                            callbacks.on_access_normal(callbacks.user_data, vertex_offset + i, normal);
                        }
                    });
                }

                if (auto attrib = k.findAttribute("TEXCOORD_0"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor, [&](glm::vec2 uv, usize i) {  //
                        if (callbacks.on_access_texcoord) {
                            callbacks.on_access_texcoord(callbacks.user_data, vertex_offset + i, uv);
                        }
                    });
                }

                if (auto attrib = k.findAttribute("COLOR"); attrib != k.attributes.end()) {
                    auto &accessor = asset.accessors[attrib->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, accessor, [&](glm::vec4 color, usize i) {  //
                        if (callbacks.on_access_color) {
                            callbacks.on_access_color(callbacks.user_data, vertex_offset + i, color);
                        }
                    });
                }

                if (k.materialIndex.has_value()) {
                    primitive.material_index = k.materialIndex.value();
                }

                vertex_offset += primitive.vertex_count;
                index_offset += primitive.index_count;
            }
        }
    }

    return model;
}

}  // namespace lr
