import lorr;

[[vk::push_constant]] struct {
    float4x4 view_proj_mat;
} c;

struct CoarseVertex {
    float4 color;
};

struct VertexOutput {
    float4 position     : SV_Position;
    CoarseVertex cv     : CoarseVertex;
};

static const float3 colors[] = {
    // back - black
    { 0.0, 0.0, 0.0 },
    // front - blue
    { 0.0, 0.0, 1.0 },
    // left 
    { 1.0, 1.0, 0.0 },
    // right
    { 0.0, 1.0, 1.0 },
    // bottom - green
    { 0.0, 1.0, 0.0 },
    // top - red
    { 1.0, 0.0, 0.0 }
};

static const float3 vertices[] = {
    // Back face
    { -1.0, -1.0, -1.0 }, // bottom-left
    { 1.0, 1.0, -1.0 }, // top-right
    { 1.0, -1.0, -1.0 }, // bottom-right
    { 1.0, 1.0, -1.0 }, // top-right
    { -1.0, -1.0, -1.0 }, // bottom-left
    { -1.0, 1.0, -1.0 }, // top-left
    // front face
    { -1.0, -1.0, 1.0 }, // bottom-left
    { 1.0, -1.0, 1.0 }, // bottom-right
    { 1.0, 1.0, 1.0 }, // top-right
    { 1.0, 1.0, 1.0 }, // top-right
    { -1.0, 1.0, 1.0 }, // top-left
    { -1.0, -1.0, 1.0 }, // bottom-left
    // left face
    { -1.0, 1.0, 1.0 }, // top-right
    { -1.0, 1.0, -1.0 }, // top-left
    { -1.0, -1.0, -1.0 }, // bottom-left
    { -1.0, -1.0, -1.0 }, // bottom-left
    { -1.0, -1.0, 1.0 }, // bottom-right
    { -1.0, 1.0, 1.0 }, // top-right
    // right face
    { 1.0, 1.0, 1.0 }, // top-left
    { 1.0, -1.0, -1.0 }, // bottom-right
    { 1.0, 1.0, -1.0 }, // top-right
    { 1.0, -1.0, -1.0 }, // bottom-right
    { 1.0, 1.0, 1.0 }, // top-left
    { 1.0, -1.0, 1.0 }, // bottom-left
    // bottom face
    { -1.0, -1.0, -1.0 }, // top-right
    { 1.0, -1.0, -1.0 }, // top-left
    { 1.0, -1.0, 1.0 }, // bottom-left
    { 1.0, -1.0, 1.0 }, // bottom-left
    { -1.0, -1.0, 1.0 }, // bottom-right
    { -1.0, -1.0, -1.0 }, // top-right
    // top face
    { -1.0, 1.0, -1.0 }, // top-left
    { 1.0, 1.0, 1.0 }, // bottom-right
    { 1.0, 1.0, -1.0 }, // top-right
    { 1.0, 1.0, 1.0 }, // bottom-right
    { -1.0, 1.0, -1.0 }, // top-left
    { -1.0, 1.0, 1.0 }, // bottom-left
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    output.position = mul(c.view_proj_mat, float4(vertices[vertex_id], 1.0));
    output.cv.color = float4(colors[vertex_id / 6], 1.0);

    return output;
}

[shader("fragment")]
float4 fs_main(CoarseVertex input : CoarseVertex) : SV_Target {
    return input.color;
}

