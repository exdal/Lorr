module editor_grid;

import std;
import scene;

struct VertexOutput {
    f32x4 position : SV_Position;
    f32x3 near_pos;
    f32x3 far_pos;
};

func unproject_point(f32x3 position, in mat4 inv_proj_view_mat) -> f32x3 {
    const let p = mul(inv_proj_view_mat, f32x4(position, 1.0));
    return p.xyz / p.w;
}

struct PushConstants {
    Camera *camera;
};
[[vk::push_constant]] PushConstants C;

[[shader("vertex")]]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    const let uv = f32x2((vertex_id << 1) & 2, vertex_id & 2);
    VertexOutput output;
    output.position = f32x4(uv * 2.0 - 1.0, 0.5, 1.0);
    output.far_pos = unproject_point(f32x3(output.position.xy, 1.0), C.camera->inv_projection_view_mat);
    output.near_pos = unproject_point(f32x3(output.position.xy, 0.0), C.camera->inv_projection_view_mat);

    return output;
}

float4 pristine_grid(float3 uv, float2 lineWidth) {
    float2 ddx = ddx(uv.xz);
    float2 ddy = ddy(uv.xz);
    float2 uvDeriv = float2(length(float2(ddx.x, ddy.x)), length(float2(ddx.y, ddy.y)));
    bool2 invertLine = bool2(lineWidth.x > 0.5, lineWidth.y > 0.5);
    float2 targetWidth = float2(invertLine.x ? 1.0 - lineWidth.x : lineWidth.x, invertLine.y ? 1.0 - lineWidth.y : lineWidth.y);
    float2 drawWidth = clamp(targetWidth, uvDeriv, float2(0.5));
    float2 lineAA = uvDeriv * 1.5;
    float2 gridUV = abs(fract(uv.xz) * 2.0 - 1.0);
    gridUV.x = invertLine.x ? gridUV.x : 1.0 - gridUV.x;
    gridUV.y = invertLine.y ? gridUV.y : 1.0 - gridUV.y;
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);

    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = lerp(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2.x = invertLine.x ? 1.0 - grid2.x : grid2.x;
    grid2.y = invertLine.y ? 1.0 - grid2.y : grid2.y;
    float minimumz = min(uvDeriv.y, 1.0);
    float minimumx = min(uvDeriv.x, 1.0);

    float4 color = float4(0.4, 0.4, 0.4, lerp(grid2.x, 1.0, grid2.y));
    // z axis
    if (uv.x > -1.0 * minimumx && uv.x < 1.0 * minimumx)
        color.z = 1.0;
    // x axis
    if (uv.z > -1.0 * minimumz && uv.z < 1.0 * minimumz)
        color.x = 1.0;

    return color;
}

struct FragmentOutput {
    f32x4 color : SV_Target;
    f32 depth : SV_Depth;
};

[[shader("fragment")]]
FragmentOutput fs_main(VertexOutput input) {
    FragmentOutput output;
    float t = -input.near_pos.y / (input.far_pos.y - input.near_pos.y);
    if (t < 0.0)
        discard;

    float3 pixel_pos = input.near_pos + t * (input.far_pos - input.near_pos);
    pixel_pos.y -= 0.1;

    float4 clip_space_pos = mul(C.camera->projection_view_mat, float4(pixel_pos, 1.0));
    float depth = clip_space_pos.z / clip_space_pos.w;

    output.color = pristine_grid(pixel_pos, float2(0.005)) * float(t > 0.0);
    output.depth = depth;
    return output;
}
