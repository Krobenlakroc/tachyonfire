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
  fn_vec3* orientations;
  float* lifes;

  th_FrustumCullData* frustum_data;
  int frustum_offset;

  a_VirtualSource** sources;

  float* blast_delay;
}th_GemObject;

void th_gemInit(th_Allocator* alloc,th_GemObject* gem,int count,th_LevelState* levelstate);

void th_gemUpdate(th_GemObject* gem,float dt);

int th_gemSpawn(th_GemObject* gem,fn_vec3 position,fn_vec3 velocity);
