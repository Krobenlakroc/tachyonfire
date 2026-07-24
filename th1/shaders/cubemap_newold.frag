#version 460 core
#extension GL_ARB_shader_ballot : enable
#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_NV_shader_thread_shuffle : enable
#extension GL_NV_shader_thread_group : enable
// #define OCCLUSION
#define REALTIME_BDRF
//#define TEMPORAL_REPROJ
//#define FASTCUBES
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;
layout(binding=0) uniform sampler2D texInfo;
layout(binding=4) uniform sampler2D brdfLUT;
layout(binding=3) uniform sampler2D occTex;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=22) uniform samplerCubeArray reflections;
layout(binding=21) uniform samplerCubeArray reflectionsDepth;
layout(binding=23) uniform samplerCube atmosphereCube;
layout(binding=5) uniform sampler2D texCoordTex;
layout(binding=25) uniform sampler2D noiseTex;
layout(binding=27) uniform sampler2D old_reflectiondata;
layout(binding=28) uniform sampler2D old_depthtexture;
layout(binding=30) uniform sampler2D oldnormalTex;
uniform mat4 old_mvp;
uniform mat4 projMatrixInv_old;
uniform mat4 viewMatrixInv_old;


uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform vec3 eyePos;
uniform vec2 screenSize;
uniform mat4 viewMat;
uniform mat4 projMat;
uniform float frame;

uniform float zNear = 0.1;
uniform float zFar = 1000;
uint num_tiles_x = 16;
uint num_tiles_y = 8;
uint num_slices = 20;
const float PI = 3.14159265359;

layout (std140 , binding = 0) uniform EnvBoxes
{
  vec3 EnvBoxPos[20];// 0
  vec3 EnvBoxMin[20];// size*16
  vec3 EnvBoxMax[20];//size * 16*2
  int EnvBoxCount;//size*16*3
  //total size is size*16*3 + 4
};

layout (std140 , binding = 5) uniform Access_Buffer_CUBES //
{
  uniform uvec4 acessbuffercubes[4096];//use the max amount of memory
};

layout (std140 , binding = 4) uniform Offsets_buffer_CUBES //size of grid
{
  uniform uvec4 offsets[(16*8*20)];//pack into vec4 for std140
};

layout (std430 , binding = 0) restrict readonly buffer BlueNoise //size of grid
{
   uint g_scrambling_tile_buffer[128*128*8];
};




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

vec3 WorldPosFromDepth(float depth,out vec4 viewSpacePosition2) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(outTexcoord * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;
  viewSpacePosition2 = viewSpacePosition;

  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
}

vec3 WorldPosFromDepthOld(float depth,out vec4 viewSpacePosition2) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(outTexcoord * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;
  viewSpacePosition2 = viewSpacePosition;

  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
}

vec3 when_gt3(vec3 x, vec3 y) {
  return max(sign(x - y), 0.0);
}
vec3 when_le3(vec3 x, vec3 y) {
  return 1.0 - when_gt3(x, y);
}
const vec4 bitDec = vec4(1.0/1.0,1.0/255.0,1.0/65025.0,1.0/16581375.0);
float DecodeFloatRGBA (vec4 v) {
  v.x = floor(v.x * 255.0 + 0.5) / 255.0;
  v.y = floor(v.y * 255.0 + 0.5) / 255.0;
  v.z = floor(v.z * 255.0 + 0.5) / 255.0;
  v.w = floor(v.w * 255.0 + 0.5) / 255.0;


  return dot(v, bitDec)*20000.0;
}

// const float zNear = 0.1;
// const float zFar = 1024;
//
// float getlindepth(float x)
// {
//   float z_n = 2.0 * x - 1.0;
//   float z_e = 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));
//   return z_e;
// }

float not(float a) {
  return 1.0 - a;
}

float or(float a, float b) {
  return min(a + b, 1.0);
}

float when_neq3(vec3 x, vec3 y) {
  return  or(or(abs(sign(x.x - y.x)),abs(sign(x.y - y.y))),abs(sign(x.z - y.z)));
}

float when_eq3(vec3 x, vec3 y) {
  return  or(or(1-abs(sign(x.x - y.x)),1-abs(sign(x.y - y.y))),1-abs(sign(x.z - y.z)));
}

float when_eq(float x, float y) {
  return  1- abs(sign(x - y));
}

float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}
float n0(float x)
{
  return x*not(when_eq(x,0)) + (x + 0.1)*when_eq(x,0);
}

vec3 getLookup(vec3 world,vec3 dir,float l,vec3 rel)
{
  return normalize(world + dir*l - rel);
}
float when_ge(float x, float y) {
  return 1.0 - max(sign(y - x), 0.0);
}

const float EPSILON = 0.0001;
const float zthick = 0.1;
vec3 slerp(vec3 a,vec3 b,float t)
{
  float omega = acos(dot(a,b));
  float sinomega = 1.0/sin(omega);
  float A = sin((1-t)*omega)*sinomega;
  float B = sin((t)*omega)*sinomega;
  return a*A + b*B;
}
float distance_squared(vec3 a,vec3 b)
{
  vec3 d = a -b;
  return dot(d,d);
}
vec3 surfnormal;



// const int iterations = 5;
float raymarch(vec3 worldspaceRay,vec3 worldSpaceDirection,samplerCubeArray mp,float cubeindex,vec3 cubepos,out vec3 result,out float dist_to_hit,int iterations,int steps)
{
  vec3 worldSpaceRay = worldspaceRay;
  // vec3 cubespace_pos = worldSpaceRay - cubepos;
  // vec3 cubespace_dir = worldSpaceDirection;
  //vec3 cubespace = worldspaceRay - cubepos;
  float depth = 50;// texture(mp, vec4(worldSpaceDirection*vec3(-1,1,1),cubeindex)).r;
  vec3 minp = vec3(worldspaceRay);
  vec3 maxp = vec3(worldspaceRay +worldSpaceDirection*2000 );
  float hit = 0;
  // const int steps = 20;
  float delta = 2000.0/steps;

  // vec3 start = normalize((worldspaceRay +worldSpaceDirection*2 - cubepos));
  // vec3 end = normalize((worldspaceRay +worldSpaceDirection*2000 - cubepos));
  // float theta_total = acos(dot(start,end));
  // float theta_delta = theta_total/steps;


  float thickness = 100;
  for (int i = 0; i < steps; i++) {
      vec3 p = worldspaceRay + worldSpaceDirection*depth;
      maxp = p;
      float dist = texture(mp, vec4((p - cubepos)*vec3(-1,1,1),cubeindex)).r;
      float cond = when_lt(dist*dist,distance_squared(p,cubepos));
      if (dist*dist < distance_squared(p,cubepos) )
      {
        hit = 1;
        break;
      }
      minp = p;
      // vec3 old = normalize((p - cubepos));
      // float theta1 = acos(dot(old,worldSpaceDirection));
      // float delta_step = (length((p - cubepos))*theta1)/sin(3.14159 - (theta_delta + theta1) );
      depth += delta;
    }
    //binary search
    //refines by delta*0.5^iterations



    vec3 mid = vec3(0);
    float dist3 = 0;
    float dist2 = 0;
    float d2 = 0;
    if (hit == 1)
    {
      for (int i = 0 ; i < iterations;i++)
      {
         mid = (maxp + minp)*vec3(0.5);
         dist3 = texture(mp, vec4((mid - cubepos)*vec3(-1,1,1),cubeindex)).r;
        float dist2 = dist3*dist3;
        float d2 = distance_squared(mid,cubepos);
        float cmp = when_lt(dist2,d2);
        // hit = or(hit,1-cmp);
        maxp = maxp*(1-cmp) + cmp*mid;
        minp = mid*(1-cmp) + cmp*minp;
        // if (dist < distance(mid,cubepos))
        // {
        //   maxp = mid;
        // }
        // else
        // {
        //   minp = mid;
        // }
      }
    }

    // if (abs(dist3 - distance(mid,cubepos)) > 800)
    // {
    //   hit = 0;
    // }

    hit *= 1 - when_gt(abs(dist3 - distance(mid,cubepos)),800);



    // vec3 test_pos = worldSpaceRay + surfnormal*3;
    // float cubetoworld_squared = texture(mp, vec4(normalize(test_pos - cubepos)*vec3(-1,1,1),cubeindex)).r;
    // cubetoworld_squared = cubetoworld_squared*cubetoworld_squared;
    // hit = when_lt(distance_squared(test_pos,cubepos),cubetoworld_squared);

   mid = (maxp + minp)*vec3(0.5);
  vec3 intersection = mid;//worldspaceRay + worldSpaceDirection * depth;
  result =  intersection - cubepos;
  float dist = texture(mp, vec4((result)*vec3(-1,1,1),cubeindex)).r;
  hit = hit*when_lt(dist,2000);//when_lt(dist*dist,distance_squared(maxp,cubepos));
  //float dist_finaldir =

  result = result*hit + (1-hit)*worldSpaceDirection;

  dist_to_hit = distance(worldSpaceRay,intersection)*(hit);// + 0*(1-1);
  return hit;
}

float raymarchCoarse(vec3 worldspaceRay,vec3 worldSpaceDirection,samplerCubeArray mp,float cubeindex,vec3 cubepos)
{
  vec3 worldSpaceRay = worldspaceRay;
  // vec3 cubespace_pos = worldSpaceRay - cubepos;
  // vec3 cubespace_dir = worldSpaceDirection;
  //vec3 cubespace = worldspaceRay - cubepos;
  float depth = 100;// texture(mp, vec4(worldSpaceDirection*vec3(-1,1,1),cubeindex)).r;
  vec3 minp = vec3(worldspaceRay);
  vec3 maxp = vec3(worldspaceRay +worldSpaceDirection*2000 );
  float hit = 0;
  const int steps = 32;
  float delta = 2000.0/steps;


  for (int i = 0; i < steps; i++) {
      vec3 p = worldspaceRay + worldSpaceDirection*depth;
      maxp = p;
      float dist = texture(mp, vec4((p - cubepos)*vec3(-1,1,1),cubeindex)).r;
      float cond = when_lt(dist*dist,distance_squared(p,cubepos));
      if (dist*dist < distance_squared(p,cubepos) )
      {
        hit = 1;
        break;
      }
      minp = p;

      depth += delta;
    }

  return hit;
}


const int NITER = 8;
float Hit(vec3 x, vec3 R, samplerCubeArray mp,float cubeindex,vec3 cubepos,out vec3 result)
{
  x = x - cubepos;
  float rl = texture(mp, vec4(R*vec3(-1,1,1),cubeindex)).r; // |r|
  float dp = rl - dot(x, R);
  vec3 p = x + R * dp;
  float ppp = length(p)/(texture(mp,vec4(p*vec3(-1,1,1),cubeindex)).r);
  float dun =0, dov =0, pun = ppp, pov = ppp;
  dun = dp*when_lt(ppp,1);
  dov = dp*not(when_lt(ppp,1));

  float dl = max(dp + rl * (1 - ppp), 0);
  vec3 l = x + R * dl;
  // //  iteration
  float ddl = 0;
  float llp;
  for(int i = 0; i < NITER; i++) {

    llp = length(l)/(texture(mp,vec4(l*vec3(-1,1,1),cubeindex)).r);
    float l_p = when_lt(llp,1.0);
    float n_lp = not(l_p);
    dun = dl*l_p + dun*n_lp;
    dov = dl*n_lp + dov *l_p;
    pun = llp*l_p + pun*n_lp;
    pov = llp*n_lp + pov *l_p;

    ddl = l_p*(when_eq(dov,0)*rl*(1 - llp) + not(when_eq(dov,0))*(dl-dov)*(1-llp)/n0(llp-pov) );

    ddl += n_lp*(when_eq(dun,0)*rl*(1 - llp) + not(when_eq(dun,0))*(dl-dun)*(1-llp)/n0(llp-pun) );

    dl = max(dl + ddl, 0); // avoid flip
    l = x + R * dl;
  }
  float fdist  = texture(mp, vec4(l*vec3(-1,1,1),cubeindex)).r;
  float hit  = when_lt(fdist,3000);
  result = l*hit + R*(1-hit);
  return hit;
}

float rerange(float x,float a,float b,float a1,float b1)
{
  float OldRange = (b1 - a1);
  float NewRange = (b - a);
  return (((x - a1) * NewRange) / OldRange) + a;

}

float CheckCollision(vec3 pos, vec3 bmin,vec3 bmax)
{
  return when_lt(pos.x,bmax.x)*when_lt(pos.y,bmax.y)*when_lt(pos.z,bmax.z)*when_gt(pos.x,bmin.x)*when_gt(pos.y,bmin.y)*when_gt(pos.z,bmin.z);
}

vec2  unpack_2half(float a)
{
  return (unpackHalf2x16(floatBitsToUint(a)));
}



vec2 Hammersley(uint i, uint N)
{
  return vec2(
    float(i) / float(N),
    float(bitfieldReverse(i)) * 2.3283064365386963e-10
  );
}
// ----------------------------------------------------------------------------


float DistributionGGX(float ndoth, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(ndoth, 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float InterleavedGradientNoise(vec2 position_screen)
{
  vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
  return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

vec3 fresnelSchlick(float cosTheta, vec3 F0,float roughness)
{
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
  //  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


float screen2EyeDepth(float depth, float near, float far)
{

    float ndc = 2.0 * depth - 1.0;
    float eye = 2.0 * far * near / (far + near + ndc * (near - far));
    return eye;
}

uint getClusterZIndex(float screenDepth)
{

    float eyeDepth = screen2EyeDepth(screenDepth, zNear, zFar);
    uint zIndex = uint(float(num_slices)*(log(eyeDepth/zNear)/log(zFar/zNear)));
    // uint zIndex = uint(max(log(eyeDepth) * scale + bias, 0.0));
    return zIndex;
}

vec3 hash(vec3 a)
{
    a = fract(a * vec3(0.8, 0.8, 0.8));
    a += dot(a, a.yxz + 19.19);
    a = fract((a.xxy + a.yxx)*a.zyx);
    return normalize(a);
}

vec3 avg(vec3 a,vec3 b)
{
  return (a + b);
}

 float computeDistanceBaseRoughness (
 float distInteresectionToShadedPoint ,
 float distInteresectionToProbeCenter ,
 float linearRoughness )
 {
 // To avoid artifacts we clamp to the original linearRoughness
 // which introduces an acceptable bias and allows conservation
 // of mirror reflection behavior for a smooth surface .
 float newLinearRoughness = clamp ( distInteresectionToShadedPoint /
distInteresectionToProbeCenter * linearRoughness , 0, linearRoughness );
 return mix ( newLinearRoughness , linearRoughness , linearRoughness );
 }

 vec3 screen_to_view(vec3 screen)
 {
   float z = screen.z * 2.0 - 1.0;

   vec4 clipSpacePosition = vec4(screen.xy * 2.0 - 1.0, z, 1.0);
   vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

   // Perspective division
   viewSpacePosition /= viewSpacePosition.w;
   return viewSpacePosition.xyz;
 }

 vec3 screen_to_world_old(vec3 screen)
 {
   float z = screen.z * 2.0 - 1.0;

   vec4 clipSpacePosition = vec4(screen.xy * 2.0 - 1.0, z, 1.0);
   vec4 viewSpacePosition = projMatrixInv_old * clipSpacePosition;

   // Perspective division
   viewSpacePosition /= viewSpacePosition.w;

   vec4 worldSpacePosition = viewMatrixInv_old * viewSpacePosition;
   return worldSpacePosition.xyz;
 }

 vec3 view_to_world(vec4 view)
 {
   vec4 worldSpacePosition = viewMatrixInv * view;

   return worldSpacePosition.xyz;
 }

 vec3 world_to_prev_screen(vec4 world)
 {
   vec4 clip = old_mvp*world;
   clip.xyz = clip.xyz / clip.w;
   clip.xyz = clip.xyz * 0.5 + 0.5;
   return clip.xyz;
 }

 vec4 reprojectHitPosition(vec2 uv, float reflected_ray_length,float z,out vec2 uvs) {
  //  float z = FFX_DNSR_Reflections_LoadDepth(dispatch_thread_id);
    vec3 view_space_ray = screen_to_view(vec3(uv, z));

    // We start out with reconstructing the ray length in view space.
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    float surface_depth = length(view_space_ray);
    float ray_length = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray
    // of the same length "straight through" the reflecting surface
    // and reprojecting the tip of that ray to the previous frame.
    view_space_ray /= surface_depth; // == normalize(view_space_ray)
    view_space_ray *= ray_length;
    vec3 world_hit_position = view_to_world(vec4(view_space_ray, 1)); // This is the "fake" hit position if we would follow the ray straight through the surface.
    vec3 prev_hit_position = world_to_prev_screen(vec4(world_hit_position,1));
    vec2 history_uv = prev_hit_position.xy;

    vec4 tex = texture(old_reflectiondata,history_uv.xy);
    uvs = history_uv.xy;
     float cancel = (1-when_gt(prev_hit_position.z,1.0));
    tex.w *= cancel;
    tex.xyz *= cancel;
    return tex;
}

float reprojectDepth(vec3 world,out vec2 uvs)
{

  vec4 clip = old_mvp*vec4(world,1.0);
  clip.xyz = clip.xyz / clip.w;
  clip.xyz = clip.xyz * 0.5 + 0.5;
  float current_depth = clip.z;
  float old_depth = texture(old_depthtexture,clip.xy).r;
  uvs = clip.xy;
return   1 - when_gt(current_depth - 0.000005, old_depth);//*(1-when_gt(clip.z,1.0));
  //vec2 uvs = clip.xy;
  // vec4 tex = texture(old_reflectiondata,clip.xy);
  //  float cancel = ;
  // tex.w *= cancel;
  // tex.xyz *= cancel;
//  return screen_to_world_old(clip.xyz);
}



// float calcWeight(vec2 uv,  vec3 normal, float roughness,  float roughness_sigma_min, float roughness_sigma_max,  float temporalStabilityFactor) {
//
//
//     vec3 history_normal =   old_normal(texel_coords);
//     float history_roughness = old_roughness(texel_coords);
//
//     const float normal_sigma = 8.0;
//
//     float accumulation_speed = temporalStabilityFactor
//         * GetEdgeStoppingNormalWeight(normal, history_normal, normal_sigma)
//         * GetEdgeStoppingRoughnessWeight(roughness, history_roughness, roughness_sigma_min, roughness_sigma_max)
//         * GetRoughnessAccumulationWeight(roughness)
//         ;
//
//     return saturate(accumulation_speed);
// }

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness*roughness;

	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta);

	// from spherical coordinates to cartesian coordinates - halfway vector
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space H vector to world-space sample vector
  float lt = when_lt(abs(N.z) , 0.999);
	vec3 up          =  vec3(0.0, 0.0, 1.0)*lt + (1-lt)*vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}



vec3 SampleGGXVNDF(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2) {
    // Section 3.2: transforming the view direction to the hemisphere configuration
    vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
    vec3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    float r = sqrt(U1);
    float phi = 2.0 * PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

mat3 CreateTBN(vec3 N) {
    vec3 U;
    float fac = when_gt(abs(N.z) , 0.0);
    // if (abs(N.z) > 0.0) {
    //     float k = sqrt(N.y * N.y + N.z * N.z);
    //     U.x = 0.0; U.y = -N.z / k; U.z = N.y / k;
    // }
    // else {
    //     float k = sqrt(N.x * N.x + N.y * N.y);
    //     U.x = N.y / k; U.y = -N.x / k; U.z = 0.0;
    // }
    float a = N.z*fac + N.x*(1-fac);
    float k = sqrt(a * a + N.y*N.y);
    U.x = (N.y/k)*fac;
    U.y = (-N.z / k)*fac + (-N.x / k)*(1-fac);
    U.z = (N.y / k)*fac;

    mat3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return (TBN);
}

vec3 SampleReflectionVector(vec3 view_direction, vec3 normal, float roughness, vec2 u) {
    mat3 inv_tbn_transform = CreateTBN(normal);
    mat3 tbn_transform = transpose(inv_tbn_transform);

    vec3 view_direction_tbn = tbn_transform*-view_direction;

    float roughness2 = roughness*roughness;
    vec3 sampled_normal_tbn = SampleGGXVNDF(view_direction_tbn, roughness2,roughness2, u.x, u.y);


    vec3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    // Transform reflected_direction back to the initial space.

    return  inv_tbn_transform*reflected_direction_tbn;
}

// Blue Noise Sampler by Eric Heitz. Returns a value in the range [0, 1].
float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, float sample_dimension) {
    // Wrap arguments
    pixel_i = pixel_i & 127u;
    pixel_j = pixel_j & 127u;
    // sample_index = sample_index & 255u;
    // sample_dimension = sample_dimension & 255u;

    const uint ranked_sample_index = sample_index ^ 0;


    // Fetch value in sequence
    uint value = uint(32.0*(1.0-float(sample_dimension)) + 226.0*float(sample_dimension));//g_sobol_buffer[sample_dimension + ranked_sample_index * 256u];

    // // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ g_scrambling_tile_buffer[(uint(sample_dimension) ) + (pixel_i + pixel_j * 128u)*8u ];//

    // Convert to float and return
    return (float(value) + 0.5) / 256.0f;
}

vec2 SampleRandomVector2D(uvec2 pixel,uint offset) {
    vec2 u = vec2(
        mod(SampleRandomNumber(pixel.x, pixel.y, 0, 0.0) + float((uint(frame) + offset) & 0xFFu) * 1.61803398875f, 1.0f),
        mod(SampleRandomNumber(pixel.x, pixel.y, 0, 1.0) + float((uint(frame) + offset) & 0xFFu) * 1.61803398875f, 1.0f));
    return u;
}

vec2 values[9] = {vec2(-1,0),vec2(1,0),vec2(-1,1),vec2(1,1),vec2(-1,-1),vec2(1,-1),vec2(0,-1),vec2(0,1),vec2(0,0)};
// Estimates spatial reflection radiance standard deviation
vec3 neightborhood(vec2 uv) {
    vec3 color_sum = vec3(0.0);
    vec3 color_sum_squared = vec3(0.0);

    float radius = 1;
    float weight = 9;

    for (int i = 0 ; i < 9 ;i ++) {

            vec2 texel_coords = uv*screenSize + values[i];
            vec4 value2 = texture(old_reflectiondata,texel_coords/screenSize);
            vec3 value = value2.xyz/value2.w;
            color_sum += value;
            color_sum_squared += value * value;

    }

    vec3 color_std = (color_sum_squared - color_sum * color_sum / weight) / (weight - 1.0);
    return sqrt(max(color_std, 0.0));
}

float ClipAABB(vec3 aabb_min, vec3 aabb_max, vec3 prev_sample) {

    vec3 aabb_center = 0.5 * (aabb_max + aabb_min);
    vec3 extent_clip = 0.5 * (aabb_max - aabb_min) + 0.001;

    // Find color vector
    vec3 color_vector = prev_sample - aabb_center;
    // Transform into clip space
    vec3 color_vector_clip = color_vector / extent_clip;
    // Find max absolute component
    color_vector_clip = abs(color_vector_clip);
    float max_abs_unit = max(max(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z);

    if (max_abs_unit > 1.0) {
        return 1;//aabb_center + color_vector / max_abs_unit; // clip towards color vector
    }
    else {
        return 0;//prev_sample; // point is inside aabb
    }
}

float normalweight(vec3 normal_p, vec3 normal_q, float sigma) {
    return pow(max(dot(normal_p, normal_q), 0.0), sigma);
}

float roughnessweight(float roughness_p, float roughness_q, float sigma_min, float sigma_max) {
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

float synflag = 0;
vec4 traceCubes(float depthval,vec3 position,vec3 normal,vec3 refnormal,float roughness,vec3 viewDir,float metal,int quality)
{
  float alpha = 1;
  float index = -1;
  float bounded = 0;//CheckCollision(position,EnvBoxMin[0],EnvBoxMax[0]);
  float cur_dist = -1;
  float closest_probe = 0;
  float dist_2 = 1.0/0.0;//infinity :)

  ivec2 tile_id = ivec2(gl_FragCoord.xy / vec2(screenSize.x/16.0,screenSize.y/8.0));
  uint cluster_slice_id = getClusterZIndex(depthval);

  uint tile_index = tile_id.x + tile_id.y*num_tiles_x + cluster_slice_id*num_tiles_x*num_tiles_y;

  // uint s_firstLaneCellIdx = readFirstInvocationARB(tile_index);
  //
  // uint64_t laneMask = ballotARB(tile_index == s_firstLaneCellIdx);
  //
  // bool fastPath2 = (laneMask == ballotARB(true));
  // if (fastPath2)
  // {
  //   return vec4(1,0,0,0);
  // }

  uvec2 arr[2];
  arr[0] = offsets[tile_index/2 + (16*8*20)/2].xy;
  arr[1] = offsets[tile_index/2 + (16*8*20)/2].zw;
  uint count = arr[tile_index%2].y;
  uint offset = arr[tile_index%2].x;
  float fastNdotL = uintBitsToFloat(packHalf2x16(vec2(-1 ,1)));//dot(normal,refnormal);
  // index = 0;
  if (count ==0)
  {
    vec4 lookup_fast = textureLod(atmosphereCube,(refnormal*vec3(-1,1,1)),roughness*4.0);
    return vec4(lookup_fast.xyz,fastNdotL);
  }

  //
  for (int i = 0 ; i < count;i++)
  {
    uint a = offset + i;
    uint key =  acessbuffercubes[a/4][a%4];

    //*when_gt(dot((EnvBoxPos[key] - position),normal),0)

    float factor_normal = when_gt(dot((EnvBoxPos[key] - position),normal),0);
    float bounded_i = CheckCollision(position,EnvBoxMin[key],EnvBoxMax[key]);//*(factor_normal*synflag + (1.0-synflag));
    float dist = distance_squared(position,EnvBoxPos[key]);
  //  float nw = when_lt(dist,dist_2);
    // closest_probe = key*nw + closest_probe*(1 - nw);
    // dist_2 = dist*nw + dist_2*(1-nw);



    float replace = or(when_eq(index,-1),(1-when_eq(index,-1))*when_lt(dist,cur_dist) )*bounded_i;
    index = replace*key + (1-replace)*index;
    cur_dist = cur_dist*(1-replace) + dist*replace;//distance_squared(position,EnvBoxPos[int(index)]);

  }
  if (index == -1)
  {
    vec4 lookup_fast = textureLod(atmosphereCube,(refnormal*vec3(-1,1,1)),roughness*4.0);
    return vec4(lookup_fast.xyz,fastNdotL);
  }



  float dist_to_hit = 0;
  float occ = 1;

  #ifdef OCCLUSION
  // if (outTexcoord.x < 0.5)
  // {
  const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);
  occ = (1 - clamp(dot(vec4(normalize(refnormal),1).yzxw, texture(occTex,outTexcoord)*sh2_weight),0,1));
  //}

  #endif
  vec3 l;

  #ifdef REALTIME_BDRF

    float metalfactor = 0;//when_eq(metal,0)*when_gt(roughness,0.5);

  const uint sample_count = 30 ;//+ uint(when_gt(roughness,0.2))*31;//uint(clamp(roughness*3,1,2));
  vec3 accum = vec3(0);
  float total_weight = 0;
  vec4 lookup;
  float h = 0;
  // roughness = 0.5;
    vec3 lu;
  // for (uint i = 0 ; i < sample_count;i++)
  // {
    //
    //

    float NdotL = -1;
    int iterations = 0;
    vec3 L;

    vec2 Xi = SampleRandomVector2D(uvec2(gl_FragCoord.xy),iterations);//Hammersley(uint(noise*939) , 1001 );
    Xi.x = mix(Xi.x,0.0,0.2);
    if (roughness < 0.1)
    {
      Xi = vec2(0,0);
    }

    L =  -SampleReflectionVector(viewDir,normal,roughness,Xi);

    NdotL = dot(normal, -L);



    NdotL = max(NdotL,0);

  //  float coarse = raymarchCoarse(position,-L,reflectionsDepth,index,EnvBoxPos[int(index)]);
    vec4 lookup2;
    L = L*(1-metalfactor) + metalfactor*-refnormal;


    int iters[] = {2,4,6};
    int steps[] = {12,17,25};
    float h2 = raymarch(position,-L,reflectionsDepth,index,EnvBoxPos[int(index)],lu,dist_to_hit,iters[quality - 1],steps[quality -1]);//Hit((position - EnvBoxPos[int(index)]), refnormal, reflectionsDepth,index); // ray hit


    float cone = clamp(dist_to_hit/500,0,1)*roughness*roughness;
    lookup2 = textureLod(reflections,vec4(lu*vec3(-1,1,1),index),0);

     lookup2 = lookup2*h2 + textureLod(atmosphereCube,(refnormal*vec3(-1,1,1)),roughness*4.0)*(1-h2);
   NdotL = NdotL*(h2) + (1-h2)*1;
    h = clamp(h + h2,0,1);
    accum += lookup2.xyz*(NdotL)*when_gt(NdotL,0);
    total_weight += NdotL*when_gt(NdotL,0);


//  }



  lookup.xyz = accum*when_gt(total_weight,0);
  alpha = uintBitsToFloat(packHalf2x16(vec2(dist_to_hit,total_weight)))*h2 + fastNdotL*(1 - h2) ;



  // uvec2 pixel = uvec2(gl_FragCoord.xy);
  // float n = mod(SampleRandomNumber(pixel.x, pixel.y, pixel.x, 1u) + float(uint(1) & 0xFFu) * 1.61803398875f, 1.0f);
  // lookup.xyz = vec3(n);
  // lookup.xyz = vec3(disoccluded);//vec3(when_lt(uvs.x,1.0)*when_lt(uvs.y,1.0)*when_gt(uvs.x,0.0)*when_gt(uvs.y,0.0)*disoccluded);
  // alpha = 1;
  // lookup.xyz = vec3(float(count)/5.0);
  // alpha = 1;

  #elif defined(FASTCUBES)
  vec4 lookup = vec4(1,1,1,1);//textureLod(reflections,vec4(refnormal*vec3(-1,1,1),index),roughness*4.0);//
  alpha = fastNdotL;
  #else
  float h = raymarch(position,refnormal,reflectionsDepth,index,EnvBoxPos[int(index)],l,dist_to_hit);//Hit((position - EnvBoxPos[int(index)]), refnormal, reflectionsDepth,index); // ray hit
  //bounded = or(bounded,1-when_lt( (texture(reflectionsDepth,vec4(l*vec3(-1,1,1),index)).r) , distance(position,EnvBoxPos[int(index)])));
  // roughness = computeDistanceBaseRoughness(dist_to_hit,200,roughness);
  vec4 lookup = textureLod(reflections,vec4(l*vec3(-1,1,1),index),roughness*4.0);
  // lookup = vec4(l,0);
  lookup = lookup*h + textureLod(atmosphereCube,(l*vec3(-1,1,1)),roughness*4.0)*(1-h);
  #endif

  //  return vec4(vec3(when_lt(NdotL,0.01)),alpha);
  return vec4(lookup.xyz*occ,alpha);
}


void main(void)
{
  vec3 ti = texture(texInfo,outTexcoord).xyz;
   synflag = clamp( 1 - sign(ti.z),0,1);

  float depthval = texture2D(depthTex,outTexcoord ).r;
  vec4 viewSpacePosition;
  vec3 position = WorldPosFromDepth(depthval,viewSpacePosition);


  vec3 viewDir    = normalize(position - eyePos);
  vec3 normalt = texture(normalTex,outTexcoord).xyz;
  vec3 normal = (vec4(decodeNormal(normalt.xy),0)).xyz;
  surfnormal = normal;
  vec3 refnormal = reflect(viewDir,normal);
  float roughness = normalt.z;

  vec4 a = texture(texCoordTex, outTexcoord);
float metallic  = a.a;
vec3 albedo     = pow(a.rgb,vec3(2.2));//pow(texture(albedoMap, outTexcoord).rgb, vec3(2.2));

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallic);
  vec2 brdf  = texture(brdfLUT, vec2(max(dot(normal, -viewDir), 0.0), roughness)).rg;
  vec3 F = fresnelSchlick(max(dot(normal, -viewDir), 0.0), F0, roughness);


  vec3 val  = F*brdf.x + brdf.y;

  // float ismetal = when_gt(metallic,0.001);
  // float rough_gt01 = when_gt(roughness,0.1);
  // float rough_gt05 = when_gt(roughness,0.5);
  // float ishighfresnel = when_gt(max(max(val.x,val.y),val.z) , 0.001);
  //
  //
  //
  // vec3 normalgrad_v = fwidth(normal);
  // float normalgrad = max(normalgrad_v.x,max(normalgrad_v.y,normalgrad_v.z));
  // float is_high_normalgradient = when_gt(normalgrad,0.1);
  //
  //
  // float hq = clamp(ismetal*(1-rough_gt05) + (1-ismetal)*(1-rough_gt01),0,1);
  // float mq = (1-hq)*clamp(ismetal*(rough_gt05) + (1-ismetal)*(rough_gt01)*ishighfresnel,0,1);
  // float lq = (1-hq)*(1-mq);
  //
  // float lv = hq*3 + mq*2 + lq*1;
  // lv = lv - is_high_normalgradient;
  // lv = clamp(lv,1,3);
  float dist_2 = distance_squared(eyePos,position);
  float lv = 3;//when_lt(dist_2,200*200) + when_lt(dist_2,600*600) + 1;

  int quality = int(lv);

   //max(max(val.x,val.y),val.z) < 0.001;
  // bool fastpath_condit = mod((gl_FragCoord.x )/16.0,2) > 1  && mod((gl_FragCoord.y )/16.0,2) < 1 && roughness > 0.1;
  // bool fastpath = allInvocations(fastpath_condit);
  bool fastpath = false;

  // bool screen_space_condit = dot(viewDir,refnormal) < 0;
  // bool screenspace = allInvocations(screen_space_condit);
  // if (fastpath)
  // {
  //   fragColor = vec4(0,0,0,1);
  // }
  // else
  // {
  //   fragColor = vec4(1,1,1,1);
  // }

  //fragColor = vec4(sign(ti.z),0,0,uintBitsToFloat(packHalf2x16(vec2(-1 ,1))));
  if (fastpath || metallic < 0.01 && roughness > 0.9 )
  {
    fragColor = vec4(0,0,0,1);////traceCubesFast( depthval, position, normal, refnormal, roughness,viewDir);
  }
  else
  {
    fragColor =  traceCubes( depthval, position, normal, refnormal, roughness,viewDir,metallic,quality);//*vec4(val,1);
  }
}
