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

uniform mat4 modelViewprojection;
uniform int frame_index = 0;

layout (std140 , binding = 1) buffer BoneMats
{
 mat4 bones[6144];
};

layout (std140 , binding = 6) uniform BoneIndices //
{
  uniform uvec4 boneindices[4096];//use the max amount of memory
};

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

  //mat4 skeleton = mat4(1.0);


//   mat4 transform = inTransform*skeleton;
//   vec4 p = transform*vec4(inPosition.xyz ,1.0);
//
//   gl_Position = (modelViewprojection*p);
  gl_Position = modelViewprojection * (inTransform * (skeleton * vec4(inPosition.xyz, 1.0)));

 //gl_CullDistance[0] = -min(when_lt(q.x,0)+when_gt(q.x,gridsize.x)+when_lt(q.y,0)+when_gt(q.y,gridsize.y) + when_lt(q.z,0)+when_gt(q.z,gridsize.z),1.0);
}
