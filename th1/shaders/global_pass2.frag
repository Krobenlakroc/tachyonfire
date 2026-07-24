#version 450 core
 //#define OCCLUSION
in vec2 outTexcoord;
layout (location = 0) out vec3 fragColor;
struct Light
{
  mat3 a;
  mat3 b;
  mat3 c;
};
layout(binding=3) uniform sampler2D occTex;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=20) uniform sampler2D lights;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;
uniform vec2 dims_harmtex;
uniform vec3 goffset ;
uniform vec3 gridsize ;
uniform float griddist ;



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
//     return n;
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

// vec3 sphericalHarmonics(vec3 normal,Light l)
// {
//     return max(vec3(l.a[0] +
//       l.a[1]  * normal.x +
//       l.a[2]  * normal.y +
//       l.b[0]  * normal.z +
//       l.b[1]  * normal.x*normal.z +
//       l.b[2]  * normal.y*normal.z +
//       l.c[0]  * normal.y*normal.x +
//       l.c[1]  * (3.0*normal.z*normal.z - 1.0) +
//       l.c[2]  * (normal.x*normal.x - normal.y*normal.y)),vec3(0));
// }

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

float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

float when_ge(float x, float y) {
  return 1.0 - when_lt(x, y);
}

float when_le(float x, float y) {
  return 1.0 - when_gt(x, y);
}

// float when_eq(float x, float y) {
//   return  1- abs(sign(x - y));
// }

float or(float a, float b) {
  return min(a + b, 1.0);
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


float compress2_range(vec2 i)
{
  return 0.25*(i.x * pow(2,8) + i.y);
}

vec2 extract2_range(float i)
{
  i = i*4;
  return vec2(floor(i / pow(2,8)),mod(i,pow(2,8)));
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


float when_eq(float x, float y) {
  return  1- abs(sign(x - y));
}

float when_neq(float x, float y) {
  return  abs(sign(x - y));
}

Light lerpLight(Light a,Light b,float m)
{
  a.a = a.a*(1 - m) + b.a*m;
  a.b = a.b*(1 - m) + b.b*m;
  a.c = a.c*(1 - m) + b.c*m;
  return a;
}

vec3 getPos(vec3 indices)
{
  float x = indices.x +1;
  float y = indices.y +1;
  float z = indices.z +1;
  return vec3((x-(gridsize.x*0.5))*griddist + goffset.x,(y-(gridsize.y*0.5))*griddist + goffset.y,(z-(gridsize.z*0.5))*griddist + goffset.z);
}



vec2  unpack_2half(float a)
{
  return (unpackHalf2x16(floatBitsToUint(a)));
  // vec2 result;
  // result.x = float((int(a) >> 0) & 0xFF);
  // result.y = float((int(a) >> 16) & 0xFF);
  // return result;
}
float getOcclusion(vec3 pos,vec3 worldpos,vec3 normal,vec4 sh)
{
  // if (outTexcoord.x < 0.5)
  // {
  // // vec4 t3 = texture(occTex,outTexcoord);
  // // vec4 c0123 = vec4(unpack_2half(t3.x).xy,unpack_2half(t3.y).xy);
  // // vec4 c4567 = vec4(unpack_2half(t3.z).xy,unpack_2half(t3.w).xy);
  // // vec3 n = normalize(pos-worldpos);
  // // return 1 - clamp(c0123.w +
  // //   c0123.x  * n.x +
  // //   c0123.y  * n.y +
  // //   c0123.z  * n.z +
  // //   c4567.x  * n.x*n.z +
  // //   c4567.y  * n.y*n.z +
  // //   c4567.z  * n.y*n.x +
  // //   c4567.w  * (3.0*n.z*n.z - 1.0),0,1);
  #ifdef OCCLUSION
//  const vec4 sh2_weight = vec4(vec3(0.48860), 0.28209);
  //     return (1- clamp(dot(vec4(normalize(pos-worldpos),1), sh*sh2_weight),0,1));
  // #else
  const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);
   return (1 - clamp(dot(vec4(normalize(pos-worldpos),1).yzxw, sh*sh2_weight),0,1));
   #else
   return 1;//max(0,dot(normalize(pos-worldpos),normal));
  #endif
  //  }
  // else
  // {
  //   return 1;
  // }
//  return
  //}

}
Light addLight(Light a,Light b)
{
  a.a = a.a + b.a;
  a.b = a.b + b.b;
  a.c = a.c + b.c;
  return a;
}




void mult3(
    out Light y,
    in Light f,
    in Light g)
{


    vec3 tf, tg, t;
    // [0,0]: 0,
    y.a[0] = vec3(0.282094792935999980)*f.a[0] * g.a[0];

    // [1,1]: 0,6,8,
    tf = vec3(0.282094791773000010)*f.a[0] + vec3(-0.126156626101000010)*f.c[0] + vec3(-0.218509686119999990)*f.c[2];
    tg = vec3(0.282094791773000010)*g.a[0];
    y.a[1] = tf*g.a[1] + tg*f.a[1];
    t = f.a[1] * g.a[1];
    y.a[0] += vec3(0.282094791773000010)*t;
    y.c[0] = vec3(-0.126156626101000010)*t;
    y.c[2] = vec3(-0.218509686119999990)*t;

    // [1,2]: 5,
    tf = vec3(0.218509686118000010)*f.b[2];
    y.a[1] += tf*g.a[2];
    y.a[2] = tf*g.a[1];
    t = f.a[1] * g.a[2] + f.a[2] * g.a[1];
    y.b[2] = vec3(0.218509686118000010)*t;

    // [1,3]: 4,
    tf = vec3(0.218509686114999990)*f.b[1];
    y.a[1] += tf*g.b[0];
    y.b[0] = tf*g.a[1];
    t = f.a[1] * g.b[0] + f.b[0] * g.a[1];
    y.b[1] = vec3(0.218509686114999990)*t;

    // [2,2]: 0,6,
    tf = vec3(0.282094795249000000)*f.a[0] + vec3(0.252313259986999990)*f.c[0];
  //  tg = vec3(0.282094795249000000)*g.a[0];
    y.a[2] += tf*g.a[2] + tg*f.a[2];
    t = f.a[2] * g.a[2];
    y.a[0] += vec3(0.282094795249000000)*t;
    y.c[0] += vec3(0.252313259986999990)*t;

    // [2,3]: 7,
    tf = vec3(0.218509686118000010)*f.c[1];
    y.a[2] += tf*g.b[0];
    y.b[0] += tf*g.a[2];
    t = f.a[2] * g.b[0] + f.b[0] * g.a[2];
    y.c[1] = vec3(0.218509686118000010)*t;

    // [3,3]: 0,6,8,
    tf = vec3(0.282094791773000010)*f.a[0] + vec3(-0.126156626101000010)*f.c[0] + vec3(0.218509686119999990)*f.c[2];
  //  tg = vec3(0.282094791773000010)*g.a[0];
    y.b[0] += tf*g.b[0] + tg*f.b[0];
    t = f.b[0] * g.b[0];
    y.a[0] += vec3(0.282094791773000010)*t;
    y.c[0] += vec3(-0.126156626101000010)*t;
    y.c[2] += vec3(0.218509686119999990)*t;

    // [4,4]: 0,6,
  //  tf = vec3(0.282094791770000020)*f.a[0] + vec3(-0.180223751576000010)*f.c[0];
  //  tg = vec3(0.282094791770000020)*g.a[0];
    y.b[1] += tg*f.b[1];


    // [4,5]: 7,
    // tf = vec3(0.156078347226000000)*f.c[1];
    // y.b[1] += tf*0;
    // y.b[2] += tf*0;


    // [5,5]: 0,6,8,
  //  tf = vec3(0.282094791773999990)*f.a[0] + vec3(0.090111875786499998)*f.c[0] + vec3(-0.156078347227999990)*f.c[2];
    // tg = vec3(0.282094791773999990)*g.a[0];
    y.b[2] += tg*f.b[2];


    // [6,6]: 0,6,
    // tg = vec3(0.282094797560000000)*g.a[0];
    y.c[0] +=  tg*f.c[0];


    // [7,7]: 0,6,8,
    // tf = vec3(0.282094791773999990)*f.a[0] + vec3(0.090111875786499998)*f.c[0] + vec3(0.156078347227999990)*f.c[2];
    // tg = vec3(0.282094791773999990)*g.a[0];
    y.c[1] +=  tg*f.c[1];


    // [8,8]: 0,6,
    // tg = vec3(0.282094791770000020)*g.a[0];
    y.c[2] += tg*f.c[2];


    // multiply count=120

}

void main(void)
{
float d = texture2D(depthTex,outTexcoord).r;
vec3 pos = WorldPosFromDepth(d);
vec3 normalVal = texture2D(normalTex,outTexcoord).xyz;
vec3 normal = (vec4(decodeNormal(normalVal.xy),0)).xyz;
vec3 q = quantify(pos);

vec3 m = lerppoint(pos);
vec4 sh = texture(occTex,outTexcoord);

float x = 1;//1 - min(when_lt(q.x,0)+when_gt(q.x,gridsize.x)+when_lt(q.y,0)+when_gt(q.y,gridsize.y) + when_lt(q.z,0)+when_gt(q.z,gridsize.z),1.0);//1 - or(when_lt(x,0),when_gt(x,gridsize.x*gridsize.y*gridsize.z));

float excluded = 0.f;
vec4 weights;
float occlusion_1 = getOcclusion(getPos(q + vec3(0,0,1)),pos,normal,sh);
excluded += (1-occlusion_1)*(1- m.x)*(1 - m.y)*( m.z);
weights.x = (1- m.x)*(1 - m.y)*( m.z);
float occlusion_2 = getOcclusion(getPos(q + vec3(1,0,1)),pos,normal,sh);
excluded += (1-occlusion_2)*(m.x)*(1 - m.y)*( m.z);
weights.y = (m.x)*(1 - m.y)*(m.z);
float occlusion_3 = getOcclusion(getPos(q + vec3(0,1,1)),pos,normal,sh);
excluded += (1-occlusion_3)*(1- m.x)*(m.y)*(m.z);
weights.z = (1- m.x)*( m.y)*( m.z);
float occlusion_4 = getOcclusion(getPos(q + vec3(1,1,1)),pos,normal,sh);
excluded += (1-occlusion_4)*(m.x)*(m.y)*( m.z);
weights.w = (m.x)*(m.y)*(m.z);


float occlusion_5 = getOcclusion(getPos(q + vec3(0,0,0)),pos,normal,sh);
excluded += (1-occlusion_5)*(1- m.x)*(1 - m.y)*( 1-m.z);

float occlusion_6 = getOcclusion(getPos(q + vec3(1,0,0)),pos,normal,sh);
excluded += (1-occlusion_6)*(m.x)*(1 - m.y)*( 1-m.z);

float occlusion_7 = getOcclusion(getPos(q + vec3(0,1,0)),pos,normal,sh);
excluded += (1-occlusion_7)*(1- m.x)*(m.y)*(1-m.z);

float occlusion_8 = getOcclusion(getPos(q + vec3(1,1,0)),pos,normal,sh);
excluded += (1-occlusion_8)*(m.x)*(m.y)*( 1-m.z);

float scale = 1 /(1- excluded);

Light a,b,c;
a = aquireLight(indexify(q + vec3(0,0,1)),scale*occlusion_1*weights.x);
a = addLight(a,aquireLight(indexify(q + vec3(1,0,1)),scale*occlusion_2*weights.y));
a = addLight(a,aquireLight(indexify(q + vec3(0,1,1)),scale*occlusion_3*weights.z));
a = addLight(a,aquireLight(indexify(q + vec3(1,1,1)),scale*occlusion_4*weights.w));

// a.a[0] *= ( 1 - clamp(dot(vec4((vec3(0,0,0)),1), sh),0,1));
// a.a[1] *= ( 1 - clamp(dot(vec4(normalize(vec3(1,0,0)),1), sh),0,1));
// a.a[2] *= ( 1 - clamp(dot(vec4(normalize(vec3(0,1,0)),1), sh),0,1));
// a.b[0] *= ( 1 - clamp(dot(vec4(normalize(vec3(0,0,1)),1), sh),0,1));
// a.b[1] *= ( 1 - clamp(dot(vec4(normalize(vec3(1,0,1)),1), sh),0,1));
// a.b[2] *= ( 1 - clamp(dot(vec4(normalize(vec3(0,1,1)),1), sh),0,1));
// a.c[0] *= ( 1 - clamp(dot(vec4(normalize(vec3(1,1,0)),1), sh),0,1));
// a.c[1] *= ( 1 - clamp(dot(vec4(normalize(vec3(1,0,1)),1), sh),0,1));
// a.c[2] *= ( 1 - clamp(dot(vec4(normalize(vec3(0,1,1)),1), sh),0,1));
// mult2(a,a,sh.wxyz);

// float weight1 = -1/0.282094791773878140;//1/0.282094795249000000;
// float weight2 = -(0.488602511902919920);
// float weight3 = (-1.092548);
// float weight4 = -(0.315392);
// float weight5 = -(0.546274);
// if (outTexcoord.x < 0.5)
// {
  // b.a[0] = 0.282094791773878140*4*3.14159 - vec3(sh.w);
  // b.a[1] = -vec3(sh.x);
  // b.a[2] = -vec3(sh.y);
  // b.b[0] = -vec3(sh.z);
  // b.b[1] = vec3(0);
  // b.b[2] = vec3(0);
  // b.c[0] = vec3(0);
  // b.c[1] = vec3(0);
  // b.c[2] = vec3(0);
  // mult3(c,a,b);
  // vec3 o1 = sphericalHarmonics(normal,c);
  // //const vec4 sh2_weight = vec4(vec3(-0.48860,0.48860,-0.48860), 0.28209);
  // //float oc =(1 - clamp(dot(vec4(normalize(normal),1).yzxw, sh*sh2_weight),0,1));//(1- clamp(dot(vec4(normalize(normal),1), texture(occTex,outTexcoord)),0,1));
  // // oc = max(oc,0.25);
  // float oc = 1;
  // fragColor = vec3(o1*x*oc);//
// }
// else
// {
  #ifdef OCCLUSION
  const vec4 sh2_weight = vec4(vec3(0.48860,0.48860,0.48860), 0.28209);
  float oc = (1 - clamp(dot(vec4(normalize(normal),1).yzxw, sh*sh2_weight),0,1));
  #else
  float oc =1;
  #endif

  vec3 o1 = sphericalHarmonics(normal,a);
  //const vec4 sh2_weight = vec4(vec3(-0.48860,0.48860,-0.48860), 0.28209);
  //float oc =(1 - clamp(dot(vec4(normalize(normal),1).yzxw, sh*sh2_weight),0,1));//(1- clamp(dot(vec4(normalize(normal),1), texture(occTex,outTexcoord)),0,1));
  // oc = max(oc,0.25);
  fragColor = vec3(o1*x*oc);//
// }
}
