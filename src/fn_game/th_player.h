#pragma once

#include "th_weapon.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_time.h"
#include "../fn_engine/th_gpu.h"


struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  th_LevelState* levelstate;
  int level_weapon[TH_NUM_WEAPONS];
  int gem_count[TH_NUM_WEAPONS];
  const char* weapon_name[TH_NUM_WEAPONS];
  int hp;
  char* healthbar;
  char* health_number;
  char* level_bar;
  char* level_number;
  fn_vec3* levelcolorptr;

  char* gem_cap_number;
  fn_vec2* level_pos;

  float screenshake_f;
  float screenshake_t;
  float screenshake_amplitude;

  float fov_delta;
  float fov_delta_target;

  bool request_uncrouch;

  bool is_dead;

  a_VirtualSource* slide_source;

  th_timer_t crouch_time;
  th_timer_t uncrouch_time;
  float physics_y;
  bool set_physics_y;

  int stepoffset;

  th_timer_t overheal_timer;

  bool noclip;

  float* level_pct_size;
  float* health_pct_size;
  fn_vec3* health_pct_color;
  fn_vec2* health_pos;
}th_PlayerObject;

void th_playerInitialize(th_PlayerObject* obj,char* healthbar,
char* health_number,
char* level_bar,
char* level_number,fn_vec3* levelcolorptr,char* gem_cap_number,th_LevelState* levelstate);

void th_playerUpdate(th_PlayerObject* obj,float dt,th_Character* cmap,fn_vec2 screenSize);

void th_incrementPlayerGem(th_PlayerObject* obj);

void th_incrementPlayerHealth(th_PlayerObject* obj,int points);

void th_decrementPlayerHealth(th_PlayerObject* obj,int points);

void th_decrementPlayerGem(th_PlayerObject* obj,int points);

void th_playerPhysicsUpdate(th_LevelState* ls,fn_RawInput* input,float dt,a_AudioSystem* audiosystem,fn_vec3 normal,fn_vec3 right,fn_vec3 look);
