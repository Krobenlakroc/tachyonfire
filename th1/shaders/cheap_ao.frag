#version 460 core
// layout(early_fragment_tests) in;
#define texture2D texture
layout (location = 0) out float ao_output;

layout(binding=2) uniform sampler2D depthTex;
layout(binding=3) uniform sampler2D normalTex;

uniform vec2 invWindow;
uniform mat4 projMatrixInv;
uniform mat4 viewMatrixInv;

in vec4 sphere;

vec3 WorldPosFromDepth(vec2 tc,float depth) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(tc * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = projMatrixInv * clipSpacePosition;
    // linDepth = viewSpacePosition.z;
    // Perspective division

    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = viewMatrixInv * viewSpacePosition;

    return worldSpacePosition.xyz;
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

// Sphere occlusion
float sphOcclusion( in vec3 pos, in vec3 nor, in vec4 sph )
{
    vec3  di = sph.xyz - pos;
    float l  = length(di);
    float nl = dot(nor,di/l);
    float h  = l/sph.w;
    float h2 = h*h;
    float k2 = 1.0 - h2*nl*nl;

    // above/below horizon
    // EXACT: Quilez - https://iquilezles.org/articles/sphereao
    float res = max(0.0,nl)/h2;

    // intersecting horizon
//     if( k2 > 0.001 )
//     {
//        res = (nl*h+1.0)/h2;
//        res = 0.33*res*res;
//     }

    return res;
}

float when_gt(float x, float y) {
    return max(sign(x - y), 0.0);
}


void main()
{

    float depthval = texture2D(depthTex,gl_FragCoord.xy*invWindow ).r;
    vec3 outPosition = WorldPosFromDepth(gl_FragCoord.xy*invWindow,depthval);

    vec4 norm_rough_ao = texture(normalTex, gl_FragCoord.xy*invWindow );


    vec3 N = (vec4(decodeNormal(norm_rough_ao.xy),0)).xyz;

    vec3 di = sphere.xyz - outPosition;
    float ldi = length(di);
    vec3 V = di/ldi;

//     const float epsilon = -0.5; *when_gt(dot(di,di),(sphere.w + epsilon)*(sphere.w + epsilon))
    ao_output = sphOcclusion(outPosition,N,sphere)*when_gt(dot(N,V),0.0)*0.65;//clamp(100.0/distance(spherePos,outPosition),0,100.0);

}
