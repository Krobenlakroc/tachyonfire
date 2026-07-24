#pragma once
#include "../fn_math/fn_math.h"

//store on the cpu
typedef struct
{
  fn_vec3 pos;
  fn_vec3 scale;
  fn_vec3 normal;
  fn_vec3 tangent;
  fn_vec2 material_handle;
  fn_mat4 world;
  fn_mat4 viewproj;
}th_DecalOrientation;

//pass to the gpu
typedef struct
{
  fn_mat4 decal_matrix;//0 - 64
  fn_vec4 decal_properties;//64-80
}th_Decal;

fn_vec3 th_getDecalTangentVector(fn_vec3 look);

fn_mat4 th_makeDecalMatrix(fn_vec3 pos,fn_vec3 scale,fn_vec3 normal,fn_vec3 tangent);
fn_mat4 th_makeDecalMatrixWorld(fn_vec3 pos,fn_vec3 scale,fn_vec3 normal,fn_vec3 tangent);

void th_resetDecals();
void th_addDecal(th_DecalOrientation new_decal);
void th_getDecals(th_Decal* out,int* count);

th_DecalOrientation* th_getDecalOrientations();
