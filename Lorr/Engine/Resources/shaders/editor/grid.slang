import "../lorr";

struct VertexOutput {
    float4 position     : SV_Position;
    float3 near_pos     : POSITION;
    float3 far_pos      : POSITION;
};

struct FragmentOutput {
    float4 color : SV_Target;
    float depth  : SV_Depth;
};

[[vk::binding(0, 0)]] Scene scene;

float3 UnprojectPoint(float x, float y, float z, float4x4 view_proj_mat_inv) {
    float4 unprojectedPoint =  mul(view_proj_mat_inv, float4(x, y, z, 1.0));
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

float4 pristine_grid(float3 uv, float2 lineWidth) {
    float2 ddx = ddx_fine(uv.xz);
    float2 ddy = ddy_fine(uv.xz);
    float2 uvDeriv = float2(length(float2(ddx.x, ddy.x)), length(float2(ddx.y, ddy.y)));
    bool2 invertLine = bool2(lineWidth.x > 0.5, lineWidth.y > 0.5);
    float2 targetWidth = float2(
      invertLine.x ? 1.0 - lineWidth.x : lineWidth.x,
      invertLine.y ? 1.0 - lineWidth.y : lineWidth.y
      );
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
    if(uv.x > -1.0 * minimumx && uv.x < 1.0 * minimumx)
        color.z = 1.0;
    // x axis
    if(uv.z > -1.0 * minimumz && uv.z < 1.0 * minimumz)
        color.x = 1.0;

    return color;
}

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    Camera *camera = &scene.cameras[scene.active_camera_index];

    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(uv * float2(2.0, 2.0) - float2(1.0, 1.0), 0.5, 1);
    output.near_pos = UnprojectPoint(output.position.x, output.position.y, 0.0, camera->inv_projection_view_mat);
    output.far_pos =  UnprojectPoint(output.position.x, output.position.y, 1.0, camera->inv_projection_view_mat);

    return output;
}

[shader("fragment")]
FragmentOutput fs_main(VertexOutput input) {
    Camera *camera = &scene.cameras[scene.active_camera_index];

    FragmentOutput output;
    float t = -input.near_pos.y / (input.far_pos.y - input.near_pos.y);
    if (t < 0.0)
        discard;

    float3 pixel_pos = input.near_pos + t * (input.far_pos - input.near_pos);
    pixel_pos.y -= 0.1;

    float4 clip_space_pos = mul(camera->projection_view_mat, float4(pixel_pos, 1.0));
    float depth = clip_space_pos.z / clip_space_pos.w;

    output.color = pristine_grid(pixel_pos, float2(0.005)) * float(t > 0.0);
    output.depth = depth;
    return output;
}

