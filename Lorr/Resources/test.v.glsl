#version 450

layout(location = 0) in vec3 i_Position;

void main()
{
    gl_Position = vec4(i_Position, 1.0);
}