#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_light.h"
#include "th_builtins.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  th_LevelState* levelstate;
  th_Entity* entities;
  fn_mat4* transforms;
  int entity_count;
  fn_vec3* orientations;
  int num_used;
  int current_count;

  a_VirtualSource** sources;

  th_LightQuery* lightq;
  th_LightIdTuple* lights;

  th_timer_t* timings;
}th_RocketObject;

void th_rocketInit(th_Allocator* alloc,th_RocketObject* rocket,int count,th_LightQuery* lightq,th_LevelState* levelstate);

void th_rocketUpdate(th_RocketObject* rocket,float dt);

void th_rocketSpawn(th_RocketObject* rocket,fn_vec3 position,fn_vec3 velocity,fn_vec3 orientation);
