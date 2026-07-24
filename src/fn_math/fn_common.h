#pragma once
#include "fn_vec3.h"
#include "fn_vec2.h"
#include "fn_quaternion.h"
#include "fn_mat3.h"

float fn_remap(float x,float omin,float omax,float newmin,float newmax);

float fn_clamp(float x,float y,float z);

float fn_radians(float x);

float fn_degrees(float x);

fn_vec3 fn_abs(fn_vec3 v);

fn_vec3 fn_signVec3(fn_vec3 v);

fn_vec3 fn_floorVec3(fn_vec3 v);

float fn_sign(float x);

float fn_length(fn_vec3 v);

float fn_distanceVec2(fn_vec2 a,fn_vec2 b);
float fn_distance(fn_vec3 a,fn_vec3 b);
float fn_distance2(fn_vec3 a,fn_vec3 b);
float fn_length2(fn_vec3 v);

float fn_lerp(float a,float b,float t);

float fn_unlerp(float a,float b,float p);

double fn_unlerpd(double a,double b,double p);

void fn_removeErrorsVec3(fn_vec3* v);

float fn_max(float a,float b);

int fn_clampi(int x,int y,int z);

float fn_min(float a,float b);

fn_vec3 fn_minVec3s(fn_vec3 a,float b);

float fn_switch(float* f);

fn_vec2 fn_lookAngles(fn_vec3 pos,fn_vec3 target);

float fn_almostEqualf(float a,float b,float margin);

float fn_pointInPlane(fn_vec3 A,fn_vec3 B,fn_vec3 P);

float fn_round(float f);

float fn_step(float f,float incr);

float fn_cantorPair(float k1,float k2);

fn_vec2 fn_cantorUnPair(float i);

//fix rounding error for axial planes
fn_vec3 fn_cleanUpNormal(fn_vec3 n);

int fn_maxi(int a,int b);

void fn_SampleCone(fn_vec3 dir, float half_angle,
                   int n_rings, int n_per_ring,
                   fn_vec3 *out);


fn_vec3 th_computeBlackBody(float interp,float max_temp);

void th_computeBlackBodyLookupTable(int steps);
