#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "th_player.h"
#include "th_weapon.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


//variable number of meshes
//1 mesh per instance of gib
//1 spawn creates many new gib instances
typedef struct
{
    th_LevelState* levelstate;
    uint num_used;
    int current_count;
    fn_mat4* transforms;

    fn_vec3* positions;
    fn_vec3* velocity;
    fn_mat4* originals;
    float* lifes;

    int entity_count;
}th_GibInstance;

typedef struct
{
    th_GibInstance* instances;
    int instance_count;
    pthread_mutex_t giblock;
}th_GibCollection;

void th_gibCreateInstances(th_Allocator* alloc,th_GibCollection* gib,int num_instances);

void th_gibInit(th_Allocator* alloc,th_GibCollection* gib,int count,th_LevelState* levelstate);

void th_gibUpdate(th_GibCollection* gib,float dt);

void th_gibSpawn(th_GibCollection* gib,fn_vec3 position,fn_mat4 original_orient);
