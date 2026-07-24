#version 450 core
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;

uniform float occlusion_radius;
uniform float occlusion_max_distance;

uniform vec3 eyePos;
uniform mat4 projection;
uniform mat4 viewMatrixInv;
uniform mat4 projMatrixInv;
uniform vec2 resolution;
uniform mat4 view;

layout(binding=2) uniform sampler2D depthTex;
layout(binding=3) uniform sampler2D normalTex;
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

vec4 dssdo_accumulate()
{
	vec3 points[] =
	{
    vec3(-0.0315447,-0.0232464,0.0238357),
vec3(0.00790363,-0.0677885,0.0056737),
vec3(0.0189841,0.0460257,0.0203028),
vec3(0.033089,-0.0465322,0.00267247),
vec3(0.00234968,-0.00675872,0.00263192),
vec3(-0.0261317,0.0591498,0.0932627),
vec3(0.0850802,0.0066185,0.0113015),
vec3(-0.0181497,0.0434705,0.0983448),
vec3(-0.0193436,-0.0368503,0.0299699),
vec3(0.0144654,0.0279638,0.0540406),
vec3(-0.0547696,0.104479,0.0782331),
vec3(0.0500616,-0.141184,0.104346),
vec3(-0.0546577,-0.0152915,0.0921616),
vec3(-0.0278617,-0.0239163,0.0190861),
vec3(-0.000368003,0.0108539,0.0124082),
vec3(0.0800813,0.00089509,0.0510874),
vec3(0.0284419,-0.000352034,0.00777743),
vec3(0.0912931,-0.0869068,0.0510437),
vec3(-0.0738345,0.137064,0.0877309),
vec3(0.192536,-0.196438,0.166242),
vec3(0.204695,-0.233928,0.00495403),
vec3(0.242756,0.0853975,0.242667),
vec3(0.124922,0.0971874,0.0291627),
vec3(-0.214956,0.0191514,0.426432),
vec3(-0.0586544,0.227835,0.0898362),
vec3(0.0284548,-0.024472,0.10954),
vec3(-0.0148562,0.00307415,0.0172125),
vec3(0.0398702,-0.00175903,0.552839),
vec3(0.0720644,0.515872,0.412479),
vec3(-0.0652227,-0.0550656,0.0685051),
vec3(-0.323294,-0.178172,0.00118549),
vec3(-0.153523,0.0680838,0.152168),


	};

	const int num_samples = 12;

	vec2 noise_texture_size = vec2(4,4);
  vec4 vfd = ViewPosFromDepth(texture(depthTex,lookup).r);
	vec3  center_pos  = vfd.xyz;//tex2D(smp_position, tex).xyz;
  vec3 center_world = worldfromview(vfd);
	vec3 eye_pos = eyePos;

	float  center_depth  = distance(eye_pos, center_pos);

	float radius = occlusion_radius / center_depth;
	float max_distance_inv = 1.f / occlusion_max_distance;
	float attenuation_angle_threshold = 0.1;

	vec3 noise = (textureLod(noiseTex, (lookup*resolution.xy)/noise_texture_size,0).xyz*2-1);
  noise.z = 0;
  noise = normalize(noise);
	//radius = min(radius, 0.1);
	vec3 center_normal = normalize(decodeNormal(texture(normalTex, lookup).xy));
  vec3 world_N = center_normal;
  vec3 wn = center_normal;
  center_normal = (view*vec4(center_normal,0)).xyz;
  vec3 tangent = normalize(noise - center_normal * dot(noise, center_normal));
    vec3 bitangent = cross(center_normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, center_normal);

	vec4 occlusion_sh2 = vec4(0);

	const float fudge_factor_l0 = 2.0;//2.0;
	const float fudge_factor_l1 = 10.0;//10.0;

	// const float sh2_weight_l0 = fudge_factor_l0 * 0.28209; //0.5*sqrt(1.0/pi);
	// const vec3 sh2_weight_l1 = vec3(fudge_factor_l1 * 0.48860); //0.5*sqrt(3.0/pi);
  // const vec3 sh_weight_l2_0 = vec3(1.092548);
  // const vec2 sh_weight_l2_1 = vec2(0.315392,0.546274);

	const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860)*fudge_factor_l1, 0.28209*fudge_factor_l0);
  c0123 = vec4(0);
  c4567 = vec4(0);
  c8 = 0;
  //unroll
  float num_samples_got = 0.0;
	for( int i=0; i<num_samples; ++i )
	{
    vec3 point = TBN * points[i]; // from tangent to view-space
    point = center_pos + point * occlusion_radius;

    vec4 offset = vec4(point,1);
    offset = projection * offset; // from view to clip-space
    offset.xyz /= offset.w; // perspective divide
    offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
    vec4 sfv = ViewPosFromDepthUV(texture(depthTex,offset.xy).r,offset.xy);

    float EdgeError = step(0.0, offset.x) * step(0.0, 1.0 - offset.x) *
                      step(0.0, offset.y) * step(0.0, 1.0 - offset.y);

    vec3 sample_pos = sfv.xyz;
     vec3 sample_world = worldfromview(sfv);
     float rangeCheck = when_lt(distance(center_world, sample_world),occlusion_max_distance);//when_lt(abs(center_pos.z - sample_pos.z),occlusion_max_distance);//smoothstep(0.0, 1.0, occlusion_max_distance / abs(center_pos.z - sample_pos.z));
	  // //   vec2 textureOffset = reflect( points[ i ].xy, noise.xy ).xy * radius;
		// // vec2 sample_tex = lookup + textureOffset;
		// // vec3 sample_pos = WorldPosFromDepth(texture(depthTex,sample_tex).r);
		// // vec3 center_to_sample = sample_pos - center_pos;
    // vec3 center_to_sample_normalized = normalize(sample_pos - center_pos);
  //  vec3 lv = normalizeNoNAN(sample_pos - center_pos);
    vec3 worldnormal = normalizeNoNAN(sample_world - center_world);//normalizeNoNAN((viewMatrixInv*vec4(lv,0)).xyz);
		// float dist = distance(sample_pos,center_pos);
		// // // // vec3 center_to_sample_normalized = center_to_sample / dist;
		//  float attenuation = 1-clamp(dist * max_distance_inv,0,1);
		//  float dp = dot(center_normal, center_to_sample_normalized);
    // //
		//  attenuation = attenuation*attenuation * step(attenuation_angle_threshold, dp);
    //
  //  occlusion_sh2 += vec4(attenuation);//attenuation*sh2_weight*vec4(center_to_sample_normalized,1);
  //
    vec3 occ_n = normalize(decodeNormal(texture(normalTex, offset.xy).xy));
    float inc = (when_ge(sample_pos.z, point.z + 0.9)*EdgeError)*when_gt(length(occ_n - world_N),0.1);
		 occlusion_sh2 += inc*vec4(occ_n,1);//*sh2_weight*vec4(worldnormal,1);
     num_samples_got += inc;
     //occlusion_sh2 += (1)*sh2_weight*vec4(-worldnormal,1);
     // float val = dot(lv,center_normal)*(when_ge(sample_pos.z, point.z + 0.025)* rangeCheck);
     // c0123 += val*vec4(worldnormal,1)*vec4(sh2_weight_l1,sh2_weight_l0);
     // c4567 += val*vec4(worldnormal.x*worldnormal.z,worldnormal.y*worldnormal.z,worldnormal.x*worldnormal.y,3*worldnormal.z*worldnormal.z - 1)*vec4(sh_weight_l2_0,sh_weight_l2_1.x);
     // c8 += val*(worldnormal.x*worldnormal.x - worldnormal.y*worldnormal.y)*sh_weight_l2_1.y;

  }
  // c0123 *= (4*3.14159)/(2*num_samples);
  // c4567 *= (4*3.14159)/(2*num_samples);
  // c8 *= (4*3.14159)/(2*num_samples);
   occlusion_sh2.xyz *= (1.0)/max(num_samples_got,0.1);
//occlusion_sh2 = vec4(1 - (occlusion_sh2 / num_samples));
  //
//  vec4 bn = vec4(texture(normalTex, lookup).xyz*2 -1,1);
	return occlusion_sh2 ;//vec4(1 - clamp(dot(bn,occlusion_sh2),0,1));
}

void main(void)
{
  // c0123 = vec4(0);
  // c4567 = vec4(0);
  // c8 = 0;
//  vec2 dims = vec2(1366.0/2,768.0/2);
//  vec3 normal = texture(normalTex, lookup).xyz*2 -1;
// if (mod(lookup.x*dims.x +1 + mod(lookup.y*dims.y + 1,2),2) == 0.0)
// {

  fragColor = dssdo_accumulate();
// }

  // fragColor = vec4(pack_2half(c0123.x,c0123.y),pack_2half(c0123.z,c0123.w),pack_2half(c4567.x,c4567.y),pack_2half(c4567.z,c4567.w));

}
