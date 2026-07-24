#pragma once
#include "th_time.h"
#include "th_collision.h"
#include "../fn_math/fn_octree.h"
#include "th_allocator.h"
#define MAX_IMPACTS 64
#define TH_MAX_ENTITY_PTRS 1000
#include <pthread.h>

//return the amount of damage to do, takes entity ptr as an argument, takes player ptr as  argument
//takes dt, takes world ptr
typedef int (*damage_callback_t)(void*,void*,float,void*);

typedef enum
{
  TH_MACHINEGUN_BULLET = 1 ,
  TH_SHOTGUN_SHELL = 2,
  TH_HAMMER_PROJECTILE = 3,
  TH_PLAYER_ENTITY = 4,
  TH_ROCKET_ENTITY = 5,
  TH_UNKNOWN_ENTITY = 6,
}th_EntityType;

typedef struct
{
  void* entity;
  fn_vec3 pos;
}th_Impact;

//use for multithreading
typedef struct
{
  void* col_e;//the entity to do the impact on
  th_Impact impact;
}th_ImpactBuffered;

typedef enum
{
  TH_SLIDE_MODE,
  TH_IMPACT_MODE
}th_EntityCollisionMode;

typedef struct
{
  th_Collider aabb;
  fn_vec3 velocity;
  bool grounded;
  bool collided;
  fn_vec3 collision_normal;
  fn_vec3 ground_normal;
  fn_vec3 collision_position;
  bool alive;
  bool impact;
  th_Impact impacts[MAX_IMPACTS];
  int impact_count;
  th_EntityType type;
  bool delete_me;

  bool playercollideable;
  damage_callback_t damage_callback;
  th_timer_t time_of_damage;
  void* damage_callback_data;
  int index;

  th_CollisionPacket packet;
  fn_vec3 radius;
  th_EntityCollisionMode mode;

  bool robust_collisions;
  bool can_jump;

  bool slide_no_gravity_step;
}th_Entity;

#define TH_DEFAULT_ENTITY (th_Entity){.robust_collisions = true,.playercollideable = true,.alive = true,.damage_callback = NULL,.time_of_damage = 0,.damage_callback_data = NULL,.index = 0,.type = TH_UNKNOWN_ENTITY,.mode = TH_IMPACT_MODE,.can_jump = false,.slide_no_gravity_step = false}


typedef struct
{
  char* indices;
  int* pvols;
  th_CollidableVolume* volumes;
}th_CollisionMemory;

typedef struct
{
  th_CollidableVolume* volumes;
  int volumecount;
  th_AABB* aabbs;
  fn_Octree octree;
  th_CollisionMemory* phys_mem;

  th_Entity*** eptrs;
}th_World;

typedef struct
{
  float friction;
  float gravity ;
  float jumpSpeed ;
  float runAcceleration;
  float runDeacceleration ;
  float moveSpeed ;
  float sideStrafeSpeed ;
  float sideStrafeAcceleration ;
  float airDecceleration ;
  float airAcceleration ;
}th_PlayerDefs;

//https://github.com/solenum/exengine

void th_allocateEntityPointers(th_Allocator* alloc,th_World* w,int threads);
th_Entity** th_getEntityPointers(th_World* w,int thread);

void th_allocatePhysicsMemory(th_Allocator* alloc,th_World* w,int threads);
th_CollisionMemory* th_getPhysicsMemory(th_World* w,int thread);

fn_vec3 th_traceVolume(th_World* w,fn_vec3 start,fn_vec3 end,float radius,fn_vec3* normal,bool* is_hit,th_CollisionMemory* memory);

fn_vec3 th_trace(th_World* w,fn_vec3 start,fn_vec3 end,fn_vec3* normal,bool* is_hit,th_CollisionMemory* memory,float* out_t);

bool th_checkCollisionWorld(th_World* w,th_Entity* e,th_CollisionMemory* memory);

void th_updateEntity(th_Entity* e,th_World* w,float timeleft,th_CollisionMemory* memory);

void th_updatePlayer(th_Entity* e,th_World* w,fn_vec3 direction,bool jumping,float t,th_PlayerDefs pdefs,th_CollisionMemory* memory,bool flying,fn_vec3 look);

bool th_updatePlayerSlide(th_Entity* e,th_World* w,fn_vec3 direction,bool jumping,float t,th_PlayerDefs pdefs,th_CollisionMemory* memory,bool flying,fn_vec3 look,fn_vec3 normal,bool slide);

void th_initWorld(th_World* w);

th_timer_t th_get_slide_timer();
