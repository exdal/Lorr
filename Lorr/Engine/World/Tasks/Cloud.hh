#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

namespace lr::Tasks {
inline ComputePipelineInfo cloud_noise_pipeline_info(AssetManager &asset_man) {
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);
    return { .shader_module_info = {
                 .module_name = "cloud.detail_noise",
                 .root_path = shaders_root,
                 .shader_path = shaders_root / "cloud" / "detail_noise.slang",
                 .entry_points = { "cs_main" },
             } };
}
}  // namespace lr::Tasks
