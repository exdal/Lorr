#version 460 core

#pragma $extensions
#pragma $stage_vertex

#extension GL_EXT_debug_printf : enable

layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer BufferData
{
    uint Value;
};

layout(scalar, set = 0, binding = 0) restrict readonly buffer _BufferRef
{
    uint64_t DeviceAddress[];
} BufferRef;

layout(set = 1, binding = 0) uniform texture2D u_Images[];
layout(set = 3, binding = 0) uniform sampler u_Samplers[];

out gl_PerVertex { vec4 gl_Position; };
void main()
{
    BufferData data = BufferData(BufferRef.DeviceAddress[0]);
    debugPrintfEXT("Buffer Data: %u = Memory Addr: %llu \\n", data.Value, BufferRef.DeviceAddress[0]);

    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}