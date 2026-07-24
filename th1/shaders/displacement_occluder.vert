#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 inTransform;
layout(location = 9) in vec2 inID;

out vec3 vPosition;
out vec2 vTexCoord;
out vec3 vNormal;
out mat4 vTransform;
out vec2 vID;


void main()
{
  vPosition = inPosition;
  vTexCoord = inTexcoord;
  vNormal = inNormal;
  vTransform = inTransform;
  vID = inID;
}
