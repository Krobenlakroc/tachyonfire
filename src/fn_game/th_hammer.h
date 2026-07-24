#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "../fn_engine/th_audio.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;

#define HAMMER_LIGHT_SEGMENTS 6
typedef struct
{
  th_Entity* entities_impacts;

  th_LevelState* levelstate;
  th_Entity* entities;
  fn_mat4* transforms;
  int entity_count;
  bool is_held[2];
  bool has_gravity[2];

  a_VirtualSource* flysound[2];
  th_timer_t last_hit_time[2];
  int num_hits[2];
  th_timer_t time_fired[2];
  fn_vec3 old_up[2];
  float rot_angles_flight[2];
  bool caught_up_animation[2];


  fn_mat4* sledge_transform;
  bool is_held_sledge;
  th_timer_t time_fired_sledge;
  fn_vec3 sledge_forward;
  fn_vec3 sledge_up;
  fn_vec3 sledge_position;

  fn_vec3 sledge_position_collider;

  bool sledge_fly;

  th_timer_t sledge_impact_timer;
  bool sledge_impact;
  th_Entity sledge_entity;

  fn_vec3 lightning_tips[6];
  int randstate_lightning_beam[6][(HAMMER_LIGHT_SEGMENTS - 2)*2];
  th_timer_t lightning_timer;
  int num_lightning_tips;

  th_timer_t impact_timer;
  fn_vec3 impact_pos;

  bool animation_interpose[2];
  th_timer_t timer_equip_animation[2];
  fn_mat4 equip_frame_0[2];

  float equip_duration;
}th_HammerObject;

void th_hammerInitialize(th_Allocator* alloc,th_HammerObject* object,fn_mat4* transforms,fn_mat4* sledge_transform,th_LevelState* levelstate);

void th_hammerUpdate(th_HammerObject* object,float dt,fn_RawInput* input,fn_vec3 direction,fn_vec3 right,fn_vec3 up,fn_vec3 target_pos,fn_vec2 target_angles);
