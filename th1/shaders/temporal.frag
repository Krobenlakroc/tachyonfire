#version 460 core
#define CURVE_MODEL
// #define OCCLUSION
#define REALTIME_BDRF
#define TEMPORAL_REPROJ
#define STOCHASTIC_TRUE
//#define FASTCUBES
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;
layout(binding=0) uniform sampler2D texInfo;
layout(binding=4) uniform sampler2D brdfLUT;
// layout(binding=3) uniform sampler2D occTex;
layout(binding=3) uniform sampler2D norInfo;
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
layout(binding=29) uniform sampler2D reflectiondata;
uniform mat4 old_mvp;
uniform mat4 projMatrixInv_old;
uniform mat4 viewMatrixInv_old;
uniform vec3 eyePos;
uniform float frame;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform vec2 screenSize;
uniform mat4 viewMat;
uniform mat4 projMat;

layout (std430 , binding = 0) restrict readonly buffer BlueNoise //size of grid
{
   uint g_sobol_buffer[256*256];
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

vec3 WorldPosFromDepth_old(float depth,vec2 old_texcoord) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(old_texcoord * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv_old * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;

  vec4 worldSpacePosition = viewMatrixInv_old * viewSpacePosition;

  return worldSpacePosition.xyz;
}

vec3 WorldPosFromDepth_new(float depth,vec2 old_texcoord) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(old_texcoord * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;


  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
}


float not(float a) {
  return 1.0 - a;
}

float or(float a, float b) {
  return min(a + b, 1.0);
}

float when_neq(float x, float y) {
  return  abs(sign(x - y));
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


float when_ge(float x, float y) {
  return 1.0 - max(sign(y - x), 0.0);
}


const float PI = 3.14159265359;



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
 vec2 downSample(vec2 uv)
 {
   return uv;//floor(uv*screenSize*0.5)/(screenSize*0.5);
 }

/*
 Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

 vec4 reprojectHitPosition(vec2 uv, float reflected_ray_length,float z,out vec2 uvs) {

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

    vec4 tex = texture(old_reflectiondata,downSample(history_uv.xy));
    uvs = history_uv.xy;
     float cancel = (1-when_gt(prev_hit_position.z,1.0));
    tex.w *= cancel;
    tex.xyz *= cancel;
    return tex;
}

float reprojectDepth(vec3 world,out vec2 uvs,out float old_deptho)
{

  vec4 clip = old_mvp*vec4(world,1.0);
  clip.xyz = clip.xyz / clip.w;
  clip.xyz = clip.xyz * 0.5 + 0.5;
  float current_depth = clip.z;
  float old_depth = texture(old_depthtexture,clip.xy).r;
  old_deptho = old_depth;
  uvs = clip.xy;
return   1 - when_gt(current_depth - 0.000005, old_depth);//*(1-when_gt(clip.z,1.0));

}
vec3 viewDir;

//Eric Heitz Blue Noise Sampler
float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, uint sample_dimension) {
    // Wrap arguments
    pixel_i = pixel_i & 127u;
    pixel_j = pixel_j & 127u;
    sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    const uint ranked_sample_index = sample_index ^ 0;


    // Fetch value in sequence
    uint value = g_sobol_buffer[sample_dimension + ranked_sample_index * 256u];

    // // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ g_scrambling_tile_buffer[(sample_dimension % 8u) + (pixel_i + pixel_j * 128u) * 8u];//

    // Convert to float and return
    return (float(value) + 0.5) / 256.0f;
}

vec2 SampleRandomVector2D(uvec2 pixel,uint offset) {
    vec2 u = vec2(
        mod(SampleRandomNumber(pixel.x, pixel.y, 0, 0u) + float((uint(frame) + offset) & 0xFFu) * 1.61803398875f, 1.0f),
        mod(SampleRandomNumber(pixel.x, pixel.y, 0, 1u) + float((uint(frame) + offset) & 0xFFu) * 1.61803398875f, 1.0f));
    return u;
}




float normalweight(vec3 normal_p, vec3 normal_q, float sigma) {
    return pow(max(dot(normal_p, normal_q), 0.0), sigma);
}

float roughnessweight(float roughness_p, float roughness_q, float sigma_min, float sigma_max) {
    return 1.0 - smoothstep(sigma_min, sigma_max, abs(roughness_p - roughness_q));
}

float ManhattanDistance( vec3 a, vec3 b )
{ return dot( abs( a - b ), vec3(1.0) ); }

float EstimateCurvature( vec3 n, vec3 v, vec3 N, vec3 X )
{
    // https://computergraphics.stackexchange.com/questions/1718/what-is-the-simplest-way-to-compute-principal-curvature-for-a-mesh-triangle

    // float NoV = dot( N, v );
    // vec3 x = 0 + v * dot( X - 0, N ) / NoV;
    // vec3 edge = x - X;
    // float edgeLenSq = dot( edge ,edge);
    // float curvature = dot( n - N, edge ) * (1.0/max( edgeLenSq,0.0001 ));
    float edgeLenSq = dot(v- X,v - X);
    float curvature = dot(n - N,v - X)* (1.0/max( edgeLenSq,1e-8 )) ;
    return curvature;
}

float ApplyThinLensEquation( float NoV, float hitDist, float curvature )
{
    /*
    Thin lens equation:
        1 / F = 1 / O + 1 / I
        F = R / 2 - focal distance
        C = 1 / R - curvature
    Sign convention:
        convex  : O(-), I(+), C(+)
        concave : O(+), I(-), C(-)
    Why NoV?
        hitDist is not O, we need to find projection to the axis:
            O = hitDist * NoV
        hitDistFocused is not I, we need to reproject it back to the view direction:
            hitDistFocused = I / NoV
    Combine:
        2C = 1 / O + 1 / I
        1 / I = 2C - 1 / O
        1 / I = ( 2CO - 1 ) / O
        I = O / ( 2CO - 1 )
        I = [ ( O * NoV ) / ( 2CO * NoV - 1 ) ] / NoV
        I = O / ( 2CO * NoV - 1 )
    */
    float closetozero = when_lt(curvature,1e-7)*when_gt(curvature,-1e-7);

    float hitDistFocused = hitDist;
    hitDistFocused *= -( sign(curvature))*(1- closetozero) + -closetozero;
    curvature *= 1 - closetozero;
    hitDistFocused /= 2.0 * curvature * NoV * hitDistFocused - 1.0;
    //
    // // A mystical fix for silhouettes of convex surfaces observed under a grazing angle
     hitDistFocused *= mix( NoV, 1.0, 1.0 / ( 1.0 + max( curvature * hitDist, 0.0 ) ) );

    // float hitDistFocused = 1.0 / (2.0*curvature*NoV - (1.0/hitDist));

    return hitDistFocused;
}



float LinearStep(float a, float b, float x)
{
  return clamp( ( x - a ) / ( b - a ) ,0.0,1.0);
}
 #define NRD_NORMAL_ENCODING_ERROR                           ( 0.5 / 256.0 )

void main(void)
{
  float depthval = texture2D(depthTex,outTexcoord ).r;
  vec4 viewSpacePosition;
  vec3 position = WorldPosFromDepth(depthval,viewSpacePosition);
   viewDir    = normalize(position - eyePos);

  vec3 normalt = texture(normalTex,outTexcoord).xyz;
  vec3 normal = (vec4(decodeNormal(normalt.xy),0)).xyz;
  float roughness = normalt.z;

  vec3 N_prime = decodeNormal(texture(norInfo,outTexcoord).zw);



  // vec3 dposx = dFdx(position);
  // vec3 dposy = dFdy(position);
  // vec3 surface_normal = normalize(cross(dposx,dposy));


  // vec3 enorm = normalize((normal_a + normal_b + normal_c + normal)*vec3(0.25));
  // vec3 epos = ((pos_a + pos_b + pos_c + position)*vec3(0.25));


  vec4 new_values = texture(reflectiondata,outTexcoord);
  vec2 upack = unpackHalf2x16(floatBitsToUint(new_values.w));
  //uintBitsToFloat(packHalf2x16(dist_to_hit,total_weight))
  vec3 accum = new_values.xyz;
  float sky = (1 - clamp(sign(upack.x),0,1))*when_neq(upack.x,0);
  float dist_to_hit = abs(upack.x)*when_gt(upack.x,0) ;
  float disable_temporal = max(-sign(upack.y),0.0);
  float total_weight = abs(upack.y);

  float factor = when_gt(roughness,0.7);

  vec2 old_uvs;
  float old_depth;
  float disoccluded = reprojectDepth(position,old_uvs,old_depth);

  #ifdef CURVE_MODEL
  vec2 delta = (1.0/screenSize);


  vec3 old_normal = decodeNormal(texture(norInfo,outTexcoord+vec2(delta.x,0)).zw);
  vec3 old_position = WorldPosFromDepth_new(texture2D(depthTex,outTexcoord+vec2(delta.x,0)).r,outTexcoord+vec2(delta.x,0));//texture2D(depthTex,outTexcoord+uvmotion ).r

  vec3 old_normalb = decodeNormal(texture(norInfo,outTexcoord+vec2(0,delta.y)).zw);
  vec3 old_positionb = WorldPosFromDepth_new(texture2D(depthTex,outTexcoord+vec2(0,delta.y)).r,outTexcoord+vec2(0,delta.y));//

  vec3 old_normalc = decodeNormal(texture(norInfo,outTexcoord-vec2(delta.x,0)).zw);
  vec3 old_positionc = WorldPosFromDepth_new(texture2D(depthTex,outTexcoord-vec2(delta.x,0)).r,outTexcoord-vec2(delta.x,0));//

  vec3 old_normald = decodeNormal(texture(norInfo,outTexcoord-vec2(0,delta.y)).zw);
  vec3 old_positiond = WorldPosFromDepth_new(texture2D(depthTex,outTexcoord-vec2(0,delta.y)).r,outTexcoord-vec2(0,delta.y));//

  //
  // float curve_a = EstimateCurvature(normal_a,pos_a,normal,position);
  // float curve_b = EstimateCurvature(normal_b,pos_b,normal,position);
  float curvea = EstimateCurvature(old_normal,(old_position),N_prime,position);
  float curveb = EstimateCurvature(old_normalb,(old_positionb),N_prime,position);
  float curvec = EstimateCurvature(old_normalc,(old_positionc),N_prime,position);
  float curved = EstimateCurvature(old_normald,(old_positiond),N_prime,position);
  // float curvee = EstimateCurvature(old_normale,(old_positione),N_prime,position);
  // float curvef = EstimateCurvature(old_normalf,(old_positionf),N_prime,position);
  // float curveg = EstimateCurvature(old_normalg,(old_positiong),N_prime,position);
  // float curveh = EstimateCurvature(old_normalh,(old_positionh),N_prime,position);
  float curve = curvea + curveb + curvec + curved ;//+ curvee + curvef + curveg + curveh;
  curve *= 0.25;


  // curve *= 10000;
  //
  // float curve = (curve_a + curve_b + curve_c)*0.3333;
  // // float curve = EstimateCurvature(enorm,epos,normal,position);

   dist_to_hit = ApplyThinLensEquation(abs(dot(N_prime,viewDir)),dist_to_hit,curve);
  // // if (abs(dist_to_hit) < 0.0001)
  // // {
  // dist_to_hit = abs(dist_to_hit);
  // }
  #endif

    vec2 uvs;
    vec4 reproj;
    //roughness chooses reprojection method
    if (factor == 1)
    {
      reproj = texture(old_reflectiondata,downSample(old_uvs));
    }
    else
    {
       reproj = reprojectHitPosition(outTexcoord,dist_to_hit,depthval,uvs);
    }

    float spec_sky_old = (1 - clamp(sign(reproj.w),0,1))*abs(sign(reproj.w));

    uvs= old_uvs*factor + uvs*(1 - factor);

    vec3 history = texture(oldnormalTex,uvs).xyz;
    float history_roughness = history.z;
    vec3 history_normal = (vec4(decodeNormal(history.xy),0)).xyz;



    reproj.w = abs(reproj.w);


    float resample = 0;

    //weight old sample
    float deviation_weight =0.99*normalweight(normal,history_normal,8.0)*roughnessweight(roughness,history_roughness,0.001,0.01);//clamp((clamp((dist_to_hit)/(500),(0),(1))*(roughness*roughness))*(10),(0),(1));
    deviation_weight = clamp(deviation_weight,0.75,1.0);
    reproj.xyz *= deviation_weight;
    reproj.w *= deviation_weight;


    //new sample based on this frame and the pixel history
    vec3 accum2 = accum + reproj.xyz;
    float total_weight2 = total_weight+ reproj.w;
    float weight = 0;


    //decide whether to use history or not
    weight = 1*when_lt(uvs.x,1.0)*when_lt(uvs.y,1.0)*when_gt(uvs.x,0.0)*when_gt(uvs.y,0.0)*disoccluded*when_gt(roughness,0.1)*(1-resample);
    weight *= when_gt(dot(normal,history_normal),0.8)*when_lt(abs(roughness - history_roughness),0.2);
    weight *=  1 - disable_temporal;
  //  weight = 0;

    accum = mix(accum,accum2,weight);
    total_weight = mix(total_weight,total_weight2,weight);

    float anysky = mix(sky,clamp(sky+spec_sky_old,0,1),weight  );
    fragColor = vec4(accum*when_gt(total_weight,0),-total_weight*anysky + total_weight*(1-anysky));
//    fragColor = vec4(when_lt(curve,1e-7)*when_gt(curve,-1e-7),0,0,0);

}
