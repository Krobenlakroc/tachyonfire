#pragma once
#include <stdbool.h>
typedef union
{
  float v[2];
  struct {
  float x;
  float y;
  };
}fn_vec2;

typedef union
{
  int v[2];
  struct {
  int x;
  int y;
  };
}fn_ivec2;

fn_vec2 fn_createVec2(float x,float y);
fn_vec2 fn_addVec2(fn_vec2 a,fn_vec2 b);
fn_vec2 fn_subVec2(fn_vec2 a,fn_vec2 b);
fn_vec2 fn_multVec2(fn_vec2 a,fn_vec2 b);
fn_vec2 fn_multVec2s(fn_vec2 a,float b);
fn_vec2 fn_divVec2(fn_vec2 a,fn_vec2 b);
bool fn_equalVec2(fn_vec2 a,fn_vec2 b);
bool fn_almostequalVec2(fn_vec2 a,fn_vec2 b,float epsilon);

void fn_printVec2(fn_vec2 v);

fn_vec2 fn_floorVec2(fn_vec2 a);

float fn_lengthVec2(fn_vec2 a);

fn_vec2 fn_normalizeVec2(fn_vec2 x);

float fn_dotVec2(fn_vec2 a,fn_vec2 b);

fn_vec2 fn_unLerpVec2(fn_vec2 a,fn_vec2 b,fn_vec2 point);

fn_vec2 fn_lerpVec2(fn_vec2 a,fn_vec2 b,float x);
