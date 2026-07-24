#version 450 core
layout(early_fragment_tests) in;
in vec2 outTexcoord;
in vec3 outPosition;
in mat3 TBN;
in vec2 ID;

layout (location = 0) out vec4 fragColor;

samplerCube cmap;
void main()
{


    vec3 color = texture(cmap,normalize(outPosition));
    fragColor = vec4(color, 1.0);
}
