#version 450 core
//#define OCCLUSION

in vec2 outTexcoord;
layout (location = 2) out vec4 fragColor;

layout(binding=3) uniform sampler2D occTex;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=22) uniform samplerCubeArray reflections;
layout(binding=21) uniform samplerCubeArray reflectionsDepth;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform vec3 eyePos;


layout (std140 , binding = 0) uniform EnvBoxes
{
  vec3 EnvBoxPos[20];// 0
  vec3 EnvBoxMin[20];// size*16
  vec3 EnvBoxMax[20];//size * 16*2
  int EnvBoxCount;//size*16*3
  //total size is size*16*3 + 4
};


// vec3 decodeNormal (vec2 enc)
// {
//   vec2 ang = enc*2-1;
//     vec2 scth = vec2(sin(ang.x * 3.1415926536),cos(ang.x * 3.1415926536));
//     vec2 scphi = vec2(sqrt(1.0 - ang.y*ang.y), ang.y);
//     return vec3(scth.y*scphi.x, scth.x*scphi.x, scphi.y);
// }
// vec3 decodeNormal (vec2 enc)
// {
//     vec2 fenc = enc*4-2;
//     float f = dot(fenc,fenc);
//     float g = sqrt(1-f/4);
//     vec3 n;
//     n.xy = fenc*g;
//     n.z = 1-f/2;
//     return normalize(n);
// }
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
  //  iteration
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
  //float fdist  = (texture(mp,l*vec3(-1,1,1)).r);
  return l;//*when_lt(length(x),fdist);
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

// float getOcclusion(vec3 n)
// {
//
//   vec4 t3 = texture(occTex,outTexcoord);
//   vec4 c0123 = vec4(unpack_2half(t3.x).xy,unpack_2half(t3.y).xy);
//   vec4 c4567 = vec4(unpack_2half(t3.z).xy,unpack_2half(t3.w).xy);
//   return 1 - clamp(c0123.w +
//     c0123.x  * n.x +
//     c0123.y  * n.y +
//     c0123.z  * n.z +
//     c4567.x  * n.x*n.z +
//     c4567.y  * n.y*n.z +
//     c4567.z  * n.y*n.x +
//     c4567.w  * (3.0*n.z*n.z - 1.0),0,1);
//
//
// }

void main(void)
{

  float depthval = texture2D(depthTex,outTexcoord ).r;
  vec3 position = WorldPosFromDepth(depthval);

  vec3 viewDir    = normalize(position - eyePos);
  vec3 normalt = texture(normalTex,outTexcoord).xyz;
  vec3 normal = (vec4(decodeNormal(normalt.xy),0)).xyz;
  vec3 refnormal = reflect(viewDir,normal);
  float roughness = normalt.z;
  float index = -1;
  float bounded = 0;//CheckCollision(position,EnvBoxMin[0],EnvBoxMax[0]);
  float cur_dist = -1;
  float closest_probe = 0;
  float dist_2 = 1.0/0.0;
  for (int i = 0 ; i < EnvBoxCount;i++)
  {
    float bounded_i = CheckCollision(position,EnvBoxMin[i],EnvBoxMax[i]);//*when_gt(dot(normalize(EnvBoxPos[int(i)] - position),normal),0);
    float dist = distance(position,EnvBoxPos[int(i)]);
    float nw = when_lt(dist,dist_2);
    closest_probe = i*nw + closest_probe*(1 - nw);
    dist_2 = dist*nw + dist_2*(1-nw);

    bounded = or(bounded_i,bounded);

    float replace = or(when_eq(index,-1),(1-when_eq(index,-1))*when_lt(dist,cur_dist) )*bounded_i;
    index = replace*i + (1-replace)*index;
    cur_dist = distance(position,EnvBoxPos[int(index)]);
  }
  bounded *=  1- when_eq(index,-1);
  index += when_eq(index,-1);//*closest_probe + index*(1 - when_eq(index,-1));



  // vec3 RayLS = EnvBoxMat * nrdir;
  // vec3 PositionLS = EnvBoxMat * (position);
  // vec3 tfmax = EnvBoxMat *EnvBoxPosWorld + vec3(1);
  // vec3 tfmin = EnvBoxMat *EnvBoxPosWorld - vec3(1);
  //
  // float affected = when_lt(PositionLS.x,tfmax.x)*when_lt(PositionLS.y,tfmax.y)*when_lt(PositionLS.z,tfmax.z);
  // affected =  affected * when_gt(PositionLS.x,tfmin.x)*when_gt(PositionLS.y,tfmin.y)*when_gt(PositionLS.z,tfmin.z);
  // float bounded = CheckCollision(position,EnvBoxMin[0],EnvBoxMax[0]);

  float occ = 1;

  #ifdef OCCLUSION
  // if (outTexcoord.x < 0.5)
  // {
    const vec4 sh2_weight = vec4(vec3(-0.48860,0.48860,-0.48860), 0.28209);
     occ = (1 - clamp(dot(vec4(normalize(refnormal),1).yxzw, texture(occTex,outTexcoord)*sh2_weight),0,1));
  //}

   #endif

  vec3 l = Hit((position - EnvBoxPos[int(index)]), refnormal, reflectionsDepth,index); // ray hit
//bounded = or(bounded,1-when_lt( (texture(reflectionsDepth,vec4(l*vec3(-1,1,1),index)).r) , distance(position,EnvBoxPos[int(index)])));
  vec4 lookup = textureLod(reflections,vec4(l*vec3(-1,1,1),index),roughness*4.0);
  fragColor = vec4(lookup.xyz*bounded*occ,1);
}
