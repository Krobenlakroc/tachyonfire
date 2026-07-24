#version 460 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBiTangent;
layout(location = 5) in mat4 inTransform;
layout(location = 9) in vec2 inID;
layout(location = 10) in vec4 inBoneweights;
layout(location = 11) in vec4 inBoneindex;

flat out vec2 ID;
out vec2 outTexcoord;
//out mat3 TBN;
out vec3 T;
// out vec3 B;
out vec3 N;

out float dynamic_flag;

uniform mat4 modelViewprojection;
uniform mat4 view;
uniform int frame_index = 0;
uniform int dynamic_cutoff;

layout (std430 , binding = 1) restrict readonly buffer BoneMats
{
 mat4 bones[6144];
};

layout (std140 , binding = 6) uniform BoneIndices //
{
  uniform uvec4 boneindices[4096];//use the max amount of memory
};

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

  uvec4 boneindices_data = boneindices[uint(gl_DrawID/uint(2))];
  uvec2 arr[2];
  arr[0] = boneindices_data.xy;
  arr[1] = boneindices_data.zw;
  uvec2 offsetinfo = arr[gl_DrawID%2];//boneindices[gl_DrawID];
  uint oset = offsetinfo.x + gl_InstanceID*offsetinfo.y;

//     uint oset = 0;

  vec4 boneindex = inBoneindex*255.0;
  vec4 boneweights = inBoneweights*255.0;

  mat4 skeleton = bones[int(boneindex.x) + oset + 2048*frame_index]* boneweights.x +
  bones[int(boneindex.y) + oset + 2048*frame_index] * boneweights.y +
  bones[int(boneindex.z) + oset + 2048*frame_index] * boneweights.z +
  bones[int(boneindex.w) + oset + 2048*frame_index] * boneweights.w;

  mat4 transform = inTransform*skeleton;
//   vec4 p = transform*vec4(inPosition.xyz ,1.0);
//
//   gl_Position = (modelViewprojection*p);

  gl_Position = modelViewprojection * (inTransform * (skeleton * vec4(inPosition.xyz, 1.0)));

  outTexcoord = vec2(inTexcoord.x,1 -inTexcoord.y);
  ID = inID;
   T = normalize(vec3(transform * vec4(inTangent,   0.0)));
   // B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
   N = normalize(vec3(transform * vec4(inNormal,    0.0)));
 // TBN = mat3(T, B, N);
//  vec3 q = quantify(p.xyz);

  dynamic_flag = 1;//when_gt(gl_DrawID,dynamic_cutoff);
 //gl_CullDistance[0] = -min(when_lt(q.x,0)+when_gt(q.x,gridsize.x)+when_lt(q.y,0)+when_gt(q.y,gridsize.y) + when_lt(q.z,0)+when_gt(q.z,gridsize.z),1.0);
}
