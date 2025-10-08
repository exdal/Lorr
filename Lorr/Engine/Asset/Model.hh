#pragma once

#include "Engine/Asset/UUID.hh"

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Scene/GPUScene.hh"

namespace lr {
enum class ModelID : u64 { Invalid = std::numeric_limits<u64>::max() };
struct Model {
    constexpr static auto MAX_MESHLET_INDICES = 64_sz;
    constexpr static auto MAX_MESHLET_PRIMITIVES = 64_sz;

    using Index = u32;

    struct MeshGroup {
        std::string name = {};
        std::vector<usize> child_indices = {};
        std::vector<usize> mesh_indices = {};
        glm::vec3 translation = {};
        glm::quat rotation = {};
        glm::vec3 scale = {};
    };

    std::vector<UUID> embedded_textures = {};
    std::vector<UUID> materials = {};
    std::vector<MeshGroup> mesh_groups = {};

    // initial material ids of meshes
    std::vector<UUID> initial_materials = {};
    std::vector<GPU::Mesh> gpu_meshes = {};
    std::vector<Buffer> gpu_mesh_buffers = {};
};
} // namespace lr
