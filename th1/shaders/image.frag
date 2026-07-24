#version 450 core
in vec2 outTexcoord;

layout (location = 0) out vec4 fragColor;
layout(binding=35) uniform sampler2D text;

uniform float alpha_blend = 1.0;


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


void main(void)
{
    vec4 sampled = texture(text, outTexcoord);

    // Compute fade factor based on distance from center
    vec2 centerDist = abs(outTexcoord - 0.5);
    float edgeFade = 1.0 - exp( 7.0* (max(centerDist.x, centerDist.y)*2.0 - 1.0)); // 0 at edges, 1 at center
    edgeFade = clamp(edgeFade, 0.0, 1.0);


//     float threshold = bayerDither4x4(outTexcoord*50.0);
//     if(edgeFade < threshold)
//         discard; // Stipple fade
    if (isDithered(gl_FragCoord.xy,edgeFade) < 0)
    {
        discard;
    }

    fragColor = vec4(sampled.xyz,edgeFade*alpha_blend*sampled.a);
}
