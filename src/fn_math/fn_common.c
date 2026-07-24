#include "fn_common.h"
#include <math.h>
#include <stdio.h>

float fn_remap(float x,float omin,float omax,float newmin,float newmax)
{
  float rr = (fn_clamp(x,omin,omax) - omin)/(omax - omin);
  return rr*(newmax - newmin) + newmin;
}

fn_vec3 fn_floorVec3(fn_vec3 v)
{
  return fn_createVec3(floorf(v.x),floorf(v.y),floorf(v.z));
}
float compress_10(float x)
{
  return x*100 + 1;
}

float uncompress_10(float x)
{
  return (x-1)/100;
}

double compress(double x)
{
  return x + 1;
}

double uncompress(double x)
{
  return x - 1;
}

float fn_cantorPair(float k1,float k2)
{
  k1 = compress_10(k1);
  k2 = compress(k2);
  return ((k1 + k2)*(k1 + k2 + 1.0))/2.0 + k2;
}

fn_vec2 fn_cantorUnPair(float i)
{
  float w = floor((sqrt(8.0*i + 1.0) - 1.0)/2.0);
  float t = (w*w + w)/2.0;
  float y = i - t;
  float x = w - y;
  x = uncompress_10(x);
  y = uncompress(y);
  return fn_createVec2(x,y);
}

float fn_clamp(float x,float y,float z)
{
  const float t = x < y ? y : x;
  return t > z ? z : t;
}

int fn_clampi(int x,int y,int z)
{
  const int t = x < y ? y : x;
  return t > z ? z : t;
}

float fn_radians(float x)
{
  return x*0.01745329;
}

float fn_degrees(float x)
{
  return x*57.29578;
}

fn_vec3 fn_abs(fn_vec3 v)
{
  return fn_createVec3(fabs(v.x),fabs(v.y),fabs(v.z));
}

fn_vec3 fn_signVec3(fn_vec3 v)
{
  return fn_createVec3(fn_sign(v.x),fn_sign(v.y),fn_sign(v.z));
}

float fn_sign(float x)
{
  if (x == 0.f)
    return 0.f;
  return x > 0 ? 1 : -1;
}

float fn_length(fn_vec3 v)
{
  float x = sqrtf(v.v[0] * v.v[0] + v.v[1] * v.v[1] + v.v[2] * v.v[2]);
  return x;
}

float fn_length2(fn_vec3 v)
{
    return v.v[0] * v.v[0] + v.v[1] * v.v[1] + v.v[2] * v.v[2];
}

float fn_lerp(float a,float b,float t)
{
  if (a ==b )
  return a;
  return (1-t)*a + t*b;
}
float fn_unlerp(float a,float b,float p)
{
  return ((p-a)/(b-a));
}

double fn_unlerpd(double a,double b,double p)
{
  return ((p-a)/(b-a));
}

float fn_max(float a,float b)
{
  return (a<b)?b:a;
}

int fn_maxi(int a,int b)
{
  return (a<b)?b:a;
}

float fn_min(float a,float b)
{
  return (a>b)?b:a;
}

float fn_switch(float* f)
{
  if (*f == 0)
  {
    *f = 1;
    return *f;
  }
  if (*f == 1)
  {
    *f = 0;
    return *f;
  }
  return 0;
}

fn_vec2 fn_lookAngles(fn_vec3 pos,fn_vec3 target)
{
  fn_vec3 diff = fn_normalizeVec3(fn_subVec3(pos,target));
  float xangle = acos(fn_dot(fn_createVec3(0,0,-1),fn_normalizeVec3(fn_createVec3(diff.x,0,diff.z)))) * fn_sign(diff.x);
  float yangle = acos(fn_dot(diff,fn_normalizeVec3(fn_createVec3(diff.x,0,diff.z)))) * fn_sign(diff.y);

  return fn_createVec2(xangle,yangle);
}

float fn_almostEqualf(float a,float b,float margin)
{
  return fabs(a - b) < margin;
}

float fn_pointInPlane(fn_vec3 A,fn_vec3 B,fn_vec3 P)
{
  return fn_dot(fn_subVec3(A,B),P);
}

float fn_round(float f)
{
  return (float)((f >= 0) ? (int)(f + 0.5) : (int)(f - 0.5));
}

float fn_step(float f,float incr)
{
  float ipart = ceil(f);

  float p1 = ipart - incr;
  float p2 = ipart;
  float p3 = ipart + incr;

  if (fabs(f - p1) < fabs(f - p2) && fabs(f - p1) < fabs(f - p3))
    return p1;

  if (fabs(f - p3) < fabs(f - p2) && fabs(f - p3) < fabs(f - p1))
    return p3;

  return p2;
}

fn_vec3 fn_minVec3s(fn_vec3 a,float b)
{
  a.x = fn_min(a.x,b);
  a.y = fn_min(a.y,b);
  a.z = fn_min(a.z,b);
  return a;
}

float fn_distanceVec2(fn_vec2 a,fn_vec2 b)
{
  return fn_lengthVec2(fn_subVec2(a,b));
}

float fn_distance(fn_vec3 a,fn_vec3 b)
{
  return fn_length(fn_subVec3(a,b));
}

float fn_distance2(fn_vec3 a,fn_vec3 b)
{
  return fn_length2(fn_subVec3(a,b));
}


void fn_removeErrorsVec3(fn_vec3* v)
{
  int msk = 0;

  if (isnan(v->x) || isinf(v->x))
  {

      msk |= 1 << 0;
  }
  if (isnan(v->y) || isinf(v->y))
  {
    msk |= 1 << 1;
  }
  if (isnan(v->z) || isinf(v->z))
  {


    msk |= 1 << 2;
  }
  // if (msk == 0x1)
  // {
  //   v->x = v->y;
  // }
  // if (msk == 0x2)
  // {
  //   v->y = v->x;
  // }
  // if (msk == 0x3)
  // {
  //   v->z = v->x;
  // }

  // if (msk & 0x1 && msk & 0x2)
  // {
  //   v->x = v->z;
  //   v->y = v->z;
  // }
  // else if (msk & 0x2 && msk & 0x3)
  // {
  //   v->y = v->x;
  //   v->z = v->x;
  // }
  // else if (msk & 0x1 && msk & 0x3)
  // {
  //   v->z = v->y;
  //   v->x = v->y;
  // }
  //  if (msk & 0x1)
  // {
  //   v->x = v->y;
  // }
  //  if (msk & 0x2)
  // {
  //   v->y = v->x;
  // }
  //  if (msk & 0x3)
  // {
  //   v->z = v->x;
  // }
}

fn_vec3 fn_cleanUpNormal(fn_vec3 n)
{
  if (n.x == -1.0 || n.x == 1.0)
  {
    return fn_createVec3(n.x,0,0);
  }
  else if (n.y == -1.0 || n.y == 1.0)
  {
    return fn_createVec3(0,n.y,0);
  }
  else if (n.z == -1.0 || n.z == 1.0)
  {
    return fn_createVec3(0,0,n.z);
  }
  return n;
}


static fn_vec3 rotate_to_dir(fn_vec3 v, fn_vec3 dir) {
  dir = fn_normalizeVec3(dir);

  /* If dir is already +Z, nothing to do */
  if (fabs(dir.z - 1.0) < 1e-9) return v;

  /* If dir is -Z, flip */
  if (fabs(dir.z + 1.0) < 1e-9) return fn_createVec3(-v.x, -v.y, -v.z );

  /* Axis = +Z cross dir, angle = acos(dir.z) */
  fn_vec3 axis = fn_normalizeVec3(fn_createVec3( -dir.y, dir.x, 0.0 ));
  float cos_a = dir.z;                  /* dot(+Z, dir) */
  float sin_a = sqrt(1.0 - cos_a*cos_a);


  float dot = axis.x*v.x + axis.y*v.y + axis.z*v.z;
  fn_vec3 cross = fn_createVec3(
    axis.y*v.z - axis.z*v.y,
    axis.z*v.x - axis.x*v.z,
    axis.x*v.y - axis.y*v.x
    );
  return fn_createVec3(
    v.x*cos_a + cross.x*sin_a + axis.x*dot*(1.0 - cos_a),
    v.y*cos_a + cross.y*sin_a + axis.y*dot*(1.0 - cos_a),
    v.z*cos_a + cross.z*sin_a + axis.z*dot*(1.0 - cos_a)
         );
}


void fn_SampleCone(fn_vec3 dir, float half_angle,
                 int n_rings, int n_per_ring,
                 fn_vec3 *out)
{
  float cos_max = cos(half_angle);
  int idx = 0;

  for (int i = 0; i < n_rings; i++) {
    float t      = (i + 0.5) / n_rings;
    float cos_th = 1.0 - t * (1.0 - cos_max);
    float sin_th = sqrt(1.0 - cos_th * cos_th);

    for (int j = 0; j < n_per_ring; j++) {

      float phi = 2.0 * 3.141596 * (j + (i % 2) * 0.5) / n_per_ring;


      fn_vec3 local = fn_createVec3(
        sin_th * cos(phi),
        sin_th * sin(phi),
        cos_th
        );


      out[idx++] = rotate_to_dir(local, dir);
    }
  }
}

static fn_vec3 blackbody(float T)
{
  float t = T / 100.0;

  fn_vec3 c;

  // Red
  if (t <= 66.0)
    c.x = 1.0;
  else
    c.x = fn_clamp(1.292936 * pow(t - 60.0, -0.133205), 0.0, 1.0);

  // Green
  if (t <= 66.0)
    c.y = fn_clamp(0.390081 * log(t) - 0.631841, 0.0, 1.0);
  else
    c.y = fn_clamp(1.129890 * pow(t - 60.0, -0.075514), 0.0, 1.0);

  // Blue
  if (t >= 66.0)
    c.z = 1.0;
  else if (t <= 19.0)
    c.z = 0.0;
  else
    c.z = fn_clamp(0.543206 * log(t - 10.0) - 1.196254, 0.0, 1.0);

  return c;
}

fn_vec3 th_computeBlackBody(float interp,float max_temp){
  float temp = fn_clamp(interp,0.0,1.0)*max_temp;
  float intensity = pow(temp / 1000.0, 4.0);

  return fn_multVec3s(fn_multVec3(blackbody(temp),fn_createVec3(1.1,1.0,0.9)),intensity);
}


void th_computeBlackBodyLookupTable(int steps)
{
  printf("vec3 blackbody_lookup[%i] = {",steps);
  for (int i = 0 ; i < steps;i++)
  {
    float t_step = (float)i/(float)(steps - 1);

    fn_vec3 bb = th_computeBlackBody(t_step,3000);
    printf("vec3(%f,%f,%f),",bb.x,bb.y,bb.z);

  }

  printf("};\n");
}
