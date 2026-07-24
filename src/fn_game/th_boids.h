#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "th_centipede.h"
#include "th_eyeball.h"
#include "th_tricolumn.h"
typedef struct
{
  th_Entity** cols;
  int allocated;
}th_BoidGroupMemory;

struct th_LevelState;
typedef struct th_LevelState th_LevelState;

typedef struct
{
  th_LevelState* levelstate;
  th_Entity* entities;
  fn_mat4* transforms;
  int boidscount;
  fn_Grid grid;
  fn_vec3 cellsize;
  int celldim;

  th_BoidGroupMemory* boidgroup_memory;
  a_VirtualSource** audio_sources;

  th_FrustumCullData* frustum_data;
  int frustum_offset;

  th_timer_t* spawn_times;
  bool* enabled_array;

  int* free_index_stack;//stack data struct for holding indexes of dead boids
  int free_index_stack_count;

  fn_vec3 hwidth;

  fn_vec3* prime_velocity;
  fn_vec3* prime_target;

  int num_audio_playing;

  th_Allocator* alloc;

  bool* has_sheild;
  fn_vec3* sheild_direction;
  fn_vec3* old_up_sheild;
  int frustum_offset_sheild;
  fn_mat4* transforms_sheild;
}th_BoidGroup;

typedef struct
{
  float neightborhood_rad;
  float speed;
  float gotoweight;
  float attraction;
  float seperation;
  float directional;
  float speedlimit;
}th_BoidProperties;



void th_boidsUpdate(th_BoidGroup* boids,float dt,th_BoidProperties* props);

void th_boidsInitialize(th_Allocator* alloc,th_BoidGroup* boids,fn_vec3 cellsize,int celldim,int bcount,fn_vec3 hwidth,th_LevelState* levelstate);

int th_boidsSpawn(th_BoidGroup* boids,fn_vec3 pos);

int th_boidsSpawnShield(th_BoidGroup* boids,fn_vec3 pos);
