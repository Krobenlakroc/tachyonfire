#version 450 core
layout(early_fragment_tests) in;
in vec2 outTexcoord;

layout (location = 0) out vec4 fragColor;


#define PI 3.141592
#define iSteps 16
#define jSteps 8

uniform vec3 uSunPos;
uniform mat4 viewMat;

uniform vec3 rayleigh = vec3(5.5e-6, 13.0e-6, 22.4e-6);
uniform float sun_intensity = 22;
uniform vec3 sun_color = vec3(1,1,1);

// uniform vec3 rayleigh = vec3(22.4e-6, 13.0e-6, 5.5e-6);
// uniform float sun_intensity = 15;
// uniform vec3 sun_color = vec3(1,0.5,0.5);

vec2 rsi(vec3 r0, vec3 rd, float sr) {
    // ray-sphere intersection that assumes
    // the sphere is centered at the origin.
    // No intersection when result.x > result.y
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b*b) - 4.0*a*c;
    if (d < 0.0) return vec2(1e5,-1e5);
    return vec2(
        (-b - sqrt(d))/(2.0*a),
                (-b + sqrt(d))/(2.0*a)
    );
}

// ------------------------------------------------------------
// Hash / noise helpers
// ------------------------------------------------------------
float hash1(float n) {
    return fract(sin(n) * 43758.5453123);
}

// float hash(vec3 p) {
//     return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
// }

float hash(vec3 p) {
    uvec3 q = uvec3(ivec3(p)) * uvec3(1597334673U, 3812015801U, 2798796415U);
    uint n = (q.x ^ q.y ^ q.z) * 1597334673U;
    return float(n) * (1.0 / float(0xffffffffU));
}

// Cheap value noise
float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash(i + vec3(0,0,0));
    float n100 = hash(i + vec3(1,0,0));
    float n010 = hash(i + vec3(0,1,0));
    float n110 = hash(i + vec3(1,1,0));
    float n001 = hash(i + vec3(0,0,1));
    float n101 = hash(i + vec3(1,0,1));
    float n011 = hash(i + vec3(0,1,1));
    float n111 = hash(i + vec3(1,1,1));

    return mix(
        mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y),
               f.z
    );
}

// Fractal noise
float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p *= 2.1;
        a *= 0.5;
    }
    return v;
}

// ------------------------------------------------------------
// Stars
// ------------------------------------------------------------
float starField(vec3 dir) {
    vec3 p = dir * 300.0; // density control
    float n = hash(floor(p));

    // Only brightest points become stars
    float star = step(0.999, n);

    // Vary brightness
    //star *= mix(0.3, 1.0, hash1(n * 91.7));
    return star;
}

// ------------------------------------------------------------
// Nebula
// ------------------------------------------------------------
vec3 nebula(vec3 dir) {
    float n = fbm(dir * 3.0);
    float n2 = fbm(dir * 8.0);

    float density = smoothstep(0.4, 0.8, n) * 0.7;
    density *= n2;

    vec3 colA = vec3(0.4, 0.04, 0.6)*2;  // purple
    vec3 colB = vec3(0.00, 0.00, 0.7)*4;  // blue

    return mix(colA, colB, n) * density;
}

// ------------------------------------------------------------
// Moon
// ------------------------------------------------------------

// 2D value noise
float noise2d(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);

    float a = hash1(dot(i, vec2(127.1, 311.7)));
    float b = hash1(dot(i + vec2(1.0,0.0), vec2(127.1, 311.7)));
    float c = hash1(dot(i + vec2(0.0,1.0), vec2(127.1, 311.7)));
    float d = hash1(dot(i + vec2(1.0,1.0), vec2(127.1, 311.7)));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Fractal noise for more interesting splotches
float fbm2d(vec2 p, int octaves) {
    float v = 0.0;
    float a = 0.5;
    for(int i=0; i<octaves; i++){
        v += a * noise2d(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

vec4 moon(vec3 dir) {
    vec3 sunDir = normalize(vec3(0,-0.5,1.0));
    vec3 moonDir = normalize(uSunPos);
    float d = dot(dir, moonDir);
    float radius = 0.1;
    float disc = smoothstep(radius, radius - 0.002, acos(d));

    if(disc > 0.0) {
        vec3 tangent = normalize(cross(moonDir, vec3(0.0, 1.0, 0.0)));
        vec3 bitangent = cross(moonDir, tangent);

        vec2 uv = vec2(dot(dir, tangent), dot(dir, bitangent)) / radius;

        float large = fbm2d(uv*2.0, 25);   // scale 2 = large features
        large = smoothstep(0.4, 0.7, large); // threshold for patch shape

        float small = fbm2d(uv*17.0, 4);
        small = smoothstep(0.4, 0.8, small);

        // Combine layers
        float craterMask = mix(small, large, 0.8);

        // Base colors
        vec3 highlands = vec3(0.78,0.76,0.7)*0.5;
        vec3 maria = vec3(0.3,0.3,0.35)*0.1;

        vec3 color = mix(highlands, maria, craterMask);

        //vec3 pointDir = normalize(moonDir + tangent*uv.x + bitangent*uv.y);
        //length(uv-vec2(.92, .88))
        float crescent = (dot(moonDir,dir) - cos(radius + 0.01))/(1 - cos(radius + 0.01));
        crescent = clamp(crescent,0.0,1.0);

        float crescent2 = (dot(normalize(moonDir + vec3(0.02)),dir) - cos(radius + 0.01))/(1 - cos(radius + 0.01));
        crescent2 = clamp(crescent2,0.0,1.0);

//         float crescent2 = (dot(moonDir,dir) - cos(radius + 0.01))/(1 - cos(radius  + 0.01));
//         crescent2 = clamp(crescent2,0.0,1.0);
        //color *= crescent;


        return vec4(color*crescent*crescent2 ,1.0);
    }

    return vec4(0.0);
}


// ------------------------------------------------------------
// Main sky function
// ------------------------------------------------------------
vec3 skyColor(vec3 dir) {
    dir = normalize(dir);

    vec3 col = vec3(0.0,0.0,0.0); // deep space base

    // Nebula
    col += nebula(dir)*0.5;

    // Stars (layered)
    float stars = 0.0;
    stars += starField(dir);
    stars += starField(dir * 1.7) * 0.6;
    stars += starField(dir * 3.1) * 0.3;

    col += vec3(stars)*4.0;



    // Moon
    vec4 mval = moon(dir);
//     if (mval.w > 0.0)
//     {
//         col = mval.xyz;
//     }
    col = mix(col,mval.xyz,mval.w);

    return col;
}

vec3 ray_dir_from_uv(vec2 uv) {
    // float PI = 3.14159265358979;
    vec3 dir;

    float x = sin(PI * uv.y);
    dir.y = cos(PI * uv.y);

    dir.x = x * sin(2.0 * PI * (0.5 - uv.x));
    dir.z = x * cos(2.0 * PI * (0.5 - uv.x));

    return dir;
}

void main(void)
{
    //  vec3 dir = ray_dir_from_uv(vec2(outTexcoord.x, 1- outTexcoord.y));
    vec2 normcoord = outTexcoord*vec2(2) - vec2(1);
    vec3 dir = normalize(vec3(0,0,1) + vec3(0,-1,0)*normcoord.y + vec3(-1,0,0)*normcoord.x);

    mat3 R = mat3(viewMat);

    // Gram-Schmidt fix
    R[0] = normalize(R[0]);
    R[1] = normalize(R[1] - dot(R[1], R[0]) * R[0]);
    R[2] = normalize(cross(R[0], R[1]));

    dir = normalize(R * dir);

//     dir = vec3(vec4(dir,0)*viewMat);
    vec3 color = skyColor(dir);
    //  color = 1.0 - exp(-1.0 * color);
    fragColor = vec4(color,1);
}
