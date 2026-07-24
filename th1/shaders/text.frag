#version 460 core
#define texture2D texture
in vec2 outTexcoord;

uniform float inUvs[100];
uniform float charCount;
uniform float charset;
uniform float time;
uniform float font_id;

layout (location = 0) out vec3 fragColor;

layout(binding=25) uniform sampler2D fontTex;
layout(binding=34) uniform sampler2DArray colorTex;



float when_ge(float x, float y) {
  return 1.0 - max(sign(y - x), 0.0);
}

float when_le(float x, float y) {
  return 1.0 - max(sign(x - y), 0.0);
}

float when_eq(float x, float y) {
  return 1- abs(sign(x - y));
}
float when_neq(float x, float y) {
  return abs(sign(x - y));
}

float or(float a, float b) {
  return min(a + b, 1.0);
}

float and(float a, float b)
{
  return a*b;
}

// float outline(vec2 uv,float base)
// {
//   vec2 size = vec2(0.001);
//
// 	float outline = texture2D(fontTex, uv + vec2(-size.x, 0)).a;
// 	outline += texture2D(fontTex, uv + vec2(0, size.y)).a;
// 	outline += texture2D(fontTex, uv + vec2(size.x, 0)).a;
// 	outline += texture2D(fontTex, uv + vec2(0, -size.y)).a;
// 	outline += texture2D(fontTex, uv + vec2(-size.x, size.y)).a;
// 	outline += texture2D(fontTex, uv + vec2(size.x, size.y)).a;
// 	outline += texture2D(fontTex, uv + vec2(-size.x, -size.y)).a;
// 	outline += texture2D(fontTex, uv + vec2(size.x, -size.y)).a;
// 	outline = min(outline, 1.0) - base;
//   return outline;
// }

void main()
{
  vec4 font = texture2D(fontTex,outTexcoord );


  vec2 uvs;
  uvs.y = outTexcoord.y;
  float xcoord = outTexcoord.x*charCount;

  float spchar = or(when_eq(inUvs[int(floor(xcoord)  )*2],-1),when_eq(inUvs[int(floor(xcoord)  )*2],-2));
  spchar = or(spchar,when_eq(inUvs[int(floor(xcoord)  )*2],-3));
  spchar = or(spchar,when_eq(inUvs[int(floor(xcoord)  )*2],-4));
  spchar = or(spchar,when_eq(inUvs[int(floor(xcoord)  )*2],-5));
  spchar = or(spchar,when_eq(inUvs[int(floor(xcoord)  )*2],-6));
  float nspchar = 1 - spchar;

  float t = mod(xcoord,1)*nspchar + 0.5*spchar;
  uvs.x = mix(inUvs[int(floor(xcoord)  )*2] ,inUvs[int(floor(xcoord) )*2 + 1],t);
  // uvs.x = clamp(uvs.x,inUvs[int(floor(xcoord))*2] + 0.00625*2,inUvs[int(floor(xcoord))*2 + 1] - 0.00625*2);
  float ntime = sin(time);
  vec4 inTex = texture2D(fontTex,uvs);
  inTex.a *= when_ge(uvs.x,inUvs[int(floor(xcoord))*2] + (1/(16*charset)));
  inTex.a *= when_le(uvs.x,inUvs[int(floor(xcoord))*2 + 1] - (1/(16*charset)));
  vec4 inColor = texture(colorTex,vec3(t,uvs.y + ntime*-0.1f ,font_id));//- 0.5*outTexcoord.x

  inTex.a = inTex.a*nspchar + when_eq(inUvs[int(floor(xcoord)  )*2],-4);
  inTex.a = inTex.a + when_eq(inUvs[int(floor(xcoord)  )*2],-2) * when_ge(outTexcoord.y,1-0.35)* when_le(mod(xcoord,1),0.35);
  inTex.a = inTex.a + when_eq(inUvs[int(floor(xcoord)  )*2],-3) * when_ge(outTexcoord.y,1-0.35);
  inTex.a = inTex.a + when_eq(inUvs[int(floor(xcoord)  )*2],-1) * when_le(abs(mod(xcoord,1) - (1-outTexcoord.y)),0.1);
  inTex.a = inTex.a + when_eq(inUvs[int(floor(xcoord)  )*2],-5) * when_ge(mod(xcoord,1),0.3) * when_le(mod(xcoord,1),0.7);
  inTex.a = inTex.a + when_eq(inUvs[int(floor(xcoord)  )*2],-6) * or(when_ge((1-outTexcoord.y),0.3) * when_le((1-outTexcoord.y),0.7),when_ge(mod(xcoord,1),0.3) * when_le(mod(xcoord,1),0.7));


  if (inTex.a  < 0.9)
    discard;
  fragColor = inColor.rgb;

}
