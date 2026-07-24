#include "fn_mat4.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "../fn_engine/fn_profile.h"
#include "fn_quaternion.h"
#include "fn_common.h"
static int setMultithread = 0;
static int multithread = 0;
int fn_getMat4Index(int x,int y)
{
  return (y*4)+x;
}

fn_mat4 fn_makeMat4Rows(fn_vec4 a,fn_vec4 b,fn_vec4 c,fn_vec4 d)
{
  fn_mat4 ret;
  memcpy(&ret.m[0],&a,sizeof(float)*4);
  memcpy(&ret.m[4],&b,sizeof(float)*4);
  memcpy(&ret.m[8],&c,sizeof(float)*4);
  memcpy(&ret.m[12],&d,sizeof(float)*4);
  // temp.m[0] = f0;
  // temp.m[1] = f1;
  // temp.m[2] = f2;
  // temp.m[3] = f3;
  // temp.m[4] = f4;
  // temp.m[5] = f5;
  // temp.m[6] = f6;
  // temp.m[7] = f7;
  // temp.m[8] = f8;
  // temp.m[9] = f9;
  // temp.m[10] = f10;
  // temp.m[11] = f11;
  // temp.m[12] = f12;
  // temp.m[13] = f13;
  // temp.m[14] = f14;
  // temp.m[15] = f15;
  return ret;
}

fn_mat4 fn_createMat4(float f0,float f1,float f2,float f3,float f4,float f5,float f6,float f7,float f8,float f9,float f10,float f11,float f12,float f13,float f14,float f15)
{
  fn_mat4 temp;
  temp.m[0] = f0;
  temp.m[1] = f1;
  temp.m[2] = f2;
  temp.m[3] = f3;
  temp.m[4] = f4;
  temp.m[5] = f5;
  temp.m[6] = f6;
  temp.m[7] = f7;
  temp.m[8] = f8;
  temp.m[9] = f9;
  temp.m[10] = f10;
  temp.m[11] = f11;
  temp.m[12] = f12;
  temp.m[13] = f13;
  temp.m[14] = f14;
  temp.m[15] = f15;
  return temp;
}

fn_mat4 fn_identityMat4()
{
  fn_mat4 temp;
  temp.m[0] = 1;
  temp.m[1] = 0;
  temp.m[2] = 0;
  temp.m[3] = 0;
  temp.m[4] = 0;
  temp.m[5] = 1;
  temp.m[6] = 0;
  temp.m[7] = 0;
  temp.m[8] = 0;
  temp.m[9] = 0;
  temp.m[10] = 1;
  temp.m[11] = 0;
  temp.m[12] = 0;
  temp.m[13] = 0;
  temp.m[14] = 0;
  temp.m[15] = 1;
  return temp;
}

fn_mat4 fn_multMat4(fn_mat4 a,fn_mat4 b)
{
  int i,j;
  // if (!setMultithread)
  // {
  //   setMultithread = 1;
  //   multithread = (int)fn_getProfileVar("multithread");
  // }
   fn_mat4 ret;
  // #pragma omp parallel for private(j) if (multithread)
  for (j = 0;j < 4;j++)
  {


    for (i = 0;i < 4;i++)
    ret.m[i+j*4]= a.m[j*4]*b.m[i] + a.m[j*4 + 1]*b.m[i+4] + a.m[j*4 + 2]*b.m[i+8] + a.m[j*4 + 3]*b.m[i+12];

  }

  // for (i = 0;i < 16;i++)
  //    ret.m[i]= a.m[RW(i)*4]*b.m[i%4] + a.m[RW(i)*4 + 1]*b.m[i%4+4] + a.m[RW(i)*4 + 2]*b.m[i%4+8] + a.m[RW(i)*4 + 3]*b.m[i%4+12];

  return ret;
}

fn_mat4 fn_rotate(fn_mat4 mat,float angle,fn_vec3 axis)
{

  fn_mat4 rotation = fn_identityMat4();
  float sine = (float)sinf(angle);
  float cosine = (float)cosf(angle);

  if (fn_equalVec3(axis,fn_createVec3(1,0,0)))
  {
    rotation.m[5] = cosine;
    rotation.m[6] = -sine;
    rotation.m[9] = sine;
    rotation.m[10] = cosine;
  }




  if (fn_equalVec3(axis,fn_createVec3(0,1,0)))
  {
    rotation.m[0] = cosine;
    rotation.m[8] = sine;
    rotation.m[2] = -sine;
    rotation.m[10] = cosine;
  }




  if (fn_equalVec3(axis,fn_createVec3(0,0,1)))
  {
    rotation.m[0] = cosine;
    rotation.m[1] = -sine;
    rotation.m[4] = sine;
    rotation.m[5] = cosine;
  }

  return fn_multMat4(rotation,mat);

}

fn_mat4 fn_makerotate(float angle,fn_vec3 axis)
{
  fn_mat4 ret = fn_rotationMat(fn_makeQuaternion(angle,axis));
  return ret;
  // fn_mat4 rotation = fn_identityMat4();
  // float sine = (float)sinf(angle);
  // float cosine = (float)cosf(angle);
  //
  // if (fn_almostequalVec3(axis,fn_createVec3(1,0,0),0.1f))
  // {
  //   rotation.m[5] = cosine;
  //   rotation.m[6] = -sine;
  //   rotation.m[9] = sine;
  //   rotation.m[10] = cosine;
  // }
  //
  //
  //
  //
  // if (fn_almostequalVec3(axis,fn_createVec3(0,1,0),0.1f))
  // {
  //   rotation.m[0] = cosine;
  //   rotation.m[8] = sine;
  //   rotation.m[2] = -sine;
  //   rotation.m[10] = cosine;
  // }
  //
  //
  //
  //
  // if (fn_almostequalVec3(axis,fn_createVec3(0,0,1),0.1f))
  // {
  //   rotation.m[0] = cosine;
  //   rotation.m[1] = -sine;
  //   rotation.m[4] = sine;
  //   rotation.m[5] = cosine;
  // }
  // return rotation;
}

fn_mat4 fn_maketranslaterotate(fn_vec3 t,float angle,fn_vec3 axis)
{
  fn_mat4 ret = fn_rotationMat(fn_makeQuaternion(angle,axis));

  ret.m[12] = t.x;
  ret.m[13] = t.y;
  ret.m[14] = t.z;
  return ret;
}

fn_mat4 fn_translate(fn_mat4 mat,fn_vec3 t)
{
  fn_mat4 temp = fn_identityMat4();
  temp.m[12] = t.x;
  temp.m[13] = t.y;
  temp.m[14] = t.z;
  return fn_multMat4(temp,mat);
}

fn_mat4 fn_maketranslate(fn_vec3 t)
{
  fn_mat4 temp = fn_identityMat4();
  temp.m[12] = t.x;
  temp.m[13] = t.y;
  temp.m[14] = t.z;
  return temp;
}

fn_mat4 fn_makescale(fn_vec3 s)
{
  fn_mat4 temp = fn_identityMat4();
  temp.m[0] = s.x;
  temp.m[5] = s.y;
  temp.m[10] = s.z;
  return temp;
}

fn_mat4 fn_scale(fn_mat4 mat,fn_vec3 s)
{
  fn_mat4 temp = fn_identityMat4();
  temp.m[0] = s.x;
  temp.m[5] = s.y;
  temp.m[10] = s.z;
  return fn_multMat4(temp,mat);
}

fn_mat4 fn_translatescale(fn_vec3 t,fn_vec3 s)
{
  fn_mat4 temp = fn_identityMat4();
  temp.m[0] = s.x;
  temp.m[5] = s.y;
  temp.m[10] = s.z;
  temp.m[12] = t.x;
  temp.m[13] = t.y;
  temp.m[14] = t.z;
  return temp;
}

fn_mat4 fn_perspective(float fov,float aspect_ratio,float near,float far)
{



  const float tanHalfFovy = tanf(fov / 2);

  fn_mat4 Result = fn_identityMat4();



  Result.m[0] = 1.f / (aspect_ratio * tanHalfFovy);
  Result.m[5] = 1.f / (tanHalfFovy);
  Result.m[10] = -(far + near) / (far - near);
  Result.m[11] = - 1.f;
  Result.m[14] = -2*(far * near) / (far - near);
  Result.m[15] = 0;

  return Result;

//   float xymax = near * tanf(fov/2);
// float ymin = -xymax;
// float xmin = -xymax;
//
// float width = xymax - xmin;
// float height = xymax - ymin;
//
// float depth = far - near;
// float q = -(far + near) / depth;
// float qn = -2 * (far * near) / depth;
//
// float w = 2 * near / width;
// w = w / aspect_ratio;
// float h = 2 * near / height;
//
// fn_mat4 Result;
// Result.m[0]  = w;
// Result.m[1]  = 0;
// Result.m[2]  = 0;
// Result.m[3]  = 0;
//
// Result.m[4]  = 0;
// Result.m[5]  = h;
// Result.m[6]  = 0;
// Result.m[7]  = 0;
//
// Result.m[8]  = 0;
// Result.m[9]  = 0;
// Result.m[10] = q;
// Result.m[11] = -1;
//
// Result.m[12] = 0;
// Result.m[13] = 0;
// Result.m[14] = qn;
// Result.m[15] = 0;
// return Result;
}

fn_mat4 fn_infPerspective(float fov,float aspect_ratio,float near)
{
  const float tanHalfFovy = tanf(fov / 2);

  fn_mat4 Result = fn_identityMat4();
  Result.m[0] = 1.f / (aspect_ratio * tanHalfFovy);
  Result.m[5] = 1.f / (tanHalfFovy);

  Result.m[11] = 1;
  float epsilon = 0.01;

  Result.m[10] = 1 - epsilon;
  Result.m[14] = near*(epsilon - 1);


  return Result;
}

fn_mat4 fn_ortho(float left,float right,float bottom,float top)
{

  float far = 1;
  float near = -1;
  fn_mat4 m;
  m.m[0] = 2.0f / (right - left);
  m.m[1] = 0.0f;
  m.m[2] = 0.0f;
  m.m[3] = 0.0f;

  m.m[4] = 0.0f;
  m.m[5] = 2.0f / (top - bottom);
  m.m[6] = 0.0f;
  m.m[7] = 0.0f;

  m.m[8] = 0.0f;
  m.m[9] = 0.0f;
  m.m[10] = -2.0f / (far - near);
  m.m[11] = 0.0f;

  m.m[12] = -(right + left  ) / (right - left  );
  m.m[13] = -(top   + bottom) / (top   - bottom);
  m.m[14] = -(far   + near  ) / (far   - near  );
  m.m[15] = 1.0f;
  return m;
}

fn_mat4 fn_ortho3D(float left,float right,float bottom,float top,float far,float near)
{


  fn_mat4 m;
  m.m[0] = 2.0f / (right - left);
  m.m[1] = 0.0f;
  m.m[2] = 0.0f;
  m.m[3] = 0.0f;

  m.m[4] = 0.0f;
  m.m[5] = 2.0f / (top - bottom);
  m.m[6] = 0.0f;
  m.m[7] = 0.0f;

  m.m[8] = 0.0f;
  m.m[9] = 0.0f;
  m.m[10] = -2.0f / (far - near);
  m.m[11] = 0.0f;

  m.m[12] = -(right + left  ) / (right - left  );
  m.m[13] = -(top   + bottom) / (top   - bottom);
  m.m[14] = -(far   + near  ) / (far   - near  );
  m.m[15] = 1.0f;
  return m;
}

fn_mat4 fn_lookat(fn_vec3 pos,fn_vec3 look,fn_vec3 up)
{
  fn_vec3 f;
  fn_vec3 s;
  fn_vec3 u;

  f = fn_subVec3(look, pos);
  f = fn_normalizeVec3(f);

  s = fn_cross(f, up);
  s = fn_normalizeVec3(s);

  u = fn_cross(s, f);

  fn_mat4 pOut;
  pOut.m[0] = s.x;
  pOut.m[1] = u.x;
  pOut.m[2] = -f.x;
  pOut.m[3] = 0.0;

  pOut.m[4] = s.y;
  pOut.m[5] = u.y;
  pOut.m[6] = -f.y;
  pOut.m[7] = 0.0;

  pOut.m[8] = s.z;
  pOut.m[9] = u.z;
  pOut.m[10] = -f.z;
  pOut.m[11] = 0.0;

  pOut.m[12] = -fn_dot(s, pos);
  pOut.m[13] = -fn_dot(u, pos);
  pOut.m[14] = fn_dot(f, pos);
  pOut.m[15] = 1.0;

  return pOut;
}

// fn_mat4 fn_6doflookat(fn_vec3 up_orig,fn_vec3 right_orig,fn_vec3 forward_orig,float yaw, float pitch, float roll)
// {
//   bool flip = true;
//   fn_vec3 up, right, forward;
//
//   // if (flip) {
//   //     printf(" ----- %f   %f %f %f\n", sqrt( vec3abs_sq( *forward ) ), vec3dot(*forward, *right), vec3dot(*forward, *up), vec3dot(*up, *right)  ) ;
//   // }
//
//   //roll
//   //does not affect up vector
//
//   if (roll != 0.0)
//   {
//       //add a little bit of the right vector to the up vector:
//       vec3madd(*up, roll, right_orig);
//
//       //add some down to the right vector
//       vec3madd(*right, -roll, up_orig);
//
//   }
//
//
//   //yaw
//   //does not affect up
//   if (yaw != 0.0)
//   {
//       //add some 'right' to 'forward'
//       vec3madd( *forward, yaw, right_orig);
//
//
//       //add equal amount of 'backward' to right.
//       vec3madd( *right, -yaw, forward_orig);
//   }
//
//
//
//   if (pitch != 0.0)
//   {
//       //add some 'up' to the forward vector
//       vec3madd( *forward, pitch, up_orig);
//
//
//       //add some 'back' to the up vector/
//       vec3madd( *up, -pitch, forward_orig);
//
//   }
//
//   //normalize all vectors
//   vec3scale(  *up,  1.0/  sqrt( vec3abs_sq( *up ) ) );
//   vec3scale(  *right,  1.0/  sqrt( vec3abs_sq( *right ) ) );
//   vec3scale(  *forward,  1.0/  sqrt( vec3abs_sq( *forward ) ) );
//
//   //do a couple cross products to force things to stay orthogonal
//   //ideally we don't need to do this since all the above operations
//   //end with everything orthogonal, but you will have numeric drift
//
//   //can probably just do this every few frames in stead of every time
//   //its the cross products that are slow anyway.
//
//   //remake the up vector
//   if (flip)
//   {
//       vec3cross( *up, *right, *forward  );
//   }
//   else
//   {
//       vec3cross( *up, *forward, *right  );
//   }
//
//
//   //cross product to give new right vector
//
//   if (flip)
//   {
//       vec3cross( *right, *forward, *up  );
//   }
//   else
//   {
//       vec3cross( *right, *up, *forward  );
//   }
// }
//
// void gx_spin(zbool flip, zfloat32 yaw, zfloat32 pitch, zfloat32 roll, vec3* right, vec3* up, vec3* forward)
// {
//
//
//
// }

fn_mat4 fn_transpose(fn_mat4 m)
{
  fn_mat4 ret;
  ret.m[0] = m.m[0];
  ret.m[1] = m.m[4];
  ret.m[2] = m.m[8];
  ret.m[3] = m.m[12];
  ret.m[4] = m.m[1];
  ret.m[5] = m.m[5];
  ret.m[6] = m.m[9];
  ret.m[7] = m.m[13];
  ret.m[8] = m.m[2];
  ret.m[9] = m.m[6];
  ret.m[10] = m.m[10];
  ret.m[11] = m.m[14];
  ret.m[12] = m.m[3];
  ret.m[13] = m.m[7];
  ret.m[14] = m.m[11];
  ret.m[15] = m.m[15];
  return ret;
}

float fn_determinant(fn_mat4 in)
{
  float
  a00 = in.m[0], a01 = in.m[1], a02 = in.m[2], a03 = in.m[3],
  a10 = in.m[4], a11 = in.m[5], a12 = in.m[6], a13 = in.m[7],
  a20 = in.m[8], a21 = in.m[9], a22 = in.m[10], a23 = in.m[11],
  a30 = in.m[12], a31 = in.m[13], a32 = in.m[14], a33 = in.m[15],

  b00 = a00 * a11 - a01 * a10,
  b01 = a00 * a12 - a02 * a10,
  b02 = a00 * a13 - a03 * a10,
  b03 = a01 * a12 - a02 * a11,
  b04 = a01 * a13 - a03 * a11,
  b05 = a02 * a13 - a03 * a12,
  b06 = a20 * a31 - a21 * a30,
  b07 = a20 * a32 - a22 * a30,
  b08 = a20 * a33 - a23 * a30,
  b09 = a21 * a32 - a22 * a31,
  b10 = a21 * a33 - a23 * a31,
  b11 = a22 * a33 - a23 * a32,

  det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
  return det;
}

fn_mat4 fn_inverse(fn_mat4 in) {
  // float m[16];
  // memcpy(m,in.m,sizeof(float)*16);

  float
  a00 = in.m[0], a01 = in.m[1], a02 = in.m[2], a03 = in.m[3],
  a10 = in.m[4], a11 = in.m[5], a12 = in.m[6], a13 = in.m[7],
  a20 = in.m[8], a21 = in.m[9], a22 = in.m[10], a23 = in.m[11],
  a30 = in.m[12], a31 = in.m[13], a32 = in.m[14], a33 = in.m[15],

  b00 = a00 * a11 - a01 * a10,
  b01 = a00 * a12 - a02 * a10,
  b02 = a00 * a13 - a03 * a10,
  b03 = a01 * a12 - a02 * a11,
  b04 = a01 * a13 - a03 * a11,
  b05 = a02 * a13 - a03 * a12,
  b06 = a20 * a31 - a21 * a30,
  b07 = a20 * a32 - a22 * a30,
  b08 = a20 * a33 - a23 * a30,
  b09 = a21 * a32 - a22 * a31,
  b10 = a21 * a33 - a23 * a31,
  b11 = a22 * a33 - a23 * a32,

  det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

  fn_mat4 r;

  r.m[0] = (a11 * b11 - a12 * b10 + a13 * b09) / det;
  r.m[1] = (a02 * b10 - a01 * b11 - a03 * b09) / det;
  r.m[2] = (a31 * b05 - a32 * b04 + a33 * b03) / det;
  r.m[3] = (a22 * b04 - a21 * b05 - a23 * b03) / det;
  r.m[4] = (a12 * b08 - a10 * b11 - a13 * b07) / det;
  r.m[5] = (a00 * b11 - a02 * b08 + a03 * b07) / det;
  r.m[6] = (a32 * b02 - a30 * b05 - a33 * b01) / det;
  r.m[7] = (a20 * b05 - a22 * b02 + a23 * b01) / det;
  r.m[8] = (a10 * b10 - a11 * b08 + a13 * b06) / det;
  r.m[9] = (a01 * b08 - a00 * b10 - a03 * b06) / det;
  r.m[10] = (a30 * b04 - a31 * b02 + a33 * b00) / det;
  r.m[11] = (a21 * b02 - a20 * b04 - a23 * b00) / det;
  r.m[12] = (a11 * b07 - a10 * b09 - a12 * b06) / det;
  r.m[13] = (a00 * b09 - a01 * b07 + a02 * b06) / det;
  r.m[14] = (a31 * b01 - a30 * b03 - a32 * b00) / det;
  r.m[15] = (a20 * b03 - a21 * b01 + a22 * b00) / det;

  return r;
}

fn_vec3 fn_transformVec3(fn_vec3 v,fn_mat4 m)
{
  fn_vec4 v4 = fn_createVec4(v.x,v.y,v.z,1.f);
  v4 = fn_multVec4Mat4(m,v4);
  return fn_createVec3(v4.x,v4.y,v4.z);
}

fn_vec3 fn_transformNormal(fn_vec3 v,fn_mat4 m)
{
  fn_vec4 v4 = fn_createVec4(v.x,v.y,v.z,0.f);
  v4 = fn_multVec4Mat4(m,v4);
  return fn_normalizeVec3(fn_createVec3(v4.x,v4.y,v4.z));
}

fn_vec4 fn_multVec4Mat4(fn_mat4 a,fn_vec4 b)
{
  int i;
  fn_vec4 out;

  for(i = 0 ; i < 4; ++i) {
    out.v[i] =
    (b.v[0] * a.m[i + 0]) +
    (b.v[1] * a.m[i + 4]) +
    (b.v[2] * a.m[i + 8]) +
    (b.v[3] * a.m[i + 12]);
  }

  return out;
}

bool fn_isIdentity(fn_mat4 a)
{
  return
  ( a.m[0] == 1&&
    a.m[1] == 0&&
    a.m[2] == 0&&
    a.m[3] == 0&&
    a.m[4] == 0&&
    a.m[5] == 1&&
    a.m[6] == 0&&
    a.m[7] == 0&&
    a.m[8] == 0&&
    a.m[9] == 0&&
    a.m[10] == 1&&
    a.m[11] == 0&&
    a.m[12] == 0&&
    a.m[13] == 0&&
    a.m[14] == 0&&
    a.m[15] == 1
  );
}

bool fn_isNullMatrix(fn_mat4 a)
{
  return
  ( a.m[0] == 0&&
    a.m[1] == 0&&
    a.m[2] == 0&&
    a.m[3] == 0&&
    a.m[4] == 0&&
    a.m[5] == 0&&
    a.m[6] == 0&&
    a.m[7] == 0&&
    a.m[8] == 0&&
    a.m[9] == 0&&
    a.m[10] == 0&&
    a.m[11] == 0&&
    a.m[12] == 0&&
    a.m[13] == 0&&
    a.m[14] == 0&&
    a.m[15] == 0
  );
}

fn_mat4 fn_sphericalHarmonicMatrix(float* coeffs)
{
  int i;
  fn_mat4 ret;
  memcpy(ret.m,coeffs,sizeof(float)*4.f*3.f);

  // ret.m[0] = s.x;//coeffs[0];
  // ret.m[1] = s.y;//coeffs[1];
  // ret.m[2] = s.z;//coeffs[2];
  // ret.m[3] = t.x;//coeffs[3];
  // ret.m[4] = t.y;//s.x;//coeffs[4];
  // ret.m[5] = t.z;//coeffs[5];
  // ret.m[6] = coeffs[0];//coeffs[6];
  // ret.m[7] = coeffs[1];//coeffs[7];
  // ret.m[8] = coeffs[2];//coeffs[8];
  // ret.m[9] = coeffs[3];//s.x;
  // ret.m[10] = coeffs[4];//s.y;
  // ret.m[11] = coeffs[5];//s.z;
  // ret.m[12] = coeffs[6];
  // ret.m[13] = coeffs[7];//t.x ;
  // ret.m[14] = coeffs[8];//t.y ;
  // ret.m[15] = 0.f;//t.z;

  return ret;
}

void fn_printMat4(fn_mat4 a)
{
 //  printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
 //   a.m[0] ,
 //    a.m[1] ,
 //    a.m[2] ,
 //    a.m[3] ,
 //    a.m[4] ,
 //    a.m[5] ,
 //    a.m[6] ,
 //    a.m[7] ,
 //    a.m[8] ,
 //    a.m[9] ,
 //    a.m[10] ,
 //    a.m[11] ,
 //    a.m[12] ,
 //    a.m[13] ,
 //    a.m[14] ,
 //    a.m[15]
 // );

 printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
  a.m[0] ,
   a.m[4] ,
   a.m[8] ,
   a.m[12] ,
   a.m[1] ,
   a.m[5] ,
   a.m[9] ,
   a.m[13] ,
   a.m[2] ,
   a.m[6] ,
   a.m[10] ,
   a.m[14] ,
   a.m[3] ,
   a.m[7] ,
   a.m[11] ,
   a.m[15]
);
}

fn_mat4 fn_translaterotatescale(fn_vec3 t,float angle,fn_vec3 axis,fn_vec3 s)
{
  fn_mat4 scale =fn_makescale(s);
  fn_mat4 translate = fn_maketranslate(t);
  fn_mat4 rotate = fn_makerotate(angle,axis);
  return fn_multMat4(fn_multMat4(rotate,scale),translate);
}

fn_mat4 fn_translaterotatescaleq(fn_vec3 t,fn_quat q,fn_vec3 s)
{
  fn_mat4 scale =fn_makescale(s);
  fn_mat4 translate = fn_maketranslate(t);
  fn_mat4 rotate = fn_rotationMat(q);
  return fn_multMat4(fn_multMat4(scale,rotate),translate);
}

fn_mat4 fn_translaterotatescalem(fn_vec3 t,fn_mat4 m,fn_vec3 s)
{
  fn_mat4 scale =fn_makescale(s);
  fn_mat4 translate = fn_maketranslate(t);
  fn_mat4 rotate = m;
  return fn_multMat4(fn_multMat4(scale,rotate),translate);
}

fn_Transform fn_decomposeMat4(fn_mat4 m)
{


  fn_vec3 tr_a = fn_createVec3(m.m[12],m.m[13],m.m[14]);



  fn_vec4 a_x_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(m.m[0],m.m[1],m.m[2]));
  fn_vec4 a_y_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(m.m[4],m.m[5],m.m[6]));
  fn_vec4 a_z_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(m.m[8],m.m[9],m.m[10]));



  fn_vec3 s_a = fn_createVec3(a_x_basis_mag.w,a_y_basis_mag.w,a_z_basis_mag.w);

  fn_vec3 a_x_basis = a_x_basis_mag.xyz;
  fn_vec3 a_y_basis = a_y_basis_mag.xyz;
  fn_vec3 a_z_basis = a_z_basis_mag.xyz;




  fn_quat qa = fn_mat4toquat(fn_createMat4(a_x_basis.x,a_x_basis.y,a_x_basis.z,0,a_y_basis.x,a_y_basis.y,a_y_basis.z,0,a_z_basis.x,a_z_basis.y,a_z_basis.z,0,0,0,0,1));

  fn_Transform ret;
  ret.translate = tr_a;
  ret.scale = s_a;
  ret.rotate = fn_normalizeVec4(qa);

  return ret;
}

fn_vec3 fn_getTranslationMat4(fn_mat4 m)
{
  fn_vec3 tr_a = fn_createVec3(m.m[12],m.m[13],m.m[14]);
  return tr_a;
}

fn_Transform fn_identityTransform()
{
  fn_Transform ret;
  ret.translate = fn_createVec3(0,0,0);
  ret.scale = fn_createVec3(1,1,1);
  ret.rotate = fn_createVec4(0,0,0,1);
  return ret;
}

fn_Transform fn_lerpTransforms(fn_Transform a,fn_Transform b,float f)
{
  fn_vec3 tr = fn_lerpVec3(a.translate,b.translate,f);
  fn_vec3 s = fn_lerpVec3(a.scale,b.scale,f);



  fn_vec4 r;
  r = fn_slerpVec4(fn_normalizeVec4(a.rotate), fn_normalizeVec4(b.rotate), f);
  r= fn_normalizeVec4(r);

  fn_Transform ret;
  ret.translate = tr;
  ret.scale = s;
  ret.rotate = r;
  return ret;
}

fn_mat4 fn_transformToMat(fn_Transform t)
{
  return fn_translaterotatescaleq(t.translate,t.rotate,t.scale);
}

fn_Transform fn_multTransform(fn_Transform a,fn_Transform b)
{
  fn_mat4 ma = fn_transformToMat(a);
  fn_mat4 mb = fn_transformToMat(b);

  return fn_decomposeMat4(fn_multMat4(ma,mb));
}

fn_Transform fn_interpTransforms(fn_Transform* t,float* alphas,int count,float f)
{
  int a_index = -1;
  int b_index = count;
  for (int i = 0 ; i < count;i++)
  {
    if (f >= alphas[i] && i > a_index)
    {
      a_index = i;
    }

    if (f <= alphas[i] && i < b_index)
    {
      b_index = i;
    }
  }

  if (a_index < 0)
  {
    a_index = 0;
  }

  if (b_index >= count)
  {
    b_index = count - 1;
  }

  float alpha = (f - alphas[a_index]) / (alphas[b_index] - alphas[a_index]);

  alpha = fn_clamp(alpha,0.0,1.0);

  return fn_lerpTransforms(t[a_index],t[b_index],alpha);
}
