#pragma once
#include "../fn_math/fn_common.h"
#include "th_physics.h"

void th_initOcclusion();

void th_beginOccluderFrame(fn_vec3 eyepos,fn_vec4* frustum_planes);

void th_pushOccluderFrame(fn_vec4 occluder); //add a single occluder to the frame

void th_getOccluders(fn_vec4* dest,int* count);
