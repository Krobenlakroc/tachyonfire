#version 450 core
layout(location = 0) in vec3 inPosition;
layout(location = 1)in vec2 inTexcoord;

out vec2 outTexcoord;

uniform mat4 modelViewprojection;

uniform float time = 0.0;

uniform float magnitude = 35.0;

void main()
{

    vec4 p = vec4(inPosition.xyz ,1.0);
    p.x = p.x + sin(time*0.01*0.2 + 3.14159*0.3*inTexcoord.y)*magnitude;

    gl_Position = (modelViewprojection*p);
    outTexcoord = vec2(inTexcoord.x,1 -inTexcoord.y);
}
