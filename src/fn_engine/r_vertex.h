#pragma once
#include "../fn_math/fn_vec2.h"
#include "../fn_math/fn_vec3.h"
typedef struct
{
    fn_vec3 position;
    fn_vec2 texCoord;
    fn_vec3 normal;
}fn_Vertex;

typedef struct
{
    fn_vec3 position;
    fn_vec2 texCoord;
    fn_vec3 normal;
    fn_vec3 position2;
    fn_vec3 normal2;
}fn_AnimVertex;
