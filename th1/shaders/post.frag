#version 450 core

in vec2 outTexcoord;

out vec3 fragColor;

layout(binding=0) uniform sampler2D colorbuffer;
layout(binding=3) uniform sampler2D specbuffer;

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

void main()
{
  vec3 color =  texture(colorbuffer,outTexcoord).rgb;// +texture(specbuffer,outTexcoord).rgb;//
  // //texture(colorbuffer,outTexcoord).rgb +
  color *= vec3(8);
   // color = color / (color + vec3(1.0));
    vec3 curr = Uncharted2Tonemap(color);
    vec3 whiteScale = 1.0f/Uncharted2Tonemap(vec3(W));
     color = curr*whiteScale;

   color = pow(color, vec3(1.0/2.2));
  fragColor = vec3(color);
}
