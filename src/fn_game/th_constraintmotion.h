#pragma once

#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_time.h"
/*
 * CONSTRAIN BASED MOTION PLAN FOR TRICOLUMN
 *
 *
 * TODO fix random convergence
 */


typedef struct
{
    fn_vec3 center_pos;
    float* leg_lengths;//times 3
    bool* leg_grounded;

    fn_vec3* foot_directions;//times 3

    fn_vec3* leg_directions;

}th_MotionState;

void th_deepCopyMotionState(th_MotionState* dest,th_MotionState* src);

fn_vec3 th_getContactPointMotion(th_MotionState* state,int index,fn_vec3* end_joint);

th_MotionState* th_constraintPlan(th_Allocator* alloc,fn_vec3 A,fn_vec3 B,th_World* world,int* out_states,th_MotionState* pre_state,int step_idx);

th_MotionState* th_constraintPlanCourse(th_Allocator* alloc,fn_vec3* course,int num_course,th_World* world,int* out_states);
