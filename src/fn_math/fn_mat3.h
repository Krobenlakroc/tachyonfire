#pragma once
#include "fn_mat4.h"
typedef union
{
  float m[9];
}fn_mat3;

fn_mat3 fn_identityMat3();

fn_mat3 fn_makescalerotateMat3(fn_vec3 scale,float angle,fn_vec3 axis);

fn_mat3 fn_getMat3Mat4(fn_mat4 m);

fn_mat3 fn_multMat3(fn_mat3 a,fn_mat3 b);

fn_vec3 fn_multMat3Vec3(fn_mat3 a,fn_vec3 b);

fn_mat4 fn_getMat4Mat3(fn_mat3 m);
