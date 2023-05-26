#version 460 core

#pragma $extensions
#pragma $stage_vertex

#extension GL_EXT_debug_printf : enable

layout(scalar, set = 0, binding = 0) restrict readonly buffer _BufferRef
{
    uint64_t DeviceAddress[];
} BufferRef;

out gl_PerVertex { vec4 gl_Position; };
void main()
{
    debugPrintfEXT("Buffer Idx: %d = Memory Addr: %llu \\n", gl_VertexIndex, BufferRef.DeviceAddress[gl_VertexIndex]);

    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}