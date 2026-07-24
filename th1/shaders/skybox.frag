#version 450 core
layout(early_fragment_tests) in;
in vec2 outTexcoord;

layout (location = 0) out vec3 fragColor;

uniform mat4 viewMat;
layout(binding=23) uniform samplerCube atmosphereCube;

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 10;

vec3 Uncharted2Tonemap(vec3 x)
{
     return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}


void main(void)
{
//  vec3 dir = ray_dir_from_uv(vec2(outTexcoord.x, 1- outTexcoord.y));
  vec2 normcoord = outTexcoord*vec2(2) - vec2(1);
  vec3 dir = normalize(vec3(0,0,1) + vec3(0,-1,0)*normcoord.y + vec3(-1,0,0)*normcoord.x);
  dir = vec3(vec4(dir,0)*viewMat);
  vec3 color = textureLod(atmosphereCube,dir*vec3(-1,-1,-1),0).rgb;
  //  color = 1.0 - exp(-1.0 * color);
  // fragColor = vec3(color);
  color *= vec3(8);
   // color = color / (color + vec3(1.0));
    vec3 curr = Uncharted2Tonemap(color);
    vec3 whiteScale = 1.0f/Uncharted2Tonemap(vec3(W));
     color = curr*whiteScale;

   color = pow(color, vec3(1.0/2.2));
  fragColor = vec3(color);
}
