#version 450 core
#define texture2D texture
//#define OCCLUSION
in vec2 outTexcoord;
layout (location = 0) out vec3 texInfo;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=3) uniform sampler2D norInfo;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;




vec2 when_ge2(vec2 x, vec2 y) {
  return 1.0 - max(sign(y - x), 0.0);
}
vec3 decodeNormal( vec2 f )
{
    f = f * 2.0 - 1.0;
    vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = clamp( -n.z,0,1 );
    n.xy += -sign(when_ge2(n.xy,vec2(0)) - vec2(0.5))*t;
    return normalize( n );
}

vec3 WorldPosFromDepth(float depth) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(outTexcoord * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;

  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
}

float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

float CheckCollision(vec3 pos, vec3 bmin,vec3 bmax)
{
  return when_lt(pos.x,bmax.x)*when_lt(pos.y,bmax.y)*when_lt(pos.z,bmax.z)*when_gt(pos.x,bmin.x)*when_gt(pos.y,bmin.y)*when_gt(pos.z,bmin.z);
}

float when_bounded(vec2 v)
{
  return when_lt(v.x,1)*when_lt(v.y,1)*when_gt(v.x,0)*when_gt(v.y,0);
}

void main(void)
{

  //-- 2. compute world matrix.
//-- then we premultiply it by the scale matrix. world = scale * rotateTranslate.


vec3 scale = vec3(50,50,50);
vec3 pos   = vec3(-50,-50,-50);
vec3 zAxis = normalize(vec3(1,0,0));
vec3 yAxis = normalize(vec3(0,-1,0));
vec3 xAxis = cross(yAxis, zAxis);
mat4 scaleMat =
{
  vec4(scale.x, 0, 0, 0),vec4(0, scale.y, 0, 0),vec4(0, 0, scale.z, 0),vec4(0, 0, 0, 1)
};
mat4 worldMat =
{
  vec4(xAxis, 0),vec4(yAxis, 0),vec4(zAxis, 0),vec4(pos,   1)
};
//-- 3. final world matrix.
worldMat = worldMat*scaleMat;
//-- 4. compute data for the decal view matrix.
mat4 lookAtMat =
{
  vec4(xAxis.x,            yAxis.x,          zAxis.x,           0),
  vec4(xAxis.y,            yAxis.y,          zAxis.y,           0),
  vec4(xAxis.z,            yAxis.z,          zAxis.z,           0),
  vec4(-dot(xAxis, pos),  -dot(yAxis, pos), -dot(zAxis, pos),   1)
};
//-- 5. compute data for the decal proj matrix.
mat4 projMat =
{
  vec4(2.0f / scale.x, 0,              0, 0),
  vec4(0,              2.0f / scale.y, 0, 0),
  vec4(0,              0,              1, 0),
  vec4(0,              0,              0, 1)
};
//-- 6. caclulate final view-projection decal matrix.
mat4 viewProjMat = projMat*lookAtMat;



  vec3 worldNormal = normalize(decodeNormal(texture(norInfo,outTexcoord).zw));
  float depthval = texture2D(depthTex,outTexcoord ).r;
  vec3 worldpos = WorldPosFromDepth(depthval);

  //-- calculate texture coordinates for projection texture.
  vec4 pixelClipPos = viewProjMat*vec4(worldpos, 1.0f);
  pixelClipPos.xyz /= pixelClipPos.w;
  pixelClipPos.xyz = pixelClipPos.xyz * 0.5 + 0.5;

  float angle = dot(worldNormal, zAxis);
  float inside = CheckCollision(worldpos,vec3(-100,-100,-100),vec3(0,0,0))*(1-when_gt(pixelClipPos.z,1.0))*when_bounded(pixelClipPos.xy);
  //
  if (inside == 1 && angle < -0.1 )
  {
    texInfo = vec3(mod(pixelClipPos.xy,vec2(1,1)),3);
  }
  else
  {
    discard;
  }
}
