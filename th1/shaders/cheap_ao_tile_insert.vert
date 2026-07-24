#version 460 core

layout (std140 , binding = 10) uniform TiledLights
{
    vec4 ao_lights[2048];//view space
};

uniform mat4 Projection;
uniform vec2 resolution;
uniform mat4 view;
uniform float zNear;

out vec4 sphere;
out uint light_id;

void GetProjectedBounds(vec3 center, float radius, inout vec3 boxMin, inout vec3 boxMax)
{




    float d2 = dot(center,center);

    float a = sqrt(max(d2 - radius * radius,0.0));

    /// view-aligned "right" vector (right angle to the view plane from the center of the sphere. Since  "up" is always (0,n,0), replaced cross product with vec3(-c.z, 0, c.x)
    vec3 right = (radius / a) * vec3(-center.z, 0, center.x);
    vec3 up = vec3(0,radius,0);

    vec4 projectedRight  = Projection * vec4(right,0);
    vec4 projectedUp     = Projection * vec4(up,0);

    vec4 projectedCenter = Projection * vec4(center,1);

    vec4 north  = projectedCenter + projectedUp;
    vec4 east   = projectedCenter + projectedRight;
    vec4 south  = projectedCenter - projectedUp;
    vec4 west   = projectedCenter - projectedRight;

    north /= north.w ;
    east  /= east.w  ;
    west  /= west.w  ;
    south /= south.w ;

    boxMin = min(min(min(east,west),north),south).xyz;
    boxMax = max(max(max(east,west),north),south).xyz;

}

void main()
{
    //TODO convert to view space
    vec3 center = (view*vec4(ao_lights[gl_InstanceID].xyz,1.0)).xyz;
    float radius = ao_lights[gl_InstanceID].w/sqrt(0.01);

    sphere = vec4(center,radius);
    light_id = uint(gl_InstanceID);

    vec2 uMin,uMax;

    if (dot(center,center) < radius*radius + 0.001)
    {
       uMin = vec2(-1,-1);
       uMax = vec2(1,1);
    }
    else if (center.z + radius > -zNear)
    {
        uMin = vec2(-1,-1);
        uMax = vec2(1,1);
    }
    else
    {
        vec3 bMin,bMax;
        GetProjectedBounds(center,radius,bMin,bMax);

        uMin = bMin.xy;
        uMax = bMax.xy;

        // One pixel in NDC space, plus one extra pixel of padding for borderline cases
        vec2 pixelSize = 2.0 / resolution;
        vec2 padding = pixelSize * 1.5; // 1.5 pixels: covers the pixel + slop on each side

        vec2 center2D = (uMin + uMax) * 0.5;
        vec2 halfSize = max((uMax - uMin) * 0.5, padding);

        uMin = center2D - halfSize;
        uMax = center2D + halfSize;

        // Expand outward by one pixel on every edge (conservative outward bias)
        uMin -= pixelSize;
        uMax += pixelSize;
    }





//     uMin = vec2(-1,-1);
//     uMax = vec2(1,1);

    // Define the 6 vertices of the quad (two triangles)
    vec2 positions[6] = vec2[](
        vec2(0.0, 0.0),
                               vec2(1.0, 0.0),
                               vec2(1.0, 1.0),

                               vec2(0.0, 0.0),
                               vec2(1.0, 1.0),
                               vec2(0.0, 1.0)
    );

    // Interpolate between min and max
    vec2 pos = mix(uMin, uMax, positions[gl_VertexID]);

    gl_Position = vec4(pos, 0.0, 1.0);

}
