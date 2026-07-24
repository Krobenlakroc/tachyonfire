#pragma once
#include "../fn_math/fn_math.h"
#include "th_decal.h"
#include "th_clusters.h"
#include "th_physics.h"
#include "th_light.h"
#include "th_allocator.h"
#include <stdint.h>



typedef enum
{
  TH_COLLISION_PARTICLE,
}th_ParticleMode;

typedef enum
{
  TH_SMOKEA = 0,
  TH_SMOKEB = 1,
  TH_BLOOD = 2,
  TH_SPARKS = 3,
  TH_BULLETHOLE = 4,
  TH_EXPLOSION = 5,
  TH_IMPACTRING = 6,
  TH_BLOOD_DECAL = 7,
  TH_LIGHTNING = 8,
  TH_BLOOD_SPURT = 9,
  TH_LASERBEAM = 10,
  TH_MACHINEGUN_FLASH = 11,
  TH_SHOTGUN_FLASH = 12,
  TH_TRACER_BULLET = 13,
  TH_METALSPLASH = 14,
}th_ParticleName;

typedef struct
{
  fn_vec3 position;
  fn_vec3 velocity;
  float theta;
  float stretch;
  float scale;
  float overbright;
  float alphascale;
  float alphascale_base;
  bool hasphysics;
  bool has_gravity;

  th_timer_t start_time;
  float life;
  float distance_to_camera;
  bool alive;
  th_ParticleMode flags;
  fn_vec2 texture_handle;

  bool makedecal;
  th_DecalOrientation decal;
  float decay;

  bool animated;
  int frame_id;
  th_timer_t last_frame_start;
  float time_per_frame;
  float offset_per_frame_y;
  float offset_per_frame_x;
  int num_frames;
  float blend;

  bool has_velocity;
  bool constrained_billboard;
  float use_add_blending;

  bool fade_out;
  bool fade_out_sharp;
  int num_frames_alive;
  bool self_destruct;

  bool haslight;
  bool tubelight;
  fn_vec3 lightcolor;
  th_LightIdTuple light_id;
  th_LightQuery* lightq;
  float lightthresh;

  uint64_t creation_id;
  int index_in_array;

  float scale_end;
  float scale_internal;

  float use_geom_normal;

  float use_reflections;

  float use_premultiplied;
}th_Particle;

void th_particle_framecap(uint64_t* out_ids,fn_mat4* out_mats, int* out_count);
fn_mat4* th_particle_playback(fn_vec3 pos,fn_vec3 look,uint64_t* a_ids,uint64_t* b_ids,fn_mat4* a_mats,fn_mat4* b_mats,int a_count,int b_count,int* out_count,float f);

fn_mat4* th_particle_matrices();
int th_particle_count();

th_Particle th_defaultParticle();

void th_initParticles(th_Allocator* alloc,int count);
void th_simulateParticles(float dt,fn_vec3 pos,fn_vec3 look,th_World* w);
bool th_addParticle(th_Particle p);


void th_registerParticle(fn_vec2 id,th_ParticleName name);
fn_vec2 th_getParticleTexture(th_ParticleName name);
