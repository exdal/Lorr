import "../lorr";

struct VertexOutput {
    float4 position     : SV_Position;
    float3 near_pos     : POSITION;
    float3 far_pos      : POSITION;
};

[[vk::push_constant]] struct
{
    float4x4 view_proj_mat_inv;
} c;


float3 UnprojectPoint(float x, float y, float z, float4x4 view_proj_mat_inv) {
    float4 unprojectedPoint =  mul(view_proj_mat_inv, float4(x, y, z, 1.0));
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

float4 grid(float3 fragPos3D, float scale) {
    float2 coord = fragPos3D.xz * scale; // use the scale variable to set the distance between the lines
    float2 derivative = fwidth(coord);
    float2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    float4 color = float4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));
    // z axis
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        color.z = 1.0;
    // x axis
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        color.x = 1.0;
    return color;
}

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(uv * float2(2.0, 2.0) - float2(1.0, 1.0), 0.5, 1);
    output.near_pos = UnprojectPoint(output.position.x, output.position.y, 0.0, c.view_proj_mat_inv);
    output.far_pos =  UnprojectPoint(output.position.x, output.position.y, 1.0, c.view_proj_mat_inv);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET
{
    float t = -input.near_pos.y / (input.far_pos.y - input.near_pos.y);
    float3 fragPos3D = input.near_pos + t * (input.far_pos - input.near_pos);
    return grid(fragPos3D, 10) * float(t > 0);
}

