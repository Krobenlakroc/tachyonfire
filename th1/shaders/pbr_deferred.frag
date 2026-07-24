// #version 460 core
// #define POSTPROCESS
// #define USE_FOG_POST
// #define TH_Z_SLICES 15
// #define TH_Z_MEMORY 10
//#define OCCLUSION
#define STOCHASTIC_TRUE
#define texture2D texture
//#define FULLBRIGHT
in vec2 outTexcoord;
layout (location = 0) out vec3 fragColor;
const float constantbias = 0.003;//0.01*0.25;//0.00002;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform mat4 lightSpaceMatrix;
uniform mat4 lightSpaceMatrices[48];
uniform vec3 eyePos;

// uniform float fog_dens = 0.007;

const float PI = 3.14159265359;
layout(binding=5) uniform sampler2D texCoordTex;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=0) uniform sampler2D irradianceMap;
layout(binding=32) uniform sampler2D prefilterMap;
layout(binding=4) uniform sampler2D brdfLUT;
layout(binding=23) uniform samplerCube atmosphereCube;
layout(binding=33) uniform sampler2D forward;
#ifdef USE_FOG_POST
layout(binding=38) uniform sampler2D fog_tex;
#endif
uniform mat4 viewMat;
uniform mat4 projMat;
uniform vec2 invWindow;
uniform vec2 screenSize;
uniform float sky_boost = 1.0;
layout(binding=24) uniform sampler2D shadowMap;
uniform vec3 shadowPos;

layout(binding=39) uniform sampler2D occTex;

layout(binding=41) uniform sampler3D bitmasksTex;

uniform float shadow_blur_radius = 0.001;
uniform float th_shadowmap_resolution = 1024;
uniform float th_shadowmap_sun_scale = 1;
uniform float th_shadowmap_omni_scale = 1;
uniform float omni_light_jitter = 10.0;
uniform float zNear = 0.1;
uniform float zFar = 1000;

uniform float displayGamma = 2.2;
uniform float displayExposure = 0.0;

uint num_tiles_x = 16;
uint num_tiles_y = 8;
uint num_slices = TH_Z_SLICES;

uniform vec3 sunlightColor;
uniform vec3 sunlightDir;

// uniform vec3 blackbody_barrel_color = vec3(0,0,0);

uniform float blackbody_barrel_tip_temp = 0.0;
uniform float blackbody_barrel_shape = 0.0;

uniform vec3 blackbody_barrel_pointa = vec3(0,0,0);

uniform vec3 blackbody_barrel_pointb = vec3(1,0,0);

uniform float blackbody_maxtemp = 3000.0;

// uniform float time;

uniform float glow_factor = 0.0;
uniform vec3 glow_color;

struct Light
{
  mat3 a;
  mat3 b;
  mat3 c;
};
layout(binding=20) uniform sampler2D lights;
uniform vec2 dims_harmtex;
uniform vec3 goffset ;
uniform vec3 gridsize ;
uniform float griddist ;

uniform vec3 goffset_occ ;
uniform vec3 gridsize_occ ;
uniform float griddist_occ ;
uniform float th_crosshair_size = 1.0;

struct PointLight
{
  vec4 pos;//0 - 16
  vec4 color;//16 - 32
  mat4 lightmat; //32 - 96
  vec4 shadowindex; //96 - 112
  vec4 pos2;
};

layout (std140 , binding = 2) uniform Lights
{
  uniform PointLight pointlights[256];
};

layout (std140 , binding = 3) uniform Access_Buffer //
{
  uniform uvec4 acessbuffer[4096];//use the max amount of memory
};

layout (std140 , binding = 4) uniform Offsets_buffer //size of grid
{
  uniform uvec4 offsets[(16*8*TH_Z_MEMORY)];//pack into vec4 for std140
};

layout (std430 , binding = 0) restrict readonly buffer BlueNoise //size of grid
{
   uint g_scrambling_tile_buffer[128*128*8];
};

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

vec3 WorldPosFromDepth(vec2 tc,float depth,out float linDepth) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(tc * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;
  // linDepth = viewSpacePosition.z;
  // Perspective division

  viewSpacePosition /= viewSpacePosition.w;

  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
}

float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

vec3 aces_approx(vec3 v)
{
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

float when_ge(float x, float y) {
  return 1.0 - max(sign(y - x), 0.0);
}

float when_le(float x, float y) {
  return 1.0 - when_gt(x,y);
}

float when_eq(float x, float y) {
  return  1- abs(sign(x - y));
}

/*
r
----
1 2-r
3 4-
----
5 6
*/


void indexCubeMap(vec3 d, inout float face, inout float s, inout float t)
{

	vec3 absd;

	float sc, tc, ma;

	absd.x = abs(d.x);

	absd.y = abs(d.y);

	absd.z = abs(d.z);

	face = 0.0;
  ma = 0.0;
  sc = 0.0;
  tc = 0.0;

   float f01_cond = when_ge(absd.x,absd.y)*when_ge(absd.x,absd.z);



   tc += -d.y*f01_cond;
   ma += absd.x*f01_cond;
   sc += d.z*f01_cond *(-1.0*when_gt(d.x,0.0f) + 1.0*when_le(d.x,0.0f));
   face += f01_cond*(0.0*when_gt(d.x,0.0f) + 1.0*when_le(d.x,0.0f));


   float f23_cond = when_ge(absd.y,absd.x)*when_ge(absd.y,absd.z);

   tc += d.z*f23_cond*(1.0*when_gt(d.y,0.0f) + -1.0*when_le(d.y,0.0f));
   ma += absd.y*f23_cond;
   sc += d.x*f23_cond ;
   face = face*(1.0-f23_cond)+ f23_cond*(2.0*when_gt(d.y,0.0f) + 3.0*when_le(d.y,0.0f));



   float f45_cond = when_ge(absd.z,absd.x)*when_ge(absd.z,absd.y);

   tc += -d.y*f45_cond;
   ma += absd.z*f45_cond;
   sc += d.x*f45_cond*(1.0*when_gt(d.z,0.0f) + -1.0*when_le(d.z,0.0f)) ;
   face += f45_cond*(4.0*when_gt(d.z,0.0f) + 5.0*when_le(d.z,0.0f));


   s = (((sc / ma) + 1.0f) * 0.5f)* (1.0 - when_eq(ma,0.0f));
   t = (((tc / ma) + 1.0f) * 0.5f)* (1.0 - when_eq(ma,0.0f));

}
//dying is natral, everything that has ever lived has died. The way we live isn't natural, you should be more worried about living than dying.
//https://stackoverflow.com/questions/15095909/from-rgb-to-hsv-in-opengl-glsl
// All components are in the range [0…1], including hue.
vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0…1], including hue.
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}


const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 3; //3
const float colorS = 8.0;
//10
vec3 Uncharted2Tonemap(vec3 x)
{
     return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

const float samplesCount_fog = 2;

const float samplesCount = TH_SHADOW_NSAMPLES;

const float samplesCountPoint = 4;

vec3 VogelSphereSample(float sampleIndex,  float phi)
{
  float GoldenAngle = 2.4f;


  float theta = sampleIndex * GoldenAngle + phi;
  float z = (1.0 - (1.0/samplesCountPoint)) * ( 1.0 - ((2.0*sampleIndex)/(samplesCountPoint - 1) ));
  float r = sqrt( 1 - z*z);

  float sine = sin(theta);
  float cosine = cos(theta);

  return vec3(r * cosine, r * sine,z);
}

vec3 VogelSphereSample_fog(float sampleIndex,  float phi)
{
  float GoldenAngle = 2.4f;


  float theta = sampleIndex * GoldenAngle + phi;
  float z = (1.0 - (1.0/samplesCount_fog)) * ( 1.0 - ((2.0*sampleIndex)/(samplesCount_fog - 1) ));
  float r = sqrt( 1 - z*z);

  float sine = sin(theta);
  float cosine = cos(theta);

  return vec3(r * cosine, r * sine,z);
}

vec2 VogelDiskSample(float sampleIndex,  float phi)
{
  float GoldenAngle = 2.4f;

  float r = sqrt(sampleIndex + 0.5f) / sqrt(samplesCount);
  float theta = sampleIndex * GoldenAngle + phi;

  float sine = sin(theta);
  float cosine = cos(theta);

  return vec2(r * cosine, r * sine);
}

vec2 VogelDiskSampleFog(float sampleIndex,  float phi)
{
  float GoldenAngle = 2.4f;

  float r = sqrt(sampleIndex + 0.5f) / sqrt(samplesCount_fog);
  float theta = sampleIndex * GoldenAngle + phi;

  float sine = sin(theta);
  float cosine = cos(theta);

  return vec2(r * cosine, r * sine);
}

float InterleavedGradientNoise(vec2 position_screen)
{
  vec3 magic = vec3(0.06711056f, 0.00583715f, 52.9829189f);
  return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

// float AvgBlockersDepthToPenumbra(float lightSize, float z_shadowMapView, float avgBlockersDepth)
// {
//   return lightSize * (z_shadowMapView - avgBlockersDepth) / avgBlockersDepth;
// }

float AvgBlockersDepthToPenumbra(float z_shadowMapView, float avgBlockersDepth)
{
  float penumbra = (z_shadowMapView - avgBlockersDepth) / avgBlockersDepth;
  // penumbra *= penumbra;
  return penumbra*1;//clamp( penumbra,0.05,1.0);
}


  //make less when you make the shadow region bigger
float shadowmaxsize = 0.015*0.5*2;//0.15
float penumbrasize = 0.05*2;//0.05;
float Penumbra(float gradientNoise, vec2 shadowMapUV, float z_shadowMapView,vec2 deltas,sampler2D smap)
{
  float avgBlockersDepth = 0.0f;
  float blockersCount = 0.0f;

  for(float i = 0; i < samplesCount; i++)
  {
    vec2 sampleUV = VogelDiskSample(i, gradientNoise);
    vec2 offset =penumbrasize * sampleUV;
    sampleUV = shadowMapUV + offset;
    float bias = deltas.x*offset.x + deltas.y*offset.y;
    float sampleDepth = texture(smap, sampleUV).r;

    // if(sampleDepth < z_shadowMapView)
    // {
    //   avgBlockersDepth += sampleDepth;
    //   blockersCount += 1.0f;
    // }
    float conditional = when_gt(z_shadowMapView + bias -constantbias ,sampleDepth);
    avgBlockersDepth += conditional*sampleDepth;
    blockersCount += conditional;
  }

  return when_gt(blockersCount,0.0)*AvgBlockersDepthToPenumbra(z_shadowMapView, avgBlockersDepth/max(blockersCount,0.001));

}

/*
* |---|---|---|---|
* |sun|1|2|1|2|1|2|
* |sun|3|4|3|4|3|4|
* ----|---|---|---|
* |1|2|5|6|5|6|5|6|
* |3|4|1|2|1|2|1|2|
* ----|---|---|---|
* |5|6|3|4|3|4|3|4|
* |1|2|5|6|5|6|5|6|
* ----|---|---|---|
* |3|4|   |   |   |
* |5|6|   |   |   |
* ----|---|---|---|
*/

float ShadowCalculation(vec4 fragPosLightSpace,float NdotL,sampler2D smap,vec2 atlasCoord)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
  //  vec4 closestDepths = textureGather(shadowMap, projCoords.xy,0);
//  float bias = 0.00001;//clamp(0.001*tan(acos(NdotL)),0,0.1);//max(0.05 * (1.0 - NdotL), 0.005);

//float bias = tan(acos(NdotL));
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
  //  vec2 q = vec2(dFdx(currentDepth),dFdy(currentDepth)*1024);
    // float bias = max(q.x,q.y)*0.0001 + 0.00001;



    // check whether current frag pos is in shadow
    float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy)*2*3.1459;
    float penumbra = 0.05*5*2.0*0.8;//Penumbra(gradientNoise, projCoords.xy, currentDepth ,deltas,smap);

    float shadow = 0.0f;

for(float i = 0; i < samplesCount; i+=1)
{
  vec2 sampleUV = VogelDiskSample(i,  gradientNoise);
  vec2 offset = sampleUV * shadow_blur_radius * (1.0/(th_shadowmap_resolution*th_shadowmap_sun_scale));
  sampleUV = projCoords.xy + offset;

  float bias = 0;//deltas.x*offset.x + deltas.y*offset.y;
  float closestDepths = texture(smap, sampleUV*(0.25*th_shadowmap_sun_scale) + atlasCoord*(0.25*th_shadowmap_sun_scale),0).r;
  shadow += when_gt(currentDepth +bias - constantbias , closestDepths) ;

  // shadow +=  when_gt(currentDepth - bias , closestDepths.x) + when_gt(currentDepth - bias , closestDepths.y) +
  //  when_gt(currentDepth - bias , closestDepths.z) + when_gt(currentDepth - bias , closestDepths.w);
}
shadow /= (samplesCount);
return shadow*(1-when_gt(projCoords.z,1.0));
    // float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    // float shadow = when_gt(currentDepth - bias , closestDepths.x)*0.75 + when_gt(currentDepth - bias , closestDepths.y)*0.75 +
    // when_gt(currentDepth - bias , closestDepths.z)*0.75 + when_gt(currentDepth - bias , closestDepths.w)*0.75;
    //
    // return shadow/4;
}

float ShadowCalculationFog(vec4 fragPosLightSpace,float NdotL,sampler2D smap,vec2 atlasCoord)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;
  //  vec2 q = vec2(dFdx(currentDepth),dFdy(currentDepth)*1024);
    // float bias = max(q.x,q.y)*0.0001 + 0.00001;
    vec3 DDX = vec3(0);//vec3(dFdx(projCoords.x),dFdx(projCoords.y),dFdx(projCoords.z));
    vec3 DDY = vec3(0);//vec3(dFdy(projCoords.y),dFdy(projCoords.y),dFdy(projCoords.z));

    vec2 righttexel = vec2(0,0);
    vec2 uptexel = vec2(0,0);

    mat2 toscreen = inverse(mat2(DDX.x,DDY.x,DDX.y,DDY.y));
    vec2 rightratio = toscreen*righttexel;
    vec2 upratio = toscreen*uptexel;
    float rightdelta = rightratio.x * DDX.z
            + rightratio.y * DDY.z;
    float updelta = upratio.x*DDX.z + upratio.y*DDY.z;
    vec2 deltas = vec2(rightdelta,updelta)*(th_shadowmap_resolution);


    // check whether current frag pos is in shadow
    float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy)*2*3.1459;
    float penumbra = 0.05*5;//Penumbra(gradientNoise, projCoords.xy, currentDepth ,deltas,smap);

    float shadow = 0.0f;

for(float i = 0; i < samplesCount_fog; i+=1)
{
  vec2 sampleUV = VogelDiskSampleFog(i,  gradientNoise);
  vec2 offset = sampleUV * penumbra * shadowmaxsize;
  sampleUV = projCoords.xy + offset;

  float bias = deltas.x*offset.x + deltas.y*offset.y;
  float closestDepths = texture(smap, sampleUV*0.25 + atlasCoord*0.25,0).r;
  shadow += when_gt(currentDepth +bias - constantbias , closestDepths) ;

  // shadow +=  when_gt(currentDepth - bias , closestDepths.x) + when_gt(currentDepth - bias , closestDepths.y) +
  //  when_gt(currentDepth - bias , closestDepths.z) + when_gt(currentDepth - bias , closestDepths.w);
}
shadow /= (samplesCount_fog);
return shadow*(1-when_gt(projCoords.z,1.0));

}

float  shadowCalculationPoint_fog(vec3 lightpos,vec3  fragpos,sampler2D smap,vec2 atlasCoord,int matrix_offset)
{
  #define gridscale 0.5
  vec2 offsets[6];
  // offsets[0] = vec2(0.5*gridscale,0);
  // offsets[1] = vec2(0.75*gridscale,0);
  // offsets[2] = vec2(0.5*gridscale,0.25*gridscale);
  // offsets[3] = vec2(0.75*gridscale,0.25*gridscale);
  // offsets[4] = vec2(0.5*gridscale,0.5*gridscale);
  // offsets[5] = vec2(0.75*gridscale,0.5*gridscale);
  offsets[0] = vec2(0.25,0);
  offsets[1] = vec2(0.375,0);
  offsets[2] = vec2(0.25,0.125);
  offsets[3] = vec2(0.375,0.125);
  offsets[4] = vec2(0.25,0.25);
  offsets[5] = vec2(0.375,0.25);

  float totalshadow = 0;
  float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy)*2*3.1459;
  for(float i = 0; i < samplesCount_fog; i+=1)
  {
    vec3 pos_jitter = VogelSphereSample_fog(i,gradientNoise)*10;
    vec3 dir = normalize(fragpos - (lightpos + pos_jitter));
    float face;
    float u;
    float v;
    indexCubeMap(dir*vec3(-1,1,1),face,u,v);

    vec2 uv = vec2(u,v);

    float closestDepths = texture(smap, uv*0.125 + offsets[int(face)] + atlasCoord*0.25,0).r;

    vec4 fragPosLightSpace =  lightSpaceMatrices[int(face) + matrix_offset*6]*vec4(fragpos,1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;

    float shadow = when_gt(currentDepth -  0.00001, closestDepths) ;//0.000005
    totalshadow += shadow;
  }
  totalshadow /= samplesCount_fog;
  return 1 - totalshadow;
}


float  shadowCalculationPoint(vec3 lightpos,vec3  fragpos,sampler2D smap,vec2 atlasCoord,int matrix_offset,float lindepth)
{
  #define gridscale 0.5
  vec2 offsets[6];
  // offsets[0] = vec2(0.5*gridscale,0);
  // offsets[1] = vec2(0.75*gridscale,0);
  // offsets[2] = vec2(0.5*gridscale,0.25*gridscale);
  // offsets[3] = vec2(0.75*gridscale,0.25*gridscale);
  // offsets[4] = vec2(0.5*gridscale,0.5*gridscale);
  // offsets[5] = vec2(0.75*gridscale,0.5*gridscale);

  float iscale = (0.25*th_shadowmap_omni_scale*0.5);

  offsets[0] = vec2(0.25,0);
  offsets[1] = vec2(0.25 + iscale,0);
  offsets[2] = vec2(0.25,iscale);
  offsets[3] = vec2(0.25 + iscale,iscale);
  offsets[4] = vec2(0.25,iscale*2);
  offsets[5] = vec2(0.25 + iscale,iscale*2);

  float totalshadow = 0;
  float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy)*2*3.1459;

  bool fastpath = allInvocations(lindepth > 0.05);
  float pjitter = omni_light_jitter;
  if (fastpath)
  {

    vec3 pos_jitter =  VogelSphereSample(1,gradientNoise)*pjitter;
    vec3 dir = normalize(fragpos - (lightpos + pos_jitter));
    float face;
    float u;
    float v;
    indexCubeMap(dir*vec3(-1,1,1),face,u,v);

    vec2 uv = vec2(u,v);

    float closestDepths = texture(smap, uv*iscale + offsets[int(face)] + atlasCoord*0.25,0).r;

    vec4 fragPosLightSpace =  lightSpaceMatrices[int(face) + matrix_offset*6]*vec4(fragpos,1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;
    //0.00001
    //0.00007
    float shadow = when_gt(currentDepth -  0.00019, closestDepths) ;//0.000005
    totalshadow += shadow;

    totalshadow /= 1;
    return 1 - totalshadow;
  }
  else
  {
    for(float i = 0; i < samplesCountPoint; i+=1)
    {
      vec3 pos_jitter = VogelSphereSample(i,gradientNoise)*pjitter;
      vec3 dir = normalize(fragpos - (lightpos + pos_jitter));
      float face;
      float u;
      float v;
      indexCubeMap(dir*vec3(-1,1,1),face,u,v);

      vec2 uv = vec2(u,v);

      float closestDepths = texture(smap, uv*iscale + offsets[int(face)] + atlasCoord*0.25,0).r;

      vec4 fragPosLightSpace =  lightSpaceMatrices[int(face) + matrix_offset*6]*vec4(fragpos,1.0);
      vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

      projCoords = projCoords * 0.5 + 0.5;

      float currentDepth = projCoords.z;
      //0.00001
      //0.00007
      float shadow = when_gt(currentDepth -  0.00019, closestDepths) ;//0.000005
      totalshadow += shadow;
    }
    totalshadow /= samplesCountPoint;
    return 1 - totalshadow;
  }


}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 1e-4);
}



float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 1e-4);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0,float roughness)
{
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
    //return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}


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

// float when_lt(float x, float y) {
//   return max(sign(y - x), 0.0);
// }

vec3 calculateDirectionalLight(vec3 lightdir,vec3 V,vec3 N,vec3 lightcolor,float roughness,vec3 F0,float metallic,vec3 albedo,vec4 sh)
{
    // calculate per-light radiance
    vec3 L = lightdir;//normalize(lightpos - outPosition);
    vec3 H = normalize(V + L);
    vec3 radiance     = lightcolor * 1;//attenuation;

    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0,roughness);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);

    vec3 specular     = numerator / max(denominator, 0.001);
    #ifdef NOSPECULAR
    specular = vec3(0,0,0);
    #endif

    // add to outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);

    const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);

  float ofactor = 1;

  #ifdef OCCLUSION
    ofactor =  (1 - clamp(dot(vec4((-lightdir),1), sh),0,1));
    #endif

    return (kD * albedo / PI + specular) * radiance * NdotL*ofactor;
}

vec4 calculateEmissiveLight(vec3 lightpos,vec3 outPosition,vec3 N,vec3 lightcolor,vec3 albedo,float radius,out vec3 em_ndl)
{

  float d    = length(lightpos - outPosition);
  float s = 1.5;
  float attenuation = (1.0 - min(max(d/(radius*s) - 1/(s) ,0.0),1.0));
  attenuation = pow(attenuation,6.0);
  vec3 radiance     = lightcolor * attenuation;
  em_ndl = radiance*max(dot(N, normalize(lightpos - outPosition)), 0.0);

  return vec4(radiance,attenuation);
}

vec3 calculatePointLight(vec3 lightpos,vec3 outPosition,vec3 V,vec3 N,vec3 lightcolor,float roughness,vec3 F0,float metallic,vec3 albedo,vec4 sh)
{
    float thresh = 0.001;
    float radius = max(sqrt(max(lightcolor.x,max(lightcolor.y,lightcolor.z))/thresh),0.1);


    // calculate per-light radiance
    vec3 L = normalize(lightpos - outPosition);
    vec3 H = normalize(V + L);
    float d    = length(lightpos - outPosition);
    float attenuation = 1.0 / (d * d);
    float t = d / radius;
    float fade = 1.0 - smoothstep(0.0, 1.0, t);
    attenuation = attenuation * fade;
    vec3 radiance     = lightcolor * attenuation;

    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0,roughness);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular     = numerator / max(denominator, 0.001);
    #ifdef NOSPECULAR
    specular = vec3(0,0,0);
    #endif

    // add to outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);

    const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);

  float ofactor = 1;

  #ifdef OCCLUSION
    ofactor =  (1 - clamp(dot(vec4((-L),1), sh),0,1));
    #endif

    return (kD * albedo / PI + specular) * radiance * NdotL*ofactor;
}

vec3 calculateTubeLight(vec3 L0_world,vec3 L1_world,float radius,vec3 outPosition,vec3 V,vec3 N,vec3 lightcolor,float roughness,vec3 F0,float metallic,vec3 albedo,vec4 sh)
{

  vec3 r = reflect (-V, N);
  vec3 L0 = L0_world - outPosition;
  vec3 L1 = L1_world - outPosition;
  float len_l0 = length(L0);
  float len_l1 = length(L1);
  vec3 Ld = L1_world - L0_world;
  float len_ld = length(Ld);
  float r_dot_ld = dot(r,Ld);
  float t = (dot(r,L0)*dot(r,Ld) - dot(L0,Ld))/(len_ld*len_ld - r_dot_ld*r_dot_ld);
  t = clamp(t,0.0,1.0);
  vec3 lightpos = L0_world + (Ld)*t;

  vec3 L_prime = lightpos - outPosition;
  vec3 centerToRay = dot(L_prime,r)*r - L_prime ;
  vec3 closestPoint = L_prime + centerToRay*clamp(radius/length(centerToRay),0.0,1.0);
  vec3 lightdir_new = normalize(closestPoint);
  // lightpos = lightpos + lightdir_new*radius;
  //lightpos = closestPoint;

    // calculate per-light radiance
    vec3 L = normalize(lightpos - outPosition);

    L = lightdir_new;
    float NdotL = max(dot(N, L), 0.0);

    vec3 H = normalize(V + L);
    float d    = length(lightpos - outPosition);
    float attenuation = 1.0 / (d * d);
    //float attenuation = (2.0*clamp((dot(N,L0)/(2.0*len_l0)) + (dot(N,L1)/(2.0*len_l1)),0.0,1.0))/(len_l1*len_l0 + dot(L1,L0) + 2);
    vec3 radiance     = lightcolor * attenuation;

    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0,roughness);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular     = numerator / max(denominator, 0.001);
    #ifdef NOSPECULAR
    specular = vec3(0,0,0);
    #endif

    // add to outgoing radiance Lo


    const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);

  float ofactor = 1;

  #ifdef OCCLUSION
    ofactor =  (1 - clamp(dot(vec4((-L),1), sh),0,1));
    #endif
    return (kD * albedo / PI + specular) * radiance * NdotL*ofactor;
}

uint ijkToCluster(uint i,uint j, uint k)
{
  uint pack = (i&0xFF) | ((j&0xFF) << 8) | ((k&0x3FF) << 16);
  return pack;
}

void clusterToIJK(uint pack, out uint i,out uint j, out uint k)
{
  i = (pack&0xFF);
  j = (pack >> 8)&0xFF;
  k = (pack >> 16)&0x3FF;
}



float screen2EyeDepth(float depth, float near, float far)
{

    float ndc = 2.0 * clamp(depth,0.0,1.0) - 1.0;
    float eye = 2.0 * far * near / (far + near + ndc * (near - far));
    return eye;
}

// uint getClusterZIndex(float screenDepth)
// {
//     // this can be calculated on the CPU and passed as a uniform
//     // only leaving it here to keep most of the relevant code in the shaders for learning purposes
//     // float scale = float(20) / log(zFar / zNear);
//     // float bias = -(float(20) * log(zNear) / log(zFar / zNear));
//
//     float eyeDepth = screen2EyeDepth(screenDepth, zNear, zFar);
//     uint zIndex = uint(float(num_slices)*(log(eyeDepth/zNear)/log(zFar/zNear)));
//     // uint zIndex = uint(max(log(eyeDepth) * scale + bias, 0.0));
//     return zIndex;
// }

uint getClusterZIndex(float screenDepth)
{

    float eyeDepth = screen2EyeDepth(screenDepth, zNear, zFar);
    eyeDepth = clamp(eyeDepth, zNear, zFar);
    uint zIndex = max(int( max(0,float(num_slices)*(log((eyeDepth/zNear))/log(zFar/zNear)) ) ) - 5,0);
    // uint zIndex = uint(max(log(eyeDepth) * scale + bias, 0.0));
    return zIndex;
}


vec3 processCell(uint id,vec3 outPosition,vec3 V,vec3 N,float roughness,vec3 F0,float metallic,vec3 albedo,vec4 sh)
{

  uvec2 arr[2];
  arr[0] = offsets[id/2].xy;
  arr[1] = offsets[id/2].zw;
  uint count = arr[id%2].y;
  uint offset = arr[id%2].x;
  vec3 Lo = vec3(0);
  for (int i = 0 ; i < count;i++)
  {
     uint a = offset + i;
    uint key = acessbuffer[a/4][a%4];

    PointLight light = pointlights[key];


  //  vec4 lightSpacePosition = light.lightmat*vec4(outPosition,1.0);
    float shadow = 1;
    // if (light.shadowindex != -1)
    // {
    //   shadow = (1 - ShadowCalculation(lightSpacePosition,1.0,shadowMap));//use index of light.shadowindex in array of shadows
    // }

    Lo += calculatePointLight(light.pos.xyz,outPosition,V,N,light.color.xyz,roughness,F0,metallic,albedo,sh)*shadow;
  }
  return Lo;
}



float planeDist(vec3 ppos,vec3 pnormal, vec3 point)
{
  return (dot(pnormal,point - ppos));
}
// uniform vec3 fogPlaneNormal = normalize(vec3(0,1,0));
// uniform vec3 fogPlanePos = vec3(0,-90,0);
// uniform float fog_pow = 1.f;
// uniform float fog_div = 100;
// uniform float fogHard =  1.0;
// uniform vec3 fogColor = vec3(0.1,0.1,0.1) ;

vec3 sphericalHarmonics(vec3 normal,Light l )
{
  return max(
    l.a[0]*0.282094791773878140 +
    l.a[1]*-0.488602511902919920*normal.y +
    l.a[2]*0.488602511902919920*normal.z +
    l.b[0]*-0.488602511902919920*normal.x +
    l.b[1]*0.546274215296039590*(normal.x*normal.y + normal.x*normal.y) +
    l.b[2]*(-1.092548430592079200)*(normal.z*normal.y) +
    l.c[0]*((0.946174695757560080)*normal.z*normal.z + (-0.315391565252520050)) +
   l.c[1]*(-1.092548430592079200)*normal.z*normal.x +
   l.c[2]*(0.546274215296039590)*(normal.x*normal.x - normal.y*normal.y)
    ,vec3(0));
}

vec3 sphericalHarmonicsReflection(vec3 normal,Light l )
{
  l.a[0]= l.a[0]*(1.0/(PI*(1.0)));
  l.a[1]= l.a[1]*(1.0/(PI*(2.0/3.0)));
  l.a[2]= l.a[2]*(1.0/(PI*(2.0/3.0)));
  l.b[0]= l.b[0]*(1.0/(PI*(2.0/3.0)));
  l.b[1]= l.b[1]*(1.0/(PI*(1.0/4.0)));
  l.b[2]= l.b[2]*(1.0/(PI*(1.0/4.0)));
  l.c[0]= l.c[0]*(1.0/(PI*(1.0/4.0)));
  l.c[1]= l.c[1]*(1.0/(PI*(1.0/4.0)));
  l.c[2]= l.c[2]*(1.0/(PI*(1.0/4.0)));
  return max(
    l.a[0]*0.282094791773878140 +
    l.a[1]*-0.488602511902919920*normal.y +
    l.a[2]*0.488602511902919920*normal.z +
    l.b[0]*-0.488602511902919920*normal.x +
    l.b[1]*0.546274215296039590*(normal.x*normal.y + normal.x*normal.y) +
    l.b[2]*(-1.092548430592079200)*(normal.z*normal.y) +
    l.c[0]*((0.946174695757560080)*normal.z*normal.z + (-0.315391565252520050)) +
   l.c[1]*(-1.092548430592079200)*normal.z*normal.x +
   l.c[2]*(0.546274215296039590)*(normal.x*normal.x - normal.y*normal.y)
    ,vec3(0));
}



vec2 IdToCoord(float ID)
{
  float f = ID/dims_harmtex.x;
  float yval = floor(f);
  float xval = f - yval;
  yval /=dims_harmtex.y;
  return vec2(xval + (0.5*(1.f/dims_harmtex.x)),yval + (0.5*(1.f/dims_harmtex.y)));
}

ivec2 IdToCoordi(int ID)
{
  int x = int(ID) % int(dims_harmtex.x);
  int y = int(ID) / int(dims_harmtex.x);
  return ivec2(x,y);
  // float f = ID/dims_harmtex.x;
  // float yval = floor(f);
  // float xval = f - yval;
  // yval /=dims_harmtex.y;
  // return vec2(xval + (0.5*(1.f/dims_harmtex.x)),yval + (0.5*(1.f/dims_harmtex.y)));
}



Light aquireLight(float coord,float scale)
{
//texture2D(lights,IdToCoord(coord + 0)).xyz
  //float vv = 1 - or(when_lt(coord,0),when_gt(coord,gridsize.x*gridsize.y*gridsize.z));
  Light l;


  l.a[0] = texture2D(lights,IdToCoord(coord + 0)).xyz*scale;
  l.a[1] = texture2D(lights,IdToCoord(coord + 1)).xyz*scale;
  l.a[2] = texture2D(lights,IdToCoord(coord + 2)).xyz*scale;
  l.b[0] = texture2D(lights,IdToCoord(coord + 3)).xyz*scale;
  l.b[1] = texture2D(lights,IdToCoord(coord + 4)).xyz*scale;
  l.b[2] = texture2D(lights,IdToCoord(coord + 5)).xyz*scale;
  l.c[0] = texture2D(lights,IdToCoord(coord + 6)).xyz*scale;
  l.c[1] = texture2D(lights,IdToCoord(coord + 7)).xyz*scale;
  l.c[2] = texture2D(lights,IdToCoord(coord + 8)).xyz*scale;
  // l.lambert = lambert;

//   l.a[0] = texelFetch(lights,IdToCoordi(int(coord) + 0),0).xyz*scale;
//   l.a[1] = texelFetch(lights,IdToCoordi(int(coord) + 1),0).xyz*scale;
//   l.a[2] = texelFetch(lights,IdToCoordi(int(coord) + 2),0).xyz*scale;
//   l.b[0] = texelFetch(lights,IdToCoordi(int(coord) + 3),0).xyz*scale;
//   l.b[1] = texelFetch(lights,IdToCoordi(int(coord) + 4),0).xyz*scale;
//   l.b[2] = texelFetch(lights,IdToCoordi(int(coord) + 5),0).xyz*scale;
//   l.c[0] = texelFetch(lights,IdToCoordi(int(coord) + 6),0).xyz*scale;
//   l.c[1] = texelFetch(lights,IdToCoordi(int(coord) + 7),0).xyz*scale;
//   l.c[2] = texelFetch(lights,IdToCoordi(int(coord) + 8),0).xyz*scale;


  return l;
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

float indexify(vec3 quant)
{
//  return (int(quant.x)*int(gridsize.z) * int(gridsize.y) + int(quant.y)*int(gridsize.z) + int(quant.z))*9;
    return clamp((clamp(quant.x,0,gridsize.x) * gridsize.z * gridsize.y) + (clamp(quant.y,0,gridsize.y) * gridsize.z) + clamp(quant.z,0,gridsize.z),0,gridsize.x*gridsize.y*gridsize.z)*9.0;
}


Light addLight(Light a,Light b)
{
  a.a = a.a + b.a;
  a.b = a.b + b.b;
  a.c = a.c + b.c;
  return a;
}

vec3 getPos(vec3 indices)
{
  float x = indices.x +1;
  float y = indices.y +1;
  float z = indices.z +1;
  return vec3((x-(gridsize.x*0.5))*griddist + goffset.x,(y-(gridsize.y*0.5))*griddist + goffset.y,(z-(gridsize.z*0.5))*griddist + goffset.z);
}


float getOcclusion(vec3 pos,vec3 worldpos,vec3 normal)
{
  // float alt_occ = max(0,dot(normalize(pos-worldpos),normalize(normal2)));
  // float valid = when_gt(dot(normal2,normal2),0.3*0.3);
     return max(0,dot(normalize(pos-worldpos),normal));
}

float getSHOcclusion(vec4 sh,vec3 lpos,vec3 pos)
{
  // float alt_occ = max(0,dot(normalize(pos-worldpos),normalize(normal2)));
  // float valid = when_gt(dot(normal2,normal2),0.3*0.3);
     return (1 - clamp(dot(vec4(normalize(lpos - pos),1), sh),0,1));
}

float getOcclusionSH(vec3 pos,vec3 worldpos,vec3 normal,vec4 sh)
{
  // const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);
  //float ofactor =  (1 - clamp(dot(vec4((normalize(pos-worldpos)),1), sh),0,1));
  float ofactor = max(0,dot(normalize(pos-worldpos),normalize(sh.xyz) ));
     return max(0,dot(normalize(pos-worldpos),normal))*(ofactor*when_gt(sh.w,0) + (1 - when_gt(sh.w,0)));//*ofactor;
}

vec3 SchlickFresnel(vec3 r0, float rad)
{
   // -- The common Schlick Fresnel approximation
   float exponential = pow(1.0f - rad, 5.0f);
   return r0 + (1.0f - r0) * exponential;
}

vec3 hash3( vec2 p )
{
    vec3 q = vec3( dot(p,vec2(127.1,311.7)),
				   dot(p,vec2(269.5,183.3)),
				   dot(p,vec2(419.2,371.9)) );
	return fract(sin(q)*43758.5453);
}

float voronoise( in vec2 p, float u, float v )
{
	float k = 1.0+63.0*pow(1.0-v,6.0);

    vec2 i = floor(p);
    vec2 f = fract(p);

	vec2 a = vec2(0.0,0.0);
    for( int y=-2; y<=2; y++ )
    for( int x=-2; x<=2; x++ )
    {
        vec2  g = vec2( x, y );
		vec3  o = hash3( i + g )*vec3(u,u,1.0);
		vec2  d = g - f + o.xy;
		float w = pow( 1.0-smoothstep(0.0,1.414,length(d)), k );
		a += vec2(o.z*w,w);
    }

    return a.x/a.y;
}

float gaussian(vec2 xy,float std)
{
  return (1.0/(2.0*3.14159*std))*exp(-(xy.x*xy.x + xy.y*xy.y)/(2.0*std*std));
}

float curve_func(float x,float a,float b)
{
  float xpowa = pow(x,a);
  return xpowa/(xpowa + pow(1 - x,b));
}

// float encodeFlags(int flags) {
//   flags = clamp(flags, 0, 256);
//   return float(flags/255.0);
// }

int decodeFlags(float f) {
  // add 0.5 to avoid rounding errors from float
  return int(f*255.0 + 0.5);
}

// float setFlag(float f, int bit) {
//   int flags = decodeFlags(f);
//   flags |= (1 << bit);
//   return float(flags);
// }
//
// float setFlagConditional(float f, int bit,float val) {
//   int flags = decodeFlags(f);
//   flags |= (int(val) << bit);
//   return float(flags);
// }

float hasFlag(float f, int bit) {
  int flags = decodeFlags(f);
  return float((flags & (1 << bit)) != 0);
}

vec3 thinFilmIridescence(vec3 N, vec3 V, float thickness, float ior)
{
  float cosTheta = clamp(dot(normalize(N), normalize(V)), 0.0, 1.0);


  vec3 lambda = vec3(650.0, 510.0, 475.0);


  vec3 phase = (4.0 * 3.14159265 * ior * thickness * cosTheta) / lambda;


  vec3 interference = 0.5 + 0.5 * cos(phase);


  return interference;
}

vec3 blackbody(float T)
{
  float t = T / 100.0;

  vec3 c;

  // Red
  if (t <= 66.0)
    c.x = 1.0;
  else
    c.x = clamp(1.292936 * pow(t - 60.0, -0.133205), 0.0, 1.0);

  // Green
  if (t <= 66.0)
    c.y = clamp(0.390081 * log(t) - 0.631841, 0.0, 1.0);
  else
    c.y = clamp(1.129890 * pow(t - 60.0, -0.075514), 0.0, 1.0);

  // Blue
  if (t >= 66.0)
    c.z = 1.0;
  else if (t <= 19.0)
    c.z = 0.0;
  else
    c.z = clamp(0.543206 * log(t - 10.0) - 1.196254, 0.0, 1.0);

  return c;
}

vec3 blackBodyLookup(float interp){
  float temp = clamp(interp,0.0,1.0)*blackbody_maxtemp;
  float intensity = pow(temp / 1000.0, 4.0);

  return ((blackbody(temp)*vec3(1.1,1.0,0.9))*intensity);
}

float blackBodyDelay(float x,float s)
{
  return 1.0 - exp(-((x*x)/(2*s*s)));
}

float getBarrelLengthTerm(vec3 pos,vec3 a,vec3 b)
{
  vec3 projected = (pos - a);

  vec3 projdir = normalize(b - a);
  float projdist = max(length(b - a),0.01);

//   projected = a + projdir*clamp(dot(projected,projdir),0.0,projdist );

  return clamp(dot(projected,projdir),0.0,projdist )/projdist;

}



void main()
{
  #ifdef BATCH_MODE
  vec2 normcoord = vec2(mod(gl_FragCoord.x,32)*(1.0/32.0), mod(gl_FragCoord.y,32)*(1.0/32.0) )*vec2(2) - vec2(1);
  #else
  vec2 normcoord = (gl_FragCoord.xy*invWindow)*vec2(2) - vec2(1);
  normcoord.x = normcoord.x*(screenSize.x/screenSize.y);

  float tan_half_fov = 1.0/(projMat[1][1]);
  normcoord = normcoord*tan_half_fov;
  #endif


  vec3 dir = normalize(vec3(0,0,1) + vec3(0,-1,0)*normcoord.y + vec3(-1,0,0)*normcoord.x);
  dir = vec3(vec4(dir,0)*viewMat);
  vec3 sky_color = textureLod(atmosphereCube,dir*vec3(1,-1,-1),0).rgb*sky_boost;

  float depthval = texture2D(depthTex,gl_FragCoord.xy*invWindow ).r;


  bool fastpath_condit = depthval == 1;
  bool fastpath = allInvocations(fastpath_condit);
  vec3 color;
  vec3 acess = vec3(0,0,0);
  float dynamic_flag = 0.0;


    float ldepth;
//     vec3 outPosition = WorldPosFromDepth(gl_FragCoord.xy*invWindow,depthval,ldepth);

  #ifdef BATCH_MODE
  vec3 outPosition = WorldPosFromDepth(vec2(mod(gl_FragCoord.x,32)*(1.0/32.0), mod(gl_FragCoord.y,32)*(1.0/32.0) ),depthval,ldepth);
  #else
  vec3 outPosition = WorldPosFromDepth(gl_FragCoord.xy*invWindow,depthval,ldepth);

  #endif
      vec3 N_geom = normalize(cross(dFdx(outPosition),dFdy(outPosition)));
  if (fastpath)
  {
    color = sky_color;
  }
  else
  {
//  vec4
//   vec4 sh = texture(occTex,gl_FragCoord.xy*invWindow);

    #ifdef BATCH_MODE
    float cheap_ao = 1.0;
    #else
    float cheap_ao = clamp(1.0 - texture(occTex,gl_FragCoord.xy*invWindow).r,0.0,1.0);
    cheap_ao = mix(0.25,1.0,cheap_ao);
    #endif


  vec4 sh = vec4(0,0,0,0);


  vec4 a = texture(texCoordTex, gl_FragCoord.xy*invWindow);
  vec4 norm_rough_ao = texture(normalTex, gl_FragCoord.xy*invWindow );
  vec3 norm_rough = norm_rough_ao.rgb;
  // vec4 b = texture(materials[int(ID.x)],vec3(outTexcoord,(ID.y + 1)));
  // vec3 normal = b.rgb;//texture(normalMap, outTexcoord).rgb;
  // normal = normal * 2.0 - 1.0;
  // normal = normalize(TBN * normal);
  float metallic  = a.a;//texture(metallicMap, outTexcoord).r;
  //float roughness = max(roughness_input,0.04);
  float roughness = max(norm_rough.z,0.04);//texture(roughnessMap, outTexcoord).r;
  float ao        =1;//= texture(aoMap, outTexcoord).r;
  vec3 albedo     = pow(a.rgb,vec3(2.2));//pow(texture(albedoMap, outTexcoord).rgb, vec3(2.2));
  float ambient_occ = 1.0;//norm_rough_ao.w;
  dynamic_flag = hasFlag(norm_rough_ao.w,0);
  float glow_flag = hasFlag(norm_rough_ao.w,1);
  float iridescent_flag = hasFlag(norm_rough_ao.w,2);
  float hotbarel_flag = hasFlag(norm_rough_ao.w,3);




  vec3 N = (vec4(decodeNormal(norm_rough.xy),0)).xyz;
    vec3 V = normalize(eyePos - outPosition);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    vec4 lightSpacePosition = lightSpaceMatrix*vec4(outPosition,1.0);
    float shadowfactor = (1 - ShadowCalculation(lightSpacePosition,1.0,shadowMap,vec2(0,0)));
    Lo += calculateDirectionalLight(sunlightDir,V,N,sunlightColor,roughness,F0,metallic,albedo,sh)*shadowfactor;



    // vec3 plpos = vec3(207.802399,-400.645264,-979.814941);
    // float shadow_pl = shadowCalculationPoint(plpos,outPosition,shadowMap,vec2(0,0),0);
    // Lo += calculatePointLight(plpos,outPosition,V,N,vec3(10000,10000,50000),roughness,F0,metallic,albedo,sh)*shadow_pl;
    //
    // plpos = vec3(287.077026, -1413.032471 ,-530.029907);
    // shadow_pl = shadowCalculationPoint(plpos,outPosition,shadowMap,vec2(1,0),1);
    // Lo += calculatePointLight(plpos,outPosition,V,N,vec3(10000,10000,50000),roughness,F0,metallic,albedo,sh)*shadow_pl;

    // for (int i = 0 ; i < num_omni_lights;i++)
    // {
    //   shadow_pl = shadowCalculationPoint(plpos,outPosition,shadowMap,vec2(1,0),1);
    //   Lo += calculatePointLight(plpos,outPosition,V,N,vec3(10000,10000,50000),roughness,F0,metallic,albedo,sh)*shadow_pl;
    // }

    //clustered lights
    ivec2 tile_id = ivec2(gl_FragCoord.xy / vec2(screenSize.x/16.0,screenSize.y/8.0));

   float eye_linear = (screen2EyeDepth(depthval, zNear, zFar) - zNear)/(zFar - zNear);
   uint cluster_slice_id = getClusterZIndex(depthval);

    // uint tile_index = ijkToCluster(tile_id.x,tile_id.y,cluster_slice_id);
    // uint tile_light_num = light_visiblities[tile_index].count;

   #ifdef BATCH_MODE
    uint tile_index = 0;
   #else
    uint tile_index = tile_id.x + tile_id.y*num_tiles_x + cluster_slice_id*num_tiles_x*num_tiles_y;
   #endif

    // uint s_firstLaneCellIdx = readFirstInvocationARB(tile_index);
    //
    // uint64_t laneMask = ballotARB(tile_index == s_firstLaneCellIdx);
    //
    // bool fastPath2 = (laneMask == ballotARB(true));
    //
    // if (fastPath2)
    // {
    //   acess = vec3(1,0,0);
    //   Lo += processCell(s_firstLaneCellIdx,outPosition,V,N,roughness,F0,metallic,albedo,sh);
    // }
    // else
    // {
    //   // uvec2 arr[2];
    //   // arr[0] = offsets[tile_index/2].xy;
    //   // arr[1] = offsets[tile_index/2].zw;
    //   // uint count = arr[tile_index%2].y;
    //   // uint offset = arr[tile_index%2].x;
    //   //
    //   // uint lightOffset = 0;
    //   // uint iter = 0;
    //   // while (lightOffset < count && iter < 30)
    //   // {
    //   //   uint a = offset + lightOffset;
    //   //   uint key = acessbuffer[a/4][a%4];
    //   //    uint key_s = waveMin(key);
    //   //
    //   //   if (key_s >= key)
    //   //   {
    //   //     lightOffset++;
    //   //     PointLight light = pointlights[key_s];
    //   //     Lo += calculatePointLight(light.pos.xyz,outPosition,V,N,light.color.xyz,roughness,F0,metallic,albedo,sh);
    //   //   }
    //   //   iter++;
    //   //
    //   // }
    //
    //   uvec2 arr[2];
    //   arr[0] = offsets[tile_index/2].xy;
    //   arr[1] = offsets[tile_index/2].zw;
    //   uint count = arr[tile_index%2].y;
    //   uint offset = arr[tile_index%2].x;
    //
    //
    //   uint v_laneID = gl_SubGroupInvocationARB;
    //   uint64_t execMask = 0xffffffff;
    //   uint64_t curLaneMask = uint64_t(1) << uint64_t(v_laneID);
    //
    //   while( ( execMask & curLaneMask ) != 0 )
    //   {
    //     uint s_tile_index = readFirstInvocationARB(tile_index);
    //     uint64_t laneMask = ballotARB(tile_index == s_tile_index);
    //     execMask = execMask & ~laneMask;
    //     if(tile_index == s_tile_index)
    //     {
    //       Lo += processCell(s_tile_index,outPosition,V,N,roughness,F0,metallic,albedo,sh);
    //     }
    //   }
    //
    // }
    uvec2 arr[2];
    arr[0] = offsets[tile_index/2].xy;
    arr[1] = offsets[tile_index/2].zw;
    uint count = arr[tile_index%2].y;
    uint offset = arr[tile_index%2].x;


    // uint v_laneID = gl_SubGroupInvocationARB;
    // uint64_t execMask = 0xffffffff;
    // uint64_t curLaneMask = uint64_t(1) << uint64_t(v_laneID);
    //
    // while( ( execMask & curLaneMask ) != 0 )
    // {
    //   uint s_tile_index = readFirstInvocationARB(tile_index);
    //   uint64_t laneMask = ballotARB(tile_index == s_tile_index);
    //   execMask = execMask & ~laneMask;
    //   if(tile_index == s_tile_index)
    //   {
    //     Lo += processCell(s_tile_index,outPosition,V,N,roughness,F0,metallic,albedo,sh);
    //   }
    // }
    vec3 emissive = vec3(0.0);
    float emissive_is_1 = 0.0;
    vec3 emissive_ndotl = vec3(0.0);

    for (int i = 0 ; i < count;i++)
    {
       uint a = offset + i;
      uint key_bits = acessbuffer[a/8][(a%8)/2];
      int bit_offset = int((a % 2) *  16);
      uint key = bitfieldExtract(key_bits,bit_offset,16);

      PointLight light = pointlights[key];


    //  vec4 lightSpacePosition = light.lightmat*vec4(outPosition,1.0);
      float shadow = cheap_ao;
      if  (light.shadowindex.x != 0.0)
      {
        shadow = shadowCalculationPoint(light.pos.xyz,outPosition,shadowMap,light.shadowindex.yz,int(light.shadowindex.w),eye_linear);
      }
      // if (light.shadowindex != -1)
      // {
      //   shadow = (1 - ShadowCalculation(lightSpacePosition,1.0,shadowMap));//use index of light.shadowindex in array of shadows
      // }
      if (light.pos2.w == 0.0)
      {
        Lo += calculatePointLight(light.pos.xyz,outPosition,V,N,light.color.xyz,roughness,F0,metallic,albedo,sh)*shadow;
      }
      else if (light.pos2.w > 1.5)
      {
        vec3 em_ndl;
        vec4 em_str = calculateEmissiveLight(light.pos.xyz,outPosition,N,light.color.xyz,albedo,light.pos2.w - 2.0,em_ndl);
        emissive += em_str.xyz;
        emissive_ndotl += em_ndl;
        emissive_is_1 = clamp(when_gt(em_str.w,0.999)*when_lt(dot(N,normalize(light.pos.xyz - outPosition)),0.0) + emissive_is_1,0.0,1.0);
      }
      else
      {
        Lo += calculateTubeLight(light.pos.xyz,light.pos2.xyz,10.0,outPosition,V,N,light.color.xyz,roughness,F0,metallic,albedo,sh)*shadow;
      }
      // Lo += calculatePointLight(light.pos.xyz,outPosition,V,N,light.color.xyz,roughness,F0,metallic,albedo,sh)*shadow;

    }

    //sampel from spherical harmonic buffer precomputed
    vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;


    vec3 pos= outPosition + N*griddist*0.25;// + vec3(0,1,0)*griddist;

    //rad sample begin
    vec3 q = quantify(pos);

    vec3 m = lerppoint(pos);
    pos = outPosition;
    // vec4 sh =texture(occTex,outTexcoord);

    // uint mask = uint(sh.x);
    // float b0 = float((mask >> 0) & 1);
    // float b1 = float((mask >> 1) & 1);
    // float b2 = float((mask >> 2) & 1);
    // float b3 = float((mask >> 3) & 1);
    // float b4 = float((mask >> 4) & 1);
    // float b5 = float((mask >> 5) & 1);
    // float b6 = float((mask >> 6) & 1);
    // float b7 = float((mask >> 7) & 1);
    //vec3 pos_sample = pos + N*100 ;//+ N_geom*20;
      // vec3 bmask_coords =  floor((outPosition + N*griddist*0.25 - goffset_occ  + 0.5*gridsize_occ*griddist_occ)/griddist_occ )- vec3(1,1,1);
      // float sample_bitmask = texelFetch(bitmasksTex,ivec3(bmask_coords),0).x;


     // (p - l->occ_gridpos.x)/l->occ_gridsize + floor(l->occ_griddims.x*0.5) = (x-)
     // vec3 bmask_coords =  ((pos - goffset_occ)/griddist_occ + floor(gridsize_occ*0.5)) - vec3(1,1,1);//floor((pos - goffset_occ  + 0.5*gridsize_occ*griddist_occ)/griddist_occ )- vec3(1,1,1);
     // bmask_coords += N ;
     // vec4 sample_bitmask_w = texture(bitmasksTex,vec3(bmask_coords)/gridsize_occ + 0.5/gridsize_occ,0);
     // vec3 sample_bitmask = sample_bitmask_w.xyz;//texture(bitmasksTex,vec3(bmask_coords)/gridsize_occ + 0.5/gridsize_occ,0).xyz;
     // float useful = sign(sample_bitmask_w.w);
     //uint mask = uint(sample_bitmask);
    // float b0 = float((mask >> 0) & 1);
    // float b1 = float((mask >> 1) & 1);
    // float b2 = float((mask >> 2) & 1);
    // float b3 = float((mask >> 3) & 1);
    // float b4 = float((mask >> 4) & 1);
    // float b5 = float((mask >> 5) & 1);
    // float b6 = float((mask >> 6) & 1);
    // float b7 = float((mask >> 7) & 1);

    // float b0 = 1.0;//float((mask >> 0) & 1);
    // float b1 = 1.0;//float((mask >> 1) & 1);
    // float b2 = 1.0;//float((mask >> 2) & 1);
    // float b3 = 1.0;//float((mask >> 3) & 1);
    // float b4 = 1.0;//float((mask >> 4) & 1);
    // float b5 = 1.0;//float((mask >> 5) & 1);
    // float b6 = 1.0;//float((mask >> 6) & 1);
    // float b7 = 1.0;//float((mask >> 7) & 1);

    // float b0 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(0,0,0))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 0) & 1);
    // float b1 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(1,0,0))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 1) & 1);
    // float b2 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(0,1,0))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 2) & 1);
    // float b3 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(1,1,0))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 3) & 1);
    // float b4 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(0,0,1))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 4) & 1);
    // float b5 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(1,0,1))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 5) & 1);
    // float b6 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(0,1,1))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 6) & 1);
    // float b7 = 1.0 - texture(bitmasksTex,vec3(bmask_coords + normalize(getPos(q + vec3(1,1,1))-pos))/gridsize_occ + 0.5/gridsize_occ,0).x;//float((mask >> 7) & 1);
    //outPosition - N_geom*griddist*0.25
  //  vec3 bmask_coords =  (((outPosition) - goffset_occ  + 0.5*gridsize_occ*griddist_occ)/griddist_occ ) - vec3(0.5);
  //  vec4 b0 = texture(bitmasksTex,(bmask_coords + vec3(0,0,0))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;//*2.0 - 1.0;
    // vec4 b1 = texture(bitmasksTex,(bmask_coords + vec3(1,0,0))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;
    // vec4 b2 = texture(bitmasksTex,(bmask_coords + vec3(0,1,0))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;
    // vec4 b3 = texture(bitmasksTex,(bmask_coords + vec3(1,1,0))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;
    // vec4 b4 = texture(bitmasksTex,(bmask_coords + vec3(0,0,1))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;
    // vec4 b5 = texture(bitmasksTex,(bmask_coords + vec3(1,0,1))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;
    // vec4 b6 = texture(bitmasksTex,(bmask_coords + vec3(0,1,1))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;
    // vec4 b7 = texture(bitmasksTex,(bmask_coords + vec3(1,1,1))/gridsize_occ + 0.5/(gridsize_occ),0).xyzw;
    // float b2 = texelFetch(bitmasksTex,ivec3(bmask_coords + vec3(0,1,0)),0).r;
    // float b3 = texelFetch(bitmasksTex,ivec3(bmask_coords + vec3(1,1,0)),0).r;
    // float b4 = texelFetch(bitmasksTex,ivec3(bmask_coords + vec3(0,0,1)),0).r;
    // float b5 = texelFetch(bitmasksTex,ivec3(bmask_coords + vec3(1,0,1)),0).r;
    // float b6 = texelFetch(bitmasksTex,ivec3(bmask_coords + vec3(0,1,1)),0).r;
    // float b7 = texelFetch(bitmasksTex,ivec3(bmask_coords + vec3(1,1,1)),0).r;


    float x = 1 - min(when_lt(q.x,0)+when_gt(q.x,gridsize.x)+when_lt(q.y,0)+when_gt(q.y,gridsize.y) + when_lt(q.z,0)+when_gt(q.z,gridsize.z),1.0);//1 - or(when_lt(x,0),when_gt(x,gridsize.x*gridsize.y*gridsize.z));
    float excluded = 0.f;
    vec4 weights;
    float occlusion_1 = getOcclusion(getPos(q + vec3(0,0,0)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(0,0,0)),pos,sample_bitmask));
    excluded += (1-occlusion_1)*(1- m.x)*(1 - m.y)*(1 - m.z);
    weights.x = (1- m.x)*(1 - m.y)*(1 - m.z);
    float occlusion_2 = getOcclusion(getPos(q + vec3(1,0,0)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(1,0,0)),pos,sample_bitmask));
    excluded += (1-occlusion_2)*(m.x)*(1 - m.y)*(1 - m.z);
    weights.y = (m.x)*(1 - m.y)*(1 - m.z);
    float occlusion_3 = getOcclusion(getPos(q + vec3(0,1,0)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(0,1,0)),pos,sample_bitmask));
    excluded += (1-occlusion_3)*(1- m.x)*(m.y)*(1 - m.z);
    weights.z = (1- m.x)*( m.y)*(1 - m.z);
    float occlusion_4 = getOcclusion(getPos(q + vec3(1,1,0)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(1,1,0)),pos,sample_bitmask));
    excluded += (1-occlusion_4)*(m.x)*(m.y)*(1 - m.z);
    weights.w = (m.x)*(m.y)*(1 - m.z);
    // float scale = 1 /(1- excluded);

    vec4 weights2;
    float occlusion_5 = getOcclusion(getPos(q + vec3(0,0,1)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(0,0,1)),pos,sample_bitmask));
    excluded += (1-occlusion_5)*(1- m.x)*(1 - m.y)*( m.z);
    weights2.x = (1- m.x)*(1 - m.y)*( m.z);

    float occlusion_6 = getOcclusion(getPos(q + vec3(1,0,1)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(1,0,1)),pos,sample_bitmask));
    excluded += (1-occlusion_6)*(m.x)*(1 - m.y)*( m.z);
    weights2.y = ( m.x)*(1 - m.y)*( m.z);

    float occlusion_7 = getOcclusion(getPos(q + vec3(0,1,1)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(0,1,1)),pos,sample_bitmask));
    excluded += (1-occlusion_7)*(1- m.x)*(m.y)*(m.z);
    weights2.z = (1- m.x)*( m.y)*( m.z);

    float occlusion_8 = getOcclusion(getPos(q + vec3(1,1,1)),pos,N);//*((1.0 - useful) + useful*getOcclusion(getPos(q + vec3(1,1,1)),pos,sample_bitmask));
    excluded += (1-occlusion_8)*(m.x)*(m.y)*( m.z);
    weights2.w = (m.x)*( m.y)*( m.z);

    float scale = 1 /(1- excluded);

    Light sum;
    sum = aquireLight(indexify(q + vec3(0,0,0)),scale*occlusion_1*weights.x);//
    sum = addLight(sum,aquireLight(indexify(q + vec3(1,0,0)),scale*occlusion_2*weights.y));
    sum = addLight(sum,aquireLight(indexify(q + vec3(0,1,0)),scale*occlusion_3*weights.z));
    sum = addLight(sum,aquireLight(indexify(q + vec3(1,1,0)),scale*occlusion_4*weights.w));

    sum = addLight(sum,aquireLight(indexify(q + vec3(0,0,1)),scale*occlusion_5*weights2.x));
    sum = addLight(sum,aquireLight(indexify(q + vec3(1,0,1)),scale*occlusion_6*weights2.y));
    sum = addLight(sum,aquireLight(indexify(q + vec3(0,1,1)),scale*occlusion_7*weights2.z));
    sum = addLight(sum,aquireLight(indexify(q + vec3(1,1,1)),scale*occlusion_8*weights2.w));


      vec3 o1 = sphericalHarmonics( N,sum);
      // fragColor = vec3(o1*x);//
    //  o1 *= max((1 - clamp(dot(vec4((N),1), sh),0,1)),0.3);

    //rad sampel EDND
    vec3 irrad_sample = vec3(o1*x);//texture(irradianceMap, gl_FragCoord.xy*invWindow).xyz;
    vec3 irradiance = irrad_sample.rgb;
    vec3 diffuse = irradiance * albedo;

    //sample from reflection buffer precomputed
   vec4 prefilteredColor = texture(prefilterMap, outTexcoord);

   float spec_sky = (1 - clamp(sign(prefilteredColor.w),0,1))*abs(sign(prefilteredColor.w));
   prefilteredColor.w = abs(prefilteredColor.w);
   //prevent divide by zero
   prefilteredColor.xyz /= max(prefilteredColor.w,0.0000001);
   prefilteredColor.xyz *= when_gt(prefilteredColor.w,0);


     // vec3 up = texture(prefilterMap, (gl_FragCoord.xy + ivec2(0,2 ))*invWindow).xyz;
     // vec3 down = texture(prefilterMap, (gl_FragCoord.xy + ivec2(0,-2 ))*invWindow).xyz;
     // vec3 left = texture(prefilterMap, (gl_FragCoord.xy + ivec2(-2 ,0))*invWindow).xyz;
     // vec3 right = texture(prefilterMap, (gl_FragCoord.xy + ivec2(2 ,0))*invWindow).xyz;
     //
     // float normal_derivative = clamp(dot(dFdx(N) + dFdy(N),vec3(1,1,1)),0,1);
     // vec3 total = (up + down+ left + right + prefilteredColor.xyz )/vec3(5.0);
     // total =total*0.5 + prefilteredColor.xyz*0.5;
     // prefilteredColor.xyz = mix(total,prefilteredColor.xyz,min(normal_derivative + when_lt(roughness,0.1),1) );

   // if (prefilteredColor.w == 0.5)
   // {
   //   vec3 up = texture(prefilterMap, (gl_FragCoord.xy + ivec2(0,16 ))*invWindow).xyz;
   //   vec3 down = texture(prefilterMap, (gl_FragCoord.xy + ivec2(0,-16 ))*invWindow).xyz;
   //   vec3 left = texture(prefilterMap, (gl_FragCoord.xy + ivec2(-16 ,0))*invWindow).xyz;
   //   vec3 right = texture(prefilterMap, (gl_FragCoord .xy + ivec2(16 ,0))*invWindow).xyz;
   //   prefilteredColor.xyz = (up + down+ left + right)/vec3(4.0);
   // }
   // prefilteredColor.xyz /= prefilteredColor.w;
   vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;

   #ifdef STOCHASTIC_TRUE
  vec3 specular = prefilteredColor.xyz;// * (F * brdf.x + brdf.y);
  #else
  vec3 specular = prefilteredColor.xyz * (F * brdf.x + brdf.y);
  #endif
  // vec3 specular = prefilteredColor.xyz * (F * brdf.x + brdf.y);
   //specular = specular*(spec_sky*shadowfactor + (1 - spec_sky));
   //exclude specular and add it in later
   #ifdef FULLBRIGHT
   diffuse = albedo;
   #endif
   // brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), 1.0)).rg;
   // if (roughness > 0.5)
   // {
   //   specular = specular*0.75 + 0.25*sphericalHarmonics( reflect(-V,N)*vec3(-1,1,1),sum)* (F * brdf.x + brdf.y);
   // }

   specular = specular*(1.0 - dynamic_flag) + specular*(1.0 + pow(max(dot(N, V), 0.0), 24.0)*0.5)*dynamic_flag;

    vec3 ambient = vec3(kD*diffuse*(1.0/PI)  +specular  )*(ambient_occ);//
    float condit = when_eq(depthval,1);
    Lo += pow(1.0 - max(0.0,dot(N,V)),4.0)*0.01*albedo*dynamic_flag;


    // 0.01 +
    Lo += (pow(1.0 - max(0.0,dot(N,V)),2.0)*0.07 + pow(max(dot(N, V), 0.0), 24.0)*0.02)*vec3(1.0)*albedo*glow_flag ;


    float barrel_len = 1.0 - getBarrelLengthTerm(pos,blackbody_barrel_pointa,blackbody_barrel_pointb);
    //blackbody_barrel_tip_temp - blackBodyDelay(barrel_len,blackbody_barrel_shape)

    float bb_param = blackbody_barrel_tip_temp - blackBodyDelay(barrel_len,blackbody_barrel_shape);
    vec3 blackbody_barrel_color = blackBodyLookup(clamp(bb_param,0.0,1.0));

    Lo += blackbody_barrel_color*hotbarel_flag;

    //(albedo / PI )


    Lo += emissive_ndotl*(1.0 - emissive_is_1);
    Lo += emissive*emissive_is_1;
    //TODO dynamic object outlien glow
    //Lo += pow(1.0 - max(0.0,dot(N,V)),4.0)*0.2*albedo*dynamic_flag;
     color = (  ambient*cheap_ao + Lo*ambient_occ );//*(1-condit) + sky_color*condit;
//      color = color*(1.0 - emissive_is_1) + emissive*emissive_is_1;
     color = color*(1-condit) + sky_color*condit;

     //color = (  specular )*(1-condit) + sky_color*condit;

     //ambient + Lo*ambient_occ
    // albedo
     //
      // color = sh.xyz*0.5 + 0.5;
    // // color = texture(prefilterMap, outTexcoord).xyz;
    //    #undef POSTPROCESS
      //       color = vec3(m);//vec3(length(sample_bitmask - outPosition);
     // uint mask = uint(sh.x);
     // float b1 = float((mask >> 0) & 1);
     //  float b2 = float((mask >> 1) & 1);
     //  float b3 = float((mask >> 2) & 1);
     // // float b4 = float((mask >> 3) & 1);
     // color = vec3(b1,b2,b3);
    //  	const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);
    //  float ofactor =  (1 - clamp(dot(vec4((vec3(1,0,0)),0).yzxw, sh*sh2_weight),0,1));
    // color = vec3(ofactor);
     float boost_base = 1.0;
     float boost_fresnel = 4.0;

     boost_base = boost_base*(1.0 - glow_flag) + 2.0*glow_flag;

     boost_fresnel = boost_fresnel*(1.0 - glow_flag) + 8.0*glow_flag;


     //dynamic objetc boost
     vec3 boost = vec3(boost_base) + vec3(1.0)*pow(1.0 - max(0.0,dot(N,V)),1.3)*boost_fresnel;//fresnelSchlick(max(0.0,dot(N,V)),vec3(0.04),0.1);

    //vec3(5.0)
     color = (color * boost)*dynamic_flag + color*(1.0 - dynamic_flag);


     vec3 iridescence = thinFilmIridescence(N, V, 500, 1.4);



      color = color + iridescence*0.1*iridescent_flag ;



   }

   vec4 forward_value = texture(forward,outTexcoord);
  //color = mix(color,forward_value.xyz,forward_value.w);



  // float factor = 1.0 - clamp(exp(-0.001 * distance(eyePos,outPosition)), 0.0, 1.0);
   //color = color*(1 - factor) + Lo_fog*(factor);
   #ifdef USE_FOG_POST
   vec3 fogval = vec3(0);
   // float std = 20.0;
   // float r = 20.0*SampleRandomNumber(uint(gl_FragCoord.x),uint(gl_FragCoord.y),0,0);
   // float weight = 0.0;
  // float g = gaussian(vec2(0,-r),std);
  // float fog_ceiling = 1.0;
  //  float factor = (1.0 - clamp(exp(-fog_dens * distance(eyePos,outPosition)), 0.0, 1.0))*fog_ceiling;

   vec4 ftex = texture2D(fog_tex,(gl_FragCoord.xy )*invWindow);
   fogval += ftex.xyz;//*factor;
   float optical_depth_weight = ftex.w;
   // weight += 1;

   color =  color*(optical_depth_weight) + fogval;///(weight);//Lo_fog*2.0;// (Lo_fog*32.0)/(steps);
   #endif


   color = color*(1.0 - forward_value.w) + forward_value.xyz;




   // color = color + ambient_fog_color*(1.0 - clamp(exp(-ambient_fog_density * distance(eyePos,outPosition)), 0.0, 1.0));
    //color = mix(color,Lo_fog,1.0 - fog_factor);
    // vec4 viewsp = inverse(viewMatrixInv)*vec4(outPosition,1.0);
    // vec4 clipsp = inverse(projMatrixInv)*inverse(viewMatrixInv)*vec4(outPosition,1.0);
    //  clipsp.z = clipsp.z/clipsp.w;
    //  clipsp.z = clipsp.z*0.5 + 0.5;


  // fragColor = vec3(color);


   // //texture(colorbuffer,outTexcoord).rgb +

   // vec3 dpos = planeDist(vec3(0,0,0),fogPlaneNormal,eyePos)*fogPlaneNormal;
   //
   // float iv = when_lt(planeDist(dpos,fogPlaneNormal,outPosition),0);
   // float density  = (1-iv)*(max((planeDist(fogPlanePos,fogPlaneNormal,outPosition)),0)/fog_div);
   // density += (iv)*(max((planeDist(fogPlanePos,fogPlaneNormal,eyePos)),0)/fog_div);
   //
   // float depthVal2 = fogHard/(exp((depthval )*density)) ;
   //
   // float f =   ( 1 - clamp( depthVal2,0,1))  ;
   //
   // f = pow(f,fog_pow);
   // color = mix(color,fogColor , clamp(f,0,1));
   // color = texture(old_reflectiondata, gl_FragCoord.xy*invWindow).xyz;
//   #undef POSTPROCESS
   #ifdef POSTPROCESS
   color *= vec3(colorS*exp2(displayExposure)) ;
    // color = color / (color + vec3(1.0));
     vec3 curr = Uncharted2Tonemap(color);
     vec3 whiteScale = 1.0f/Uncharted2Tonemap(vec3(W));
    color = curr*whiteScale;

    // float saturation = 1.0; // >1 = more color, <1 = grayscale
    //
    // float luma = dot(color, vec3(0.299, 0.587, 0.114)); // perceived brightness
    // color = mix(vec3(luma), color, saturation);

    vec3 hsv_space = rgb2hsv(color);
    hsv_space.y = clamp(hsv_space.y + 0.07,0,1);
    hsv_space.z = clamp(curve_func(clamp(hsv_space.z,0,1),1.2,1.6) ,0,1);
    color = hsv2rgb(hsv_space);
    // vec3 hsv_space = rgb2hsv(color);
    // hsv_space.y = clamp(hsv_space.y + 0.2,0,1);
    // hsv_space.z = clamp(curve_func(clamp(hsv_space.z,0,1),1.4,1.4) ,0,1);
    // color = hsv2rgb(hsv_space);





    color = pow(color, vec3(1.0/displayGamma));
    //color *= 0.85;
    vec2 uv = gl_FragCoord.xy*invWindow;

  uv *=  1.0 - uv.yx;   //vec2(1.0)- uv.yx; -> 1.-u.yx; Thanks FabriceNeyret !

  float vig = uv.x*uv.y * 15.0; // multiply with sth for intensity

  float vnoise = max(voronoise(gl_FragCoord.xy*invWindow*12.0,1.0,0.0)*0.3 + 0.7,0.3);
  vig = pow(vig, glow_factor*vnoise); // change pow for modifying the extend of the  vignette


  color = mix(color,glow_color,1 - vig);
  #define CROSSHAIR
  #ifdef CROSSHAIR
//   float anglep = abs(dot(normalize(gl_FragCoord.xy - screenSize*vec2(0.5)),vec2(1,0)));
//   if (distance(gl_FragCoord.xy,screenSize*vec2(0.5)) < th_crosshair_size*20*(screenSize.y/1080.0) && ((anglep  < 0.1 && anglep > -0.1 ) || (anglep > 0.99 && anglep < 1.11)))
//   {
//     color = vec3(1.0) - color;//vec3(0,1,0);
//   }

// {  vec2 center = screenSize * 0.5;
//   vec2 d = gl_FragCoord.xy - center;
//
//   float dist = length(d);
//
//   // normalized direction
//   vec2 dir = normalize(d);
//   float anglep = max(abs(dot(dir, vec2(1.0, 0.0))),abs(dot(dir, vec2(0.0, 1.0))));
//
//   // scale
//   float radius = th_crosshair_size * 20.0 * (screenSize.y / 1080.0);
//
//   // thickness controls
//   float lineWidth = 0.02;
//   float aa = fwidth(anglep); // automatic AA width
//
//   // horizontal + vertical lines
//   float lineMask =
//   smoothstep(lineWidth + aa, lineWidth - aa, anglep) +
//   smoothstep(1.0 - lineWidth - aa, 1.0 - lineWidth + aa, anglep);
//
//   // radial cutoff with AA
//   float distAA = fwidth(dist);
//   float circleMask = smoothstep(radius + distAA, radius - distAA, dist);
//
//   // final mask
//   float mask = clamp(lineMask * circleMask, 0.0, 1.0);
//
//   // blend instead of hard replace
//   color = mix(color, vec3(1.0) - color, mask);}


  {
    vec2 center = screenSize * 0.5;
    vec2 d = gl_FragCoord.xy - center;

    // distance from axes
    vec2 ad = abs(d);

    // crosshair settings (in pixels)
    float halfLength = th_crosshair_size*0.65 * 20.0 * (screenSize.y / 1080.0);
    float thickness  = 1.5;

    // AA width (pixel footprint)
    float aaX = max(fwidth(ad.x), 1e-4);
    float aaY = max(fwidth(ad.y), 1e-4);

    float horiz =
    (1.0 - smoothstep(thickness - aaY, thickness + aaY, ad.y)) *
    (1.0 - smoothstep(halfLength - aaX, halfLength + aaX, ad.x));

    float vert =
    (1.0 - smoothstep(thickness - aaX, thickness + aaX, ad.x)) *
    (1.0 - smoothstep(halfLength - aaY, halfLength + aaY, ad.y));

    // combine
    float mask = clamp(horiz + vert, 0.0, 1.0);

    // apply
    color = mix(color, vec3(1.0) - color, mask);
  }
  #endif
    #endif



   float lum = 0.2126*color.r + 0.7152*color.g + 0.0722*color.b;
  //#define CLUSTER_DEBUG_VIEW
   #ifdef CLUSTER_DEBUG_VIEW
//     uint cluster_slice_id = getClusterZIndex(depthval);

   ivec2 tile_id = ivec2(gl_FragCoord.xy / vec2(screenSize.x/16.0,screenSize.y/8.0));


   uint cluster_slice_id = getClusterZIndex(depthval);


   uint tile_index = tile_id.x + tile_id.y*num_tiles_x + cluster_slice_id*num_tiles_x*num_tiles_y;

   uvec2 arr[2];
   arr[0] = offsets[tile_index/2].xy;
   arr[1] = offsets[tile_index/2].zw;
   uint count = arr[tile_index%2].y;

   fragColor = vec3(float(count)/5.0 + 0.5,0,0) + lum*0.1;
   #else


   fragColor = vec3(color);

   #endif

 // #define AO_DEBUG_VIEW
   #ifdef AO_DEBUG_VIEW

   float aodebug = texture(occTex,gl_FragCoord.xy*invWindow).r;
    fragColor = vec3(aodebug);

   #endif
   // #define LIGHT_COUNT_DEBUG_VIEW
    #ifdef LIGHT_COUNT_DEBUG_VIEW
    ivec2 tile_id = ivec2(gl_FragCoord.xy / vec2(screenSize.x/16.0,screenSize.y/8.0));


    uint cluster_slice_id = getClusterZIndex(depthval);
    uint tile_index = tile_id.x + tile_id.y*num_tiles_x + cluster_slice_id*num_tiles_x*num_tiles_y;
    uvec2 arr[2];
    arr[0] = offsets[tile_index/2].xy;
    arr[1] = offsets[tile_index/2].zw;
    uint count = arr[tile_index%2].y;
    uint offset = arr[tile_index%2].x;

    fragColor = vec3(0.0,0.0,float(count)/15.0);
    #endif


}
