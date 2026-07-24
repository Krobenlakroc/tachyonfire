#version 460 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 inTransform;
layout(location = 9) in vec2 inID;

uniform mat4 modelViewprojection;

void main()
{
  vec4 p = inTransform*vec4(inPosition.xyz ,1.0);
  //vec4(p.xyz,1.0)
   gl_Position = (modelViewprojection*p);

}
