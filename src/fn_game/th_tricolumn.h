#pragma once

#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_time.h"


#include "th_constraintmotion.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;

typedef enum
{
    TRICOL_FINDGROUND,
    TRICOL_SPAWN,
    TRICOL_IDLE,
    TRICOL_EXPLORE,
    TRICOL_GIBBED,
}th_TricolumnState;

typedef enum
{
    TRICOL_RETRACT,
    TRICOL_ROTATE,
    TRICOL_EXTEND,
    TRICOL_END,
}th_TricolInterpState;

typedef struct
{
    th_TricolInterpState states[3];
    float lengths[3];
    fn_vec3 old_dir[3];
    fn_vec3 old_dir_foot[3];

    bool done_rotate[3][2];

    float alpha;
    int frame;
    bool reverse;
}th_TricolumnInterpData;

typedef struct
{
    th_TricolumnState state;
    bool set_up;
    fn_vec3 position;
    fn_quat orientation;
    fn_vec3 direction_vector;
    fn_vec3 old_forwards_orient;
    fn_vec3 old_rights_orient;
    fn_vec3 old_ups_orient;

    fn_quat orientation_eyedir;
    fn_vec3 direction_vector_eyedir;
    fn_vec3 old_ups_eyedir;
    fn_vec3 orient_trgt_eyedir;
    th_timer_t orient_eye_timer;
    fn_vec3 orient_trgt_basedir;


    fn_quat col_orientations[3];
    fn_vec3 col_points[3];

    fn_vec3 old_ups[3];
    fn_vec3 old_rights[3];
    fn_vec3 old_forwards[3];



    a_VirtualSource* impact_sounds[7];

    bool hasgem[7];
    float gemhealth[7];

    float gem_jitter_t[7]; //time
    fn_vec3 gem_jitter_x[7]; //direction of oscillation

    th_Entity baseplate_gib;
    th_Entity leg_gib[3][2];

    float fadeout_body;
    fn_mat4 old_body;
    fn_mat4 old_eye;

    float fadeout_legs[3];
    fn_mat4 old_legs[3];

    th_timer_t aggrotimer;
    th_timer_t lightning_hit_timer;
    int hit_counter; // make it so that the player doesnt get BLASTED for a long time
    th_timer_t lightning_cooldown_timer;

    th_timer_t spawntimer;
    th_timer_t spawn_heartbeat;
    int spawn_flip_flop;

    fn_vec3* course;
    int course_count;
    int course_progress;

    a_VirtualSource* spawn_sound;

    a_VirtualSource* mech1;
    a_VirtualSource* mech2;
    a_VirtualSource* mech3;
    a_VirtualSource* lazer;


    float fadeout_spawn;
    float spawn_when;
    bool spawn_init;
    bool spawn_finished;


    th_MotionState* motion_states;
    int num_motion_states;

    th_TricolumnInterpData interp_state_machine;

    th_MotionState motion_state_interp;


    float hinge_interp[3];

    int spawn_heartbeat_index;

}th_TricolumnData;

typedef struct
{
    th_Entity* entities_gems;
    th_TricolumnData* data;
    th_LevelState* levelstate;
    fn_mat4* transforms;
    fn_mat4* transforms_gems;
    fn_mat4* transforms_cols;
    fn_mat4* transforms_eye;

    fn_mat4* transforms_trapdoors;//one per leg
    fn_mat4* transforms_leg;
    fn_mat4* transforms_foot;

    th_Allocator* alloc;
    int count;
    int gem_count;
    int col_count;

    fn_vec3 trapdoor_hinge;
    fn_vec3 pivot_center;

    float* extension;
    fn_vec3* foot_dir;
    fn_vec3* old_up_foot;

    bool* fastmode;

}th_TricolumnGroup;


void th_tricolumnSetCourse(th_TricolumnGroup* c,int i,fn_vec3* target_points,int target_count);

void th_tricolumnInitialize(th_Allocator* alloc,th_TricolumnGroup* c,int count,fn_vec3* positions,float* times,bool* fastmode,th_LevelState* levelstate);

void th_tricolumnUpdate(th_TricolumnGroup* c,float dt);
