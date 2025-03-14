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
static auto get_default_extensions() -> fastgltf::Extensions {
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

    return extensions;
}

static auto get_default_options() -> fastgltf::Options {
    auto options = fastgltf::Options::None;
    options |= fastgltf::Options::LoadExternalBuffers;
    // options |= fastgltf::Options::DontRequireValidAssetMember;

    return options;
}

static auto to_vuk_filter(fastgltf::Filter f) -> vuk::Filter {
    switch (f) {
        case fastgltf::Filter::Nearest:
            return vuk::Filter::eNearest;
        case fastgltf::Filter::Linear:
        default:
            return vuk::Filter::eLinear;
    }
}

static auto to_vuk_sampler_address_mode(fastgltf::Wrap w) -> vuk::SamplerAddressMode {
    switch (w) {
        case fastgltf::Wrap::ClampToEdge:
            return vuk::SamplerAddressMode::eClampToEdge;
        case fastgltf::Wrap::MirroredRepeat:
            return vuk::SamplerAddressMode::eMirroredRepeat;
        case fastgltf::Wrap::Repeat:
            return vuk::SamplerAddressMode::eRepeat;
    }
}

static auto to_asset_file_type(fastgltf::MimeType mime) -> AssetFileType {
    switch (mime) {
        case fastgltf::MimeType::JPEG:
            return AssetFileType::JPEG;
        case fastgltf::MimeType::PNG:
            return AssetFileType::PNG;
        default:
            return AssetFileType::None;
    }
}

auto GLTFModelInfo::parse(const fs::path &path, GLTFModelCallbacks callbacks) -> ls::option<GLTFModelInfo> {
    ZoneScoped;

    auto gltf_buffer = fastgltf::GltfDataBuffer::FromPath(path);
    auto gltf_type = fastgltf::determineGltfFileType(gltf_buffer.get());
    if (gltf_type == fastgltf::GltfType::Invalid) {
        LOG_ERROR("GLTF model type is invalid!");
        return ls::nullopt;
    }

    fastgltf::Parser parser(get_default_extensions());
    auto result = parser.loadGltf(gltf_buffer.get(), path.parent_path(), get_default_options());
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
        std::visit(
            fastgltf::visitor {
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
            v.data
        );
    }

    ///////////////////////////////////////////////
    // Samplers
    ///////////////////////////////////////////////

    for (const auto &v : asset.samplers) {
        auto &sampler = model.samplers.emplace_back();
        sampler.mag_filter = to_vuk_filter(v.magFilter.value_or(fastgltf::Filter::Linear));
        sampler.min_filter = to_vuk_filter(v.minFilter.value_or(fastgltf::Filter::Linear));
        sampler.address_u = to_vuk_sampler_address_mode(v.wrapS);
        sampler.address_v = to_vuk_sampler_address_mode(v.wrapT);
    }

    ///////////////////////////////////////////////
    // Images
    ///////////////////////////////////////////////

    for (const auto &v : asset.images) {
        std::visit(
            fastgltf::visitor {
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
            v.data
        );
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

    ///////////////////////////////////////////////
    // Geometry
    ///////////////////////////////////////////////

    u32 global_vertex_offset = 0;
    u32 global_index_offset = 0;
    for (const auto &node : asset.nodes) {
        if (!node.meshIndex.has_value()) {
            continue;
        }

        auto mesh_index = static_cast<u32>(node.meshIndex.value());
        auto &mesh = asset.meshes[mesh_index];

        u32 mesh_vertex_count = 0;
        u32 mesh_index_count = 0;
        for (const auto &primitive : mesh.primitives) {
            if (!primitive.materialIndex.has_value()) {
                continue;
            }

            auto &index_accessor = asset.accessors[primitive.indicesAccessor.value()];
            mesh_index_count += index_accessor.count;
            if (auto attrib = primitive.findAttribute("POSITION"); attrib != primitive.attributes.end()) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                mesh_vertex_count += accessor.count;
            }
        }

        glm::mat4 transform = {};
        if (auto *trs = std::get_if<fastgltf::TRS>(&node.transform)) {
            const auto t = glm::make_vec3(trs->translation.data());
            const auto r = glm::make_quat(trs->rotation.value_ptr());
            const auto s = glm::make_vec3(trs->scale.data());

            transform = glm::translate(glm::mat4(1.0f), t);
            transform *= glm::mat4(r);
            transform = glm::scale(transform, s);
        } else if (auto *mat = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
            transform = glm::make_mat4(mat->data());
        }

        if (callbacks.on_new_node) {
            auto name = std::string(node.name.begin(), node.name.end());
            auto child_node_indices = std::vector<u32>(node.children.begin(), node.children.end());
            ls::option<u32> mesh_index_opt = ls::nullopt;
            if (node.meshIndex.has_value()) {
                mesh_index_opt = static_cast<u32>(node.meshIndex.value());
            }

            callbacks.on_new_node(
                callbacks.user_data,
                mesh.primitives.size(),
                mesh_vertex_count,
                mesh_index_count,
                std::move(name),
                std::move(child_node_indices),
                std::move(mesh_index_opt),
                transform
            );
        }

        for (const auto &primitive : mesh.primitives) {
            if (!primitive.materialIndex.has_value()) {
                continue;
            }

            u32 primitive_vertex_count = 0;
            u32 primitive_index_count = 0;
            auto &index_accessor = asset.accessors[primitive.indicesAccessor.value()];
            primitive_index_count += index_accessor.count;

            if (callbacks.on_access_index) {
                fastgltf::iterateAccessorWithIndex<u32>(asset, index_accessor, [&](u32 index, usize i) { //
                    callbacks.on_access_index(callbacks.user_data, mesh_index, global_index_offset + i, index);
                });
            }

            if (auto attrib = primitive.findAttribute("POSITION"); attrib != primitive.attributes.end() && callbacks.on_access_position) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                primitive_vertex_count += accessor.count;

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 pos, usize i) { //
                    callbacks.on_access_position(callbacks.user_data, mesh_index, global_vertex_offset + i, pos);
                });
            }

            if (auto attrib = primitive.findAttribute("NORMAL"); attrib != primitive.attributes.end() && callbacks.on_access_normal) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, accessor, [&](glm::vec3 normal, usize i) { //
                    callbacks.on_access_normal(callbacks.user_data, mesh_index, global_vertex_offset + i, normal);
                });
            }

            if (auto attrib = primitive.findAttribute("TEXCOORD_0"); attrib != primitive.attributes.end() && callbacks.on_access_texcoord) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, accessor, [&](glm::vec2 uv, usize i) { //
                    callbacks.on_access_texcoord(callbacks.user_data, mesh_index, global_vertex_offset + i, uv);
                });
            }

            if (auto attrib = primitive.findAttribute("COLOR"); attrib != primitive.attributes.end() && callbacks.on_access_color) {
                auto &accessor = asset.accessors[attrib->accessorIndex];
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, accessor, [&](glm::vec4 color, usize i) { //
                    callbacks.on_access_color(callbacks.user_data, mesh_index, global_vertex_offset + i, color);
                });
            }

            if (callbacks.on_new_primitive) {
                callbacks.on_new_primitive( //
                    callbacks.user_data,
                    mesh_index,
                    primitive.materialIndex.value(),
                    global_vertex_offset,
                    primitive_vertex_count,
                    global_index_offset,
                    primitive_index_count
                );
            }

            global_vertex_offset += primitive_vertex_count;
            global_index_offset += primitive_index_count;
        }
    }

    return model;
}

auto GLTFModelInfo::parse_info(const fs::path &path) -> ls::option<GLTFModelInfo> {
    ZoneScoped;

    auto gltf_buffer = fastgltf::GltfDataBuffer::FromPath(path);
    auto gltf_type = fastgltf::determineGltfFileType(gltf_buffer.get());
    if (gltf_type == fastgltf::GltfType::Invalid) {
        LOG_ERROR("GLTF model type is invalid!");
        return ls::nullopt;
    }

    fastgltf::Parser parser(get_default_extensions());
    auto result = parser.loadGltf(gltf_buffer.get(), path.parent_path(), get_default_options());
    if (!result) {
        LOG_ERROR("Failed to load GLTF! {}", fastgltf::getErrorMessage(result.error()));
        return ls::nullopt;
    }

    fastgltf::Asset asset = std::move(result.get());
    GLTFModelInfo model = {};

    ///////////////////////////////////////////////
    // Images
    ///////////////////////////////////////////////

    for (const auto &v : asset.images) {
        std::visit(
            fastgltf::visitor {
                [](const auto &) {},
                [&](const fastgltf::sources::ByteView &view) {
                    // Embedded buffer
                    auto &image_info = model.images.emplace_back();
                    image_info.image_data.emplace<std::vector<u8>>();
                    image_info.file_type = to_asset_file_type(view.mimeType);
                },
                [&](const fastgltf::sources::BufferView &view) {
                    // Embedded buffer
                    auto &image_info = model.images.emplace_back();
                    image_info.image_data.emplace<std::vector<u8>>();
                    image_info.file_type = to_asset_file_type(view.mimeType);
                },
                [&](const fastgltf::sources::Array &arr) {
                    // Embedded array
                    auto &image_info = model.images.emplace_back();
                    image_info.image_data.emplace<std::vector<u8>>();
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
            v.data
        );
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

    // We currently need only these, their data doesn't matter
    // for now. Can change it in future.

    return model;
}

} // namespace lr
