#pragma once
#define TH_NUM_WEAPONS 5

#include "../fn_engine/th_physics.h"
#include "../fn_input.h"
#include "../fn_engine/th_allocator.h"
typedef enum
{
  TH_HAMMER = 0,//level 1 throwable, level 2 returns, level 3 lightning
  TH_MACHINEGUN = 1, // level 1 single stream, level 2 2 streams, level 3 death beam
  TH_SHOTGUN = 2, //level 1 blast, level 2 auto shotgun, level 3
  TH_NOWEAPON = 3
}th_WeaponState;

struct th_LevelState;
typedef struct th_LevelState th_LevelState;

typedef struct
{
  th_LevelState* levelstate;
  int weapon_transform_count;
  fn_mat4* weapon_transforms;

  fn_mat4* weapon_transforms_bolt;

  float sten_kickback;
  float sten_kickback_velocity; //assuem constant acceleration forward

  //sten axis
  fn_vec3 sten_axis_a;
  fn_vec3 sten_axis_b;

  //chaingun axis
  fn_vec3 chaingun_axis_a;
  fn_vec3 chaingun_axis_b;

  //spas12 axis
  fn_vec3 spas12_axis_a;
  fn_vec3 spas12_axis_b;

  float shotgun_temp_interp;
  float machinegun_temp_interp;

  int weapon_transform_count_hammer;
  fn_mat4* weapon_transforms_hammer;

  int weapon_transform_count_shotgun;
  fn_mat4* weapon_transforms_shotgun;

  int weapon_transform_count_level2;
  fn_mat4* weapon_transforms_level2;

  int weapon_transform_count_shotgun_level2;
  fn_mat4* weapon_transforms_shotgun_level2;

  int weapon_transform_count_flak_cannon_front;
  fn_mat4* weapon_transforms_flak_cannon_front;

  int weapon_transform_count_flak_cannon_back;
  fn_mat4* weapon_transforms_flak_cannon_back;

  int weapon_transform_count_sledge;
  fn_mat4* weapon_transforms_sledge;

  fn_mat4 weapon_transform_sledge_ref;

  float angle;
  float angle_velocity;

  fn_vec3 currentPos;
  fn_vec2 currentAngles;

  float angle_akimbo;
  float angle_velocity_akimbo;

  fn_vec3 currentPos_akimbo;
  fn_vec2 currentAngles_akimbo;

  float kickback_akimbo;
  float kickback_velocity_akimbo;
  float kickback_acceleration_akimbo;

  float kickback;
  float kickback_velocity;
  float kickback_acceleration;


  th_WeaponState chosen_weapon;

  th_timer_t weapon_switch_time;
  th_WeaponState weapon_switch_request;

  float lowering_interp;

  int shell_id;

  int spawned_shell_id;
  th_timer_t spawned_shell_time;
  fn_vec3 spawn_shell_pos;
  float shell_respawn_scale;


  fn_Transform hammer_transforms[2];

  th_timer_t weapon_levelup_timers[TH_NUM_WEAPONS];

}th_Weapon;

void th_weaponInitialize(th_Allocator* alloc,th_Weapon* object,th_LevelState* levelstate);
void th_weaponUpdate(th_Weapon* object, fn_RawInput* input,float dt,fn_vec3 target_pos,fn_vec2 target_angles,fn_vec3 direction,fn_vec3 up);
void th_weaponCheckLowering(th_Weapon* object,float dt,fn_vec3 target_pos,fn_vec3 direction);
