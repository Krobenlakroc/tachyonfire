#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "th_brass.h"
#include "th_player.h"
#include "../fn_engine/th_particle.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  th_LevelState* levelstate;
  float* scale;
  int* explosive;
  th_Entity* entities;
  fn_mat4* transforms;
  int entity_count;
  th_PointLight* lights;
  bool firing;

  uint num_used;
  uint light_current;
  th_timer_t time_fired;

  th_Entity expl_entity;

  th_Particle flash_particle;
  float flash_2_angle;
  float time_flash_scalechange;

  fn_vec3* fakevelocity;
}th_PlasmaObject;

void th_plasmaInitialize(th_Allocator* alloc,th_PlasmaObject* plasma,int count,th_LevelState* levelstate);

void th_plasmaUpdate(th_PlasmaObject* plasma,float dt,fn_RawInput* input,fn_vec3 direction,fn_vec3 right,fn_vec3 up);
