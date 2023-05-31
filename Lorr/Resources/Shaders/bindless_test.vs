#version 460 core

#pragma $extensions
#pragma $stage_vertex

#include "shaders/common.glsl"

#extension GL_EXT_debug_printf : enable

LR_INIT_SHADER();

layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer BufferData
{
    uint Value;
};

out gl_PerVertex { vec4 gl_Position; };
void main()
{
    BufferData data = LR_GET_BUFFER(BufferData, 0);

    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}