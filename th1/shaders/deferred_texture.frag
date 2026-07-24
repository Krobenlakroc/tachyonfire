
#define BLUE_NOISE
#define texture2D texture
in vec2 outTexcoord;
layout (location = 0) out vec4 albedometal;
layout (location = 1) out vec4 normalroughness;
// layout(binding=0) uniform sampler2D texInfo;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=3) uniform sampler2D norInfo;
layout(binding=1) uniform sampler2D dynamicFlagTex;

layout(binding=5) uniform sampler2D albedometalTex;

layout(binding=6) uniform sampler2DArray materials[3];


uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform vec2 screenSize;

uniform float zNear = 0.1;
uniform float zFar = 1000;
uint num_tiles_x = 16;
uint num_tiles_y = 8;
uint num_slices = TH_Z_SLICES;

struct Decal
{
  mat4 decal_matrix;//0 - 64
  vec4 decal_properties;//64-80
};

layout (std140 , binding = 7) uniform Decals
{
  uniform Decal decals_list[800];
};

layout (std140 , binding = 8) uniform Access_Buffer_DECALS //
{
  uniform uvec4 acessbufferdecals[4096];//use the max amount of memory
};

layout (std140 , binding = 9) uniform Offsets_buffer_DECALS //size of grid
{
  uniform uvec4 offsetsdecals[(16*8*TH_Z_MEMORY)];//pack into vec4 for std140
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


vec3 WorldPosFromDepth(float depth) {
  float z = depth * 2.0 - 1.0;

  vec4 clipSpacePosition = vec4(outTexcoord * 2.0 - 1.0, z, 1.0);
  vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;

  // Perspective division
  viewSpacePosition /= viewSpacePosition.w;

  vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

  return worldSpacePosition.xyz;
}


int when_eq(float x, float y) {
  return  int(1- abs(sign(x - y)));
}
vec2 encode (vec3 n)
{
  return (vec2(atan(n.y,n.x)/3.1415926536, n.z))*vec2(1);
}

vec3 decode (vec2 enc)
{
  vec2 ang = enc;//*2-1;
    vec2 scth = vec2(sin(ang.x * 3.1415926536),cos(ang.x * 3.1415926536));
    vec2 scphi = vec2(sqrt(1.0 - ang.y*ang.y), ang.y);
    return vec3(scth.y*scphi.x, scth.x*scphi.x, scphi.y);
}


// vec2 encodeNormalFinal (vec3 n)
// {
//     float p = sqrt(n.z*8+8);
//     return vec2(n.xy/p + 0.5);
// }
//
// vec3 decodeNormal (vec2 enc)
// {
//     vec2 fenc = enc*4-2;
//     float f = dot(fenc,fenc);
//     float g = sqrt(1-f/4);
//     vec3 n;
//     n.xy = fenc*g;
//     n.z = 1-f/2;
//     return n;
// }

// vec2 encodeNormalFinal(vec3 n)
// {
//    float scale = 1.7777;
//    float d= clamp(floor(dot(n,vec3(0,0,-1))),0,1);
//    n.x = n.x + 0.0099995000374969*d;
//    n.z = n.z + (1-0.99995000374969)*d;
//    vec2  enc = vec2(n.x / (n.z + 1),n.y / (n.z + 1));
//    enc /= vec2(scale);
//    enc = enc*0.5 + 0.5;
//       return enc;
//
// }
//
// vec3  decodeNormal ( vec2 enc)
// {
//     float scale = 1.7777;
//     vec3 nn = vec3(enc.x*2*scale + -scale,enc.y*2*scale + -scale,1);
//     float g = 2.0 / dot(nn,nn);
//     vec3 n = vec3(nn.x*g,nn.y*g,g-1);
//     return n;
// }

vec2 when_ge2(vec2 x, vec2 y) {
  return 1.0 - max(sign(y - x), 0.0);
}



//TODO FIX THIS, GET RID OF SIGN, it will be zero on zero
//https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
//do it liek the website
vec2 OctWrap( vec2 v )
{
    return ( 1.0 - abs( v.yx ) ) * ( sign(when_ge2(v.xy,vec2(0)) - 0.5) );
}


float when_ge(float x, float y) {
  return 1.0 - max(sign(y - x), 0.0);
}


vec2 encodeNormalFinal( vec3 n )
{
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
    n.xy = when_ge(n.z,0)*n.xy + (1 - when_ge(n.z,0))*OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

vec3 decodeNormal( vec2 f )
{
    f = f * 2.0 - 1.0;
    vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = clamp( -n.z,0,1 );
    n.xy += -sign(when_ge2(n.xy,vec2(0)) - vec2(0.5))*t;
    return normalize( n );
}

mat3 makematrix(vec4 q){
  mat3 m;
    float sqw = q.w*q.w;
    float sqx = q.x*q.x;
    float sqy = q.y*q.y;
    float sqz = q.z*q.z;

    // invs (inverse square length) is only required if quaternion is not already normalised
    float invs = 1 / (sqx + sqy + sqz + sqw);
    m[0][0] = ( sqx - sqy - sqz + sqw)*invs ; // since sqw + sqx + sqy + sqz =1/invs*invs
    m[1][1] = (-sqx + sqy - sqz + sqw)*invs ;
    m[2][2] = (-sqx - sqy + sqz + sqw)*invs ;

    float tmp1 = q.x*q.y;
    float tmp2 = q.z*q.w;
    m[0][1] = 2.0 * (tmp1 + tmp2)*invs ;
    m[1][0] = 2.0 * (tmp1 - tmp2)*invs ;

    tmp1 = q.x*q.z;
    tmp2 = q.y*q.w;
    m[0][2] = 2.0 * (tmp1 - tmp2)*invs ;
    m[2][0] = 2.0 * (tmp1 + tmp2)*invs ;
    tmp1 = q.y*q.z;
    tmp2 = q.x*q.w;
    m[1][2] = 2.0 * (tmp1 + tmp2)*invs ;
    m[2][1] = 2.0 * (tmp1 - tmp2)*invs ;
    return m;
}

float mip_map_level(in vec2 texture_coordinate)
{
    // The OpenGL Graphics System: A Specification 4.2
    //  - chapter 3.9.11, equation 3.21


    vec2  dx_vtc        = dFdx(texture_coordinate);
    vec2  dy_vtc        = dFdy(texture_coordinate);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));


    //return max(0.0, 0.5 * log2(delta_max_sqr) - 1.0); // == log2(sqrt(delta_max_sqr));
    return 0.5 * log2(delta_max_sqr); // == log2(sqrt(delta_max_sqr));
}

float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

float when_bounded(vec3 v)
{
  return when_lt(v.x,1)*when_lt(v.y,1)*when_gt(v.x,0)*when_gt(v.y,0)*when_lt(v.z,1)*when_gt(v.z,0);
}

float CheckCollision(vec3 pos, vec3 bmin,vec3 bmax)
{
  return when_lt(pos.x,bmax.x)*when_lt(pos.y,bmax.y)*when_lt(pos.z,bmax.z)*when_gt(pos.x,bmin.x)*when_gt(pos.y,bmin.y)*when_gt(pos.z,bmin.z);
}

float screen2EyeDepth(float depth, float near, float far)
{

    float ndc = 2.0 * depth - 1.0;
    float eye = 2.0 * far * near / (far + near + ndc * (near - far));
    return eye;
}

// uint getClusterZIndex(float screenDepth)
// {
//
//     float eyeDepth = screen2EyeDepth(screenDepth, zNear, zFar);
//     uint zIndex = uint(float(num_slices)*(log(eyeDepth/zNear)/log(zFar/zNear)));
//     // uint zIndex = uint(max(log(eyeDepth) * scale + bias, 0.0));
//     return zIndex;
// }

// uint getClusterZIndex(float screenDepth)
// {
//
//     float eyeDepth = screen2EyeDepth(screenDepth, zNear, zFar);
//     uint zIndex = max(uint(float(num_slices)*(log(eyeDepth/zNear)/log(zFar/zNear))) - 5,0);
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

// vec2 decodeBits(float i)
// {
//   vec2 ret;
//   int final = floatBitsToInt(i);
//   ret.x = float(final & 0x7FFF);
//   ret.y = 1;//float(final >> 16);
//   return ret;//ret;
// }

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

const vec2 divamt = vec2(1.0,1.0);



int decodeFlags(float f) {
  // add 0.5 to avoid rounding errors from float
  return int(f*255.0 + 0.5);
}




float hasFlag(float f, int bit) {
  int flags = decodeFlags(f);
  return float((flags & (1 << bit)) != 0);
}

void main(void)
{
  // vec3 ti = texture(texInfo,outTexcoord).xyz;
  // float synflag = clamp( 1 - sign(ti.z),0,1);

  vec4 base_albedometal = texture(albedometalTex,outTexcoord);
  vec4 base_normalroughness = texture(dynamicFlagTex,outTexcoord);

  float dtexflag = base_normalroughness.w;
  float synflag = 1 - hasFlag(dtexflag,0);
  // ti.z = ti.z - 1*sign(ti.z);
  // ti.z = abs(ti.z);
  //
  //
  vec4 tangent_normal = texture(norInfo,outTexcoord);//texture(norInfo,outTexcoord);
  // vec2 sb = tangent_normal.zw;
   vec3 N = normalize(decodeNormal(tangent_normal.xy));

  float depthval = texture2D(depthTex,outTexcoord ).r;
  vec3 worldpos = WorldPosFromDepth(depthval);

  ivec2 tile_id = ivec2(gl_FragCoord.xy / vec2(screenSize.x/16.0,screenSize.y/8.0));
  uint cluster_slice_id = getClusterZIndex(depthval);

  uint tile_index = tile_id.x + tile_id.y*num_tiles_x + cluster_slice_id*num_tiles_x*num_tiles_y;
  uvec2 arr[2];
  arr[0] = offsetsdecals[tile_index/2].xy;
  arr[1] = offsetsdecals[tile_index/2].zw;
  uint count = arr[tile_index%2].y;
  uint offset = arr[tile_index%2].x;

  float inside = 0;
  float decal_id_final = 0;
  vec2 decal_coords_final = vec2(0,0);

  // bool fastpath_condit = depthval == 1;
  // bool fastpath = allInvocations(fastpath_condit);

    float final_blend = 0;
    for (int i = 0 ; i < count;i++)
    {
      uint a = offset + i;
      uint key =  acessbufferdecals[a/4][a%4];
      mat4 decal_mat = decals_list[key].decal_matrix;
      float decal_id = decals_list[key].decal_properties.x;
      vec3 decal_normal = decals_list[key].decal_properties.yzw;

      //-- calculate texture coordinates for projection texture.
      vec4 pixelClipPos = decal_mat*vec4(worldpos, 1.0f);
      pixelClipPos.xyz /= pixelClipPos.w;
      pixelClipPos.xyz = pixelClipPos.xyz * 0.5 + 0.5;

      float angle = dot(N, decal_normal);//*CheckCollision(worldpos,vec3(-100,-100,-100),vec3(0,0,0))
      float inside_this = when_lt(angle,-0.1)*when_bounded(pixelClipPos.xyz)*synflag;

      float height = 0;
      if (inside_this == 1)
      {
        //height = textureLod(materials,vec3(pixelClipPos.xy,decal_id + 2),7).r;
        height = textureLod(materials[2],vec3(pixelClipPos.xy,decal_id*2.0 + 1.0),7).x;


      }

//       #ifdef BLUE_NOISE
//       //float blueNoise = SampleRandomNumber(uint(gl_FragCoord.x),uint(gl_FragCoord.y),0,0);
//       inside_this = inside_this*when_gt(step(1.0 - height,blueNoise),0);
//       #else
//       inside_this = inside_this*when_gt(isDithered(gl_FragCoord.xy,height),0);
//       #endif
      inside_this = inside_this*when_gt(height,final_blend);

      final_blend = final_blend*(1 - inside_this) + height*inside_this;

      inside += inside_this;
      decal_id_final = decal_id_final*(1- inside_this) + decal_id*inside_this;
      decal_coords_final = decal_coords_final*(1- inside_this) + pixelClipPos.xy*inside_this;
    }
    inside= clamp(inside,0,1);
    if (inside != 1.0)
    {
      discard;
    }







      // //
      // if (inside == 1 && angle < -0.1 )
      // {
      vec3 decal_info = vec3(mod(decal_coords_final,vec2(1,1)),decal_id_final);
      // }
      // else
      // {
      //   discard;
      // }
    vec3 ti = decal_info;
      // ti.y = 1 - ti.y;
//     vec4 b = texture(materials,vec3(ti.xy*divamt,(ti.z + 1)));
//     albedometal = texture(materials,vec3(ti.xy*divamt,ti.z));//*vec4(vec3(synflag),1);
//     final_blend = 0;
    float mip = mip_map_level(ti.xy);

    vec4 albedometal_d = textureLod(materials[0],vec3(ti.xy*divamt,ti.z),mip);

    albedometal = mix(base_albedometal,albedometal_d,final_blend);//*vec4(vec3(synflag),1);
    vec2 b = textureLod(materials[1],vec3(ti.xy*divamt,ti.z),mip).xy;
    float c = textureLod(materials[2],vec3(ti.xy*divamt,ti.z*2.0),mip).x;


   vec3 normal = vec3(b.xy,1.0);//texture(normalMap, outTexcoord).rgb;
   normal.xy = normal.xy * 2.0 - 1.0;
   normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
   normal = normalize(normal);


   vec3 base_normal = decodeNormal(base_normalroughness.xy);



   c = mix(base_normalroughness.z,c,final_blend);
   // vec2 sa =tangent_normal.xy;


   vec3 T = normalize(decodeNormal(tangent_normal.zw));
   // vec3 B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));

    vec3 B = cross(N,T);
//   vec3 B = normalize(dFdy(worldpos));
   // N = normalize(cross(B,T));
  mat3 TBN = mat3(-T, B, N);

    normal = normalize(TBN * normal);
    normal = mix(base_normal,normal,final_blend);


    normalroughness = vec4(encodeNormalFinal(normal),c,dtexflag);



}
