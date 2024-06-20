import lorr;

struct CoarseVertex {
    float4 color;
};

struct VertexOutput {
    float4 position     : SV_Position;
    CoarseVertex cv     : CoarseVertex;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    float4 pos_arr[3] = {
        { 0.0, -0.3, 0.0, 1.0},
        {-0.3,  0.3, 0.0, 1.0},
        { 0.3,  0.3, 0.0, 1.0},
    };

    float4 color_arr[3] = {
        {0.0, 1.0, 0.0, 1.0},
        {1.0, 0.0, 0.0, 1.0},
        {0.0, 0.0, 1.0, 1.0},
    };

    VertexOutput output;
    output.position = pos_arr[vertex_id];
    output.cv.color = color_arr[vertex_id];

    return output;
}

[shader("fragment")]
float4 fs_main(CoarseVertex input : CoarseVertex) : SV_Target {
    return input.color;
}

