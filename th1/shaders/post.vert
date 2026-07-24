#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1)in vec2 inTexcoord;

out vec2 outTexcoord;

uniform mat4 modelViewprojection;

void main()
{

  vec4 p = vec4(inPosition.xyz ,1.0);


  gl_Position = (modelViewprojection*p);
  outTexcoord = vec2(inTexcoord.x,1 -inTexcoord.y);
}
