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


typedef struct
{

    uint num_used;
    int current_count;
    th_LevelState* levelstate;
    th_Entity* entities;
    fn_mat4* transforms;
    int entity_count;
    float* lifes;


}th_LifeSphere;

void th_lifeInit(th_Allocator* alloc,th_LifeSphere* o,int count,th_LevelState* levelstate);

void th_lifeUpdate(th_LifeSphere* o,float dt);

void th_lifeSpawn(th_LifeSphere* o,fn_vec3 position);
