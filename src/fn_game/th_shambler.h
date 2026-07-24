#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_engine/th_iqm.h"
#include "../fn_engine/th_system.h"
#include "th_gems.h"
typedef enum
{
  RUNNING = 0,
  ATTACK = 1,
  PAIN = 2,
  DEAD = 3,
  LIGHTNING = 4,
  RAGDOLLED = 5
}th_Shamblerstate;


#define TH_LIGHTNING_HAND_SEGMENTS 4
#define TH_LIGHTNING_BEAM_SEGMENTS 20
#define TH_LIGHTNING_TR_TIME 10 //10ms

typedef struct
{
  fn_vec3 dir_current;
  th_Shamblerstate state;
  int model_id;
  bool reset_anims;
  float health;

  th_timer_t mele_attack_timer;

  th_timer_t lightning_timer;
  fn_vec3 lightning_target;
  th_timer_t lightning_hit_timer;
  int randstate_lightning_hand[(TH_LIGHTNING_HAND_SEGMENTS - 2)*2];
  int randstate_lightning_beam[(TH_LIGHTNING_BEAM_SEGMENTS - 2)*2];
  th_timer_t lightning_hand_timer;
  th_timer_t lightning_beam_timer;

  a_VirtualSource* audio_source_growl;
  a_VirtualSource* audio_source_growlstep;
  a_VirtualSource* audio_source_lightning_charge;
  a_VirtualSource* audio_source_lightning_strike;

  fn_mat4 old_mat;
  float fadeout;

  th_timer_t rag_to_death_timer;

  a_VirtualSource* audio_source_death;

  int hand_flip_flop;

  th_timer_t spawn_time;
}th_ShamblerData;

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  th_LevelState* levelstate;
  th_Entity* entities;
  fn_mat4* transforms;
  void* data;
  int entity_count;
  fn_Grid grid;

  th_Stack id_stack;

  fn_vec3* spawn_locations;
  float* spawn_times;
  int num_spawns;
  int spawn_offset;
}th_ShamblerGroup;

/*
0 bindpose
1 attack04
2 attack01
3 attack02
4 attack03
5 attack05
6 idle02
7 pain01
8 pain02
9 pain03
10 pain04
11 pain05
12 qshambler
13 sight
14 walking_attack01
15 walk
*/

void th_shamblerInitialize(th_Allocator* alloc,th_ShamblerGroup* shamblers,int count,th_LevelState* levelstate,fn_vec3* spawn_locations,
float* spawn_times,
int num_spawns);

void th_shamblersUpdate(th_ShamblerGroup* shamblers,float dt);

int th_shamblersSpawn(th_ShamblerGroup* shamblers,fn_vec3 pos);
