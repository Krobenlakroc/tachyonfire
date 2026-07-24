#version 460 core
layout(early_fragment_tests) in;

in vec3 light_color;

layout (location = 0) out vec4 forwardBuffer;
void main()
{
    forwardBuffer = vec4(light_color,1);
}
