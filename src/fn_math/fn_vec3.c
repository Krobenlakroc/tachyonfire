#include "fn_vec3.h"
#include <math.h>
#include <stdio.h>
#include "fn_common.h"
#define PI 3.14159265358979323846264338327950288


fn_vec3 fn_clampVec3(fn_vec3 a,fn_vec3 b,fn_vec3 c)
{
  return fn_createVec3(fn_clamp(a.x,b.x,c.x),fn_clamp(a.y,b.y,c.y),fn_clamp(a.z,b.z,c.z));
}

fn_vec3 fn_maxVec3(fn_vec3 v,fn_vec3 m)
{
  return fn_createVec3(fmax(v.x,m.x),fmax(v.y,m.y),fmax(v.z,m.z));
}

fn_vec3 fn_minVec3(fn_vec3 v,fn_vec3 m)
{
  return fn_createVec3(fmin(v.x,m.x),fmin(v.y,m.y),fmin(v.z,m.z));
}

void fn_printVec3(fn_vec3 f)
{
  printf("%f %f %f\n",f.x,f.y,f.z );
}

bool fn_bvec3AndAll(fn_vec3 b)
{
  return ((int)b.x && (int)b.y && (int)b.z );
}

void fn_bvec3Increment(fn_vec3* b)
{
  if (b->y == 1 && b->x == 1 && b->z == 0)
    b->z = 1;
  if (b->y == 0 && b->x == 1)
    b->y = 1;
  if (b->x == 0)
    b->x = 1;
}

fn_vec3 fn_createVec3v(float* v)
{
  fn_vec3 r;
  r.v[0] = v[0];
  r.v[1] = v[1];
  r.v[2] = v[2];
  return r;
}

fn_vec3 fn_createVec3(float x,float y,float z)
{
  fn_vec3 v;
  v.v[0] = x;
  v.v[1] = y;
  v.v[2] = z;
  return v;
}
fn_vec3 fn_createVec3s(float x)
{
  fn_vec3 v;
  v.v[0] = x;
  v.v[1] = x;
  v.v[2] = x;
  return v;
}
fn_vec3 fn_addVec3(fn_vec3 a,fn_vec3 b)
{
  fn_vec3 v;

  for (int i = 0;i < 3;i++)
  v.v[i] = a.v[i] + b.v[i];

  return v;
}
fn_vec3 fn_addVec3s(fn_vec3 a,float b)
{
  fn_vec3 v;

  for (int i = 0;i < 3;i++)
  v.v[i] = a.v[i] + b;

  return v;
}
fn_vec3 fn_subVec3(fn_vec3 a,fn_vec3 b)
{
  fn_vec3 v;

  for (int i = 0;i < 3;i++)
    v.v[i] = a.v[i] - b.v[i];

  return v;
}
fn_vec3 fn_multVec3(fn_vec3 a,fn_vec3 b)
{
  fn_vec3 v;

    for (int i = 0;i < 3;i++)
  v.v[i] = a.v[i] * b.v[i];

  return v;
}
fn_vec3 fn_multVec3s(fn_vec3 a,float b)
{
  fn_vec3 v;

  for (int i = 0;i < 3;i++)
  v.v[i] = a.v[i] * b;

  return v;
}

fn_vec3 fn_divVec3s(float a,fn_vec3 b)
{
  fn_vec3 v;

  for (int i = 0;i < 3;i++)
  v.v[i] = a / b.v[i];

  return v;
}
fn_vec3 fn_divVec3(fn_vec3 a,fn_vec3 b)
{
  fn_vec3 v;

  for (int i = 0;i < 3;i++)
  v.v[i] = a.v[i] / b.v[i];

  return v;
}
fn_vec3 fn_normalizeVec3(fn_vec3 x)
{

  float length_of_v = sqrtf((x.x * x.x) + (x.y * x.y) + (x.z * x.z));
  if (isnan(length_of_v) || (length_of_v == 0))
    return fn_createVec3s(0);
  return fn_createVec3(x.x / length_of_v, x.y / length_of_v, x.z / length_of_v);

}

bool fn_equalVec3(fn_vec3 a,fn_vec3 b)
{
  return (a.x == b.x && a.y == b.y && a.z == b.z);
}

bool fn_almostequalVec3(fn_vec3 a,fn_vec3 b,float margin)
{
  return (fabs(a.x - b.x) < margin && fabs(a.y - b.y) < margin && fabs(a.z - b.z) < margin);
}

float fn_dot(fn_vec3 a,fn_vec3 b)
{
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

float fn_angle(fn_vec3 a,fn_vec3 b)
{
  return acos(fn_clamp(fn_dot(a,b),-1.0,1.0));
}
fn_vec3 fn_cross(fn_vec3 a,fn_vec3 b)
{
  fn_vec3 out;

	out.v[0] = a.v[1]*b.v[2] - a.v[2]*b.v[1];
	out.v[1] = a.v[2]*b.v[0] - a.v[0]*b.v[2];
	out.v[2] = a.v[0]*b.v[1] - a.v[1]*b.v[0];
	return out;
}

fn_vec3 fn_lerpVec3(fn_vec3 a,fn_vec3 b,float scale)
{
  return fn_addVec3(fn_multVec3s(a,(1-scale)), fn_multVec3s(b,scale));
}

fn_vec3 fn_unLerpVec3(fn_vec3 a,fn_vec3 b,fn_vec3 pos)
{
  return fn_divVec3(fn_subVec3(pos,a),fn_subVec3(b,a));
}
fn_vec3 fn_cerpVec3(fn_vec3 a,fn_vec3 b,float scale)
{
  float mu2 = (1-cos(scale*PI))/2;
  return fn_addVec3(fn_multVec3s(a,(1-mu2)), fn_multVec3s(b,mu2));
}

fn_vec3 fn_slerpVec3(fn_vec3 a,fn_vec3 b,float t)
{

     float dot = fn_dot(a, b);

     dot = fn_clamp(dot, -1.0f, 1.0f);

     float theta = acosf(dot)*t;
     fn_vec3 RelativeVec = fn_subVec3(b , fn_multVec3s(a,dot));
     RelativeVec = fn_normalizeVec3(RelativeVec);

     return fn_addVec3(fn_multVec3s(a,cosf(theta)) , fn_multVec3s(RelativeVec,sinf(theta)));
}

// fn_vec3 fn_slerpVec3(
// 	 fn_vec3 a,
//    fn_vec3 b,
// 	 float            t)
// {
//   fn_vec3 q;
//
//
// 	double cosHalfTheta = a.v[0]*b.v[0]+a.v[1]*b.v[1]+a.v[2]*b.v[2];
// 	// if qa=qb or qa=-qb then theta = 0 and we can return qa
// 	if (fabs(cosHalfTheta) >= 1.0)
// 	{
// 		q = a;
// 		return q;
// 	}
// 	bool reverse_q1 = false;
// 	if (cosHalfTheta < 0) // Always follow the shortest path
// 	{
// 		reverse_q1 = true;
// 		cosHalfTheta = -cosHalfTheta;
// 	}
// 	// Calculate temporary values.
// 	const double halfTheta = acos(cosHalfTheta);
// 	const double sinHalfTheta = sqrtf(1.0 - powf(cosHalfTheta,2.0));
// 	// if theta = 180 degrees then result is not fully defined
// 	// we could rotate around any axis normal to qa or qb
// 	if (fabs(sinHalfTheta) < 0.001)
// 	{
// 		if (!reverse_q1)
// 			 for (int i=0;i<3;i++) q.v[i] = (1-t)*a.v[i] + t*b.v[i];
// 		else for (int i=0;i<3;i++) q.v[i] = (1-t)*a.v[i] - t*b.v[i];
// 		return q;
// 	}
// 	const double A = sin((1-t) * halfTheta)/sinHalfTheta;
// 	const double B = sin(t*halfTheta)/sinHalfTheta;
// 	if (!reverse_q1)
// 		 for (int i=0;i<3;i++) q.v[i] = A*a.v[i] + B*b.v[i];
// 	else for (int i=0;i<3;i++) q.v[i] = A*a.v[i] - B*b.v[i];
//   return q;
// }

fn_vec3 fn_NlerpVec3(fn_vec3 a,fn_vec3 b,float t)
{
  return fn_normalizeVec3(fn_lerpVec3(a,b,t));
}

fn_vec3 fn_vec3Swizze(fn_vec3 a,fn_Vec3Swizze_t b)
{
  switch (b) {
    case ZYX:
    return fn_createVec3(a.z,a.y,a.x);
    break;
    case ZXY:
    return fn_createVec3(a.z,a.x,a.y);
    break;
    case YXZ:
    return fn_createVec3(a.y,a.x,a.z);
    break;
    case YZX:
    return fn_createVec3(a.y,a.z,a.x);
    break;
    case XZY:
    return fn_createVec3(a.x,a.z,a.y);
    break;
    case XYZ:
    return a;
    break;
  }
  return a;
}

fn_vec3 fn_bounce(fn_vec3 velocity,fn_vec3 normal)
{
  float dot = velocity.x * normal.x + velocity.y * normal.y + velocity.z * normal.z ;
  fn_vec3 u = fn_multVec3s(normal , dot);
  fn_vec3 w = fn_subVec3(velocity , u);
  velocity = fn_multVec3s(fn_subVec3(w , u),1);//reduce bounce height
  return velocity;
}

bool fn_isAxisAligned(fn_vec3 v)
{
  return fn_equalVec3(fn_abs(v), fn_createVec3(1,0,0)) ||
  fn_equalVec3(fn_abs(v), fn_createVec3(0,1,0)) ||
  fn_equalVec3(fn_abs(v), fn_createVec3(0,0,1));
}

fn_vec3 fn_snapVec3(fn_vec3 v)
{
  float greatest = fn_max(fn_max(v.x,v.y),v.z);
  if (greatest == v.x)
  {
    return fn_createVec3(fn_sign(v.x),0,0);
  }
  if (greatest == v.y)
  {
    return fn_createVec3(0,fn_sign(v.y),0);
  }
  if (greatest == v.z)
  {
    return fn_createVec3(0,0,fn_sign(v.z));
  }

  if (fn_almostEqualf(greatest,v.x,0.0001f))
  {
    return fn_createVec3(fn_sign(v.x),0,0);
  }
  if (fn_almostEqualf(greatest,v.y,0.0001f))
  {
    return fn_createVec3(0,fn_sign(v.y),0);
  }
  if (fn_almostEqualf(greatest,v.z,0.0001f))
  {
    return fn_createVec3(0,0,fn_sign(v.z));
  }

  return v;
}

fn_vec3 fn_reflect(fn_vec3 a,fn_vec3 normal)
{
float dot = a.x * normal.x + a.y * normal.y + a.z * normal.z ;
fn_vec3 u = fn_multVec3s(normal , dot);
fn_vec3 w = fn_subVec3(a , u);
a = fn_multVec3(fn_subVec3(w , u),fn_createVec3(1,1,1));
return a;
}

fn_vec3 fn_MA(  fn_vec3 va, float scale,  fn_vec3 vb ) {
  fn_vec3 vc;
	vc.x = va.x + scale*vb.x;
	vc.y = va.y + scale*vb.y;
	vc.z = va.z + scale*vb.z;
  return vc;
}
