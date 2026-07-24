#pragma once
#include "fn_vec3.h"
typedef union
{
  float v[4];
  struct {
  float x;
  float y;
  float z;
  float w;
  };
  struct
  {
    fn_vec3 xyz;
    float padding;
  };
  struct {
  float padding2;
  fn_vec3 yzw;
  };
}fn_vec4;

typedef fn_vec4 fn_quat;

void fn_printVec4(fn_vec4 v);
fn_vec3 fn_createVec3v4(fn_vec4 v);
fn_vec4 fn_createVec4(float x,float y,float z,float w);
fn_vec4 fn_createVec4Vec3(fn_vec3 a,float w);
fn_vec4 fn_addVec4(fn_vec4 a,fn_vec4 b);
fn_vec4 fn_multVec4(fn_vec4 a,fn_vec4 b);
fn_vec4 fn_multVec4s(fn_vec4 a,float b);

float fn_dotVec4(fn_vec4 a,fn_vec4 b);

fn_vec4 fn_normalizeVec4(fn_vec4 x);
fn_vec4 fn_normalizeVec3Magnitude(fn_vec3 x);

bool fn_equalVec4(fn_vec4 a,fn_vec4 b);
fn_vec4 fn_subVec4(fn_vec4 a,fn_vec4 b);
fn_vec4 fn_vec3Tovec4(fn_vec3 v,float w);
fn_vec4 fn_slerpVec4(
	const fn_vec4 q0,
  const fn_vec4 q1,
	const double            t);
fn_vec4 fn_lerpVec4(fn_vec4 qa,fn_vec4 qb,float t);
