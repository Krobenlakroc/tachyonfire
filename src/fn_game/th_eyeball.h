#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "th_rocket.h"

typedef enum
{
  WALLRIDING, //0
  TRAVELING, // 1
  ROCKETING, // 2
  BIRTH, //3
  PREPARINGROCKET, //4
  DEADEYEBALL, //5
  EYE_SPAWNING, //6
}th_EyeballState;

typedef struct
{
  th_timer_t time_started_rocket;
  int shots_fired;
  th_timer_t last_fire_time;
  bool fired_volley;
  th_timer_t last_time_targeted;
  th_timer_t last_time_wallrode;
  fn_vec3 last_position;
  th_timer_t last_time;
  th_EyeballState state;
  fn_vec3 target;
  fn_vec3 gaze;

  float rotation;
  float health;

  th_Entity ring_gib_a;
  th_Entity ring_gib_b;

  bool gibbed;
  th_Entity eyeball_gib_a;
  th_Entity eyeball_gib_b;

  a_VirtualSource* audio_source_scream;

  th_timer_t spawn_time;
  bool enabled; //alive has a different meaning for eeyballs, so use this to disable simulation


  float fadeout_ringa;
  fn_mat4 old_ringa;

  float fadeout_ringb;
  fn_mat4 old_ringb;

  float fadeout_giba;
  fn_mat4 old_giba;

  float fadeout_gibb;
  fn_mat4 old_gibb;


  float jitter_time_a;
  float jitter_time_b;

  fn_vec3 ring_jitter_a;
  fn_vec3 ring_jitter_b;

  th_timer_t dwell_timer;
  float pupilsize;

  float stretch_amt;
  fn_vec3 stretch_axis;
  th_timer_t stretch_timer;
  float stretch_permanent;


}th_EyeballData;

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  th_LevelState* levelstate;
  th_EyeballData* data;
  th_Entity* entities;
  fn_mat4* transforms;
  fn_mat4* transforms_ring_a;
  fn_mat4* transforms_ring_b;
  fn_mat4* transforms_eye_gib_a;
  fn_mat4* transforms_eye_gib_b;
  fn_mat4* transforms_pupil;
  int count;

  th_FrustumCullData* frustum_data;
  int frustum_offset;
  int frustum_offset_ring_a;
  int frustum_offset_ring_b;
  int frustum_offset_eye_gib_a;
  int frustum_offset_eye_gib_b;

  int* free_index_stack;//stack data struct for holding indexes of dead eyeballs
  int free_index_stack_count;

  bool super;

  fn_vec3 pupil_center;
}th_EyeballGroup;


void th_eyeballUpdate(th_EyeballGroup* c,float dt,fn_RawInput* input);

void th_eyeballInitialize(th_Allocator* alloc,th_EyeballGroup* c,int count,fn_vec3* positions,th_LevelState* levelstate,bool super);

int th_eyeballSpawn(th_EyeballGroup* c,fn_vec3 position);
