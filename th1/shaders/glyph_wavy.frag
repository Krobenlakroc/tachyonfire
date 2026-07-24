#version 450 core
in vec2 outTexcoord;

layout (location = 0) out vec4 fragColor;
layout(binding=35) uniform sampler2D text;
layout(binding=34) uniform sampler2DArray colorTex;

uniform vec3 textColor = vec3(1,1,1);

uniform float time = 0.0;


const float DITHER_THRESHOLDS[16] =
{
    1.0 / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
    13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
    4.0 / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
    16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
};

float isDithered(vec2 pos, float alpha) {


    int ix = int(mod(pos.x, 4.0));
    int iy = int(mod(pos.y, 4.0));

    int index = (int(ix) % 4) * 4 + int(iy) % 4;
    return alpha - DITHER_THRESHOLDS[index];
}

void main(void)
{
    //  vec4 inColor = texture(colorTex,vec3(outTexcoord ,0.0));//- 0.5*outTexcoord.x
    float sampled = texture(text, outTexcoord).r;

    if (textColor.x == 0.0 &&textColor.y == 0.0 && textColor.z == 0.0)
    {
        fragColor = vec4(vec3(0) ,sampled);
    }
    else if (isDithered((gl_FragCoord.xy + vec2(sin(time*0.1*0.05)*5.0,-time*0.1*0.3))/3.0,outTexcoord.y) < 0.0)
    {
        fragColor = vec4(vec3(1) ,sampled);
    }
    else
    {
        fragColor = vec4(mix(vec3(1),textColor,outTexcoord.y) ,sampled);
    }



}
