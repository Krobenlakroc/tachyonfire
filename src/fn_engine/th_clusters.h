#pragma once
#include "../fn_math/fn_math.h"
#include "th_decal.h"

#include <stdint.h>
//WARNING! hardcoded in the particle shader!
#define CLUSTER_Z_COUNT 10//20
#define CLUSTER_Z_OFFSET 5
#define CLUSTER_Z_VIRTUAL 15

typedef uint32_t uint;

typedef struct
{
  uint x;
  uint y;
}th_uvec2;

typedef union
{
  struct
  {
    uint x;
    uint y;
    uint z;
    uint w;
  };
  struct
  {
    th_uvec2 a;
    th_uvec2 b;
  };
}th_uvec4;

typedef struct
{
  fn_vec4 pos;//0 - 16
  fn_vec4 color;//16 - 32
  fn_mat4 lightmat; //32 - 96
  fn_vec4 shadowindex; //96 - 112
  fn_vec4 pos2;// 112 - 128
}th_PointLight;

void th_initClusterMemory(fn_mat4 invProj);

uint th_binLightsFake(fn_vec4* planes_frust,fn_mat4 viewmatrix,uint* access,uint* offsets,th_PointLight* lights,uint lights_to_bin,fn_mat4 invProj,fn_vec2 screen,float zn,float zf);

uint th_binLights(fn_vec4* planes_frust,fn_mat4 viewmatrix,uint* access,uint* offsets,th_PointLight* lights,uint lights_to_bin,fn_mat4 invProj,fn_vec2 screen,float zn,float zf);
uint th_binCubes(fn_mat4 viewmatrix,uint* access,uint* offsets,fn_vec3* mins,fn_vec3* maxs,uint cubes_to_bin,fn_mat4 invProj,fn_vec2 screen,float zn,float zf,bool keypressed,fn_mat4 proj,fn_vec3 camera_pos);
uint th_binDecals(fn_vec4* planes_frust,fn_mat4 viewmatrix,uint* access,uint* offsets,th_DecalOrientation* decals,uint decals_to_bin,fn_mat4 invProj,fn_vec2 screen,float zn,float zf,fn_vec3 camera_pos);

void th_updateClustersFOV(fn_mat4 invProj);
