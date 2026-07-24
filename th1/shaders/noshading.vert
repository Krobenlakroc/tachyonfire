#version 460 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 inTransform;
layout(location = 9) in vec2 inID;

flat out vec2 ID;
out vec2 outTexcoord;
//out mat3 TBN;
out vec3 T;
// out vec3 B;
out vec3 N;

out float dynamic_flag;

uniform mat4 modelViewprojection;
// uniform mat4 view;

uniform int dynamic_cutoff;

vec3 goffset =vec3(-23,-75,196);
vec3 gridsize = vec3(43,23,53);
float griddist = 12;

vec3 quantify(vec3 position)
{
  return floor((position - goffset)/griddist  + 0.5*gridsize) - vec3(1,1,1);
}

float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

void main()
{
  vec4 p = inTransform*vec4(inPosition.xyz ,1.0);

  gl_Position = (modelViewprojection*p);
  outTexcoord = vec2(inTexcoord.x,1 -inTexcoord.y);
  ID = inID;
   T = normalize(vec3(inTransform * vec4(inTangent,   0.0)));
   // B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
   N = normalize(vec3(inTransform * vec4(inNormal,    0.0)));

   dynamic_flag = float(gl_DrawID > dynamic_cutoff);//when_gt(gl_DrawID,dynamic_cutoff);
 // TBN = mat3(T, B, N);
 // vec3 q = quantify(p.xyz);
  //gl_CullDistance[0] = -min(when_lt(q.x,0)+when_gt(q.x,gridsize.x)+when_lt(q.y,0)+when_gt(q.y,gridsize.y) + when_lt(q.z,0)+when_gt(q.z,gridsize.z),1.0);
}
