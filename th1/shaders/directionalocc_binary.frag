#version 460 core
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;

uniform float occlusion_radius;
uniform float occlusion_max_distance;

uniform vec3 eyePos;
uniform mat4 projection;
uniform mat4 viewMatrixInv;
uniform mat4 projMatrixInv;
uniform vec2 resolution;

uniform float zNear;
uniform float zFar;
uniform mat4 viewProjection;
uniform vec3 goffset ;
uniform vec3 gridsize ;
uniform float griddist ;


layout(binding=2) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=40) uniform sampler2D noiseTex;

vec2 invResolution;

vec2 lookup = (gl_FragCoord.xy )/resolution;
vec3 WorldPosFromDepth(float depth) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(lookup * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;

  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
}

vec4 ViewPosFromDepth(float depth) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(lookup * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;
  return viewSpacePosition;
  // vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;
  //
  // return worldSpacePosition.xyz;
}

vec3 worldfromview(vec4 viewSpacePosition)
{
  return (viewMatrixInv * viewSpacePosition).xyz;
}
float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}
float when_ge(float x, float y) {
  return 1.0 - when_lt(x, y);
}


float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_le(float x, float y) {
  return 1.0 - when_gt(x, y);
}

vec3 normalizeNoNAN(vec3 n)
{
  float len = length(n);
  return n/(len + when_le(len,0) );
}

// vec2  unpack_2half(float a)
// {
//   return (unpackHalf2x16(floatBitsToUint(a)));
//   // vec2 result;
//   // result.x = float((int(a) >> 0) & 0xFF);
//   // result.y = float((int(a) >> 16) & 0xFF);
//   // return result;
// }
//
// float pack_2half(float x,float y)
// {
//   return uintBitsToFloat(packHalf2x16(vec2(x,y)));
//   // float result;
//   // return result = float(((floatBitsToInt(y)) << 16) | floatBitsToInt(x));
// }

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

vec4 c0123;
vec4 c4567;
float c8;

float screen2EyeDepth(float depth, float near, float far)
{

    float ndc = 2.0 * depth - 1.0;
    float eye = 2.0 * far * near / (far + near + ndc * (near - far));
    return eye;
}

vec3 quantify(vec3 position)
{
  return floor((position - goffset  + 0.5*gridsize*griddist)/griddist )- vec3(1,1,1);
}

vec3 lerppoint(vec3 position)
{
  vec3 quant = ((position - goffset + 0.5*gridsize*griddist)/griddist  ) ;
   quant -= vec3(1,1,1);
  quant -= floor(quant);
  return quant;
}

vec3 getPos(vec3 indices)
{
  float x = indices.x +1;
  float y = indices.y +1;
  float z = indices.z +1;
  return vec3((x-(gridsize.x*0.5))*griddist + goffset.x,(y-(gridsize.y*0.5))*griddist + goffset.y,(z-(gridsize.z*0.5))*griddist + goffset.z);
}

vec4 ViewPosFromDepthUV(float depth,vec2 uv) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;
  return viewSpacePosition;
  // vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;
  //
  // return worldSpacePosition.xyz;
}

float getValid(vec3 direction,vec3 normal)
{
     return max(0,dot(direction,normal));
}

vec4 dssdo_accumulate()
{
  float depthval = texture2D(depthTex,gl_FragCoord.xy*(1.0/resolution) ).r;
    vec3 outPosition = WorldPosFromDepth(depthval);
    vec3 N_geom = normalize(cross(dFdx(outPosition),dFdy(outPosition)));
    vec3 N = normalize(decodeNormal(texture(normalTex, lookup).xy));

  vec3 pos= outPosition - N_geom*griddist*0.25;

  //rad sample begin
  vec3 q = quantify(pos);

  vec3 m = lerppoint(pos);
  pos = outPosition;

  vec3 positions[8];

  positions[0] = getPos(q + vec3(0,0,0));

  positions[1] = getPos(q + vec3(1,0,0));

  positions[2] = getPos(q + vec3(0,1,0));

  positions[3] = getPos(q + vec3(1,1,0));

  positions[4] = getPos(q + vec3(0,0,1));

  positions[5] = getPos(q + vec3(1,0,1));

  positions[6] = getPos(q + vec3(0,1,1));

  positions[7] = getPos(q + vec3(1,1,1));
  int steps = 4;
  float trace_mag = 100.0;
  uint mask = 0;
  for(int i = 0; i < 8; i++)
  {
    vec3 direction = normalize(positions[i] - pos);
    float occluded = 0.0;
    float valid = getValid(direction,N);
    if (valid > 0.1)
    {
      for(int j = 0; j < steps; j++)
      {
        vec3 world_step = pos + direction*(float(j + 1)/float(steps))*trace_mag;
        vec4 offset = vec4(world_step,1);
        offset = viewProjection * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        float dsample = texture(depthTex,offset.xy).r;
        float d = screen2EyeDepth(dsample,zNear,zFar);

        vec4 sfv = ViewPosFromDepthUV(dsample,offset.xy);
        vec3 world_sample = worldfromview(sfv);

        float bound = when_lt(offset.x,1)*when_gt(offset.x,0)*when_lt(offset.y,1)*when_gt(offset.y,0);//*when_le(distance(world_sample,pos),trace_mag + 20);
        occluded = occluded + when_gt(screen2EyeDepth(offset.z,zNear,zFar) - 0.9,d)*bound;
      }
    }

    occluded = clamp(occluded,0,1);
    if (occluded == 0.0)
    {
      mask = mask | (1 << i);
    }

  }
  // mask = mask | (1 << 0);




  return vec4(mask,0,0,0);

}

void main(void)
{

  fragColor = dssdo_accumulate();


}
