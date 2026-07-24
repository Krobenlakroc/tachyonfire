#pragma once

#include "../fn_math/fn_math.h"

void computeUpVectors(fn_vec3* course,fn_vec3* upvectors,fn_vec3 first_up,fn_vec3 first_side,fn_vec3 first_forward,int count_course);

float  CalculatePosition1D(float p0, float p1, float p2, float p3, float t /* between 0 and 1 */);

fn_vec3  CalculatePosition(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */);

fn_vec3 CalculateTangent(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */);

fn_vec3 CalculateJerk(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */);

fn_vec3 CalculateAccel(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */);

fn_vec3 CalculateTangentUnNorm(fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */);

fn_mat4 th_6dofCamera(fn_vec3* old_forward,fn_vec3* old_right,fn_vec3* old_up,fn_vec3 new_forward,fn_vec3 pos);

fn_vec3 CatmullRom( fn_vec3 p0, fn_vec3 p1, fn_vec3 p2, fn_vec3 p3, float t /* between 0 and 1 */);

float GetT( float t, float alpha, fn_vec3  p0, fn_vec3 p1 );
