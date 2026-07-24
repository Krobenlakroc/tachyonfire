#pragma once
#include <stdbool.h>
#include "fn_vec2.h"
typedef enum
{
  ZYX,
  ZXY,
  YXZ,
  YZX,
  XZY,
  XYZ
}fn_Vec3Swizze_t;

typedef union
{
  float v[3];
  struct {
  float x;
  float y;
  float z;
  };
  struct
  {
    fn_vec2 xy;
    float padding;
  };

}fn_vec3;

fn_vec3 fn_MA(  fn_vec3 va, float scale,  fn_vec3 vb );
fn_vec3 fn_createVec3s(float x);

fn_vec3 fn_clampVec3(fn_vec3 a,fn_vec3 b,fn_vec3 c);
fn_vec3 fn_createVec3(float x,float y,float z);
fn_vec3 fn_createVec3v(float* v);
fn_vec3 fn_addVec3(fn_vec3 a,fn_vec3 b);
fn_vec3 fn_addVec3s(fn_vec3 a,float b);
fn_vec3 fn_subVec3(fn_vec3 a,fn_vec3 b);
fn_vec3 fn_multVec3(fn_vec3 a,fn_vec3 b);
fn_vec3 fn_normalizeVec3(fn_vec3 x);


float fn_dot(fn_vec3 a,fn_vec3 b);
float fn_angle(fn_vec3 a,fn_vec3 b);
bool fn_equalVec3(fn_vec3 a,fn_vec3 b);
bool fn_almostequalVec3(fn_vec3 a,fn_vec3 b,float margin);
fn_vec3 fn_multVec3s(fn_vec3 a,float b);
fn_vec3 fn_divVec3s(float a,fn_vec3 b);
fn_vec3 fn_divVec3(fn_vec3 a,fn_vec3 b);
fn_vec3 fn_lerpVec3(fn_vec3 a,fn_vec3 b,float scale);

fn_vec3 fn_unLerpVec3(fn_vec3 a,fn_vec3 b,fn_vec3 pos);

fn_vec3 fn_cerpVec3(fn_vec3 a,fn_vec3 b,float scale);
fn_vec3 fn_slerpVec3(fn_vec3 a,fn_vec3 b,float t);
fn_vec3 fn_NlerpVec3(fn_vec3 a,fn_vec3 b,float t);
fn_vec3 fn_vec3Swizze(fn_vec3 a,fn_Vec3Swizze_t b);

fn_vec3 fn_cross(fn_vec3 a,fn_vec3 b);

fn_vec3 fn_bounce(fn_vec3 velocity,fn_vec3 normal);

fn_vec3 fn_snapVec3(fn_vec3 v);

fn_vec3 fn_reflect(fn_vec3 a,fn_vec3 normal);

fn_vec3 fn_maxVec3(fn_vec3 v,fn_vec3 m);
fn_vec3 fn_minVec3(fn_vec3 v,fn_vec3 m);
void fn_printVec3(fn_vec3 f);

bool fn_isAxisAligned(fn_vec3 v);


bool fn_bvec3AndAll(fn_vec3 b);

void fn_bvec3Increment(fn_vec3* b);
