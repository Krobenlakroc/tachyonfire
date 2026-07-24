#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  th_LevelState* levelstate;
  th_Entity* entities;
  fn_mat4* transforms;
  float* scale;
  int entity_count;
  fn_vec3* orientations;
  int num_used;
  int current_count;
  float* angles;
  float* angular_vel;
}th_BrassObject;

void th_brassInit(th_Allocator* alloc,th_BrassObject* brass,int count,th_LevelState* levelstate);

void th_brassUpdate(th_BrassObject* brass,float dt);

void th_brassUpdateTransform(th_BrassObject* brass,int i);

int th_brassSpawn(th_BrassObject* brass,fn_vec3 position,fn_vec3 velocity,fn_vec3 orientation);

int th_brassSpawnScaled(th_BrassObject* brass,fn_vec3 position,fn_vec3 velocity,fn_vec3 orientation,float scale);
