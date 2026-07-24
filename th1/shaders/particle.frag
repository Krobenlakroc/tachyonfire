//#version 460 core

#define texture2D texture
// layout(early_fragment_tests) in;
in vec2 ID;
in vec2 outTexcoord;
in vec3 outPosition;
in vec3 outTangent;
in vec3 outNormal;
flat in float colorscale;
flat in float alphascale;
flat in float blend;
flat in float use_add_blending;
flat in float light_use_norm;
flat in float use_reflections;
flat in float use_premultiplied;

// layout (location = 0,index = 0) out vec4 forwardBuffer0;
// layout (location = 0,index = 1) out vec4 forwardBuffer1;

layout (location = 0) out vec4 forwardBuffer0;


// layout (location = 0) out vec4 forwardBuffer;
layout(binding=9) uniform sampler2DArray materials[3];

const float constantbias = 0.005;//0.01*0.25;//0.00002;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform mat4 lightSpaceMatrix;
uniform mat4 lightSpaceMatrices[48];
uniform vec3 eyePos;

uniform vec3 fog_color = vec3(0.075*0.5,0.075*0.5,0.075*0.5);
uniform float fog_gain = 0.65;
//model fog as either lit or unlit, kind of stupid but I dont feel like sampling the ambient lightmap
uniform float fog_density = 0.0;
uniform float fog_ambient_density = 0.0;

uniform float fog_distance = 2000.0;
uniform float fog_quality = 1.0; // higher is worse
uniform float mu = 0.5139032120;
uniform float alpha = 0.7213475207;

const float PI = 3.14159265359;
layout(binding=5) uniform sampler2D texCoordTex;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=0) uniform sampler2D irradianceMap;
layout(binding=32) uniform sampler2D prefilterMap;
layout(binding=4) uniform sampler2D brdfLUT;
layout(binding=23) uniform samplerCube atmosphereCube;
layout(binding=33) uniform sampler2D forward;
layout(binding=22) uniform samplerCubeArray reflections;
uniform mat4 viewMat;
uniform vec2 invWindow;
uniform vec2 screenSize;
layout(binding=24) uniform sampler2D shadowMap;
uniform vec3 shadowPos;

layout(binding=26) uniform sampler2D occTex;

uniform float shadow_blur_radius = 0.001;
uniform float shadow_blur_radius_fog = 0.001;
uniform float th_shadowmap_resolution = 1024;
uniform float th_shadowmap_sun_scale = 1;
uniform float th_shadowmap_omni_scale = 1;
uniform float zNear = 0.1;
uniform float zFar = 1000;
uint num_tiles_x = 16;
uint num_tiles_y = 8;
uint num_slices = TH_Z_SLICES;

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
  uniform uvec4 offsets[(16*8*10)];//pack into vec4 for std140
};

layout (std140 , binding = 0) uniform EnvBoxes
{
  vec4 EnvBoxPos[32];// 0
  vec4 EnvBoxMin[32];// size*16
  vec4 EnvBoxMax[32];//size * 16*2
  int EnvBoxCount;//size*16*3
  //total size is size*16*3 + 4
};

layout (std140 , binding = 5) uniform Access_Buffer_CUBES //
{
  uniform uvec4 acessbuffercubes[4096];//use the max amount of memory
};




uniform vec3 sunlightColor;
uniform vec3 sunlightDir;

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

float CheckCollision(vec3 pos, vec3 bmin,vec3 bmax)
{
  return when_lt(pos.x,bmax.x)*when_lt(pos.y,bmax.y)*when_lt(pos.z,bmax.z)*when_gt(pos.x,bmin.x)*when_gt(pos.y,bmin.y)*when_gt(pos.z,bmin.z);
}

float distance_squared(vec3 a,vec3 b)
{
  vec3 d = a -b;
  return dot(d,d);
}


vec3 WorldPosFromDepth(float depth,out float linDepth) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(invWindow*gl_FragCoord.xy * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;
  // linDepth = viewSpacePosition.z;
  // Perspective division

  viewSpacePosition /= viewSpacePosition.w;

  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
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


float when_ge(float x, float y) {
  return 1.0 - max(sign(y - x), 0.0);
}

float when_le(float x, float y) {
  return 1.0 - when_gt(x,y);
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
const float W = 10;

vec3 Uncharted2Tonemap(vec3 x)
{
     return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

const float samplesCount = 1;
vec2 VogelDiskSample(float sampleIndex,  float phi)
{
  float GoldenAngle = 2.4f;

  float r = sqrt(sampleIndex + 0.5f) / sqrt(samplesCount);
  float theta = sampleIndex * GoldenAngle + phi;

  float sine = sin(theta);
  float cosine = cos(theta);

  return vec2(r * cosine, r * sine);
}

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
    // vec3 DDX = vec3(dFdx(projCoords.x),dFdx(projCoords.y),dFdx(projCoords.z));
    // vec3 DDY = vec3(dFdy(projCoords.y),dFdy(projCoords.y),dFdy(projCoords.z));
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

    // mat2 toscreen = inverse(mat2(DDX.x,DDY.x,DDX.y,DDY.y));
    // vec2 rightratio = toscreen*righttexel;
    // vec2 upratio = toscreen*uptexel;
    // float rightdelta = rightratio.x * DDX.z
    //         + rightratio.y * DDY.z;
    // float updelta = upratio.x*DDX.z + upratio.y*DDY.z;
    // vec2 deltas = vec2(rightdelta,updelta)*(th_shadowmap_resolution);


    // check whether current frag pos is in shadow
    float gradientNoise = InterleavedGradientNoise(gl_FragCoord.xy)*2*3.1459;
    float penumbra = 0.05*5;//Penumbra(gradientNoise, projCoords.xy, currentDepth ,deltas,smap);

    float shadow = 0.0f;

for(float i = 0; i < samplesCount; i+=1)
{
  vec2 sampleUV = VogelDiskSample(i,  gradientNoise);
  vec2 offset = sampleUV * shadow_blur_radius* (1.0/(th_shadowmap_resolution*th_shadowmap_sun_scale));
  sampleUV = projCoords.xy + offset;

  float bias = 0;//deltas.x*offset.x + deltas.y*offset.y;
  float closestDepths = texture(smap, sampleUV*0.25*th_shadowmap_sun_scale + atlasCoord*0.25*th_shadowmap_sun_scale,0).r;
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
  float iscale = (0.25*th_shadowmap_omni_scale*0.5);

  offsets[0] = vec2(0.25,0);
  offsets[1] = vec2(0.25 + iscale,0);
  offsets[2] = vec2(0.25,iscale);
  offsets[3] = vec2(0.25 + iscale,iscale);
  offsets[4] = vec2(0.25,iscale*2);
  offsets[5] = vec2(0.25 + iscale,iscale*2);

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

    float closestDepths = texture(smap, uv*iscale + offsets[int(face)] + atlasCoord*0.25,0).r;

    vec4 fragPosLightSpace =  lightSpaceMatrices[int(face) + matrix_offset*6]*vec4(fragpos,1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;

    float shadow = when_gt(currentDepth - 0.000005 , closestDepths) ;
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



vec2 IdToCoord(float ID)
{
  float f = ID/dims_harmtex.x;
  float yval = floor(f);
  float xval = f - yval;
  yval /=dims_harmtex.y;
  return vec2(xval + (0.5*(1.f/dims_harmtex.x)),yval + (0.5*(1.f/dims_harmtex.y)));
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


  return l;
}





vec3 quantify(vec3 position)
{
  return floor((position - goffset)/griddist  + 0.5*gridsize) - vec3(1,1,1);
}

vec3 lerppoint(vec3 position)
{
  vec3 quant = ((position - goffset)/griddist  + 0.5*gridsize) ;
   quant -= vec3(1,1,1);
  quant -= floor(quant);
  return quant;
}

float indexify(vec3 quant)
{
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



float isDithered(vec2 pos, float alpha) {


    float DITHER_THRESHOLDS[16] =
    {
        1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
        13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
        4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
        16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
    };

    int index = (int(pos.x) % 4) * 4 + int(pos.y) % 4;
    return alpha - DITHER_THRESHOLDS[index];
}

float bayer8(float x, float y)
{

  int dither[8][8] = {
    { 0, 32, 8, 40, 2, 34, 10, 42}, /* 8x8 Bayer ordered dithering */
    {48, 16, 56, 24, 50, 18, 58, 26}, /* pattern. Each input pixel */
    {12, 44, 4, 36, 14, 46, 6, 38}, /* is scaled to the 0..63 range */
    {60, 28, 52, 20, 62, 30, 54, 22}, /* before looking in this table */
    { 3, 35, 11, 43, 1, 33, 9, 41}, /* to determine the action. */
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21} };


    return float(dither[int(x) % 8][int(y) % 8]+1)/64.0;


}

float phase_hg(float cosTheta, float g) {
  float g2 = g * g;
  return (1.0 - g2) / ( 4*PI*pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

const float samplesCount_fog = 2;

vec2 VogelDiskSampleFog(float sampleIndex,  float phi)
{
  float GoldenAngle = 2.4f;

  float r = sqrt(sampleIndex + 0.5f) / sqrt(samplesCount_fog);
  float theta = sampleIndex * GoldenAngle + phi;

  float sine = sin(theta);
  float cosine = cos(theta);

  return vec2(r * cosine, r * sine);
}

float ShadowCalculationFog(vec4 fragPosLightSpace,float NdotL,sampler2D smap,vec2 atlasCoord)
{
  // perform perspective divide
  vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
  // transform to [0,1] range
  projCoords = projCoords * 0.5 + 0.5;

  float currentDepth = projCoords.z;



  float shadow = 0.0f;

    vec2 sampleUV = projCoords.xy;

    float bias = 0;//deltas.x*offset.x + deltas.y*offset.y;
    float closestDepths = texture(smap, sampleUV*0.25*th_shadowmap_sun_scale + atlasCoord*0.25*th_shadowmap_sun_scale,0).r;
    shadow += when_gt(currentDepth +bias - constantbias , closestDepths) ;


  return shadow*(1-when_gt(projCoords.z,1.0));

}

vec4 computeFog()
{
  if (fog_density == 0.0)
  {
    return vec4(0,0,0,1.0);
  }
  float to_infinity = 0.0;//when_eq(depthval,1.0);


  vec3 Lo_fog = vec3(0.0);

  vec3 V = normalize(eyePos - outPosition);

  float blueNoise = SampleRandomNumber(uint(gl_FragCoord.x),uint(gl_FragCoord.y),0,0);

  float steps = 0.0;

  float dx = 100*fog_quality*(0.75 + mod(blueNoise,0.25));
  float optical_depth = 0;

  float cosTheta = dot(V, -sunlightDir);
  const float stepScale = 1.08;

  #define USE_FOG_PHASE


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
    //  Lo_fog += factor*fog_color*0.001;

    dx *= stepScale;
  }

  vec3 color =   (Lo_fog*fog_gain);

  float color_mult = mu;
  float optical_depth_add = alpha;


  return vec4(color*(1 - to_infinity) + fog_color*sunlightColor*to_infinity*color_mult*phase,exp(-optical_depth)*(1 - to_infinity) + to_infinity*exp(-(optical_depth + optical_depth_add)) );
}


void main()
{

   vec4 a ;//= texture(materials[int(ID.x)],vec3(outTexcoord,(ID.y )));
   float b ;//= texture(materials[int(ID.x)],vec3(outTexcoord,(ID.y + 2))).r*alphascale;
   vec2 c ;//= texture(materials[int(ID.x)],vec3(outTexcoord,(ID.y + 1  )));
   float d;
  //dirty hack
  if (blend == 0.0)
  {
//      a = texture(materials[1],vec3(outTexcoord,(ID.y )));
//      b = clamp(texture(materials[1],vec3(outTexcoord,(ID.y + 2))).r*alphascale,0,1);
//      c = texture(materials[1],vec3(outTexcoord,(ID.y + 1  )));

     a = texture(materials[0],vec3(outTexcoord,(ID.x )));
     b = clamp(texture(materials[2],vec3(outTexcoord,(ID.y + 1))).r,0,1);
     c = texture(materials[1],vec3(outTexcoord,(ID.x))).xy;
     d = texture(materials[2],vec3(outTexcoord,(ID.y))).r;

//      albedometal = texture(materials[0],vec3(outTexcoord,ID.x))*float(gl_FrontFacing);//*vec4(vec3(synflag),1);
//      vec2 b = texture(materials[1],vec3(outTexcoord,(ID.x))).xy;
//      float c = texture(materials[2],vec3(outTexcoord,(ID.y))).x;
  }
  else
  {
//      a = texture(materials[1],vec3(outTexcoord,(ID.y )));
//      b = clamp(texture(materials[1],vec3(outTexcoord,(ID.y + 2))).r*alphascale,0,1);
//      c = texture(materials[1],vec3(outTexcoord,(ID.y + 1  )));
//
//      vec4 a_2 = texture(materials[1],vec3(outTexcoord,(ID.y  + 3)));
//      float b_2 = clamp(texture(materials[1],vec3(outTexcoord,(ID.y + 2 + 3))).r*alphascale,0,1);
//      vec4 c_2 = texture(materials[1],vec3(outTexcoord,(ID.y + 1 + 3  )));

    a = texture(materials[0],vec3(outTexcoord,(ID.x )));
    b = clamp(texture(materials[2],vec3(outTexcoord,(ID.y + 1))).r,0,1);
    c = texture(materials[1],vec3(outTexcoord,(ID.x))).xy;
    d = texture(materials[2],vec3(outTexcoord,(ID.y))).r;

    vec4 a_2 = texture(materials[0],vec3(outTexcoord,(ID.x + 1)));
    float b_2 = clamp(texture(materials[2],vec3(outTexcoord,(ID.y + 3))).r,0,1);
    vec2 c_2 = texture(materials[1],vec3(outTexcoord,(ID.x + 1))).xy;
    float d_2 = texture(materials[2],vec3(outTexcoord,(ID.y + 2))).r;

     a = mix(a,a_2,blend);
     b = mix(b,b_2,blend);
     c = mix(c,c_2,blend);
     d = mix(d,d_2,blend);
  }

  b = b * (((1.0 - smoothstep(0.5, 1.0, length(outTexcoord * 2.0 - 1.0))))*use_add_blending  + (1.0 - use_add_blending));

//   //#define BLUE_NOISE
//   #ifdef BLUE_NOISE
//   float blueNoise = SampleRandomNumber(uint(gl_FragCoord.x),uint(gl_FragCoord.y),0,0);
//
//   if ((1 - when_gt(step(1.0 - b,blueNoise),0)) == 1)
//   {
//     discard;
//   }
//   #else
// //   if ((1 - when_gt(isDithered(gl_FragCoord.xy,b),0)) == 1)
// //   {
// //     discard;
// //   }
//
//
//
//     if (b < bayer8(gl_FragCoord.x,gl_FragCoord.y))
//     {
//       discard;
//     }
//
//   //inside_this = inside_this*when_gt(isDithered(gl_FragCoord.xy,height),0);
//   #endif



  float metallic  = a.a;//texture(metallicMap, outTexcoord).r;
  float roughness = d;//texture(roughnessMap, outTexcoord).r;
  float ao        =1;
  vec3 albedo     = pow(a.rgb,vec3(2.2));//pow(texture(albedoMap, outTexcoord).rgb, vec3(2.2));


  // Particle depth
  float particleDepth = screen2EyeDepth(gl_FragCoord.z,zNear,zFar);

  // Scene depth
  float sceneDepth = texture(depthTex, gl_FragCoord.xy*invWindow.xy).r;
  sceneDepth = screen2EyeDepth(sceneDepth,zNear,zFar);

  // Compute fade
  float diff = sceneDepth - particleDepth;
  float fade = clamp(diff / 100, 0.0, 1.0);

  float b_premul = b*(1.0 - use_premultiplied) + use_premultiplied;

  if (colorscale <= 1.0)
  {
    vec3 normal = vec3(c.xy,0.0);
    normal.xy = normal.xy * 2.0 - 1.0;
    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
    normal = normalize(normal);

    vec3 T = outTangent;
    vec3 B = cross(outNormal,T);
    mat3 TBN = mat3(T, B, outNormal);

    normal = normalize(TBN * normal);



    vec3 N = normal;
    vec3 V = normalize(eyePos - outPosition);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    vec4 lightSpacePosition = lightSpaceMatrix*vec4(outPosition,1.0);
    float shadowfactor = (1 - ShadowCalculation(lightSpacePosition,1.0,shadowMap,vec2(0,0)));

    vec3 n_temp = N*light_use_norm + sunlightDir*(1.0 - light_use_norm);

    Lo += calculateDirectionalLight(sunlightDir,V,n_temp,sunlightColor,roughness,F0,metallic,albedo,vec4(0,0,0,0))*shadowfactor;



    // vec3 plpos = vec3(207.802399,-400.645264,-979.814941);
    // float shadow_pl = 1;//shadowCalculationPoint(plpos,outPosition,shadowMap,vec2(0,0),0);
    // Lo += calculatePointLight(plpos,outPosition,V,N,vec3(10000,10000,50000),roughness,F0,metallic,albedo,vec4(1,1,1,1))*shadow_pl;
    //
    // plpos = vec3(287.077026, -1413.032471 ,-530.029907);
    // shadow_pl = 1;//shadowCalculationPoint(plpos,outPosition,shadowMap,vec2(1,0),1);
    // Lo += calculatePointLight(plpos,outPosition,V,N,vec3(10000,10000,50000),roughness,F0,metallic,albedo,vec4(1,1,1,1))*shadow_pl;


    //clustered lights
    ivec2 tile_id = ivec2(gl_FragCoord.xy / vec2(screenSize.x/16.0,screenSize.y/8.0));


    uint cluster_slice_id = getClusterZIndex(gl_FragCoord.z);

    uint tile_index = tile_id.x + tile_id.y*num_tiles_x + cluster_slice_id*num_tiles_x*num_tiles_y;

    uvec2 arr[2];
    arr[0] = offsets[tile_index/2].xy;
    arr[1] = offsets[tile_index/2].zw;
    uint count = arr[tile_index%2].y;
    uint offset = arr[tile_index%2].x;
    #ifdef PARTICLE_LIGHTING


    for (int i = 0 ; i < count;i++)
    {
      uint a = offset + i;
      uint key_bits = acessbuffer[a/8][(a%8)/2];
      int bit_offset = int((a % 2) *  16);
      uint key = bitfieldExtract(key_bits,bit_offset,16);

      PointLight light = pointlights[key];

      float shadow = 1;
      if  (light.shadowindex.x != 0.0)
      {
        shadow = shadowCalculationPoint(light.pos.xyz,outPosition,shadowMap,light.shadowindex.yz,int(light.shadowindex.w));
      }

      vec3 n_temp = N*light_use_norm + normalize(light.pos.xyz - outPosition)*(1.0 - light_use_norm);

      if (light.pos2.w < 1.5)
      {
        Lo += calculatePointLight(light.pos.xyz,outPosition,V,n_temp,light.color.xyz,roughness,F0,metallic,albedo,vec4(0))*shadow;
      }

    }
    #endif

    //sampel from spherical harmonic buffer precomputed
    vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 pos= outPosition;
    //rad sample begin
    vec3 q = quantify(pos);

    vec3 m = lerppoint(pos);

    // vec4 sh =texture(occTex,outTexcoord);

    float x = 1 - min(when_lt(q.x,0)+when_gt(q.x,gridsize.x)+when_lt(q.y,0)+when_gt(q.y,gridsize.y) + when_lt(q.z,0)+when_gt(q.z,gridsize.z),1.0);//1 - or(when_lt(x,0),when_gt(x,gridsize.x*gridsize.y*gridsize.z));
    // float excluded = 0.f;
    // vec4 weights;
    // float occlusion_1 = getOcclusion(getPos(q + vec3(0,0,0)),pos,N);
    // excluded += (1-occlusion_1)*(1- m.x)*(1 - m.y)*(1 - m.z);
    // weights.x = (1- m.x)*(1 - m.y)*(1 - m.z);
    // float occlusion_2 = getOcclusion(getPos(q + vec3(1,0,0)),pos,N);
    // excluded += (1-occlusion_2)*(m.x)*(1 - m.y)*(1 - m.z);
    // weights.y = (m.x)*(1 - m.y)*(1 - m.z);
    // float occlusion_3 = getOcclusion(getPos(q + vec3(0,1,0)),pos,N);
    // excluded += (1-occlusion_3)*(1- m.x)*(m.y)*(1 - m.z);
    // weights.z = (1- m.x)*( m.y)*(1 - m.z);
    // float occlusion_4 = getOcclusion(getPos(q + vec3(1,1,0)),pos,N);
    // excluded += (1-occlusion_4)*(m.x)*(m.y)*(1 - m.z);
    // weights.w = (m.x)*(m.y)*(1 - m.z);
    // // float scale = 1 /(1- excluded);
    //
    // vec4 weights2;
    // float occlusion_5 = getOcclusion(getPos(q + vec3(0,0,1)),pos,N);
    // excluded += (1-occlusion_5)*(1- m.x)*(1 - m.y)*( m.z);
    // weights2.x = (1- m.x)*(1 - m.y)*( m.z);
    //
    // float occlusion_6 = getOcclusion(getPos(q + vec3(1,0,1)),pos,N);
    // excluded += (1-occlusion_6)*(m.x)*(1 - m.y)*( m.z);
    // weights2.y = ( m.x)*(1 - m.y)*( m.z);
    //
    // float occlusion_7 = getOcclusion(getPos(q + vec3(0,1,1)),pos,N);
    // excluded += (1-occlusion_7)*(1- m.x)*(m.y)*(m.z);
    // weights2.z = (1- m.x)*( m.y)*( m.z);
    //
    // float occlusion_8 = getOcclusion(getPos(q + vec3(1,1,1)),pos,N);
    // excluded += (1-occlusion_8)*(m.x)*(m.y)*( m.z);
    // weights2.w = (m.x)*( m.y)*( m.z);
    //
    // float scale = 1 /(1- excluded);

    Light sum;
    sum = aquireLight(indexify(q + vec3(0,0,0)),1);
    // sum = aquireLight(indexify(q + vec3(0,0,0)),scale*occlusion_1*weights.x);
    // sum = addLight(sum,aquireLight(indexify(q + vec3(1,0,0)),scale*occlusion_2*weights.y));
    // sum = addLight(sum,aquireLight(indexify(q + vec3(0,1,0)),scale*occlusion_3*weights.z));
    // sum = addLight(sum,aquireLight(indexify(q + vec3(1,1,0)),scale*occlusion_4*weights.w));
    //
    // sum = addLight(sum,aquireLight(indexify(q + vec3(0,0,1)),scale*occlusion_5*weights2.x));
    // sum = addLight(sum,aquireLight(indexify(q + vec3(1,0,1)),scale*occlusion_6*weights2.y));
    // sum = addLight(sum,aquireLight(indexify(q + vec3(0,1,1)),scale*occlusion_7*weights2.z));
    // sum = addLight(sum,aquireLight(indexify(q + vec3(1,1,1)),scale*occlusion_8*weights2.w));


    vec3 o1 = sphericalHarmonics( N,sum);



    //rad sampel EDND
    vec3 irrad_sample = vec3(o1*x);//texture(irradianceMap, gl_FragCoord.xy*invWindow).xyz;
    vec3 irradiance = irrad_sample.rgb;
    vec3 diffuse = irradiance * albedo;

    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = vec3(0,0,0);
    if (use_reflections == 1.0)
    {
      float alpha = 1;
      float index = -1;
      float bounded = 0;
      float cur_dist = -1;
      float closest_probe = 0;



      arr[0] = offsets[tile_index/2 + (16*8*10)/2].xy;
      arr[1] = offsets[tile_index/2 + (16*8*10)/2].zw;
      count = arr[tile_index%2].y;
      offset = arr[tile_index%2].x;



      for (int i = 0 ; i < count;i++)
      {
        uint a = offset + i;
        uint key =  acessbuffercubes[a/4][a%4];


        float cond = 1.0;


        float bounded_i = cond*CheckCollision(pos,EnvBoxMin[key].xyz + vec3(100),EnvBoxMax[key].xyz -vec3(100));
        float dist = distance_squared(pos,EnvBoxPos[key].xyz);

        float replace = or(when_eq(index,-1),(1-when_eq(index,-1))*when_lt(dist,cur_dist) )*bounded_i;
        index = replace*key + (1-replace)*index;
        cur_dist = cur_dist*(1-replace) + dist*replace;

      }
      if (index == -1)
      {
        index = 0;
      }


      specular = textureLod(reflections,vec4(reflect(-V,N)*vec3(-1,1,1),index),roughness*4.0).xyz * (F * brdf.x + brdf.y);
    }



    vec3 ambient = vec3(kD*diffuse*(1.0/PI) + specular );//vec3(0.03) * albedo ;
    vec3 color = (  ambient + Lo);

    vec4 fog_part = computeFog();

    float alpha = b*fade*clamp(alphascale,0.0,1.0);

    color =  color*(fog_part.w)*vec3(colorscale)*b_premul*fade*clamp(alphascale,0.0,1.0) + fog_part.xyz*alpha;

    forwardBuffer0 = vec4(color,alpha);
    //forwardBuffer1 = vec4(0,0,0,use_add_blending);
  }
  else
  {
//     forwardBuffer0 = vec4(albedo*vec3(colorscale)*b*0.9,b*0.9);

    float alpha = b * fade*clamp(alphascale,0.0,1.0);

    vec4 fog_part = computeFog();
    //
    albedo =  albedo*(fog_part.w)*vec3(clamp(colorscale,0.0,2.0))*b_premul*fade*clamp(alphascale,0.0,1.0) + fog_part.xyz*alpha;

    forwardBuffer0 = vec4(albedo,alpha);
    //forwardBuffer1 = vec4(0,0,0,use_add_blending);
  }



}
