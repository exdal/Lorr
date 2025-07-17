import std;
import gpu;
import scene;
import debug_drawer;

struct ShaderParameters {
    ConstantBuffer<Camera> camera;
    RWStructuredBuffer<DebugAABB> debug_aabb_draws;
    RWStructuredBuffer<DebugRect> debug_rect_draws;
};
ParameterBlock<ShaderParameters> params;

func draw_aabb(u32 vertex_index, u32 instance_index, out f32x4 position, out f32x3 color, out DebugDrawCoord coord) {
    let aabb = params.debug_aabb_draws[instance_index];
    static let offsets = f32x3[24](
        // bottom
        f32x3(-0.5, -0.5, -0.5), f32x3( 0.5, -0.5, -0.5),
        f32x3(-0.5, -0.5, -0.5), f32x3(-0.5,  0.5, -0.5),
        f32x3(-0.5,  0.5, -0.5), f32x3( 0.5,  0.5, -0.5),
        f32x3( 0.5, -0.5, -0.5), f32x3( 0.5,  0.5, -0.5),
        // top
        f32x3(-0.5, -0.5,  0.5), f32x3( 0.5, -0.5,  0.5),
        f32x3(-0.5, -0.5,  0.5), f32x3(-0.5,  0.5,  0.5),
        f32x3(-0.5,  0.5,  0.5), f32x3( 0.5,  0.5,  0.5),
        f32x3( 0.5, -0.5,  0.5), f32x3( 0.5,  0.5,  0.5),
        // connections
        f32x3(-0.5, -0.5, -0.5), f32x3(-0.5, -0.5,  0.5),
        f32x3( 0.5, -0.5, -0.5), f32x3( 0.5, -0.5,  0.5),
        f32x3(-0.5,  0.5, -0.5), f32x3(-0.5,  0.5,  0.5),
        f32x3( 0.5,  0.5, -0.5), f32x3( 0.5,  0.5,  0.5)
    );
    position = f32x4(offsets[vertex_index] * aabb.size, 1.0);
    position = position + f32x4(aabb.position, 0.0);
    color = aabb.color;
    coord = aabb.coord;
}


func draw_rect(u32 vertex_index, u32 instance_index, out f32x4 position, out f32x3 color, out DebugDrawCoord coord) {
    let rect = params.debug_rect_draws[instance_index];
    static let offsets = f32x2[8](
        float2(-0.5, -0.5), float2( 0.5, -0.5),
        float2( 0.5, -0.5), float2( 0.5,  0.5),
        float2( 0.5,  0.5), float2(-0.5,  0.5),
        float2(-0.5,  0.5), float2(-0.5, -0.5)
    );
    position = f32x4(offsets[vertex_index] * rect.extent, 0.0, 1.0);
    position = position + f32x4(rect.offset, 0.0);
    color = rect.color;
    coord = rect.coord;
}

struct VertexOutput {
    f32x4 position : SV_Position;
    f32x3 color;
};

[[shader("vertex")]]
func vs_main(
    u32 vertex_id : SV_VertexID,
    u32 instance_id : SV_InstanceID,
    uniform u32 draw_type
) -> VertexOutput {
    VertexOutput output = {};
    var coord = DebugDrawCoord::NDC;

    switch (draw_type)  {
        case 0: {
            draw_aabb(vertex_id, instance_id, output.position, output.color, coord);
        } break;
        case 1: {
            draw_rect(vertex_id, instance_id, output.position, output.color, coord);
        } break;
        default:;
    }

    switch (coord) {
        case DebugDrawCoord::World: {
            output.position = mul(params.camera.projection_view_mat, output.position);
        } break;
        default:;
    }

    return output;
}

[[shader("fragment")]]
func fs_main(VertexOutput input) -> f32x4 {
    return f32x4(input.color, 1.0);
}
