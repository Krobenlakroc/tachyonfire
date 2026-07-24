#pragma once
#include "fn_vec3.h"
#include "fn_mat4.h"

typedef enum
{
  SPLINE_LOOKAT = 0,
  SPLINE_FORWARD = 1,
  SPLINE_CONSTDIR = 2
}fn_SplineLookMode;

typedef struct
{
  float interp;
  int cprog;
  fn_vec3* course;
  int course_count;
  float speed;
  fn_vec3 pos;
  fn_vec3 forward;

  fn_vec3 targetpos;
  fn_SplineLookMode mode;

}fn_SplineState;

fn_mat4 fn_splineCamera(float dt,fn_SplineState* info);

void fn_defaultSplineState(fn_SplineState* st);
