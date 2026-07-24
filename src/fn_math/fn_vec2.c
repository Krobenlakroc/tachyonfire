#include "fn_vec2.h"
#include <math.h>
#include <stdio.h>
fn_vec2 fn_createVec2(float x,float y)
{
  fn_vec2 v;
  v.v[0] = x;
  v.v[1] = y;
  return v;
}
fn_vec2 fn_normalizeVec2(fn_vec2 x)
{
  float length_of_v = sqrtf((x.x * x.x) + (x.y * x.y));
  if (isnan(length_of_v) || (length_of_v == 0))
    return fn_createVec2(0,0);
  return fn_createVec2(x.x / length_of_v, x.y / length_of_v);
}

float fn_lengthVec2(fn_vec2 a)
{
  return sqrtf((a.x * a.x) + (a.y * a.y));
}
fn_vec2 fn_addVec2(fn_vec2 a,fn_vec2 b)
{
  fn_vec2 v;
  v.v[0] = a.v[0] + b.v[0];
  v.v[1] = a.v[1] + b.v[1];
  return v;
}

fn_vec2 fn_subVec2(fn_vec2 a,fn_vec2 b)
{
  fn_vec2 v;
  v.v[0] = a.v[0] - b.v[0];
  v.v[1] = a.v[1] - b.v[1];
  return v;
}

fn_vec2 fn_multVec2(fn_vec2 a,fn_vec2 b)
{
  fn_vec2 v;
  v.v[0] = a.v[0] * b.v[0];
  v.v[1] = a.v[1] * b.v[1];
  return v;
}

fn_vec2 fn_multVec2s(fn_vec2 a,float b)
{
  fn_vec2 v;
  v.v[0] = a.v[0] * b;
  v.v[1] = a.v[1] * b;
  return v;
}

bool fn_equalVec2(fn_vec2 a,fn_vec2 b)
{
  return (a.x == b.x && a.y == b.y);
}

bool fn_almostequalVec2(fn_vec2 a,fn_vec2 b,float epsilon)
{
  return (fabs(a.x - b.x) < epsilon && fabs(a.y - b.y) < epsilon);
}

float fn_dotVec2(fn_vec2 a,fn_vec2 b)
{
  return a.x*b.x + a.y*b.y ;
}

fn_vec2 fn_divVec2(fn_vec2 a,fn_vec2 b)
{
  fn_vec2 v;
  v.v[0] = a.v[0] / b.v[0];
  v.v[1] = a.v[1] / b.v[1];
  return v;
}

fn_vec2 fn_unLerpVec2(fn_vec2 a,fn_vec2 b,fn_vec2 point)
{
  return fn_divVec2(fn_subVec2(point,a),fn_subVec2(b,a));
}

void fn_printVec2(fn_vec2 v)
{
  printf("%f %f\n",v.x,v.y );
}

fn_vec2 fn_floorVec2(fn_vec2 a)
{
  return fn_createVec2(floor(a.x),floor(a.y));
}

fn_vec2 fn_lerpVec2(fn_vec2 a,fn_vec2 b,float x)
{
  return fn_addVec2(fn_multVec2s(a,(1-x)), fn_multVec2s(b,x));
}
