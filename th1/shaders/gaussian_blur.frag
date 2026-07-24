#version 450 core
#define texture2D texture
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;

layout(binding=31) uniform sampler2D prefilterMap;
layout(binding=1) uniform sampler2D normalTex;
uniform vec2 screenSize;
uniform vec2 direction_in;

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


vec4 getnormalRoughness(vec2 coords)
{
  vec3 normalt = texture(normalTex,coords).xyz;
  vec3 normal = (vec4(decodeNormal(normalt.xy),0)).xyz;
  return vec4(normal,normalt.z);
}

#define almost_equal(a,b) abs(a.w-b.w) < 0.01 && dot(b.xyz,b.xyz) >= 0.99

vec4 blur5(sampler2D image, vec2 uv, vec2 resolution, vec2 direction,vec4 normalroughness) {
  vec2 off1 = vec2(1.3333333333333333) * direction;
  vec2 off2 = vec2(1) * direction;
  vec2 off3 = vec2(2) * direction;
  vec4 s1 = getnormalRoughness(uv + (off2 / resolution));
  vec4 s2 = getnormalRoughness(uv - (off2 / resolution));
  vec4 s3 = getnormalRoughness(uv + (off3 / resolution));
  vec4 s4 = getnormalRoughness(uv - (off3 / resolution));
  vec4 color = vec4(0.0);

  if (almost_equal(s1,normalroughness) && almost_equal(s2,normalroughness) && almost_equal(s3,normalroughness) && almost_equal(s4,normalroughness))
  {

    color += texture2D(image, uv) * 0.29411764705882354;
    color += texture2D(image, uv + (off1 / resolution)) * 0.35294117647058826;
    color += texture2D(image, uv - (off1 / resolution)) * 0.35294117647058826;
  }
  else
  {
    color += texture2D(image, uv);
  }


  return color;
}


void main(void)
{
  vec3 normalt = texture(normalTex,outTexcoord).xyz;
  vec3 normal = (vec4(decodeNormal(normalt.xy),0)).xyz;



  float roughness = normalt.z;
  if (roughness < 0.1)
  {
    discard;//fragColor =
  }
  else
  {
    fragColor = blur5(prefilterMap,outTexcoord,screenSize,direction_in,vec4(normal,roughness));
  }


}
