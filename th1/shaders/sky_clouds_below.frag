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
uniform float cloud_enable = 0.0;
//6378e3
uniform float horizon_height = 6372e3;

uniform vec3 uMoonPos = vec3(0.4,1.0,0.7);

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

// // Simple 3D noise function for clouds
// float hash(vec3 p) {
//     p = fract(p * 0.3183099 + 0.1);
//     p *= 17.0;
//     return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
// }
//
// float noise(vec3 x) {
//     vec3 p = floor(x);
//     vec3 f = fract(x);
//     f = f * f * (3.0 - 2.0 * f);
//
//     return mix(
//         mix(mix(hash(p + vec3(0,0,0)), hash(p + vec3(1,0,0)), f.x),
//             mix(hash(p + vec3(0,1,0)), hash(p + vec3(1,1,0)), f.x), f.y),
//                mix(mix(hash(p + vec3(0,0,1)), hash(p + vec3(1,0,1)), f.x),
//                    mix(hash(p + vec3(0,1,1)), hash(p + vec3(1,1,1)), f.x), f.y),
//                f.z);
// }
//
// float fbm(vec3 p) {
//     float f = 0.0;
//     f += 0.5000 * noise(p); p *= 2.01;
//     f += 0.2500 * noise(p); p *= 2.02;
//     f += 0.1250 * noise(p); p *= 2.03;
//     f += 0.0625 * noise(p);
//     return f;
// }

// Improved hash function
vec3 hash3(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return fract(sin(p) * 43758.5453123);
}

// Perlin-style noise with smoother gradients
float perlinNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);

    // Quintic interpolation for smoother results
    vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    return mix(
        mix(mix(dot(hash3(i + vec3(0,0,0)) - 0.5, f - vec3(0,0,0)),
                dot(hash3(i + vec3(1,0,0)) - 0.5, f - vec3(1,0,0)), u.x),
            mix(dot(hash3(i + vec3(0,1,0)) - 0.5, f - vec3(0,1,0)),
                dot(hash3(i + vec3(1,1,0)) - 0.5, f - vec3(1,1,0)), u.x), u.y),
               mix(mix(dot(hash3(i + vec3(0,0,1)) - 0.5, f - vec3(0,0,1)),
                       dot(hash3(i + vec3(1,0,1)) - 0.5, f - vec3(1,0,1)), u.x),
                   mix(dot(hash3(i + vec3(0,1,1)) - 0.5, f - vec3(0,1,1)),
                       dot(hash3(i + vec3(1,1,1)) - 0.5, f - vec3(1,1,1)), u.x), u.y), u.z);
}

// Worley/Voronoi noise for more cellular cloud structure
float worleyNoise(vec3 p) {
    vec3 id = floor(p);
    vec3 fd = fract(p);

    float minDist = 1.0;

    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            for(int z = -1; z <= 1; z++) {
                vec3 coord = vec3(x, y, z);
                vec3 cellId = id + coord;
                vec3 cellPoint = hash3(cellId);

                vec3 diff = coord + cellPoint - fd;
                float dist = length(diff);
                minDist = min(minDist, dist);
            }
        }
    }

    return minDist;
}

// Enhanced FBM combining Perlin and Worley for realistic clouds
float fbm(vec3 p) {
    float f = 0.0;
    float amp = 0.5;

    // Perlin layers for overall shape
    for(int i = 0; i < 4; i++) {
        f += amp * perlinNoise(p);
        p *= 2.03;
        amp *= 0.5;
    }

    // Add Worley noise for wispy details
    f += 0.05 * (1.0 - worleyNoise(p * 0.5));

    return f * 0.5 + 0.5; // Normalize to 0-1
}

vec4 renderClouds(vec3 r0, vec3 rd, vec3 pSun) {
    float cloudAltitude = 6373e3; // 2km above surface (6371e3 + 2e3)
    float cloudThickness = 3e3; // 3km thick cloud layer

    // Check intersection with cloud layer
    vec2 cloudHit = rsi(r0, rd, cloudAltitude + cloudThickness);
    vec2 cloudBase = rsi(r0, rd, cloudAltitude);

    if (cloudHit.x > cloudHit.y) return vec4(0.0);

    // Get entry and exit points
    float tEntry = max(cloudHit.x, cloudBase.x);
    float tExit = min(cloudHit.y, cloudBase.y);

    if (tEntry > tExit || tExit < 0.0) return vec4(0.0);

    // Sample point in cloud layer
    vec3 samplePos = r0 + rd * (tEntry + tExit) * 0.5;

    // Normalize to sphere surface for texture coordinates
    vec3 spherePos = normalize(samplePos) * cloudAltitude;



    //vec3 spherePos = vec3(samplePos.x,cloudAltitude,samplePos.z);
    //spherePos.y = cloudAltitude;



    // Generate cloud density using noise
    float density_scale = normalize(samplePos).y;
    float cloudNoise = fbm(spherePos * 0.0000008 *density_scale ); // Scale for cloud pattern size


    // Threshold and shape the clouds
    float cloudDensity = smoothstep(0.5, 0.6, cloudNoise);
    cloudDensity *= smoothstep(0.0, 0.1, cloudNoise) * smoothstep(1.0, 0.9, cloudNoise);

    // Calculate lighting
    vec3 sunDir = normalize(pSun);
    float sunDot = dot(normalize(samplePos), sunDir);
    float lighting = max(0.3, sunDot * 0.7 + 0.3); // Ambient + diffuse

    // Cloud color
    vec3 cloudColor = vec3(1.0, 1.0, 1.0) * lighting;
    float cloudAlpha = cloudDensity * 0.8;

    return vec4(cloudColor, cloudAlpha);
}

float hash1(float n) {
    return fract(sin(n) * 43758.5453123);
}
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
    vec3 moonDir = normalize(uMoonPos);
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

        float crescent2 = (dot(normalize(moonDir + vec3(0.2)),dir) - cos(radius + 0.01))/(1 - cos(radius + 0.01));
        crescent2 = clamp(crescent2,0.0,1.0);

        //         float crescent2 = (dot(moonDir,dir) - cos(radius + 0.01))/(1 - cos(radius  + 0.01));
        //         crescent2 = clamp(crescent2,0.0,1.0);
        //color *= crescent;


        return vec4(color*crescent*crescent2 ,1.0);
    }

    return vec4(0.0);
}

//how to use moon
// vec4 mval = moon(dir);
// col = mix(col,mval.xyz,mval.w);

vec3 atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
    // Normalize the sun and view directions.
    pSun = normalize(pSun);
    r = normalize(r);

    // Calculate the step size of the primary ray.
    vec2 p = rsi(r0, r, rAtmos);
    if (p.x > p.y) return vec3(0,0,0);
    p.y = min(p.y, rsi(r0, r, rPlanet).x);
    float iStepSize = (p.y - p.x) / float(iSteps);

    // Initialize the primary ray time.
    float iTime = 0.0;

    // Initialize accumulators for Rayleigh and Mie scattering.
    vec3 totalRlh = vec3(0,0,0);
    vec3 totalMie = vec3(0,0,0);

    // Initialize optical depth accumulators for the primary ray.
    float iOdRlh = 0.0;
    float iOdMie = 0.0;

    // Calculate the Rayleigh and Mie phases.
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    // Sample the primary ray.
    for (int i = 0; i < iSteps; i++) {

        // Calculate the primary ray sample position.
        vec3 iPos = r0 + r * (iTime + iStepSize * 0.5);

        // Calculate the height of the sample.
        float iHeight = length(iPos) - rPlanet;

        // Calculate the optical depth of the Rayleigh and Mie scattering for this step.
        float odStepRlh = exp(-iHeight / shRlh) * iStepSize;
        float odStepMie = exp(-iHeight / shMie) * iStepSize;

        // Accumulate optical depth.
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        // Calculate the step size of the secondary ray.
        float jStepSize = rsi(iPos, pSun, rAtmos).y / float(jSteps);

        // Initialize the secondary ray time.
        float jTime = 0.0;

        // Initialize optical depth accumulators for the secondary ray.
        float jOdRlh = 0.0;
        float jOdMie = 0.0;

        // Sample the secondary ray.
        for (int j = 0; j < jSteps; j++) {

            // Calculate the secondary ray sample position.
            vec3 jPos = iPos + pSun * (jTime + jStepSize * 0.5);

            // Calculate the height of the sample.
            float jHeight = length(jPos) - rPlanet;

            // Accumulate the optical depth.
            jOdRlh += exp(-jHeight / shRlh) * jStepSize;
            jOdMie += exp(-jHeight / shMie) * jStepSize;

            // Increment the secondary ray time.
            jTime += jStepSize;
        }

        // Calculate attenuation.
        vec3 attn = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

        // Accumulate scattering.
        totalRlh += odStepRlh * attn;
        totalMie += odStepMie * attn;

        // Increment the primary ray time.
        iTime += iStepSize;

    }

    // Calculate and return the final color.
    return iSun * sun_color * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
}





// Physically-based moon with atmospheric scattering
vec3 atmosphereWithMoon(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
    // Get moon color first
    vec4 moonColor = moon(r);

    // Calculate base atmospheric scattering
    vec3 atmoColor = atmosphere(r, r0, pSun, iSun, rPlanet, rAtmos, kRlh, kMie, shRlh, shMie, g);

    // If moon is not visible, just return atmosphere
    if(moonColor.w <= 0.0) {
        return atmoColor;
    }

    // Moon is visible - apply atmospheric extinction
    r = normalize(r);
    vec2 p = rsi(r0, r, rAtmos);

    // Calculate optical depth through atmosphere to moon
    // (moon is far away, so we use full atmospheric path)
    float pathLength = p.y;

    // Sample optical depth at a few points along the ray
    float totalOdRlh = 0.0;
    float totalOdMie = 0.0;
    int numSamples = 32;

    for(int i = 0; i < numSamples; i++) {
        float t = pathLength * (float(i) + 0.5) / float(numSamples);
        vec3 pos = r0 + r * t;
        float h = length(pos) - rPlanet;

        totalOdRlh += exp(-h / shRlh);
        totalOdMie += exp(-h / shMie);
    }

    totalOdRlh *= pathLength / float(numSamples);
    totalOdMie *= pathLength / float(numSamples);

    // Calculate extinction (light absorbed/scattered away)
    vec3 extinction = exp(-(kRlh * totalOdRlh + kMie * totalOdMie));

    // Apply extinction to moon
    vec3 moonExtincted = moonColor.rgb * extinction*0.2;

    // Add in-scattered atmospheric light in front of moon
    // This is what gives the "washed out" blue tint
    vec3 finalColor = moonExtincted + atmoColor;

    return finalColor;
}

// Usage:
// vec3 skyColor = atmosphereWithMoon(rayDir, cameraPos, sunPos, sunIntensity,
//                                     planetRadius, atmosphereRadius,
//                                     rayleighCoeff, mieCoeff,
//                                     rayleighScaleHeight, mieScaleHeight, g);

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
    vec2 normcoord = outTexcoord*vec2(2) - vec2(1);
    vec3 dir = normalize(vec3(0,0,1) + vec3(0,-1,0)*normcoord.y + vec3(-1,0,0)*normcoord.x);
    dir = vec3(vec4(dir,0)*viewMat);

    // Render atmosphere
    vec3 color = atmosphereWithMoon(
        dir,           // normalized ray direction
        vec3(0,horizon_height,0),               // ray origin
                            uSunPos,                        // position of the sun
                            sun_intensity,                           // intensity of the sun
                            6371e3,                         // radius of the planet in meters
                            6471e3,                         // radius of the atmosphere in meters
                            rayleigh, // Rayleigh scattering coefficient
                            21e-6,                          // Mie scattering coefficient 21e-6
                            8e3,                            // Rayleigh scale height
                            1.2e3,                          // Mie scale height
                            0.758                           // Mie preferred scattering direction
    );

    // Render clouds
    if (cloud_enable == 1.0)
    {
        vec4 clouds = renderClouds(vec3(0,6378e3,0), dir, uSunPos);

        // Composite clouds over atmosphere
        color = mix(color, clouds.rgb, clouds.a);
    }


    fragColor = vec4(color,1);
}
