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





void main(void)
{
   // float alpha_prog =  outTexcoord.x < alpha_blend ? 1.0 : 0.0;
    float alpha_linear = 1.0 - outTexcoord.x;
    vec2 centerDist = abs(outTexcoord - 0.5);
    float sampled = 1.0 - exp( 7.0* (max(centerDist.x, centerDist.y)*2.0 - 1.0)); // 0 at edges, 1 at center
    sampled = clamp(sampled, 0.0, 1.0);

    if (alpha_blend == 1.0)
    {
        sampled = 1.0;
    }

    vec3 textColor = tint;

    // Compute fade factor based on distance from center
    //     vec2 centerDist = abs(outTexcoord - 0.5);
    const float tscale = 0.4;
//     if (isDithered((gl_FragCoord.xy + vec2(sin(time*0.1*0.05*tscale)*5.0,-time*0.5*0.3*tscale).yx)/2.0,sqrt(1.0 - outTexcoord.x)) < 0)
//     {
//         fragColor = vec4(vec3(1) ,sampled);
//     }
//     else
//     {
//         fragColor = vec4(oklabMix(vec3(1),textColor,sqrt(1.0 - outTexcoord.x)) ,sampled);
//     }
    fragColor = vec4(tint ,sampled*alpha_linear);


    //fragColor = vec4(textColor,1.0);
}
