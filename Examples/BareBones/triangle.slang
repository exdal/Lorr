import lorr;

struct CoarseVertex {
    float4 color;
};

struct VertexOutput {
    float4 position     : SV_Position;
    CoarseVertex cv  : CoarseVertex;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;
    switch (vertex_id) {
        case 0:
            output.position = float4(0.0, -0.3, 0.0, 1.0);
            output.cv.color = float4(0.0, 1.0, 0.0, 1.0);
            break;
        case 1:
            output.position = float4(-0.3, 0.3, 0.0, 1.0);
            output.cv.color = float4(1.0, 0.0, 0.0, 1.0);
            break;
        case 2:
            output.position = float4(0.3, 0.3, 0.0, 1.0);
            output.cv.color = float4(0.0, 0.0, 1.0, 1.0);
            break;
    }

    return output;
}

[shader("fragment")]
float4 fs_main(CoarseVertex input : CoarseVertex) : SV_Target {
    return input.color;
}
