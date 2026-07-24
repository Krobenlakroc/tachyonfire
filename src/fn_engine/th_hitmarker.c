#include "th_hitmarker.h"
#include "../fn_math/fn_common.h"
#include <string.h>
#include <stdio.h>

#define TH_MAX_HITMARKERS 12
#define TH_HITMARKER_FADE 250.0

static th_Hitmarker* local_hitmarkers = NULL;
static th_PointLight* local_pointlights = NULL;

static fn_vec3 eyepos_frame;
static fn_vec4* frustum_planes_frame = NULL;

static pthread_mutex_t hitmarker_lock = PTHREAD_MUTEX_INITIALIZER;

static int fn_sphereInFrustum( fn_vec3 pos, float radius ,fn_vec4 frustum [6])
{
    int p;

    bool res = true;

    for( p = 0; p < 6; p++ )
    {

        if (frustum[p].x * pos.x + frustum[p].y * pos.y + frustum[p].z * pos.z + frustum[p].w <= -radius)
        {
            res = false;
            p = 6;
        }

    }


    if (!res)
        return 0;
    return 1;//d + radius;
}

void th_resetHitmarkers()
{
    for (int i = 0 ; i < TH_MAX_HITMARKERS;i++)
    {
        local_hitmarkers[i].alive_ref = NULL;
        local_hitmarkers[i].entity_pos_ref = NULL;
        local_hitmarkers[i].entity_transform_ref = NULL;
        local_hitmarkers[i].timer = -100000;
        local_hitmarkers[i].last_good_pos = fn_createVec3s(-1000000000.0);
        local_hitmarkers[i].is_alive = false;
        local_hitmarkers[i].radius = 1.0;
    }
}


void th_initHitmarkers()
{
    if (local_hitmarkers == NULL)
    {
        local_hitmarkers = malloc(sizeof(th_Hitmarker)*TH_MAX_HITMARKERS);
        local_pointlights = malloc(sizeof(th_PointLight)*TH_MAX_HITMARKERS);
    }

    th_resetHitmarkers();

    frustum_planes_frame = malloc(sizeof(fn_vec4)*6);
    frustum_planes_frame[0] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[1] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[2] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[3] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[4] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[5] = fn_createVec4(0,0,0,0);
    eyepos_frame = fn_createVec3(0,0,0);

}

void th_beginHitmarkerFrame(fn_vec3 eyepos,fn_vec4* frustum_planes)
{
    eyepos_frame = eyepos;
    memcpy(frustum_planes_frame,frustum_planes,sizeof(fn_vec4)*6);
}

static fn_vec3 hitmarkerPos(th_Hitmarker h)
{
    if (h.entity_transform_ref != NULL && h.entity_pos_ref == NULL)
    {
        h.last_good_pos = fn_getTranslationMat4(*h.entity_transform_ref);
    }
    else if (h.entity_transform_ref == NULL && h.entity_pos_ref != NULL)
    {
        h.last_good_pos = *h.entity_pos_ref;
    }

    return h.last_good_pos;
}

static bool sameref(th_Hitmarker h,th_Hitmarker hb)
{
    if (h.entity_transform_ref != NULL && h.entity_pos_ref == NULL && h.entity_transform_ref == hb.entity_transform_ref )
    {
        return true;
    }
    else if (h.entity_transform_ref == NULL && h.entity_pos_ref != NULL && h.entity_pos_ref == hb.entity_pos_ref )
    {
        return true;
    }
    else if (h.alive_ref != NULL && h.alive_ref == hb.alive_ref )
    {
        return true;
    }

    return false;
}

void th_pushHitmarker(th_Hitmarker h)
{
    pthread_mutex_lock(&hitmarker_lock);
    if (h.entity_transform_ref != NULL && h.entity_pos_ref == NULL)
    {
        h.last_good_pos = fn_getTranslationMat4(*h.entity_transform_ref);
    }
    else if (h.entity_transform_ref == NULL && h.entity_pos_ref != NULL)
    {
        h.last_good_pos = *h.entity_pos_ref;
    }

    if (!fn_sphereInFrustum(h.last_good_pos,h.radius,frustum_planes_frame))
    {
        pthread_mutex_unlock(&hitmarker_lock);
        return;
    }

    int idx_replace = 0;
    for (int i = 0 ; i < TH_MAX_HITMARKERS;i++)
    {
        if (!local_hitmarkers[i].is_alive || (local_hitmarkers[i].is_alive && sameref(local_hitmarkers[i],h)))
        {
            local_hitmarkers[i] = h;
            pthread_mutex_unlock(&hitmarker_lock);
            return;
        }

        if (local_hitmarkers[idx_replace].timer > local_hitmarkers[i].timer )
        {
            idx_replace = i;
        }
    }

    local_hitmarkers[idx_replace] = h;
    pthread_mutex_unlock(&hitmarker_lock);
}

th_PointLight* th_getHitmarkerLights(int* count)
{
    int rcount = 0;
    for (int i = 0 ; i < TH_MAX_HITMARKERS;i++)
    {
        if (th_time() > local_hitmarkers[i].timer + TH_HITMARKER_FADE)
        {
            local_hitmarkers[i].is_alive = false;
        }

        if (local_hitmarkers[i].is_alive)
        {
            fn_vec3 pos = local_hitmarkers[i].last_good_pos;
            if ( local_hitmarkers[i].alive_ref != NULL )
            {
                if (*local_hitmarkers[i].alive_ref)
                {
                    pos =  hitmarkerPos(local_hitmarkers[i]);
                }
            }
            float alpha_color = (th_time() - local_hitmarkers[i].timer)/TH_HITMARKER_FADE;
            alpha_color = 1.0 - fn_clamp(alpha_color,0.0,1.0);

            fn_vec3 color = th_computeBlackBody(alpha_color,3000.0);

            local_pointlights[rcount].pos = fn_createVec4Vec3(pos,0.0);
            local_pointlights[rcount].color = fn_createVec4Vec3(color,0.0);
            local_pointlights[rcount].lightmat = fn_identityMat4();
            local_pointlights[rcount].shadowindex = fn_createVec4(0.0,0.0,0.0,0.0);
            local_pointlights[rcount].pos2 = fn_createVec4(0.0,0.0,0.0,2.0 + local_hitmarkers[i].radius);

            rcount++;
        }
    }


    *count = rcount;
    return local_pointlights;
}
