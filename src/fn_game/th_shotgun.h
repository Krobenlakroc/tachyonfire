#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "th_brass.h"
#include "th_weapon.h"
#include "th_player.h"
#include "../fn_engine/th_particle.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  th_LevelState* levelstate;
  th_Entity* entities;
  int* bounce;
  float* scale;
  fn_mat4* transforms;
  int entity_count;
  th_PointLight* lights;
  bool firing;

  uint light_current;
  th_timer_t time_fired;
  uint num_used;

  fn_vec3* newVelocity;


}th_ShotgunObject;

void th_shotgunInitialize(th_Allocator* alloc,th_ShotgunObject* shotgun,int count,th_LevelState* levelstate);

void th_shotgunUpdate(th_ShotgunObject* shotgun,float dt,fn_RawInput* input,fn_vec3 direction,fn_vec3 right,fn_vec3 up);
/*
** - done
* - WIP
Jumper- shambler runs and jumps at you ** drops gems
Regular Boid - flys at you in a group **
Gemsnake - charges at you * drosp gems
Crawler - small spider, runs on walls and drops on you drops gems
Flying charger - horned skull from DD drops gems
Super-Gemsnake - longer gemsnake

Slither-er - slithers on the floor at you
*/
