#version 460 core
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inTransform;




uniform mat4 modelViewprojection;
out vec4 sphere;

void main()
{
    float rad = inTransform.w/sqrt(0.01);
    vec4 p = vec4(inPosition.xyz*rad + inTransform.xyz ,1.0);

    gl_Position = (modelViewprojection*p);

    sphere = inTransform;

}
