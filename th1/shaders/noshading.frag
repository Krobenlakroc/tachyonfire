#version 460 core
layout(early_fragment_tests) in;
flat in vec2 ID;
in vec2 outTexcoord;
// in mat3 TBN;
in vec3 T;
// in vec3 B;
in vec3 N;

in float dynamic_flag;
layout (location = 0) out vec4 albedometal;
layout (location = 1) out vec4 normalroughness;
layout (location = 2) out vec4 geo_normal;
layout(binding=6) uniform sampler2DArray materials[3];


layout (std140 , binding = 1) uniform MaterialProperties
{
  ivec4 featureFlags[32];//encode as a binary
};


  // layout(binding=6) uniform sampler2DArray materials[3];

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

 float when_eq(float x, float y) {
   return  (1- abs(sign(x - y)));
 }

 float copysign(float a,float b)
 {
   return a* (-1*(1 - when_eq(sign(a),sign(b))) +when_eq(sign(a),sign(b))) ;
 }

vec4 makequat(mat3 m)
{
  vec4 quaternion;
  quaternion.w = sqrt( max( 0, 1 + m[0][0] + m[1][1] + m[2][2] ) ) / 2;
quaternion.x = sqrt( max( 0, 1 + m[0][0] - m[1][1] - m[2][2] ) ) / 2;
quaternion.y = sqrt( max( 0, 1 - m[0][0] + m[1][1] - m[2][2] ) ) / 2;
quaternion.z = sqrt( max( 0, 1 - m[0][0] - m[1][1] + m[2][2] ) ) / 2;
quaternion.x = copysign( quaternion.x, m[1][2] - m[2][1] );
quaternion.y = copysign( quaternion.y, m[2][0] - m[0][2] );
quaternion.z = copysign( quaternion.z, m[0][1] - m[1][0] );
return quaternion;
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

vec2 when_ge2(vec2 x, vec2 y) {
  return 1.0 - max(sign(y - x), 0.0);
}

float when_ge(float x, float y) {
  return 1.0 - max(sign(y - x), 0.0);
}

vec2 OctWrap( vec2 v )
{
    return ( 1.0 - abs( v.yx ) ) * ( sign(when_ge2(v.xy,vec2(0)) - 0.5) );
}


vec2 encodeNormalFinal( vec3 n )
{
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
    n.xy = when_ge(n.z,0)*n.xy + (1 - when_ge(n.z,0))*OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}

// float encodeBits(float ID,float final_bool)
// {
//   int bits = int(clamp(ID,0,32767));
//
//   int final = bits | (int(final_bool) << 16);
//
//   return intBitsToFloat(final);//float(final);
// }

float encodeFlags(int flags) {
  //flags = clamp(flags, 0, 256);
  return float(float(flags + 0.5)/255.0);
}

int decodeFlags(float f) {
  // add 0.5 to avoid rounding errors from float
  return int(f*255.0);
}

// float setFlag(float f, int bit) {
//   int flags = decodeFlags(f);
//   flags |= (1 << bit);
//   return float(flags/255.0);
// }

float setFlagConditional(float f, int bit,float val) {
  int flags = decodeFlags(f);
  flags |= (int(val + 0.5) << bit);
  return float(float(flags + 0.5)/255.0);
}

// float hasFlag(float f, int bit) {
//   int flags = decodeFlags(f);
//   return float((flags & (1 << bit)) != 0);
// }

void main()
{
  // //texture(materials[int(ID.x)],vec3(outTexcoord,(ID.y)));
  //   float sig = 1*dynamic_flag + -1*(1-dynamic_flag);
  //   //mod(outTexcoord,vec2(1.0))
  //   //2*abs(outTexcoord*0.5 - floor(outTexcoord*0.5 + 0.5))
  //  texInfo = vec3(outTexcoord,(ID.y + 1)*sig);
  //  basisbuffer = vec4(encodeNormalFinal(T),encodeNormalFinal(N));


  albedometal = texture(materials[0],vec3(outTexcoord,ID.x))*float(gl_FrontFacing);//*vec4(vec3(synflag),1);
  vec2 b = texture(materials[1],vec3(outTexcoord,(ID.x))).xy;
  float c = texture(materials[2],vec3(outTexcoord,(ID.y))).x;



 vec3 normal = vec3(b.rg,0);//texture(normalMap, outTexcoord).rgb;
 normal.xy = normal.xy * 2.0 - 1.0;
 normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));

 normal = normalize(normal);
 // vec2 sa =tangent_normal.xy;
 //
 //
 // vec3 T = normalize(decodeNormal(sa));
 // vec3 B = normalize(vec3(inTransform * vec4(inBiTangent, 0.0)));
 float sig = -1.0;

 vec3 B = cross(N,T);

 mat3 TBN = mat3(-T, B, N); //far left


  float features = encodeFlags(featureFlags[uint(ID.x) / 4][uint(ID.x) % 4]);
  features = setFlagConditional(features,0,dynamic_flag);
  //features = setFlagConditional(features,1,float(ID.x == 4.0)*float(ID.y == 6.0));//glow flag

  normal = normalize(TBN * normal);
  normalroughness = vec4(encodeNormalFinal(normal),c,features);
  geo_normal = vec4(encodeNormalFinal(N),encodeNormalFinal(B));

}
