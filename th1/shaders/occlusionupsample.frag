#version 450 core
in vec2 outTexcoord;
layout (location = 2) out vec4 fragColor;

layout(binding=26) uniform sampler2D occTex;

uniform float scale;
void main(void)
{
  fragColor =texture(occTex,vec2( outTexcoord.x, outTexcoord.y)*vec2(scale));
}
