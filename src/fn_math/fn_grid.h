#pragma once
#include "../fn_engine/fn_arrayutils.h"
#include "../fn_engine/th_physics.h"
#include "../fn_engine/th_allocator.h"

#define GRID_MAX_SECTIONS_TOUCHED 4000

typedef struct
{
  th_Entity** entities;// array of aabb pointers to collide with
  int entityCount;
  int alloced;
}fn_GridSection;

typedef struct
{
  th_Entity** usedIndices;
  th_Entity** ret;
  int* sections_touched;
}fn_GridMemory;

typedef struct
{

  fn_GridMemory* grid_mem;

  th_Entity** usedIndices;
  th_Entity** ret;
  th_Entity* entities;
  int entityCount;
  int** sectionsTouched;
  fn_GridSection* sections;
  fn_vec3 offset;
  int sectionCount;
  fn_vec3 cellsize;
  int cellcount;
  bool created;
  th_Allocator* alloc;
}fn_Grid;

typedef enum
{
  TH_ENEMY = 1,
  TH_WORLDGEOM = 2,
  TH_USE_RAY = 4,
  TH_ENEMY_ROCKET = 8,
  TH_EYEBALL = 16,
  TH_BOID = 32,
  TH_CAN_KILL_PLAYER = 64,
}th_EntityEdictFlags;



typedef struct
{
  th_Entity* entities;
  int* entityCount;
  fn_Grid* grid;
  th_EntityEdictFlags flags;
}th_EntityCollisionEdict;

// void fn_freeGrid(fn_Grid* grid);
void fn_createGrid(th_Allocator* alloc,fn_Grid* grid,th_Entity* entities,int entityCount,fn_vec3 cellsize,int cellcount);

// th_Entity** fn_getCollidableGrid(fn_Grid* grid,int entity,int* entities);
th_Entity** fn_getCollidableAABB(fn_Grid* grid,fn_AABB aabb,int* entities,fn_GridMemory* memory);

void fn_updateEntityGrid(fn_Grid* grid,int entity,float dt);
void fn_removeEntityGrid(fn_Grid* grid,int entity);

void th_freeEntityGroups();
void th_registerEntityGroup(th_EntityCollisionEdict e);

th_Entity* th_collideWithEntitiesExclusionary(th_EntityEdictFlags flags,th_EntityEdictFlags eflags,th_Entity* e,float worldtime,fn_vec3* normal,bool* is_hit,int thread_id,float dt,fn_vec3* pos,float* time);
th_Entity* th_collideWithEntities(th_EntityEdictFlags flags,th_Entity* e,float worldtime,fn_vec3* normal,bool* is_hit,int thread_id,float dt,fn_vec3* pos,float* time);
th_Entity* th_traceWithEntitites(th_EntityEdictFlags flags,fn_vec3 start,fn_vec3 end,fn_vec3* normal,bool* is_hit,int thread_id,float dt,fn_vec3* pos,float* time,float radius);

void th_getEntitiesInRadius(th_EntityEdictFlags flags,fn_vec3 pos,float radius,int thread_id,float dt,int* count,th_Entity** eptr);
