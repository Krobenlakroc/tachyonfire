#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 inTransform;
layout(location = 9) in vec2 inID;

out vec2 outTexcoord;
out vec3 outPosition;
out vec4 lightSpacePosition;
out mat3 TBN;
out vec2 ID;
out vec3 surfNorm;
uniform mat4 modelViewprojection;
uniform mat4 lightSpaceMatrix;

void main()
{

  vec4 p = inTransform*vec4(inPosition.xyz ,1.0);


  gl_Position = (modelViewprojection*p);
  outTexcoord = vec2(inTexcoord.x,1 - inTexcoord.y);
  outPosition= p.xyz;

   vec3 T = normalize(vec3(inTransform * vec4(inTangent,   0.0)));
   vec3 B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
   vec3 N = normalize(vec3(inTransform * vec4(inNormal,    0.0)));
   surfNorm = N;
  TBN = mat3(T, B, N);
  ID = inID;

  lightSpacePosition = lightSpaceMatrix*vec4(p.xyz,1.0);
}
