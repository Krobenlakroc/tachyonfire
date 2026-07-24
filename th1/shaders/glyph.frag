#version 450 core
in vec2 outTexcoord;

layout (location = 0) out vec4 fragColor;
layout(binding=35) uniform sampler2D text;
layout(binding=34) uniform sampler2DArray colorTex;

uniform vec3 textColor = vec3(1,1,1);

void main(void)
{
//  vec4 inColor = texture(colorTex,vec3(outTexcoord ,0.0));//- 0.5*outTexcoord.x
  float sampled = texture(text, outTexcoord).r;

  float metallic = mix(1.08, 0.6, outTexcoord.y);

  metallic += 0.6 * exp(-pow((outTexcoord.y - 0.28) * 8.0, 2.0));

  vec3 color = textColor * metallic;


  fragColor = vec4(color ,sampled);

}
