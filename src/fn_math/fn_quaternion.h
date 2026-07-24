#pragma once
#include "fn_mat4.h"


fn_quat fn_makeQuaternion(float angle,fn_vec3 axis);

fn_quat fn_mat4toquat(fn_mat4 in);

fn_mat4 fn_rotationMat(fn_quat q);

fn_quat fn_multquat(fn_quat a, fn_quat b);

fn_quat fn_getRotationQuaternion(fn_vec3 from, fn_vec3 to);

fn_quat fn_getRotationQuaternion2(fn_vec3 from, fn_vec3 to);

fn_vec3 fn_rotatePointQuat(fn_vec3 p,fn_quat q);

fn_quat fn_inverseq(fn_quat q);

fn_mat4 th_orientMatrix(fn_vec3 target_vec,fn_vec3* old_ups);
