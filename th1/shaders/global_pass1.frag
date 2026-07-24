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



void main(void)
{
float d = texture2D(depthTex,outTexcoord).r;
vec3 pos = WorldPosFromDepth(d);
vec3 normalVal = texture(normalTex,outTexcoord).xyz;
vec3 normal = (vec4(decodeNormal(normalVal.xy),0)).xyz;
vec3 q = quantify(pos);

vec3 m = lerppoint(pos);

// vec4 sh =texture(occTex,outTexcoord);

float x = 1 - min(when_lt(q.x,0)+when_gt(q.x,gridsize.x)+when_lt(q.y,0)+when_gt(q.y,gridsize.y) + when_lt(q.z,0)+when_gt(q.z,gridsize.z),1.0);//1 - or(when_lt(x,0),when_gt(x,gridsize.x*gridsize.y*gridsize.z));
float excluded = 0.f;
vec4 weights;
float occlusion_1 = getOcclusion(getPos(q + vec3(0,0,0)),pos,normal);
excluded += (1-occlusion_1)*(1- m.x)*(1 - m.y)*(1 - m.z);
weights.x = (1- m.x)*(1 - m.y)*(1 - m.z);
float occlusion_2 = getOcclusion(getPos(q + vec3(1,0,0)),pos,normal);
excluded += (1-occlusion_2)*(m.x)*(1 - m.y)*(1 - m.z);
weights.y = (m.x)*(1 - m.y)*(1 - m.z);
float occlusion_3 = getOcclusion(getPos(q + vec3(0,1,0)),pos,normal);
excluded += (1-occlusion_3)*(1- m.x)*(m.y)*(1 - m.z);
weights.z = (1- m.x)*( m.y)*(1 - m.z);
float occlusion_4 = getOcclusion(getPos(q + vec3(1,1,0)),pos,normal);
excluded += (1-occlusion_4)*(m.x)*(m.y)*(1 - m.z);
weights.w = (m.x)*(m.y)*(1 - m.z);
// float scale = 1 /(1- excluded);

vec4 weights2;
float occlusion_5 = getOcclusion(getPos(q + vec3(0,0,1)),pos,normal);
excluded += (1-occlusion_5)*(1- m.x)*(1 - m.y)*( m.z);
weights2.x = (1- m.x)*(1 - m.y)*( m.z);

float occlusion_6 = getOcclusion(getPos(q + vec3(1,0,1)),pos,normal);
excluded += (1-occlusion_6)*(m.x)*(1 - m.y)*( m.z);
weights2.y = ( m.x)*(1 - m.y)*( m.z);

float occlusion_7 = getOcclusion(getPos(q + vec3(0,1,1)),pos,normal);
excluded += (1-occlusion_7)*(1- m.x)*(m.y)*(m.z);
weights2.z = (1- m.x)*( m.y)*( m.z);

float occlusion_8 = getOcclusion(getPos(q + vec3(1,1,1)),pos,normal);
excluded += (1-occlusion_8)*(m.x)*(m.y)*( m.z);
weights2.w = (m.x)*( m.y)*( m.z);

float scale = 1 /(1- excluded);

Light a,b,c;
a = aquireLight(indexify(q + vec3(0,0,0)),scale*occlusion_1*weights.x);
a = addLight(a,aquireLight(indexify(q + vec3(1,0,0)),scale*occlusion_2*weights.y));
a = addLight(a,aquireLight(indexify(q + vec3(0,1,0)),scale*occlusion_3*weights.z));
a = addLight(a,aquireLight(indexify(q + vec3(1,1,0)),scale*occlusion_4*weights.w));

a = addLight(a,aquireLight(indexify(q + vec3(0,0,1)),scale*occlusion_5*weights2.x));
a = addLight(a,aquireLight(indexify(q + vec3(1,0,1)),scale*occlusion_6*weights2.y));
a = addLight(a,aquireLight(indexify(q + vec3(0,1,1)),scale*occlusion_7*weights2.z));
a = addLight(a,aquireLight(indexify(q + vec3(1,1,1)),scale*occlusion_8*weights2.w));


  vec3 o1 = sphericalHarmonics(normal,a);
  fragColor = vec3(o1*x);//
// }


}
