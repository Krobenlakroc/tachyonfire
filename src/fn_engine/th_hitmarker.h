#pragma once
#include "../fn_math/fn_common.h"
#include "th_physics.h"
#include "th_clusters.h"
#include "th_time.h"



typedef struct
{
    fn_mat4* entity_transform_ref;
    fn_vec3* entity_pos_ref;
    bool* alive_ref;
    th_timer_t timer;

    fn_vec3 last_good_pos;
    bool is_alive;
    float radius;
}th_Hitmarker;

void th_initHitmarkers();

void th_resetHitmarkers();//IMPORTANT, PREVENT OLD PTRS BEING REFERENCED

void th_beginHitmarkerFrame(fn_vec3 eyepos,fn_vec4* frustum_planes);

void th_pushHitmarker(th_Hitmarker h);

th_PointLight* th_getHitmarkerLights(int* count);
