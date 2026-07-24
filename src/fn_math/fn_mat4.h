#pragma once
#include "fn_vec3.h"
#include "fn_vec4.h"
#include <math.h>
typedef union
{
  float m[16];
  fn_vec4 v[4];
}fn_mat4;

int fn_getMat4Index(int x,int y);

fn_mat4 fn_identityMat4();

fn_mat4 fn_multMat4(fn_mat4 a,fn_mat4 b);

fn_mat4 fn_rotate(fn_mat4 mat,float angle,fn_vec3 axis);

fn_mat4 fn_makerotate(float angle,fn_vec3 axis);

fn_mat4 fn_makescale(fn_vec3 scale);

fn_mat4 fn_maketranslaterotate(fn_vec3 t,float angle,fn_vec3 axis);

fn_mat4 fn_translaterotatescale(fn_vec3 t,float angle,fn_vec3 axis,fn_vec3 s);
fn_mat4 fn_translaterotatescaleq(fn_vec3 t,fn_quat q,fn_vec3 s);
fn_mat4 fn_translaterotatescalem(fn_vec3 t,fn_mat4 m,fn_vec3 s);

fn_mat4 fn_translate(fn_mat4 mat,fn_vec3 t);

fn_mat4 fn_maketranslate(fn_vec3 t);

fn_mat4 fn_scale(fn_mat4 mat,fn_vec3 s);

fn_mat4 fn_translatescale(fn_vec3 t,fn_vec3 s);

fn_mat4 fn_perspective(float fov,float aspect_ratio,float near,float far);

fn_mat4 fn_ortho(float left,float right,float bottom,float top);

fn_mat4 fn_ortho3D(float left,float right,float bottom,float top,float far,float near);

fn_mat4 fn_lookat(fn_vec3 pos,fn_vec3 look,fn_vec3 up);

fn_mat4 fn_inverse(fn_mat4 in);

fn_mat4 fn_transpose(fn_mat4 m);

fn_vec3 fn_transformVec3(fn_vec3 v,fn_mat4 m);

fn_vec3 fn_transformNormal(fn_vec3 v,fn_mat4 m);

fn_vec4 fn_multVec4Mat4(fn_mat4 a,fn_vec4 b);

bool fn_isIdentity(fn_mat4 a);

bool fn_isNullMatrix(fn_mat4 a);

fn_mat4 fn_infPerspective(float fov,float aspect_ratio,float near);

fn_mat4 fn_sphericalHarmonicMatrix(float* coeffs);

fn_mat4 fn_createMat4(float f0,float f1,float f2,float f3,float f4,float f5,float f6,float f7,float f8,float f9,float f10,float f11,float f12,float f13,float f14,float f15);

float fn_determinant(fn_mat4 in);

void fn_printMat4(fn_mat4 a);

fn_mat4 fn_makeMat4Rows(fn_vec4 a,fn_vec4 b,fn_vec4 c,fn_vec4 d);

typedef struct
{
  fn_vec3 translate;
  fn_vec3 scale;
  fn_quat rotate;
}fn_Transform;

fn_Transform fn_decomposeMat4(fn_mat4 m);

fn_Transform fn_identityTransform();

fn_mat4 fn_transformToMat(fn_Transform t);

fn_Transform fn_lerpTransforms(fn_Transform a,fn_Transform b,float f);

fn_Transform fn_multTransform(fn_Transform a,fn_Transform b);

fn_Transform fn_interpTransforms(fn_Transform* t,float* alphas,int count,float f);

fn_vec3 fn_getTranslationMat4(fn_mat4 m);
