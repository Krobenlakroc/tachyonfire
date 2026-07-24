#include "fn_quaternion.h"
fn_quat fn_makeQuaternion(float angle,fn_vec3 axis)
{
  fn_quat ret;
  ret.x = axis.x * sin(angle / 2.f);
  ret.y = axis.y * sin(angle / 2.f);
  ret.z = axis.z * sin(angle / 2.f);
  ret.w = cos(angle / 2.f);
  return ret;
}

fn_quat fn_mat4toquat(fn_mat4 in)
{
  fn_quat ret;


  // ret.w = sqrtf(1 + in.m[0] + in.m[5] + in.m[10]) /2.f;
  // ret.x = (in.m[9] - in.m[6])/( 4.f *ret.w);
  // ret.y = (in.m[2] - in.m[8])/( 4.f *ret.w);
  // ret.z = (in.m[4] - in.m[1])/( 4.f *ret.w);

  float trace = in.m[0] + in.m[5] + in.m[10];
  if(trace > 0) {
    float s = sqrtf(trace + 1.0f) * 2.f;
    ret.w = 0.25f * s;
    ret.x = (in.m[9] - in.m[6]) / s;
    ret.y = (in.m[2] - in.m[8]) / s;
    ret.z = (in.m[4] - in.m[1]) / s;
  } else if(in.m[0] > in.m[5] && in.m[0] > in.m[10]) {
    float s = sqrtf(1.0f + in.m[0] - in.m[5] - in.m[10]) * 2.f;
    ret.w = (in.m[9] - in.m[6]) / s;
    ret.x = 0.25f * s;
    ret.y = (in.m[1] + in.m[4]) / s;
    ret.z = (in.m[2] + in.m[8]) / s;
  } else if(in.m[5] > in.m[10]) {
    float s = sqrtf(1.0f + in.m[5] - in.m[0] - in.m[10]) * 2.f;
    ret.w = (in.m[2] - in.m[8]) / s;
    ret.x = (in.m[1] + in.m[4]) / s;
    ret.y = 0.25f * s;
    ret.z = (in.m[6] + in.m[9]) / s;
  } else {
    float s = sqrtf(1.0f + in.m[10] - in.m[0] - in.m[5]) * 2.f;
    ret.w = (in.m[4] - in.m[1]) / s;
    ret.x = (in.m[2] + in.m[8]) / s;
    ret.y = (in.m[6] + in.m[9]) / s;
    ret.z = 0.25f * s;
  }

  return ret;
}

fn_quat fn_multquat(fn_quat a, fn_quat b) {
  fn_quat r ;
      r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;  // i
      r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;  // j
      r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;   // k
      r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z; //1

    return r;
}

fn_mat4 fn_rotationMat(fn_quat q)
{
  float sqw = q.w*q.w;
  float sqx = q.x*q.x;
  float sqy = q.y*q.y;
  float sqz = q.z*q.z;

  // invs (inverse square length) is only required if quaternion is not already normalised
  float invs = 1.f / (sqx + sqy + sqz + sqw);
  float m00 = ( sqx - sqy - sqz + sqw)*invs ; // since sqw + sqx + sqy + sqz =1/invs*invs
  float m11 = (-sqx + sqy - sqz + sqw)*invs ;
  float m22 = (-sqx - sqy + sqz + sqw)*invs ;

  float tmp1 = q.x*q.y;
  float tmp2 = q.z*q.w;
  float m10 = 2.0 * (tmp1 + tmp2)*invs ;
  float m01 = 2.0 * (tmp1 - tmp2)*invs ;

 tmp1 = q.x*q.z;
 tmp2 = q.y*q.w;
  float m20 = 2.0 * (tmp1 - tmp2)*invs ;
  float m02 = 2.0 * (tmp1 + tmp2)*invs ;
 tmp1 = q.y*q.z;
 tmp2 = q.x*q.w;
  float m21 = 2.0 * (tmp1 + tmp2)*invs ;
  float m12 = 2.0 * (tmp1 - tmp2)*invs ;

  fn_mat4 m;
  m.m[0] = m00;
  m.m[1] = m01;
  m.m[2] = m02;
  m.m[3] = 0;
  m.m[4] = m10;
  m.m[5] = m11;
  m.m[6] = m12;
  m.m[7] = 0;
  m.m[8] = m20;
  m.m[9] = m21;
  m.m[10] = m22;
  m.m[11] = 0;
  m.m[12] = 0;
  m.m[13] = 0;
  m.m[14] = 0;
  m.m[15] = 1;
  return m;
}

//this code is untested and porbably fails in some cases
static fn_vec3 generateTangentVector(fn_vec3 normal) {
    fn_vec3 arbitraryVector;

    if (fabs(normal.x) >= 0.01 && fabs(normal.z) >= 0.01) {
        arbitraryVector.x = 1;
        arbitraryVector.y = 0;
        arbitraryVector.z = (-normal.x) / normal.z;
    } else if (normal.y != 0) {
        arbitraryVector.x = 1;
        arbitraryVector.y = 0;
        arbitraryVector.z = 0;
    } else {
        arbitraryVector.x = 0;
        arbitraryVector.y = 1;
        arbitraryVector.z = 0;
    }

    fn_vec3 tangent = fn_cross(normal, arbitraryVector);
    return fn_normalizeVec3(tangent);
}

fn_quat fn_getRotationQuaternion(fn_vec3 from, fn_vec3 to)
{
  if (fn_almostequalVec3(from,to,0.001))
  {
    return fn_createVec4(0,0,0,1);
  }

  if (fn_almostequalVec3(from,fn_multVec3s(to,-1),0.001))
  {
    return fn_makeQuaternion(3.14159,generateTangentVector(to));
  }
     fn_quat result;

     fn_vec3 H = fn_addVec3(from, to);
     H = fn_normalizeVec3(H);

     result.w = fn_dot(from, H);
     result.x = from.y*H.z - from.z*H.y;
     result.y = from.z*H.x - from.x*H.z;
     result.z = from.x*H.y - from.y*H.x;
     return result;
}

fn_quat fn_getRotationQuaternion2(fn_vec3 from, fn_vec3 to)
{
  if (fn_almostequalVec3(from,to,0.001))
  {
    return fn_createVec4(0,0,0,1);
  }

  if (fn_almostequalVec3(from,fn_multVec3s(to,-1),0.001))
  {
    return fn_makeQuaternion(3.14159,generateTangentVector(to));
  }

  float dot = fn_dot(from,to);
  fn_vec3 cross = fn_cross(from,to);

  fn_quat ret = fn_createVec4(cross.x,cross.y,cross.z,1.0 + dot);
  ret = fn_normalizeVec4(ret);
  return ret;
}

fn_quat fn_inverseq(fn_quat q)
{
  fn_quat r = q;
  r.x = r.x*-1;
  r.y = r.y*-1;
  r.z = r.z*-1;
  return r;

}

fn_vec3 fn_rotatePointQuat(fn_vec3 p,fn_quat q)
{
  return fn_multquat(fn_multquat(fn_inverseq(q),fn_createVec4Vec3(p,0)),q).xyz;
}

fn_mat4 th_6dofmat(fn_vec3* old_up,fn_vec3 new_forward)
{
  fn_vec3 f;
  fn_vec3 s;
  fn_vec3 u;

  f = new_forward;
  f = fn_normalizeVec3(f);

  s = fn_cross(f, *old_up);
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

  pOut.m[12] = 0;
  pOut.m[13] = 0;
  pOut.m[14] = 0;
  pOut.m[15] = 1.0;
  *old_up = u;



  return pOut;
}

fn_mat4 th_orientMatrix(fn_vec3 target_vec,fn_vec3* old_ups)
{

  fn_mat4 cam = th_6dofmat(old_ups,target_vec);

  fn_mat4 m = fn_inverse(cam);

  return m;
}



// fn_quat fn_getRotationQuaternionUp(fn_vec3 from_vec,fn_vec3 target_vec,fn_vec3* old_ups)
// {
//   fn_vec3 from_vector = from_vec;
//
//
//
//
//
//   fn_vec3 bone_to = target_vec;
//
//   //c->data[i].old_ups[j] = fn_createVec3(0,1,0);
//   fn_mat4 cam = th_6dofCamera(NULL,NULL,old_ups,fn_multVec3(bone_to,fn_createVec3(1,1,1)),fn_createVec3(0,0,0));
//
//   fn_mat4 m = fn_inverse(cam);
//
//   return fn_mat4toquat(m);
// }
