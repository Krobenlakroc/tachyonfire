#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "th_gems.h"
#include "../fn_input.h"
#include <stdint.h>

#define MAX_COURSE_SIZE 100

typedef struct
{
  float scale;
  float speed;
  float spawn_speed;
  float turn_rate;
  float gem_health;
}th_CentiConfig;

typedef enum
{
  TH_SPAWNLOOP,
  TH_FOLLOW,
  TH_HASPHYSICS,
  TH_ALL_HASPHYSICS
}th_CentipedeState;

typedef struct
{
  int start;
  int count;
  fn_vec3* course;
  fn_vec3* upvectors;
  float* angles;
  th_CentipedeState state;
  int target_index;
  th_timer_t start_time;
  float t; // used for oscillation
  a_VirtualSource** audio_sources;
  float spawn_when;
  bool spawn_init;
  bool spawn_finished;

  int64_t smoke_time; //atomic
  bool spawned_legs;

  float spawnloop_time;

  th_CentiConfig config;
}th_MasterInfo;//each master controls the course of several agents

typedef struct
{
  float interp;
  int cprog;
  fn_vec3* course;
  fn_vec3* upvectors;
  float* angles;
  int course_count;
  float speed;
  fn_vec3 position;
  fn_vec3 old_forward;
  fn_vec3 old_right;
  fn_vec3 old_up;
  bool hasgem;
  th_Entity entity;
  bool hasphysics;
  th_timer_t physics_time;
  fn_vec3 velocity;
  bool has_gravity;

  fn_vec3 hit;
  bool prevhit;
  a_VirtualSource* crashsource;

  fn_mat4 old_matrix;
  float fadeout;

  float fadeout_spawn;

  float gem_health;
  float gem_jitter_t;
  fn_vec3 gem_jitter_x;

  bool spawned;

  th_MasterInfo* master;
}th_AgentInfo;



struct th_LevelState;
typedef struct th_LevelState th_LevelState;

typedef struct
{
  th_LevelState* levelstate;
  th_MasterInfo* masters;
  int num_masters;
  th_AgentInfo* agents;
  th_Entity* entities_gems;
  th_Entity* entities_body;
  fn_mat4* transforms_body;
  fn_mat4* transforms_gems;
  float* normal_perturb;
  float* normal_perturb_target;
  th_timer_t* normal_perturb_timer;
  fn_vec3* normal_perturb_offset;

  int count;
  fn_vec3 forward;
  fn_vec3 pos;
  fn_mat4 rollmat;
  int length_of_snake;

  th_FrustumCullData* frustum_data;
  int frustum_offset;
  int frustum_offset_gem;

  float cullradius;
  th_CentiConfig config;

  fn_mat4* transforms_legs_a;
  int leg_count;
  fn_mat4* transforms_legs_b;

  fn_mat4* transforms_legs_a_old;
  fn_mat4* transforms_legs_b_old;

  fn_vec3* legs_a_up;
  fn_vec3* legs_b_up;

  fn_vec3* bone_point_a;
  fn_vec3* bone_point_b;



}th_CentipedeGroup;

typedef struct
{
  fn_vec3* course;
  float* angles;
  fn_vec3* ups;
  int course_count;
  float spawn_when;
}th_CentipedeCourse;

void th_centipedeUpdate(th_CentipedeGroup* c,float dt,fn_RawInput* input);

void th_centipedeInitialize(th_Allocator* alloc,th_CentipedeGroup* c,int count,int length,int count_masters,float* spawnloop_times,th_CentipedeCourse* courses,th_LevelState* levelstate,th_CentiConfig config);
