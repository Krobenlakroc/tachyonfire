#version 450 core
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;

layout(binding=31) uniform sampler2D prefilterMap;
layout(binding=1) uniform sampler2D normalTex;
uniform vec2 screenSize;


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

/*
3x3 Median
Morgan McGuire and Kyle Whitson
http://graphics.cs.williams.edu


Copyright (c) Morgan McGuire and Williams College, 2006
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


float when_gt(float x, float y) {
  return max(sign(x - y), 0.0);
}

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

float luma(vec3 color) {
  return dot(color, vec3(1,1,1));
}

vec4 cmin(vec4 a,vec4 b)
{
  float la = luma(a.xyz);
  float lb = luma(b.xyz);
  float override_a = when_gt(abs(la),100);
  float override_b = when_gt(abs(lb),100);
  float oa = override_a*(1-override_b);
  float ob = override_b*(1-override_a);

  float cmp = clamp(when_gt(lb,la) + ob,0,1);
  cmp = cmp*(1-oa);
  return a*cmp + b*(1-cmp);
}

vec4 cmax(vec4 a,vec4 b)
{
  float la = luma(a.xyz);
  float lb = luma(b.xyz);

  float override_a = when_gt(abs(la),100);
  float override_b = when_gt(abs(lb),100);
  float oa = override_a*(1-override_b);
  float ob = override_b*(1-override_a);

  float cmp = clamp((1 - when_gt(lb,la) + ob),0,1);
  cmp = cmp*(1-oa);
  return a*cmp + b*(1-cmp);
}
//#define vec vec4
//#define toVec(x) x.rgba

#define s2(a, b)				temp = a; a = cmin(a, b); b = cmax(temp, b);
#define mn3(a, b, c)			s2(a, b); s2(a, c);
#define mx3(a, b, c)			s2(b, c); s2(a, c);

#define mnmx3(a, b, c)			mx3(a, b, c); s2(a, b);                                   // 3 exchanges
#define mnmx4(a, b, c, d)		s2(a, b); s2(c, d); s2(a, c); s2(b, d);                   // 4 exchanges
#define mnmx5(a, b, c, d, e)	s2(a, b); s2(c, d); mn3(a, c, e); mx3(b, d, e);           // 6 exchanges
#define mnmx6(a, b, c, d, e, f) s2(a, d); s2(b, e); s2(c, f); mn3(a, b, c); mx3(d, e, f); // 7 exchanges

#define almost_equal(a,b) when_lt(abs(a.w-b.w) , 0.01) * when_gt(dot(b.xyz,b.xyz) , 0.98)

vec3 ClipAABB(vec3 aabb_min, vec3 aabb_max, vec3 prev_sample) {

    vec3 aabb_center = 0.5 * (aabb_max + aabb_min);
    vec3 extent_clip = 0.5 * (aabb_max - aabb_min) + 0.001;

    // Find color vector
    vec3 color_vector = prev_sample - aabb_center;
    // Transform into clip space
    vec3 color_vector_clip = color_vector / extent_clip;
    // Find max absolute component
    color_vector_clip = abs(color_vector_clip);
    float max_abs_unit = max(max(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z);

    if (max_abs_unit > 1.0) {
        return aabb_center + color_vector / max_abs_unit; // clip towards color vector
    }
    else {
        return prev_sample; // point is inside aabb
    }
}

#define almost_equal(a,b) when_lt(abs(a.w-b.w) , 0.01) * when_gt(dot(b.xyz,b.xyz) , 0.98)
vec4 Blur(sampler2D tex,vec2 texelSize,vec2 uv) {

  vec4 v[9];
  vec4 prime = getnormalRoughness(uv);
  // Add the pixels which make up our window to the pixel array.
  vec3 avg_luma = vec3(0);
  for(int dX = -1; dX <= 1; ++dX) {
    for(int dY = -1; dY <= 1; ++dY) {
      vec2 offset = vec2(float(dX), float(dY));

      // If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the
      // pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the
      // bottom right pixel of the window at pixel[N-1].

      vec4 delta = getnormalRoughness(uv + offset*texelSize);
    //  float equality = almost_equal(delta,prime);
    //  offset = offset*equality;
      v[(dX + 1) * 3 + (dY + 1)] = (texture2D(tex, uv + offset * texelSize));
    }
  }

  vec4 temp;
  vec4 og = v[4];
  // Starting with a subset of size 6, remove the min and max each time
  mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]);
  mnmx5(v[1], v[2], v[3], v[4], v[6]);
  mnmx4(v[2], v[3], v[4], v[7]);
  mnmx3(v[3], v[4], v[8]);

  vec4 final = v[4];

  //final.xyz = ClipAABB(avg_luma.xyz - vec3(0.1,0.1,0.1),avg_luma.xyz + vec3(0.1,0.1,0.1),final.xyz);
  return final;

}


#define t2(a, b)				s2(v[a], v[b]);
#define t24(a, b, c, d, e, f, g, h)			t2(a, b); t2(c, d); t2(e, f); t2(g, h);
#define t25(a, b, c, d, e, f, g, h, i, j)		t24(a, b, c, d, e, f, g, h); t2(i, j);

float when_eq2(vec2 x, vec2 y) {
  vec2 v =  vec2(1) - abs(sign(x - y));
  return v.x*v.y;
}

vec4 Blur5(sampler2D tex,vec2 texelSize,vec2 uv) {

  vec4 v[25];
  vec4 prime = getnormalRoughness(uv);
  // Add the pixels which make up our window to the pixel array.
  vec4 og = texture2D(tex, uv);

  float flipflop = 1;
  for(int dX = -2; dX <= 2; ++dX) {
    for(int dY = -2; dY <= 2; ++dY) {
      vec2 offset = vec2(float(dX), float(dY));

      // If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) + (dY + R) of the
      // pixel array. This will fill the pixel array, with the top left pixel of the window at pixel[0] and the
      // bottom right pixel of the window at pixel[N-1].
      vec4 delta = getnormalRoughness(uv + offset*texelSize);
      float equality = clamp(almost_equal(delta,prime) + when_eq2(offset,vec2(0,0)),0,1);

      v[(dX + 2) * 5 + (dY + 2)] = (texture2D(tex, uv + offset * texelSize))*equality + (og + vec4(10000,10000,1000,0)*flipflop )*(1-equality);
      flipflop = flipflop*(equality) + -flipflop*(1- equality);
    }
  }

  vec4 temp;

  t25(0, 1,			3, 4,		2, 4,		2, 3,		6, 7);
  t25(5, 7,			5, 6,		9, 7,		1, 7,		1, 4);
  t25(12, 13,		11, 13,		11, 12,		15, 16,		14, 16);
  t25(14, 15,		18, 19,		17, 19,		17, 18,		21, 22);
  t25(20, 22,		20, 21,		23, 24,		2, 5,		3, 6);
  t25(0, 6,			0, 3,		4, 7,		1, 7,		1, 4);
  t25(11, 14,		8, 14,		8, 11,		12, 15,		9, 15);
  t25(9, 12,		13, 16,		10, 16,		10, 13,		20, 23);
  t25(17, 23,		17, 20,		21, 24,		18, 24,		18, 21);
  t25(19, 22,		8, 17,		9, 18,		0, 18,		0, 9);
  t25(10, 19,		1, 19,		1, 10,		11, 20,		2, 20);
  t25(2, 11,		12, 21,		3, 21,		3, 12,		13, 22);
  t25(4, 22,		4, 13,		14, 23,		5, 23,		5, 14);
  t25(15, 24,		6, 24,		6, 15,		7, 16,		7, 19);
  t25(3, 11,		5, 17,		11, 17,		9, 17,		4, 10);
  t25(6, 12,		7, 14,		4, 6,		4, 7,		12, 14);
  t25(10, 14,		6, 7,		10, 12,		6, 10,		6, 17);
  t25(12, 17,		7, 17,		7, 10,		12, 18,		7, 12);
  t24(10, 18,		12, 20,		10, 20,		10, 12);
  return v[12];

}

float weightfunc(int x, int y) {
    uint weights[] = { 6, 4, 1 };
    return float(weights[abs(x)] * weights[abs(y)]) / 256.0;
}

vec4 gauss(sampler2D tex,vec2 texelSize,vec2 uv) {
    vec4 sum = vec4(0.0);
    float total_weight = 0.0;
    vec4 og = (texture2D(tex, uv));
    const int radius = 2;

    vec4 prime = getnormalRoughness(uv);

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
          vec2 offset = vec2(float(dx), float(dy));

          vec4 val = (texture2D(tex, uv + offset * texelSize));
          vec4 delta = getnormalRoughness(uv + offset*texelSize);
          float equality = clamp(almost_equal(delta,prime) + when_eq2(offset,vec2(0,0)),0,1);

            float weight = weightfunc(dx, dy)*equality;

            sum += weight * val;
            total_weight += weight;
        }
    }

    sum /= max(total_weight, 0.0001);
    og = sum*when_gt(total_weight,0.0001);// + texture2D(tex, uv ).xyz*(1 - when_gt(total_weight,0.2));
    return og;
}


void main(void)
{
  vec3 normalt = texture(normalTex,outTexcoord).xyz;
  vec3 normal = (vec4(decodeNormal(normalt.xy),0)).xyz;



  float roughness = normalt.z;

  if (roughness < 0.1)
  {
     fragColor = texture(prefilterMap,outTexcoord);
  }
  else
  {
    fragColor = Blur(prefilterMap,vec2(1.0)/screenSize,outTexcoord);
  }


}
