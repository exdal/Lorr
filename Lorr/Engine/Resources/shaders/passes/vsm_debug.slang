import std;
import gpu;
import scene;
import vsm;

struct ShaderParameters {
    SamplerState sampler;
    Texture2DArray<u32> page_tables;
    Texture2D<u32> physical_pages;
};

#include <fullscreen.slang>

[[shader("fragment")]]
func fs_main(
    VertexOutput input,
    uniform ParameterBlock<ShaderParameters> params,
    uniform u32 clipmap_index,
    uniform u32 debug
) -> f32x4 {
    var color = f32x3(0.0);
    switch (debug) {
        case 0: {
            let page_data = params.page_tables.SampleLevel(params.sampler, f32x3(input.tex_coord, 0.0), 0.0);
            let page = VSMPageMetadata(page_data);
            if (page.is_visible()) {
                color.r = 1.0;
            }

            if (page.is_dirty()) {
                color.g = 1.0;
            }

            if (page.is_backed()) {
                color.b = 1.0;
            }
        } break;
        case 1: {
            let physical_page = params.physical_pages.SampleLevel(params.sampler, input.tex_coord, 0.0);
            color = asfloat(physical_page);
        } break;
        default:;
    }

    return f32x4(color, 1.0);
}