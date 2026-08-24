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
uniform float horizon_height = 6372e3;

uniform vec3 uMoonPos = vec3(0.4,1.0,0.7);

const float cloud_dens = 0.8;

const float noise_lower = 0.5;
const float noise_higher = 0.6;

float gSeed = 0.0;


vec2 rsi(vec3 r0, vec3 rd, float sr) {
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

// Improved hash function
vec3 hash3(vec3 p) {

    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));



    p += vec3(
        gSeed,
        gSeed * 1.61803398875,
        gSeed * 2.41421356237
    );

    return fract(sin(p) * 43758.5453123);
}

float perlinNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
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

float fbm(vec3 p) {
    float f = 0.0;
    float amp = 0.5;
    for(int i = 0; i < 40; i++) {
        f += amp * perlinNoise(p);
        p *= 2.03;
        amp *= 0.5;
    }
    f += 0.05 * (1.0 - worleyNoise(p * 0.5));
    return f * 0.5 + 0.5;
}

// ---------------------------------------------------------------------------
// Henyey-Greenstein phase function.
// g=0 is isotropic; g close to 1 is strongly forward-scattering.
// Real water-droplet clouds have g ≈ 0.85–0.90.
// ---------------------------------------------------------------------------
float henyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// ---------------------------------------------------------------------------
// Dual-lobe phase function for clouds.
// Forward lobe (g1 ≈ 0.88) models Mie scattering off droplets.
// Backward lobe (g2 ≈ -0.2) adds a subtle silver lining / back-scatter.
// ---------------------------------------------------------------------------
float cloudPhase(float cosTheta) {
    float forward  = henyeyGreenstein(cosTheta, 0.88);
    float backward = henyeyGreenstein(cosTheta, -0.20);
    return mix(forward, backward, 0.10);
}

// ---------------------------------------------------------------------------
// Beer-Powder approximation for multiple scattering inside the cloud.
// Without this, transmittance alone makes clouds look like smoke.
// The "powder sugar" term darkens the interior where optical depth is high,
// giving the realistic dark grey underbelly.
// ---------------------------------------------------------------------------
float beerPowder(float opticalDepth) {
    float beer   = exp(-opticalDepth);
    float powder = 1.0 - exp(-opticalDepth * 2.0);
    return beer * powder * 2.0;
}

// ---------------------------------------------------------------------------
// Shadow transmittance toward the sun from a point inside the cloud.
// We march a short ray toward uSunPos and accumulate density.
// ---------------------------------------------------------------------------
float cloudShadowTransmittance(vec3 pos, vec3 sunDir,
                                float cloudBase, float cloudTop,int iters_noise) {
    const int numSteps = 12;
    float stepLen = (cloudTop - cloudBase) / float(numSteps);
    float optDepth = 0.0;


    for (int i = 0; i < numSteps; i++) {
        vec3 samplePos = pos + sunDir * (float(i) + 0.5) * stepLen;
        float r = length(samplePos);
        //if (r < cloudBase || r > cloudTop) continue;

//         // Project onto sphere surface for noise lookup
//         vec3 spherePos = normalize(samplePos) * cloudBase;
//         float n = fbm(spherePos * noiseScale);
//         float density = smoothstep(0.50, 0.60, n);
//         optDepth += density * stepLen * 1e-4; // scale to plausible optical depth

        vec3 spherePos = normalize(samplePos) * cloudBase;

        float density_scale = (normalize(samplePos).y) ;
//         float cloudNoise = fbm(spherePos * 0.0000008 *density_scale*cloud_dens ); // Scale for cloud pattern size
//
//
//         // Threshold and shape the clouds
//         float cloudDensity = smoothstep(noise_lower, noise_higher, cloudNoise);
//         cloudDensity *= smoothstep(0.0, 0.1, cloudNoise) * smoothstep(1.0, 0.9, cloudNoise);
        float cloudDensity = 0.0;

        for (int i = 0 ; i < iters_noise;i++)
        {
            gSeed = (float(i)/float(iters_noise))*128.127277538;
            float cloudNoise = fbm(spherePos * 0.0000008 *density_scale*cloud_dens ); // Scale for cloud pattern size
            float cloudDensityl = smoothstep(noise_lower, noise_higher, cloudNoise);
            cloudDensityl *= smoothstep(0.0, 0.1, cloudNoise) * smoothstep(1.0, 0.9, cloudNoise);

            cloudDensity += cloudDensityl*(1.0/float(iters_noise));
        }

        optDepth += max(cloudDensity,0.0) * stepLen * 0.001;//0.001;
    }

    return exp(-optDepth);
}

// ---------------------------------------------------------------------------
// renderClouds – now correctly places clouds on the TOP of the sky dome.
//
// The key fix is in the entry/exit logic:
//   • rsi with cloudTopR gives hits on the outer (top) surface of the slab.
//   • rsi with cloudBaseR gives hits on the inner (bottom) surface.
//   • For an observer inside the planet (r0.y ≈ 6372e3) looking upward:
//       - The ray first exits the inner sphere at cloudBase.x (positive t).
//       - The ray later exits the outer sphere at cloudTop.y (positive t).
//   • So tEntry = cloudBase.x, tExit = cloudTop.y  →  correct top-of-sky slab.
// ---------------------------------------------------------------------------
vec4 renderClouds(vec3 r0, vec3 rd, vec3 pSun,float thickness,int iters_noise,vec3 mirror_color) {

    if (rd.y < -0.01) return vec4(0.0);

    float cloudAltitude = 6373e3; // 2km above surface (6371e3 + 2e3)
    float cloudThickness = thickness; // 3km thick cloud layer

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
    float density_scale = (normalize(samplePos).y) ;



    // Threshold and shape the clouds
//     float cloudDensity = smoothstep(noise_lower, noise_higher, cloudNoise);
//     cloudDensity *= smoothstep(0.0, 0.1, cloudNoise) * smoothstep(1.0, 0.9, cloudNoise);
    float cloudDensity = 0.0;

    for (int i = 0 ; i < iters_noise;i++)
    {
        gSeed = (float(i)/float(iters_noise))*128.127277538;
        float cloudNoise = fbm(spherePos * 0.0000008 *density_scale*cloud_dens ); // Scale for cloud pattern size
        float cloudDensityl = smoothstep(noise_lower, noise_higher, cloudNoise);
        cloudDensityl *= smoothstep(0.0, 0.1, cloudNoise) * smoothstep(1.0, 0.9, cloudNoise);

        cloudDensity += cloudDensityl*(1.0/float(iters_noise));
    }

    float sun_factor = (1.0/float(iters_noise));



    if (cloudDensity < 0.001) return vec4(0.0);

    // Calculate lighting
//     vec3 sunDir = normalize(pSun);
//     float sunDot = dot(normalize(samplePos), sunDir);
//     float lighting = max(0.3, sunDot * 0.7 + 0.3); // Ambient + diffuse
//
//     // Cloud color
//     vec3 cloudColor = vec3(1.0, 1.0, 1.0) * lighting;
//     float cloudAlpha = cloudDensity * 0.8;
//
//     return vec4(cloudColor, cloudAlpha);


    // ---------------------------------------------------------------------------
    // Physically-plausible lighting
    // ---------------------------------------------------------------------------
    vec3 sunDir = normalize(pSun);

    // Cosine of angle between view ray and sun direction (for phase function).
    float cosTheta = dot(normalize(rd), sunDir);

    // Phase function: controls how bright the cloud looks at this viewing angle
    // relative to the sun. Strongly forward-scattering → bright silver lining.
    float phase = cloudPhase(cosTheta);

    // Shadow: how much sunlight reaches this sample point from above.
    float shadowT = cloudShadowTransmittance(
        samplePos, sunDir,
        cloudAltitude, cloudAltitude + cloudThickness,
        iters_noise
    );

    //shadowT = 1.0;

    // Optical depth of this cloud sample along the view ray.
    float slabThickness = tExit - tEntry; // meters of cloud path
    float opticalDepth  = clamp(cloudDensity * slabThickness * 10.0,0.0,1.0);

    // Beer-Powder combines Beer's law extinction with a powder sugar term.
    // This brightens forward-lit regions and darkens the unlit interior.
    float beerP = beerPowder(opticalDepth);

    // Sun contribution: direct illumination modulated by phase and shadow.
    float sunZenith = max(0.0, dot(sunDir, vec3(0,1,0)));

    vec3  sunContrib = sun_color  * sun_intensity * phase * shadowT * beerP * sunZenith * 0.8 * sun_factor;

    // Ambient sky contribution: a fraction of Rayleigh sky colour leaks into
    // unlit parts (simulates multiple scattering). Use a plausible sky blue.
    // Scale the sun intensity so ambient tracks time-of-day brightness.
    vec3  ambientSky  = normalize(rayleigh) * sun_intensity * 0.8 * (0.5 + 0.5 * sunZenith);
    ambientSky = mix(ambientSky,vec3(1,1,1),0.5);

    // Cloud base darkening: underlit region gets a warmer, darker hue.
    float baseDark   = 1.0 - shadowT * 0.7;
    vec3  baseColor  = shadowT*(ambientSky*0.4 + mirror_color*0.6);//vec3(0.6, 0.55, 0.5) * baseDark;

    // Final cloud colour.
    vec3 cloudColor =  sunContrib + baseColor;

    // Alpha: how opaque this cloud patch is to the sky behind it.
    //float alpha = smoothstep(0.0, 0.3, cloudDensity) * 0.92;

     float cloudAlpha = shadowT * cloudDensity * 0.8;

    return vec4(cloudColor, cloudAlpha);
}

vec4 renderCloudsBelow(vec3 r0, vec3 rd, vec3 pSun) {
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
    vec3 moonDir = normalize(uMoonPos);
    float d = dot(dir, moonDir);
    float radius = 0.1;
    float disc = smoothstep(radius, radius - 0.002, acos(d));

    if(disc > 0.0) {
        vec3 tangent   = normalize(cross(moonDir, vec3(0.0, 1.0, 0.0)));
        vec3 bitangent = cross(moonDir, tangent);
        vec2 uv = vec2(dot(dir, tangent), dot(dir, bitangent)) / radius;

        float large = fbm2d(uv*2.0, 25);
        large = smoothstep(0.4, 0.7, large);
        float small = fbm2d(uv*17.0, 4);
        small = smoothstep(0.4, 0.8, small);
        float craterMask = mix(small, large, 0.8);

        vec3 highlands = vec3(0.78,0.76,0.7)*0.5;
        vec3 maria     = vec3(0.3,0.3,0.35)*0.1;
        vec3 color     = mix(highlands, maria, craterMask);

        float crescent  = (dot(moonDir,dir) - cos(radius + 0.01))/(1.0 - cos(radius + 0.01));
        crescent = clamp(crescent, 0.0, 1.0);
        float crescent2 = (dot(normalize(moonDir + vec3(0.2)),dir) - cos(radius + 0.01))/(1.0 - cos(radius + 0.01));
        crescent2 = clamp(crescent2, 0.0, 1.0);

        return vec4(color * crescent * crescent2, 1.0);
    }

    return vec4(0.0);
}

vec3 atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
    pSun = normalize(pSun);
    r    = normalize(r);

    vec2 p = rsi(r0, r, rAtmos);
    if (p.x > p.y) return vec3(0,0,0);
    p.y = min(p.y, rsi(r0, r, rPlanet).x);
    float iStepSize = (p.y - p.x) / float(iSteps);

    float iTime   = 0.0;
    vec3 totalRlh = vec3(0);
    vec3 totalMie = vec3(0);
    float iOdRlh  = 0.0;
    float iOdMie  = 0.0;

    float mu   = dot(r, pSun);
    float mumu = mu * mu;
    float gg   = g * g;
    float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    for (int i = 0; i < iSteps; i++) {
        vec3  iPos     = r0 + r * (iTime + iStepSize * 0.5);
        float iHeight  = length(iPos) - rPlanet;
        float odStepRlh = exp(-iHeight / shRlh) * iStepSize;
        float odStepMie = exp(-iHeight / shMie) * iStepSize;
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        float jStepSize = rsi(iPos, pSun, rAtmos).y / float(jSteps);
        float jTime     = 0.0;
        float jOdRlh    = 0.0;
        float jOdMie    = 0.0;

        for (int j = 0; j < jSteps; j++) {
            vec3  jPos    = iPos + pSun * (jTime + jStepSize * 0.5);
            float jHeight = length(jPos) - rPlanet;
            jOdRlh += exp(-jHeight / shRlh) * jStepSize;
            jOdMie += exp(-jHeight / shMie) * jStepSize;
            jTime  += jStepSize;
        }

        vec3 attn = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));
        totalRlh += odStepRlh * attn;
        totalMie += odStepMie * attn;
        iTime    += iStepSize;
    }

    return iSun * sun_color * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
}

vec3 atmosphereWithMoon(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
    vec4 moonColor = moon(r);
    vec3 atmoColor = atmosphere(r, r0, pSun, iSun, rPlanet, rAtmos, kRlh, kMie, shRlh, shMie, g);

    if(moonColor.w <= 0.0) return atmoColor;

    r = normalize(r);
    vec2 p = rsi(r0, r, rAtmos);
    float pathLength = p.y;

    float totalOdRlh = 0.0;
    float totalOdMie = 0.0;
    int numSamples   = 32;
    for(int i = 0; i < numSamples; i++) {
        float t = pathLength * (float(i) + 0.5) / float(numSamples);
        vec3  pos = r0 + r * t;
        float h   = length(pos) - rPlanet;
        totalOdRlh += exp(-h / shRlh);
        totalOdMie += exp(-h / shMie);
    }
    totalOdRlh *= pathLength / float(numSamples);
    totalOdMie *= pathLength / float(numSamples);

    vec3 extinction = exp(-(kRlh * totalOdRlh + kMie * totalOdMie));

    float gain = 0.2;
    if (cloud_enable == 1.0) //clouds below
    {
        gain = 0.65;
    }

    vec3 moonExtincted = moonColor.rgb * extinction * gain;
    return moonExtincted + atmoColor;
}

void main(void)
{
    vec2 normcoord = outTexcoord * vec2(2) - vec2(1);
    vec3 dir = normalize(vec3(0,0,1) + vec3(0,-1,0)*normcoord.y + vec3(-1,0,0)*normcoord.x);
    dir = vec3(vec4(dir,0)*viewMat);

    vec3 color = atmosphereWithMoon(
        dir,
        vec3(0, horizon_height, 0),
        uSunPos,
        sun_intensity,
        6371e3,
        6471e3,
        rayleigh,
        21e-6,
        8e3,
        1.2e3,
        0.758
    );

    vec3 color_mirrored = atmosphereWithMoon(
        dir*vec3(-1,1,-1),
        vec3(0, horizon_height, 0),
                                    uSunPos,
                                    sun_intensity,
                                    6371e3,
                                    6471e3,
                                    rayleigh,
                                    21e-6,
                                    8e3,
                                    1.2e3,
                                    0.758
    );



    if (cloud_enable == 2.0)
    {
        vec4 clouds = renderClouds(vec3(0, 6372e3, 0), dir, uSunPos,3e3,1,color_mirrored);
        // Clouds composite OVER the atmosphere (they're in the sky, not below it).
        color = mix(color, clouds.rgb, clouds.a);
    }
    if (cloud_enable == 3.0)
    {
        vec4 clouds = renderClouds(vec3(0, 6372e3, 0), dir, uSunPos,0.4e3,10,color_mirrored);
        // Clouds composite OVER the atmosphere (they're in the sky, not below it).
        color = mix(color, clouds.rgb, clouds.a);
    }
    else if (cloud_enable == 1.0)
    {
        vec4 clouds = renderCloudsBelow(vec3(0,6378e3,0), dir, uSunPos);

        // Composite clouds over atmosphere
        color = mix(color, clouds.rgb, clouds.a);
    }

    fragColor = vec4(color, 1.0);
}
