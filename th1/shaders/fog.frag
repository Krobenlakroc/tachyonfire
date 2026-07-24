// #version 460 core
// #define POSTPROCESS
// #define USE_FOG_POST
// #define TH_Z_SLICES 15
// #define TH_Z_MEMORY 10
//#define OCCLUSION
#define STOCHASTIC_TRUE
#define texture2D texture
uniform vec3 fog_color = vec3(0.075*0.5,0.075*0.5,0.075*0.5);
uniform float fog_gain = 0.65;
//model fog as either lit or unlit, kind of stupid but I dont feel like sampling the ambient lightmap
uniform float fog_density = 0.0;
uniform float fog_ambient_density = 0.0;
//#define FULLBRIGHT
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;
const float constantbias = 0.001;//0.01*0.25;//0.00002;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform mat4 lightSpaceMatrix;
uniform mat4 lightSpaceMatrices[48];
uniform vec3 eyePos;
const float PI = 3.14159265359;
layout(binding=5) uniform sampler2D texCoordTex;
layout(binding=41) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=0) uniform sampler2D irradianceMap;
layout(binding=32) uniform sampler2D prefilterMap;
layout(binding=4) uniform sampler2D brdfLUT;
layout(binding=23) uniform samplerCube atmosphereCube;
layout(binding=33) uniform sampler2D forward;
uniform mat4 viewMat;
uniform vec2 invWindow;
uniform vec2 screenSize;
uniform float sky_boost = 1.0;
layout(binding=24) uniform sampler2D shadowMap;
uniform vec3 shadowPos;

layout(binding=26) uniform sampler2D occTex;

uniform float shadow_blur_radius_fog = 0.001;
uniform float th_shadowmap_resolution = 1024;
uniform float th_shadowmap_sun_scale = 1;
uniform float th_shadowmap_omni_scale = 1;
uniform float zNear = 0.1;
uniform float zFar = 1000;
uint num_tiles_x = 16;
uint num_tiles_y = 8;
uint num_slices = TH_Z_SLICES;

uniform vec3 sunlightColor;
uniform vec3 sunlightDir;

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

uniform float fog_distance = 2000.0;
uniform float fog_quality = 1.0; // higher is worse
uniform float mu = 0.5139032120;
uniform float alpha = 0.7213475207;
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
float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, uint sample_dimension) {
    // Wrap arguments
    pixel_i = pixel_i & 127u;
    pixel_j = pixel_j & 127u;
    // sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    const uint ranked_sample_index = sample_index ^ 0;


    // Fetch value in sequence
    uint value = uint(32.0*(1.0-float(sample_dimension)) + 226.0*float(sample_dimension));//g_sobol_buffer[sample_dimension + ranked_sample_index * 256u];

    // // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ g_scrambling_tile_buffer[(uint(sample_dimension) ) + (pixel_i + pixel_j * 128u)*8u ];//

    // Convert to float and return
    return (float(value) + 0.5) / 256.0f;
}

float SampleRandomNumber3D(uint pixel_i, uint pixel_j,uint pixel_k, uint sample_index, uint sample_dimension) {
    // Wrap arguments
    pixel_i = pixel_i & 32u;
    pixel_j = pixel_j & 32u;
    pixel_k = pixel_k & 16u;
    // sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    const uint ranked_sample_index = sample_index ^ 0;


    // Fetch value in sequence
    uint value = uint(32.0*(1.0-float(sample_dimension)) + 226.0*float(sample_dimension));//g_sobol_buffer[sample_dimension + ranked_sample_index * 256u];

    // // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ g_scrambling_tile_buffer[(uint(sample_dimension) ) + (pixel_i + pixel_j * 32u + pixel_k*32u*32u)*8u ];//

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

const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 3; //3
//10
vec3 Uncharted2Tonemap(vec3 x)
{
     return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

const float samplesCount_fog = 1;

const float samplesCount = 10;

vec3 VogelSphereSample(float sampleIndex,  float phi)
{
  float GoldenAngle = 2.4f;


  float theta = sampleIndex * GoldenAngle + phi;
  float z = (1.0 - (1.0/samplesCount)) * ( 1.0 - ((2.0*sampleIndex)/(samplesCount - 1) ));
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
    vec3 DDX = vec3(0);//vec3(dFdx(projCoords.x),dFdx(projCoords.y),dFdx(projCoords.z));
    vec3 DDY = vec3(0);//vec3(dFdy(projCoords.y),dFdy(projCoords.y),dFdy(projCoords.z));
    // float invDet = 1 / ((DDX.x * DDY.y) - (DDX.y * DDY.x) );
    // vec2 deltas;
    // deltas.x = DDY.y *DDX.z;
    // deltas.x -= DDX.y *DDY.z;
    //
    // deltas.y = DDX.x *DDY.z;
    // deltas.y -= DDY.x *DDX.z;
    // deltas*=invDet;
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

for(float i = 0; i < samplesCount; i+=1)
{
  vec2 sampleUV = VogelDiskSample(i,  gradientNoise);
  vec2 offset = sampleUV * penumbra * shadowmaxsize;
  sampleUV = projCoords.xy + offset;

  float bias = deltas.x*offset.x + deltas.y*offset.y;
  float closestDepths = texture(smap, sampleUV*0.25 + atlasCoord*0.25,0).r;
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


    // check whether current frag pos is in shadow

    float shadow = 0.0f;


  vec2 sampleUV = projCoords.xy;

  float bias = 0;//deltas.x*offset.x + deltas.y*offset.y;
  float closestDepths = texture(smap, sampleUV*0.25*th_shadowmap_sun_scale + atlasCoord*0.25*th_shadowmap_sun_scale,0).r;
  shadow += when_gt(currentDepth +bias - constantbias , closestDepths) ;


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
//   offsets[0] = vec2(0.25,0);
//   offsets[1] = vec2(0.375,0);
//   offsets[2] = vec2(0.25,0.125);
//   offsets[3] = vec2(0.375,0.125);
//   offsets[4] = vec2(0.25,0.25);
//   offsets[5] = vec2(0.375,0.25);

  float iscale = (0.25*th_shadowmap_omni_scale*0.5);

  offsets[0] = vec2(0.25,0);
  offsets[1] = vec2(0.25 + iscale,0);
  offsets[2] = vec2(0.25,iscale);
  offsets[3] = vec2(0.25 + iscale,iscale);
  offsets[4] = vec2(0.25,iscale*2);
  offsets[5] = vec2(0.25 + iscale,iscale*2);

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

    float closestDepths = texture(smap, uv*iscale + offsets[int(face)] + atlasCoord*0.25,0).r;

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


float  shadowCalculationPoint(vec3 lightpos,vec3  fragpos,sampler2D smap,vec2 atlasCoord,int matrix_offset)
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
  for(float i = 0; i < samplesCount; i+=1)
  {
    vec3 pos_jitter = VogelSphereSample(i,gradientNoise)*10;
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
  totalshadow /= samplesCount;
  return 1 - totalshadow;
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

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
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
    ofactor =  (1 - clamp(dot(vec4((-lightdir),1).yzxw, sh*sh2_weight),0,1));
    #endif

    return (kD * albedo / PI + specular) * radiance * NdotL*ofactor;
}

vec3 calculatePointLight(vec3 lightpos,vec3 outPosition,vec3 V,vec3 N,vec3 lightcolor,float roughness,vec3 F0,float metallic,vec3 albedo,vec4 sh)
{
    // calculate per-light radiance
    vec3 L = normalize(lightpos - outPosition);
    vec3 H = normalize(V + L);
    float d    = length(lightpos - outPosition);
    float attenuation = 1.0 / (d * d);
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
    ofactor =  (1 - clamp(dot(vec4((-L),1).yzxw, sh*sh2_weight),0,1));
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

    float ndc = 2.0 * depth - 1.0;
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
    uint zIndex = max(uint(float(num_slices)*(log(eyeDepth/zNear)/log(zFar/zNear))) - 5,0);
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

  // l.a[0] = texelFetch(lights,IdToCoordi(coord + 0),0).xyz*scale;
  // l.a[1] = texelFetch(lights,IdToCoordi(coord + 1),0).xyz*scale;
  // l.a[2] = texelFetch(lights,IdToCoordi(coord + 2),0).xyz*scale;
  // l.b[0] = texelFetch(lights,IdToCoordi(coord + 3),0).xyz*scale;
  // l.b[1] = texelFetch(lights,IdToCoordi(coord + 4),0).xyz*scale;
  // l.b[2] = texelFetch(lights,IdToCoordi(coord + 5),0).xyz*scale;
  // l.c[0] = texelFetch(lights,IdToCoordi(coord + 6),0).xyz*scale;
  // l.c[1] = texelFetch(lights,IdToCoordi(coord + 7),0).xyz*scale;
  // l.c[2] = texelFetch(lights,IdToCoordi(coord + 8),0).xyz*scale;


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
     return max(0,dot(normalize(pos-worldpos),normal));
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

float phase_hg(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / ( 4*PI*pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

float remap(float x,float omin,float omax,float newmin,float newmax)
{
  float rr = (clamp(x,omin,omax) - omin)/(omax - omin);
  return rr*(newmax - newmin) + newmin;
}
#define USE_FOG_PHASE
void main()
{


  float depthval = texture2D(depthTex,gl_FragCoord.xy*invWindow ).r;


    float ldepth;
    vec3 outPosition = WorldPosFromDepth(gl_FragCoord.xy*invWindow,depthval,ldepth);

    float to_infinity = when_eq(depthval,1.0);

    bool fastpath = allInvocations(to_infinity == 1.0 || fog_density == 0.0);


    if (fastpath)
    {

      #ifdef USE_FOG_PHASE
      vec3 V = normalize(eyePos - outPosition);
      float cosTheta = dot(V, -sunlightDir);
      const float phase = phase_hg(cosTheta, 0.2)*4*PI;
      #else
      const float phase = 1.0;
      #endif
      float optical_depth = fog_density*fog_distance;
      float color_mult = mu;
      float optical_depth_add = alpha;
            fragColor = vec4( fog_color*sunlightColor*to_infinity*color_mult*phase ,(1.0 - to_infinity) + to_infinity*exp(-(optical_depth + optical_depth_add)) );
    }
    else
    {
      vec3 Lo_fog = vec3(0.0);

      vec3 V = normalize(eyePos - outPosition);
      // float fog_density = 0.0001;
      // float fog_factor = clamp(exp(-fog_density * distance(eyePos,outPosition)), 0.0, 1.0);
      //float fog_density_factor = 0.01;
      //0.5
      //vec3(0.075,0.075,0.075)*0.5;
      // vec3 ambient_fog_color = vec3(0.075,0.075,0.075)*0.1;//vec3(0.075,0.075,0.075)*0.001;
      // float ambient_fog_density = 0.001;
      // float steps = max(floor(distance(eyePos,outPosition)/64.0),1.0);
      float blueNoise = SampleRandomNumber(uint(gl_FragCoord.x),uint(gl_FragCoord.y),0,0);
      //float blueNoise2 = SampleRandomNumber(uint(gl_FragCoord.x*2.0),uint(gl_FragCoord.y*2.0),0,1);
      // float blueNoise_1 = SampleRandomNumber(uint(gl_FragCoord.x),uint(gl_FragCoord.y),0,1);
      // float lval = sin(time*0.001)*0.5 + 0.5;
      //  blueNoise = mix(blueNoise,blueNoise2,blueNoise2);
      float steps = 0.0;

      float dx = 100*fog_quality*(0.75 + mod(blueNoise,0.25));
      float optical_depth = 0;

      float cosTheta = dot(V, -sunlightDir);

      const float stepScale = 1.08;



      #ifdef USE_FOG_PHASE
      const float phase = phase_hg(cosTheta, 0.2)*4*PI;
      #else
      const float phase = 1.0;
      #endif

      for(float i = 0; i <= min(distance(eyePos,outPosition),fog_distance); i+= dx)
      {
        float b = i;// - 100* ;
        // i += bumo*10;//
        steps += 1.0;
        vec3 fog_pos = eyePos + -V*(b*(0.75 + mod(blueNoise,0.25)) );//distance(eyePos,outPosition)*(i/32.0)
        //blueNoise *= 1.0 + mod(SampleRandomNumber3D(uint(fog_pos.x),uint(fog_pos.y),uint(fog_pos.z),0,0),0.25);
        vec4 lightSpacePosition_fog = lightSpaceMatrix*vec4(fog_pos,1.0);
        float shadowfactor_fog = (1 - ShadowCalculationFog(lightSpacePosition_fog,1.0,shadowMap,vec2(0,0)));

        //  #define FOG_POINTLIGHTS
        float any_lit = 0.0;
        vec3 add_fogcolor = vec3(0.0);
        #ifdef FOG_POINTLIGHTS
        ivec2 tile_id = ivec2(gl_FragCoord.xy / vec2(screenSize.x/16.0,screenSize.y/8.0));
        uint cluster_slice_id = getClusterZIndex(depthval);
        uint tile_index = tile_id.x + tile_id.y*num_tiles_x + cluster_slice_id*num_tiles_x*num_tiles_y;

        uvec2 arr[2];
        arr[0] = offsets[tile_index/2].xy;
        arr[1] = offsets[tile_index/2].zw;
        uint count = arr[tile_index%2].y;
        uint offset = arr[tile_index%2].x;

        for (int j = 0 ; j < count;j++)
        {
          uint a = offset + i;
          uint key_bits = acessbuffer[a/8][(a%8)/2];
          int bit_offset = int((a % 2) *  16);
          uint key = bitfieldExtract(key_bits,bit_offset,16);

          PointLight light = pointlights[key];

          float shadow = 1;
          if  (light.shadowindex.x != 0.0)
          {
            shadow = shadowCalculationPoint_fog(light.pos.xyz,fog_pos,shadowMap,light.shadowindex.yz,int(light.shadowindex.w));
          }

          shadow = shadow*float(key >= 2 );

          float d    = length(light.pos.xyz - fog_pos);
          float attenuation = 1.0 / (d * d);
          vec3 radiance     = light.color.xyz * attenuation;

          //Lo_fog += factor*fog_color*radiance*shadow*(1.0/2.0);

          const float fudge_factor = 0.25;//prevent overintensity
          any_lit += shadow*attenuation*length(light.color.xyz)*fudge_factor;
          add_fogcolor += shadow*light.color.xyz*attenuation*fudge_factor;
        }
        #endif


        //float height_k = 0.005*mix(1.0,0.0,clamp((-fog_pos.y/1000) + 1,0.0,1.0));
        //float height_k = 0.005*(1.0 - remap(-fog_pos.y,0,1000,0.0,1.0));

        float mu = fog_density;
        float mu_prime = fog_ambient_density;
        float k = shadowfactor_fog*mu + (1.0 - shadowfactor_fog)*mu_prime + any_lit*mu;
        //k = k + height_k;

        float k_no_anylit = shadowfactor_fog*mu + (1.0 - shadowfactor_fog)*mu_prime;
        //k_no_anylit = k_no_anylit + height_k;

        //to_infinity = 0;
        //calculateDirectionalLight(sunlightDir,V,V,sunlightColor,1.0,vec3(0.04),0.0,fog_color,vec4(1,1,1,1))*
        float factor = 1.0;// 1.0 - clamp(exp(-fog_density_factor * distance(eyePos,fog_pos)), 0.0, 1.0);

        optical_depth += dx*k;
        float transmittance = exp(-optical_depth);

        //TODO throw in a configurable phase function
        //
        Lo_fog +=  factor*fog_color*sunlightColor*k_no_anylit*transmittance * dx*phase;//*phase;//(1.0/max(steps,32.0));
        Lo_fog += add_fogcolor*mu* dx*transmittance;

        dx *= stepScale;
        //  Lo_fog += factor*fog_color*0.001;


      }
      // float factor = 1.0 - clamp(exp(-0.001 * distance(eyePos,outPosition)), 0.0, 1.0);
      //color = color*(1 - factor) + Lo_fog*(factor);
      vec3 color =   (Lo_fog*fog_gain);//*2.0;//*2.0;//*2.0;// (Lo_fog*32.0)/(steps);
      //   color = color + ambient_fog_color*(1.0 - clamp(exp(-ambient_fog_density * distance(eyePos,outPosition)), 0.0, 1.0));
      //
      //when reaching out a ray to infinit ywe need to model a falloff towards a lesser density
      //we do this by adding a fudge factor
      //i dont feel like integrating so i will guess




      //  ///FUDGE FACTOR
      //  //https://www.desmos.com/calculator/prpgxo2ie8
      //https://www.desmos.com/calculator/vovoivrbhh
      // float color_mult = 0.6357;//0.814
      // float optical_depth_add = 1.009;//1.682

      // float color_mult = 0.51;
      // float optical_depth_add = 0.72;


      //for 0.0007
      //   float color_mult = 0.635;
      //   float optical_depth_add = 1.0009;

      //for 0.0005
      float color_mult = mu;
      float optical_depth_add = alpha;

      //for 0.0003
      // float color_mult = 0.3513;
      // float optical_depth_add = 0.432;

      fragColor = vec4(color*(1 - to_infinity) + fog_color*sunlightColor*to_infinity*color_mult*phase,exp(-optical_depth)*(1 - to_infinity) + to_infinity*exp(-(optical_depth + optical_depth_add)) );

      ///NO FUDGE FACTOR
      //https://www.desmos.com/calculator/prpgxo2ie8
      //fragColor = vec4(color,exp(-optical_depth) );
    }



}
