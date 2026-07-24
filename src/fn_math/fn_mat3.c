#include "fn_mat3.h"
#include "fn_quaternion.h"
#include "../fn_engine/fn_profile.h"
static int setMultithread = 0;
static int multithread = 0;

fn_mat3 fn_identityMat3()
{
  fn_mat3 temp;
  temp.m[0] = 1;
  temp.m[1] = 0;
  temp.m[2] = 0;
  temp.m[3] = 0;
  temp.m[4] = 1;
  temp.m[5] = 0;
  temp.m[6] = 0;
  temp.m[7] = 0;
  temp.m[8] = 1;

  return temp;
}

fn_mat3 fn_getMat3Mat4(fn_mat4 m)
{
  fn_mat3 temp;
  temp.m[0] = m.m[0];
  temp.m[1] = m.m[1];
  temp.m[2] = m.m[2];
  temp.m[3] = m.m[4];
  temp.m[4] = m.m[5];
  temp.m[5] = m.m[6];
  temp.m[6] = m.m[8];
  temp.m[7] = m.m[9];
  temp.m[8] = m.m[10];
  return temp;
}

fn_mat3 fn_multMat3(fn_mat3 a,fn_mat3 b)
{
  int i,j;

  fn_mat3 ret;

  for (j = 0;j < 3;j++)
  {


    for (i = 0;i < 3;i++)
    ret.m[i+j*3]= a.m[j*3]*b.m[i] + a.m[j*3 + 1]*b.m[i+3] + a.m[j*3 + 2]*b.m[i+6];

  }
  return ret;
}

fn_vec3 fn_multMat3Vec3(fn_mat3 a,fn_vec3 b)
{
  int i;
  fn_vec3 out;

  for(i = 0 ; i < 3; ++i) {
    out.v[i] =
    (b.v[0] * a.m[i + 0]) +
    (b.v[1] * a.m[i + 3]) +
    (b.v[2] * a.m[i + 6]);
  }

  return out;
}

fn_mat3 fn_makescalerotateMat3(fn_vec3 scale,float angle,fn_vec3 axis)
{
  fn_quat q = fn_makeQuaternion(angle,axis);
  fn_mat4 qm = fn_rotationMat(q);
  fn_mat3 temp = fn_identityMat3();
  temp.m[0] = scale.x;
  temp.m[4] = scale.y;
  temp.m[8] = scale.z;
  fn_mat3 qm3 = fn_getMat3Mat4(qm);

  fn_mat3 ret = fn_multMat3(temp,qm3);
  return ret;
}

fn_mat4 fn_getMat4Mat3(fn_mat3 m)
{
  fn_mat4 ret;
  ret.m[0] = m.m[0];
  ret.m[1] = m.m[1];
  ret.m[2] = m.m[2];
  ret.m[3] = 0;
  ret.m[4] = m.m[3];
  ret.m[5] = m.m[4];
  ret.m[6] = m.m[5];
  ret.m[7] = 0;
  ret.m[8] = m.m[6];
  ret.m[9] = m.m[7];
  ret.m[10] = m.m[8];
  ret.m[11] = 0;
  ret.m[12] = 0;
  ret.m[13] = 0;
  ret.m[14] = 0;
  ret.m[15] = 1;
  return ret;
}
