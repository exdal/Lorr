#version 460

#pragma $extensions
#pragma $stage_vertex

#include "shaders/common.glsl"

struct Constants
{
    vec2 Scale; 
    vec2 Translate;
    u32 StartIndex;
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

layout(buffer_reference, std430, buffer_reference_align = 4) restrict readonly buffer IndexData
{
    uint Indices[];
};

layout(location = 0) out PixelInput pixelOutput;

out gl_PerVertex { vec4 gl_Position; };
void main()
{
    VertexData vertexData = LR_GET_BUFFER(VertexData, 0);
    IndexData indexData = LR_GET_BUFFER(IndexData, 1);

    u32 vertexID = indexData.Indices[gl_VertexIndex + constants.StartIndex];
    vec2 inPosition = vertexData.Vertex[vertexID].Position;
    vec2 inTexCoord = vertexData.Vertex[vertexID].TexCoord;
    u32 inColor = vertexData.Vertex[vertexID].Color;

    pixelOutput.TexCoord = inTexCoord;
    pixelOutput.Color = unpackUnorm4x8(inColor);
    gl_Position = vec4(inPosition * constants.Scale + constants.Translate, 0.0, 1.0);
}