#pragma once
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "../fn_engine/th_iqm.h"

typedef struct
{
  float dt;
  th_World* world;
  fn_vec3 forward;
  fn_vec3 right;
  fn_vec3 up;
  th_Entity* player;
  fn_RawInput* input;
  fn_mat4 invView;
  th_PointLight* lights;
}th_GameData;

typedef void (*init_func_t)(void*,int);
typedef void (*update_func_t)(void*,th_GameData*);
typedef void (*spawn_func_t)(void*);

typedef struct
{
  th_Entity* entities;
  fn_mat4* transforms;
  int entity_count;
  void* entity_data;//data of induvidual entities
  void* object_data;//applies to every object
  init_func_t intitialize;
  update_func_t update;
  spawn_func_t spawn;
  int index;//entity to be modified next
  int active;//number of entities active
}th_GameObject;

//TODO
/*
make an array of gameobjects in level.c

iterate over them at runtime, pass in struct pointers with data

*/
