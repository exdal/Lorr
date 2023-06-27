#version 460

#pragma $extensions
#pragma $stage_vertex

#include "shaders/common.glsl"

struct Constants
{
    vec2 Scale; 
    vec2 Translate;
};

LR_INIT_SHADER_WITH_CONSTANTS(Constants, constants);

struct VertexInput
{
    vec2 Position;
    vec2 TexCoord;
    u32 Color;
};

struct PixelInput
{
    vec2 TexCoord;
    vec4 Color;
};

layout(buffer_reference, std430, buffer_reference_align = 8) restrict readonly buffer VertexData
{
    VertexInput Vertex[];
};

layout(location = 0) out PixelInput pixelOutput;

out gl_PerVertex { vec4 gl_Position; };
void main()
{
    VertexData vertexData = LR_GET_BUFFER(VertexData, 0);

    VertexInput vertex = vertexData.Vertex[gl_VertexIndex];
    pixelOutput.TexCoord = vertex.TexCoord;
    pixelOutput.Color = unpackUnorm4x8(vertex.Color);
    gl_Position = vec4(vertex.Position * constants.Scale + constants.Translate, 0.0, 1.0);
}