#version 460 core
// #define OCCLUSION
//#define REALTIME_BDRF
in vec2 outTexcoord;
layout (location = 2) out vec4 fragColor;
layout(binding=4) uniform sampler2D brdfLUT;
layout(binding=3) uniform sampler2D prefilterMap;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=22) uniform samplerCubeArray reflections;
layout(binding=21) uniform samplerCubeArray reflectionsDepth;
layout(binding=23) uniform samplerCube atmosphereCube;
layout(binding=5) uniform sampler2D texCoordTex;
layout(binding=0) uniform sampler2D finalColor;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform vec3 eyePos;
uniform vec2 screenSize;
uniform mat4 viewMat;
uniform mat4 projMat;
uniform mat4 mdlproj;

uniform float zNear = 0.1;
uniform float zFar = 1000;
uint num_tiles_x = 16;
uint num_tiles_y = 8;
uint num_slices = 20;


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

layout (std140 , binding = 4) uniform Offsets_buffer //size of grid
{
  uniform uvec4 offsets[(16*8*20)];//pack into vec4 for std140
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

float distance_squared(vec3 a,vec3 b)
{
  vec3 d = a -b;
  return dot(d,d);
}
vec3 surfnormal;
float raymarch(vec3 worldspaceRay,vec3 worldSpaceDirection,samplerCubeArray mp,float cubeindex,vec3 cubepos,out vec3 result)
{
  vec3 worldSpaceRay = worldspaceRay;
  // vec3 cubespace_pos = worldSpaceRay - cubepos;
  // vec3 cubespace_dir = worldSpaceDirection;
  //vec3 cubespace = worldspaceRay - cubepos;
  float depth = 2;// texture(mp, vec4(worldSpaceDirection*vec3(-1,1,1),cubeindex)).r;
  vec3 minp = vec3(worldspaceRay);
  vec3 maxp = vec3(worldspaceRay +worldSpaceDirection*200 );
  float hit = 1;
  int steps = 20;
  float delta = 200/steps;
  // for (int i = 0; i < 32; i++) {
  //     vec3 p = worldspaceRay + worldSpaceDirection*depth;
  //     maxp = p;
  //     float dist = texture(mp, vec4((p - cubepos)*vec3(-1,1,1),cubeindex)).r;
  //     float cond = when_lt(dist,distance(p,cubepos));
  //     if (dist < distance(p,cubepos)  )
  //     {
  //       hit = 1;
  //       break;
  //     }
  //     minp = p;
  //     depth += delta;
  //   }
    //binary search
    //refines by delta*0.5^iterations



    vec3 mid = vec3(0);
    for (int i = 0 ; i < 10;i++)
    {
       mid = (maxp + minp)*vec3(0.5);
      float dist = texture(mp, vec4((mid - cubepos)*vec3(-1,1,1),cubeindex)).r;
      float cmp = when_lt(dist*dist,distance_squared(mid,cubepos));
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

    // vec3 test_pos = worldSpaceRay + surfnormal*3;
    // float cubetoworld_squared = texture(mp, vec4(normalize(test_pos - cubepos)*vec3(-1,1,1),cubeindex)).r;
    // cubetoworld_squared = cubetoworld_squared*cubetoworld_squared;
    // hit = when_lt(distance_squared(test_pos,cubepos),cubetoworld_squared);

   mid = (maxp + minp)*vec3(0.5);
  vec3 intersection = mid;//worldspaceRay + worldSpaceDirection * depth;
  result =  intersection - cubepos;
  float dist = texture(mp, vec4((result)*vec3(-1,1,1),cubeindex)).r;
  hit = when_lt(dist,1000);//when_lt(dist*dist,distance_squared(maxp,cubepos));

  result = result*hit + (1-hit)*worldSpaceDirection;
  return hit;
}


const int NITER = 3;
vec3 Hit(vec3 x, vec3 R, samplerCubeArray mp,float cubeindex)
{
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
  // float ddl = 0;
  // float llp;
  // for(int i = 0; i < NITER; i++) {
  //
  //   llp = length(l)/(texture(mp,vec4(l*vec3(-1,1,1),cubeindex)).r);
  //   float l_p = when_lt(llp,1.0);
  //   float n_lp = not(l_p);
  //   dun = dl*l_p + dun*n_lp;
  //   dov = dl*n_lp + dov *l_p;
  //   pun = llp*l_p + pun*n_lp;
  //   pov = llp*n_lp + pov *l_p;
  //
  //   ddl = l_p*(when_eq(dov,0)*rl*(1 - llp) + not(when_eq(dov,0))*(dl-dov)*(1-llp)/n0(llp-pov) );
  //
  //   ddl += n_lp*(when_eq(dun,0)*rl*(1 - llp) + not(when_eq(dun,0))*(dl-dun)*(1-llp)/n0(llp-pun) );
  //
  //   dl = max(dl + ddl, 0); // avoid flip
  //   l = x + R * dl;
  // }
  // float fdist  = texture(mp, vec4(l*vec3(-1,1,1),cubeindex)).r;
  return l;
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

const float PI = 3.14159265359;

vec2 Hammersley(uint i, uint N)
{
  return vec2(
    float(i) / float(N),
    float(bitfieldReverse(i)) * 2.3283064365386963e-10
  );
}
// ----------------------------------------------------------------------------
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
	vec3 up          = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent   = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
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
float DistanceSquared(vec2 a, vec2 b)
{
    a -= b;
    return dot(a, a);
}

const float rayTraceStride = 1.0f;           // Step in horizontal or vertical pixels between samples.
const float rayTraceMaxStep = 512.0f;        // Maximum number of iterations. Higher gives better images but may be slow.
const float rayTraceHitThickness = 1.5f;     // Thickness to ascribe to each pixel in the depth buffer.
const float rayTraceHitThicknessBias = 7.0f; // Bias to control the thickness along distance.
const float rayTraceMaxDistance = 1000.0f;   // Maximum camera-space distance to trace before returning a miss.
const float rayTraceStrideCutoff = 100.0f;   // More distant pixels are smaller in screen space. This value tells at what point to
vec3 marchRay(vec3 position,vec3 normal,vec3 viewDir,vec3 raydir)
{

  vec2 uv = vec2(0);
  float t = 0;
  float hit = 0;
  for (int i = 0 ; i < 40;i++)
  {
    t+= 7.5;
    vec4 world = vec4(position + raydir*t,1);
    vec4 clip = mdlproj*world;
    clip.xyz /= clip.w;
    clip.xyz = (clip.xyz+1)*0.5;
    uv =clip.xy;
    float d = texture(depthTex,uv).r;
    float d1 = screen2EyeDepth(d,zNear,zFar);
    float d2 = screen2EyeDepth(clip.z,zNear,zFar);
    if (d < clip.z && (d2 - d1) <10)
    {
      hit = 1;
      break;
    }
  }

  vec3 oldpos = position + raydir*(t-7.5);
  vec3 newpos = position + raydir*(t);
  vec3 pos;// = (oldpos + newpos)/vec3(0.5);
  if (hit == 1)
  {
    for (int i = 0 ; i < 7;i++)
    {
      pos = (oldpos + newpos)*vec3(0.5);
      vec4 world = vec4(pos,1);
      vec4 clip = mdlproj*world;
      clip.xyz /= clip.w;
      clip.xyz = (clip.xyz+1)*0.5;
      uv =clip.xy;
      float d = texture(depthTex,uv).r;
      float d1 = screen2EyeDepth(d,zNear,zFar);
      float d2 = screen2EyeDepth(clip.z,zNear,zFar);
      if (d < clip.z && (d2 - d1) <5)
      {
        newpos = pos;
      }
      else
      {
        oldpos = pos;
      }
    }
  }

  float bound = when_lt(uv.x,1)*when_gt(uv.x,0)*when_lt(uv.y,1)*when_gt(uv.y,0);

  return vec3(uv*hit*bound,hit*bound);

}

// vec3 marchRay(vec3 position,vec3 normal,vec3 viewDir,vec3 raydir)
// {
//
//   if (dot(viewDir,raydir) < 0)
//   {
//     return vec3(0,0,0);
//   }
//
//   vec4 world = vec4(position + raydir*0,1);
//   vec4 clip1 = mdlproj*world;
//   clip1.xyz /= clip1.w;
//   clip1.xyz = (clip1.xyz+1)*0.5;
//
//   vec4 world2 = vec4(position + raydir*100,1);
//   vec4 clip2 = mdlproj*world2;
//   clip2.xyz /= clip2.w;
//   clip2.xyz = (clip2.xyz+1)*0.5;
//
//   float k0 = 1.0f / clip1.w;
//   float k1 = 1.0f / clip2.w;
//
//   vec3 csOrig = (world*viewMat).xyz;
//   vec3 csRayEnd = (world*viewMat).xyz;
//
//   vec3 Q0 = csOrig * k0;
//   vec3 Q1 = csRayEnd * k1;
//
//   vec2 P0 = clip1.xy*screenSize;
//   vec2 P1 = clip2.xy*screenSize;
//
//   //degenerate case
//   P1 += (DistanceSquared(P0, P1) < 0.0001f) ? vec2(0.01f, 0.01f) : vec2(0.0f);
//   vec2 screenOffset = P1 - P0;
//
//   bool permute = false;
//   if (abs(screenOffset.x) < abs(screenOffset.y))
//   {
//       permute = true;
//       screenOffset = screenOffset.yx;
//       P0 = P0.yx;
//       P1 = P1.yx;
//   }
//
//   float stepDirection = sign(screenOffset.x);
//   float stepInterval = stepDirection / screenOffset.x;
//
//   // Track the derivatives of Q and k
//    vec3 dQ = (Q1 - Q0) * stepInterval;
//    float dk = (k1 - k0) * stepInterval;
//
//    vec2 dP = vec2(stepDirection, screenOffset.y * stepInterval);
//
//    // Scale derivatives by the desired pixel stride and then offset the starting values by the jitter fraction
//    float strideScale = 1.0f - min(1.0f, csOrig.z / rayTraceStrideCutoff);
//    float stride = 1.0f + strideScale * rayTraceStride;
//
//    dP *= 1;
//    dQ *= 1;
//    dk *= 1;
//
//    P0 += dP * 0.01;
//    Q0 += dQ * 0.01;
//    k0 += dk * 0.01;
//
//    vec4 PQk = vec4(P0, Q0.z, k0);
//    vec4 dPQk = vec4(dP, dQ.z, dk);
//    vec3 Q = Q0;
//
//    // Adjust end condition for iteration direction
//    float end = P1.x * stepDirection;
//
//    // raytrace iterations based on roughness
//    // Matte materials will get less samples
//    //float roughnessTraceStep = max(rayTraceMaxStep * (1.0 - roughness), 1.0f);
//
//    float stepCount = 0.0f;
//    float level = 0.0f; // 1.0f start level. Parameter?
//
//    float prevZMaxEstimate = csOrig.z;
//    float rayZMin = prevZMaxEstimate;
//    float rayZMax = prevZMaxEstimate;
//    float sceneZMax = rayZMax + 100000.0f;
//
//    float k = k0;
//    vec2 hitPixel;
//   // float  end = P1.x * stepDirection;
//    for (vec2 P = P0;
//         ((P.x * stepDirection) <= end) && (stepCount < rayTraceMaxStep) &&
//         ((rayZMax < sceneZMax - rayTraceHitThickness) || (rayZMin > sceneZMax)) &&
//          (sceneZMax != 0);
//         P += dP, Q.z += dQ.z, k += dk, ++stepCount) {
//
//        rayZMin = prevZMaxEstimate;
//        rayZMax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
//        prevZMaxEstimate = rayZMax;
//        if (rayZMin > rayZMax) {
//           float t = rayZMin; rayZMin = rayZMax; rayZMax = t;
//        }
//
//        hitPixel = permute ? P.yx : P;
//        // is different than ours in screen space
//        sceneZMax = screen2EyeDepth(texture(depthTex,hitPixel/screenSize).r,zNear,zFar);//texelFetch(csZBuffer, int2(hitPixel), 0);
//    }
//
//    // Advance Q based on the number of steps
//    Q.xy += dQ.xy * stepCount;
//    return vec3(hitPixel/(screenSize),1);
//    // hitPoint = Q * (1.0 / k);
//
//   // vec2 uv = vec2(0);
//   // float t = 0;
//   // float hit = 0;
//   // for (int i = 0 ; i < 100;i++)
//   // {
//   //   t+= 2;
//   //   vec4 world = vec4(position + raydir*t,1);
//   //   vec4 clip = mdlproj*world;
//   //   clip.xyz /= clip.w;
//   //   clip.xyz = (clip.xyz+1)*0.5;
//   //   uv =clip.xy;
//   //   float d = texture(depthTex,uv).r;
//   //   float d1 = screen2EyeDepth(d,zNear,zFar);
//   //   float d2 = screen2EyeDepth(clip.z,zNear,zFar);
//   //   if (d < clip.z && (d2 - d1) < 5)
//   //   {
//   //     hit = 1;
//   //     break;
//   //   }
//   // }
//   // float bound = when_lt(uv.x,1)*when_gt(uv.x,0)*when_lt(uv.y,1)*when_gt(uv.y,0);
//
//   //return vec3(uv*hit*bound,hit*bound);
//
// }

float reconstructCSZ(float depthBufferValue, vec3 clipInfo) {
      return clipInfo[0] / (depthBufferValue * clipInfo[1] + clipInfo[2]);
}

float distanceSquared(vec2 a, vec2 b) { vec2 c=a -b ; return dot(c, c); }

// Returns true if the ray hit something
vec3 traceScreenSpaceRay1(
 // Camera-space ray origin, which must be within the view volume
 vec3 csOrig,

 // Unit length camera-space ray direction
 vec3 csDir,

 // A projection matrix that maps to pixel coordinates (not [-1, +1]
 // normalized device coordinates)
 mat4 proj,

 // Dimensions of csZBuffer
 vec2 csZBufferSize,

 // Camera space thickness to ascribe to each pixel in the depth buffer
 float zThickness,

 // (Negative number)
 float nearPlaneZ,

 // Step in horizontal or vertical pixels between samples. This is a float
 // because integer math is slow on GPUs, but should be set to an integer >= 1
 float stride,

 // Number between 0 and 1 for how far to bump the ray in stride units
 // to conceal banding artifacts
 float jitter,

 // Maximum number of iterations. Higher gives better images but may be slow
 const float maxSteps,

 // Maximum camera-space distance to trace before returning a miss
 float maxDistance


 ) {
   vec2 hitPixel = vec2(0);
    // Clip to the near plane
    float rayLength = ((csOrig.z + csDir.z * maxDistance) > nearPlaneZ) ?
        (nearPlaneZ - csOrig.z) / csDir.z : maxDistance;
    vec3 csEndPoint = csOrig + csDir * rayLength;

    // Project into homogeneous clip space
    vec4 H0 = proj * vec4(csOrig, 1.0);
    vec4 H1 = proj * vec4(csEndPoint, 1.0);
    float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;

    // The interpolated homogeneous version of the camera-space points
    vec3 Q0 = csOrig * k0, Q1 = csEndPoint * k1;

    // Screen-space endpoints
    vec2 P0 = H0.xy * k0, P1 = H1.xy * k1;
    P0 = P0*0.5 + 0.5;
    P1 = P1*0.5 + 0.5;

    P0*= screenSize;
    P1*= screenSize;

    //If the line is degenerate, make it cover at least one pixel
    // to avoid handling zero-pixel extent as a special case later
    P1 += vec2((distanceSquared(P0, P1) < 0.0001) ? 0.01 : 0.0);
    vec2 delta = P1 - P0;
    //
    // Permute so that the primary iteration is in x to collapse
    // all quadrant-specific DDA cases later
    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        // This is a more-vertical line
        permute = true;
        delta = delta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    //
    float stepDir = sign(delta.x);
    float invdx = stepDir / delta.x;

    // Track the derivatives of Q and k
    vec3  dQ = (Q1 - Q0) * invdx;
    float dk = (k1 - k0) * invdx;
    vec2  dP = vec2(stepDir, delta.y * invdx);

    // Scale derivatives by the desired pixel stride and then
    // offset the starting values by the jitter fraction
    dP *= stride; dQ *= stride; dk *= stride;
    //P0 += dP * jitter; Q0 += dQ * jitter; k0 += dk * jitter;

    // Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, k from k0 to k1
    vec3 Q = Q0;

    // Adjust end condition for iteration direction
    float  end = P1.x * stepDir;

    float k = k0, stepCount = 0.0, prevZMaxEstimate = csOrig.z;
    float rayZMin = prevZMaxEstimate, rayZMax = prevZMaxEstimate;
    float sceneZMax = rayZMax + 100000;
    for (vec2 P = P0;
         ((P.x * stepDir) <= end) && (stepCount < maxSteps) &&
         ((rayZMax < sceneZMax - zThickness) || (rayZMin > sceneZMax)) &&
          (sceneZMax != 0);
         P += dP, Q.z += dQ.z, k += dk, ++stepCount) {

        rayZMin = prevZMaxEstimate;
        rayZMax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
        prevZMaxEstimate = rayZMax;
        if (rayZMin > rayZMax) {
           float t = rayZMin; rayZMin = rayZMax; rayZMax = t;
        }

        hitPixel = permute ? P.yx : P;
        // You may need hitPixel.y = csZBufferSize.y - hitPixel.y; here if your vertical axis
        // is different than ours in screen space
        sceneZMax = screen2EyeDepth(texture(depthTex, hitPixel/screenSize).r,zNear,zFar);//vec3(zNear  * zFar, -zNear - -zFar, -zFar)
    }

    // Advance Q based on the number of steps
    Q.xy += dQ.xy * stepCount;
    bool hit = (rayZMax >= sceneZMax - zThickness) && (rayZMin < sceneZMax);
    return  mix(vec3(0),vec3(hitPixel/screenSize,1),hit);
}



void main(void)
{

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
  bool fastpath_condit = max(max(val.x,val.y),val.z) < 0.3;
  bool fastpath = allInvocations(fastpath_condit);




  // if (used == 0)
  // {
  //   discard;
  // }

  // if (used == 0 )
  // {
  //  fragColor =  vec4(specular,1);
  // }
  // else
  // {
 vec3 tr = marchRay(position,normal,viewDir,refnormal);

  // vec2 hp = vec2(0);
// vec3 tr = traceScreenSpaceRay1((viewMat*vec4(position,1)).xyz, (viewMat*vec4(refnormal,0)).xyz,projMat,screenSize,20,-zNear,1, 0,100, 500);
   vec3 tr2= texture(finalColor,tr.xy).xyz;
     fragColor =  vec4(tr2,1)*tr.z;
  // }



 // }

}
