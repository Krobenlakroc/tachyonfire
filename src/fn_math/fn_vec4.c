#include "fn_vec4.h"
#include <math.h>
#include <stdio.h>
#include "fn_common.h"


void fn_printVec4(fn_vec4 v)
{
  printf("%f %f %f %f\n",v.x,v.y,v.z,v.w );
}
fn_vec4 fn_vec3Tovec4(fn_vec3 v,float w)
{
  fn_vec4 out;
  out.xyz = v;
  out.w = w;
  return out;
}

fn_vec3 fn_createVec3v4(fn_vec4 v)
{
  fn_vec3 r;
  r.v[0] = v.x;
  r.v[1] = v.y;
  r.v[2] = v.z;
  return r;
}

fn_vec4 fn_createVec4(float x,float y,float z,float w)
{
  fn_vec4 v;
  v.v[0] = x;
  v.v[1] = y;
  v.v[2] = z;
  v.v[3] = w;
  return v;
}

fn_vec4 fn_createVec4Vec3(fn_vec3 a,float w)
{
  fn_vec4 v;
  v.xyz = a;
  v.w = w;
  return v;
}

fn_vec4 fn_addVec4(fn_vec4 a,fn_vec4 b)
{
  fn_vec4 v;

  for (int i = 0;i < 4;i++)
  v.v[i] = a.v[i] + b.v[i];

  return v;
}

fn_vec4 fn_subVec4(fn_vec4 a,fn_vec4 b)
{
  fn_vec4 v;

  for (int i = 0;i < 4;i++)
  v.v[i] = a.v[i] - b.v[i];

  return v;
}

fn_vec4 fn_multVec4(fn_vec4 a,fn_vec4 b)
{
  fn_vec4 v;

  for (int i = 0;i < 4;i++)
  v.v[i] = a.v[i] * b.v[i];

  return v;
}

fn_vec4 fn_normalizeVec3Magnitude(fn_vec3 x)
{
  float length_of_v = sqrtf((x.x * x.x) + (x.y * x.y) + (x.z * x.z));
  if (isnan(length_of_v) || (length_of_v == 0))
    return fn_createVec4(0,0,0,0);
  return fn_createVec4(x.x / length_of_v, x.y / length_of_v, x.z / length_of_v,length_of_v);
}


fn_vec4 fn_normalizeVec4(fn_vec4 x)
{
  fn_vec4 v;
  float sqr = x.v[0] * x.v[0] + x.v[1] * x.v[1] + x.v[2] * x.v[2] + x.v[3] * x.v[3];
  float invrt = 1.f/sqrtf(sqr);

  for (int i = 0;i < 4;i++)
  v.v[i] = x.v[i]*invrt;

  return v;
}

bool fn_equalVec4(fn_vec4 a,fn_vec4 b)
{
  return (a.v[0] == b.v[0] && a.v[1] == b.v[1] && a.v[2] == b.v[2] && a.v[3] == b.v[3]);
}

fn_vec4 fn_multVec4s(fn_vec4 a,float b)
{
  fn_vec4 v;

  for (int i = 0;i < 4;i++)
  v.v[i] = a.v[i] * b;

  return v;
}

fn_vec4 fn_lerpVec4(fn_vec4 qa,fn_vec4 qb,float t)
{
  return fn_addVec4(fn_multVec4s(qa,(1-t)), fn_multVec4s(qb,t));
}

float fn_dotVec4(fn_vec4 a,fn_vec4 b)
{
  return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

// fn_vec4 fn_slerpVec4(fn_vec4 a,fn_vec4 b,float t)
// {
//   fn_vec4 q;
//   float dot = fn_dotVec4(b, a);
//
//   fn_vec4 a2 = a;
//
//   if (dot < 0) {
//     a2 = fn_multVec4s(a,-1);
//     dot = -dot;
//   }
//
//   if (dot > 0.9995) {
//     q = fn_lerpVec4( a2, b, t);
//     return q;
//   }
//
//   dot = fn_min(fn_max(dot, -1.0f), 1.0f);
//
//   float theta = acos(dot) * t;
//
//   fn_vec4 c;
//   c = fn_multVec4s( a2, dot);
//   c = fn_subVec4( b, c);
//   c = fn_normalizeVec4( c);
//
//   a2 = fn_multVec4s( a2, cos(theta));
//   c = fn_multVec4s( c, sin(theta));
//   q = fn_addVec4( a, c);
// return q;
// }

fn_vec4 fn_slerpVec4(
	const fn_vec4 q0,
  const fn_vec4 q1,
	const double            t)
{
  fn_vec4 q;


	double cosHalfTheta = q0.v[0]*q1.v[0]+q0.v[1]*q1.v[1]+q0.v[2]*q1.v[2]+q0.v[3]*q1.v[3];
	// if qa=qb or qa=-qb then theta = 0 and we can return qa
	if (fabs(cosHalfTheta) >= 1.0)
	{
		q = q0;
		return q;
	}
	bool reverse_q1 = false;
	if (cosHalfTheta < 0) // Always follow the shortest path
	{
		reverse_q1 = true;
		cosHalfTheta = -cosHalfTheta;
	}
	// Calculate temporary values.
	const double halfTheta = acos(cosHalfTheta);
	const double sinHalfTheta = sqrtf(1.0 - powf(cosHalfTheta,2.0));
	// if theta = 180 degrees then result is not fully defined
	// we could rotate around any axis normal to qa or qb
	if (fabs(sinHalfTheta) < 0.001)
	{
		if (!reverse_q1)
			 for (int i=0;i<4;i++) q.v[i] = (1-t)*q0.v[i] + t*q1.v[i];
		else for (int i=0;i<4;i++) q.v[i] = (1-t)*q0.v[i] - t*q1.v[i];
		return q;
	}
	const double A = sin((1-t) * halfTheta)/sinHalfTheta;
	const double B = sin(t*halfTheta)/sinHalfTheta;
	if (!reverse_q1)
		 for (int i=0;i<4;i++) q.v[i] = A*q0.v[i] + B*q1.v[i];
	else for (int i=0;i<4;i++) q.v[i] = A*q0.v[i] - B*q1.v[i];
  return q;
}
