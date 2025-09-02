import gpu;

#include <fullscreen.slang>

struct ShaderParameters {
    Image2D<u32> overdraw;
};

func inferno(f32 t) -> f32x3 {
    let c0 = f32x3(0.0002189403691192265, 0.001651004631001012, -0.01948089843709184);
    let c1 = f32x3(0.1065134194856116, 0.5639564367884091, 3.932712388889277);
    let c2 = f32x3(11.60249308247187, -3.972853965665698, -15.9423941062914);
    let c3 = f32x3(-41.70399613139459, 17.43639888205313, 44.35414519872813);
    let c4 = f32x3(77.162935699427, -33.40235894210092, -81.80730925738993);
    let c5 = f32x3(-71.31942824499214, 32.62606426397723, 73.20951985803202);
    let c6 = f32x3(25.13112622477341, -12.24266895238567, -23.07032500287172);

    t = saturate(t);
    return c0 + t * (c1 + t * (c2 + t * (c3 + t * (c4 + t * (c5 + t * c6)))));
}

[[shader("fragment")]]
func fs_main(
    VertexOutput input,
    uniform ShaderParameters params,
    uniform f32 heatmap_scale
) -> f32x4 {
    let draw_scale = clamp(heatmap_scale, 0.0, 100.0) / 100.0;
    let draw_count = f32(params.overdraw.Load(i32x3(i32x2(input.position.xy), 0)));
    let heat = 1.0 - exp2(-draw_count * draw_scale);
    let color = inferno(heat);
    return f32x4(color, 1.0);
}