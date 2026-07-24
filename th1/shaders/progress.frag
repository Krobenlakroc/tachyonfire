#version 450 core
in vec2 outTexcoord;
layout (location = 0) out vec4 fragColor;
uniform float alpha_blend = 1.0;
uniform float time = 0.0;
uniform vec3 tint = vec3(1,0,0);
float isDithered(vec2 pos, float alpha) {
    float DITHER_THRESHOLDS[16] =
    {
        1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
        13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
        4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
        16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
    };
    int index = (int(pos.x) % 4) * 4 + int(pos.y) % 4;
    return alpha - DITHER_THRESHOLDS[index];
}
float roundedRectSDF(vec2 p, vec2 center, vec2 size, float r) {
    vec2 pc = abs(p - center);
    vec2 q = max(pc - (size*0.5 - vec2(r)), vec2(0.0));
    return length(q) - r;
}
float insideRoundedRect(vec2 p) {
    float d = roundedRectSDF(p, vec2(0.5), vec2(0.95), 0.12);
    float edgeSmoothness =  0.015;
    return smoothstep(edgeSmoothness, -edgeSmoothness, d);
}
vec3 toLinear(vec3 x) {
    return pow(x, vec3(2.2));
}
vec3 toSRGB(vec3 x) {
    return pow(x, vec3(1.0 / 2.2));
}
float cbrt(float x)
{
    return pow(x,0.3333);
}
vec3 rgbToOKLab(vec3 c) {
    float l = 0.4122214708*c.r + 0.5363325363*c.g + 0.0514459929*c.b;
    float m = 0.2119034982*c.r + 0.6806995451*c.g + 0.1073969566*c.b;
    float s = 0.0883024619*c.r + 0.2817188376*c.g + 0.6299787005*c.b;
    l = cbrt(l);
    m = cbrt(m);
    s = cbrt(s);
    return vec3(
        0.2104542553*l + 0.7936177850*m - 0.0040720468*s,
        1.9779984951*l - 2.4285922050*m + 0.4505937099*s,
        0.0259040371*l + 0.7827717662*m - 0.8086757660*s
    );
}
vec3 okLabToRGB(vec3 c) {
    float l = c.x + 0.3963377774*c.y + 0.2158037573*c.z;
    float m = c.x - 0.1055613458*c.y - 0.0638541728*c.z;
    float s = c.x - 0.0894841775*c.y - 1.2914855480*c.z;
    l = l*l*l;
    m = m*m*m;
    s = s*s*s;
    return vec3(
        +4.0767416621*l - 3.3077115913*m + 0.2309699292*s,
        -1.2684380046*l + 2.6097574011*m - 0.3413193965*s,
        -0.0041960863*l - 0.7034186147*m + 1.7076147010*s
    );
}
vec3 oklabMix(vec3 rgbA, vec3 rgbB, float t) {
    vec3 A = toLinear(rgbA);
    vec3 B = toLinear(rgbB);
    vec3 labA = rgbToOKLab(A);
    vec3 labB = rgbToOKLab(B);
    vec3 lab = mix(labA, labB, t);
    vec3 linOut = okLabToRGB(lab);
    return toSRGB(clamp(linOut, 0.0, 1.0));
}
void main(void)
{
    float alpha_prog =  outTexcoord.x < alpha_blend ? 1.0 : 0.0;
    float sampled = insideRoundedRect(outTexcoord);//*alpha_prog;
    vec3 textColor = tint;
    const float tscale = 0.4;
    vec3 baseColor;
    baseColor = oklabMix(textColor*1.08,textColor*0.6,sqrt(1.0 - outTexcoord.x));
    baseColor = mix(baseColor,vec3(0.8,0.8,0.8),1.0 - alpha_prog);

    float glossTop    = smoothstep(0.40, 1.0, outTexcoord.y) * 0.7;
    float glossBottom = (1.0 - smoothstep(0.0, 0.18, outTexcoord.y)) * 0.08;
    float gloss = glossTop + glossBottom;
    float topRim    = smoothstep(0.90, 1.0, outTexcoord.y) * 0.15;
    float bottomRim = (1.0 - smoothstep(0.0, 0.1, outTexcoord.y)) * 0.10;
    float pt = time*0.1*0.05*tscale;
    float glintPos = fract(outTexcoord.x * 0.5 - pt * 0.1);
    float glint = (1.0 - smoothstep(0.0, 0.06, abs(glintPos - 0.5))) * 0.4;
    vec3 glassColor = baseColor + vec3(gloss + topRim + glint) - vec3(bottomRim);
    glassColor = clamp(glassColor, 0.0, 1.0);


    float edgeDist = roundedRectSDF(outTexcoord, vec2(0.5), vec2(0.95), 0.12);
    const float BORDER_WIDTH = 0.05;

    float borderT = clamp(1.0 - (-edgeDist) / BORDER_WIDTH, 0.0, 1.0);
    float borderMask = 0.0;//smoothstep(0.0, 1.0, borderT) * (1.0 - smoothstep(0.0, BORDER_WIDTH*1.4, abs(edgeDist + BORDER_WIDTH*0.5)));
    borderMask += smoothstep(0.25,0.5,abs(outTexcoord.y - 0.5));
    borderMask += smoothstep(0.4,0.5,abs(outTexcoord.x - 0.5));


    float metalGrad = borderT; // 0 inner .. 1 outer
    vec3 steelDark   = vec3(0.32, 0.35, 0.39);
    vec3 steelMid    = vec3(0.72, 0.76, 0.80);
    vec3 steelBright = vec3(0.97, 0.98, 1.0);
    vec3 steelCool   = vec3(0.55, 0.66, 0.78); // faint aero-blue cast

    vec3 metal = mix(steelDark, steelMid, smoothstep(0.0, 0.5, metalGrad));
    metal = mix(metal, steelBright, smoothstep(0.55, 0.85, metalGrad));
    metal = mix(metal, steelCool, 0.18 * (1.0 - abs(outTexcoord.y - 0.5) * 2.0));


    float metalLight = mix(0.55, 1.25, smoothstep(0.0, 1.0, outTexcoord.y));
    metal *= metalLight;


    float rimAngle = atan(outTexcoord.y - 0.5, outTexcoord.x - 0.5);
    float sweep = sin(rimAngle * 2.0 - time * 0.01) * 0.5 + 0.5;
    metal += vec3(pow(sweep, 6.0) * 0.5);

    metal = clamp(metal, 0.0, 1.0);

    vec3 finalColor = mix(glassColor, metal, borderMask);

    float alpha = mix(min(alpha_prog + 0.2,1.0), 1.0, borderMask)*sampled;

    fragColor = vec4(finalColor, alpha);
}
