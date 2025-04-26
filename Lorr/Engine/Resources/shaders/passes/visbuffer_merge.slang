module visbuffer_merge;

import std;
import gpu;
import scene;
import passes.visbuffer;

#include <fullscreen.slang>

[[vk::binding(0, 0)]]
StorageImage2D<u64, ImageFormat::R64UI> visbuffer_image;

[[vk::binding(1, 0)]]
StorageImage2D<u32, ImageFormat::R32UI> visbuffer_data_image;

[[shader("fragment")]]
func fs_main(VertexOutput input) -> FragmentOutput {
    const u32 texel = visbuffer.load(u32x2(input.position.xy));
    if (texel == ~0u) {
        discard;
    }
}
