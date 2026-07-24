#version 450 core
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;
layout(binding=1) uniform sampler2D normalTex;
layout(binding=3) uniform sampler2D occTex;
uniform vec2 resolution;
uniform vec2 dir;

float when_lt(float x, float y) {
  return max(sign(y - x), 0.0);
}

vec2  unpack_2half(float a)
{
  return (unpackHalf2x16(floatBitsToUint(a)));
  // vec2 result;
  // result.x = float((int(a) >> 0) & 0xFF);
  // result.y = float((int(a) >> 16) & 0xFF);
  // return result;
}

float pack_2half(float x,float y)
{
  return uintBitsToFloat(packHalf2x16(vec2(x,y)));
  // float result;
  // return result = float(((floatBitsToInt(y)) << 16) | floatBitsToInt(x));
}

vec4 c0123;
vec4 c4567;

void unpack_and_add(vec2 tex,float weight)
{
  vec4 t1;
  vec4 t2;
  vec4 t3 = texture(occTex, tex);
  t1 = vec4(unpack_2half(t3.x).xy,unpack_2half(t3.y).xy);
  t2 = vec4(unpack_2half(t3.z).xy,unpack_2half(t3.w).xy);
  c0123 += t1*weight;
  c4567 += t2*weight;
}

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

vec4 dssdo_blur(vec2 tex)
{

   c0123 = vec4(0);
   c4567 = vec4(0);
	float weights[9] =
	{
		0.013519569015984728,
		0.047662179108871855,
		0.11723004402070096,
		0.20116755999375591,
		0.240841295721373,
		0.20116755999375591,
		0.11723004402070096,
		0.047662179108871855,
		0.013519569015984728
	};

	float indices[9] = {-4, -3, -2, -1, 0, 1, 2, 3, 4};

	vec2 stp = dir/resolution;

	vec3 normal[9];

	normal[0] = decodeNormal(texture(normalTex, tex + indices[0]*stp).xy);
	normal[1] = decodeNormal(texture(normalTex, tex + indices[1]*stp).xy);
	normal[2] = decodeNormal(texture(normalTex, tex + indices[2]*stp).xy);
	normal[3] = decodeNormal(texture(normalTex, tex + indices[3]*stp).xy);
	normal[4] = decodeNormal(texture(normalTex, tex + indices[4]*stp).xy);
	normal[5] = decodeNormal(texture(normalTex, tex + indices[5]*stp).xy);
	normal[6] = decodeNormal(texture(normalTex, tex + indices[6]*stp).xy);
	normal[7] = decodeNormal(texture(normalTex, tex + indices[7]*stp).xy);
	normal[8] = decodeNormal(texture(normalTex, tex + indices[8]*stp).xy);

	float total_weight = 1.0;
	float discard_threshold = 0.85;

	int i;

	for( i=0; i<9; ++i )
	{
    float cmp = when_lt(dot(normal[i], normal[4]) , discard_threshold );

			total_weight -= weights[i]*cmp;
			weights[i] = weights[i]*(1 - cmp);

	}

	//

	 vec4 res = vec4(0);

	// for( i=0; i<9; ++i )
	// {
		res += texture(occTex, tex + -4*stp)* weights[0];
    res += texture(occTex, tex + -3*stp)* weights[1];
    res += texture(occTex, tex + -2*stp)* weights[2];
    res += texture(occTex, tex + -1*stp)* weights[3];
    res += texture(occTex, tex + 0*stp)* weights[4];
    res += texture(occTex, tex + 1*stp)* weights[5];
    res += texture(occTex, tex + 2*stp)* weights[6];
    res += texture(occTex, tex + 3*stp)* weights[7];
    res += texture(occTex, tex + 4*stp)* weights[8];
	// }
  // unpack_and_add(tex + -4*stp,weights[0]);
  // unpack_and_add(tex + -3*stp,weights[1]);
  // unpack_and_add(tex + -2*stp,weights[2]);
  // unpack_and_add(tex + -1*stp,weights[3]);
  // unpack_and_add(tex + 0*stp,weights[4]);
  // unpack_and_add(tex + 1*stp,weights[5]);
  // unpack_and_add(tex + 2*stp,weights[6]);
  // unpack_and_add(tex + 3*stp,weights[7]);
  // unpack_and_add(tex + 4*stp,weights[8]);
  //

  // c0123 /= total_weight;
  // c4567 /= total_weight;
	 res /= total_weight;
  //
	 return res;
}

void main(void)
{
//  vec3 normal = texture(normalTex, outTexcoord).xyz*2 -1;
  fragColor =dssdo_blur(outTexcoord);

//  fragColor = vec4(pack_2half(c0123.x,c0123.y),pack_2half(c0123.z,c0123.w),pack_2half(c4567.x,c4567.y),pack_2half(c4567.z,c4567.w));

}
