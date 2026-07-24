#pragma once
#include "th_time.h"
#include "th_clusters.h"
#include "th_allocator.h"
#include "th_system.h"

#define VIRTUAL_LIGHTCOUNT 1024

typedef struct
{
  int id;//index in array
  int count;//nth light to use this id
}th_LightIdTuple;

typedef struct
{
  th_timer_t life;
  bool fadeout;
  bool alive;
  th_timer_t start_time;
  fn_vec3 base_color;
  bool tube;
  fn_vec3 tube_endcap;
  bool self_destruct;
  float lightthresh;
}th_LightProperties;


/*
 * Physical lights 256
 * virtual lights 1024
 * Pick 256 - offset_start closest virtual lights in frustum
 */

typedef struct
{
  //virtual light info
  int num_lights;
  int light_offset_start;
  int current_light;
  th_PointLight* pointlights;
  th_LightProperties* properties;
  int* count_array;

  th_Stack free_stack;

  pthread_mutex_t light_lock;

  //processing buffers
  int* frustum_test;
  float* distances;

  //physical light info
  int lights_physical_count;
  int light_offset_start_physical;
  th_PointLight* pointlights_physical;

  int physical_used;


  //for sorting and merging
  int* indices_times;
  th_timer_t* start_times;
}th_LightQuery;

th_LightIdTuple th_makeLightIDTuple(int id,int count);

void th_updateLights(th_LightQuery* l);

void th_assignPhysicalLights(th_LightQuery* l,fn_vec4* planes_frust,fn_vec3 viewpos);

void th_mergePhysicalLights(th_LightQuery* l,th_PointLight* merge,int num_merge);

void th_updateLightSingle(th_LightQuery* l,th_LightIdTuple idx);

void th_makeLight(th_LightQuery* l,th_LightProperties p,fn_vec3 color,fn_vec3 position);

th_LightIdTuple th_getLight(th_LightQuery* l,th_LightProperties p,fn_vec3 color,fn_vec3 position);

th_LightProperties th_getLightProperties(th_LightQuery* l,th_LightIdTuple i);

void th_killLight(th_LightQuery* l,th_LightIdTuple i);

void th_killLightAfterRender(th_LightQuery* l,th_LightIdTuple i);

void th_setLight(th_LightQuery* l,th_LightIdTuple i,fn_vec3 color,fn_vec3 position);

void th_setLightTube(th_LightQuery* l,th_LightIdTuple i,fn_vec3 color,fn_vec3 position,fn_vec4 tube);

void th_setLightProperties(th_LightQuery* l,th_LightIdTuple i,fn_vec3 color,fn_vec3 position,th_LightProperties prop);

void th_initLightQuery(th_LightQuery* l,th_PointLight* pointlights,int useable,int offset,th_Allocator* alloc);

th_LightProperties th_getDefaultLight();
