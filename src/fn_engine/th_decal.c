#include "th_decal.h"
#include "th_system.h"
#include <stdlib.h>
#include <stdio.h>
//800
#define DECALS_LIMIT 200
// vec3 scale = vec3(50,50,50);
// vec3 pos   = vec3(-50,-50,-50);
// vec3 zAxis = normalize(vec3(1,0,0));
// vec3 yAxis = normalize(vec3(0,-1,0));
// vec3 xAxis = cross(yAxis, zAxis);
// mat4 scaleMat =
// {
//   vec4(scale.x, 0, 0, 0),vec4(0, scale.y, 0, 0),vec4(0, 0, scale.z, 0),vec4(0, 0, 0, 1)
// };
// mat4 worldMat =
// {
//   vec4(xAxis, 0),vec4(yAxis, 0),vec4(zAxis, 0),vec4(pos,   1)
// };
// //-- 3. final world matrix.
// worldMat = worldMat*scaleMat;
// //-- 4. compute data for the decal view matrix.
// mat4 lookAtMat =
// {
//   vec4(xAxis.x,            yAxis.x,          zAxis.x,           0),
//   vec4(xAxis.y,            yAxis.y,          zAxis.y,           0),
//   vec4(xAxis.z,            yAxis.z,          zAxis.z,           0),
//   vec4(-dot(xAxis, pos),  -dot(yAxis, pos), -dot(zAxis, pos),   1)
// };
// //-- 5. compute data for the decal proj matrix.
// mat4 projMat =
// {
//   vec4(2.0f / scale.x, 0,              0, 0),
//   vec4(0,              2.0f / scale.y, 0, 0),
//   vec4(0,              0,              1, 0),
//   vec4(0,              0,              0, 1)
// };
// //-- 6. caclulate final view-projection decal matrix.
// mat4 viewProjMat = projMat*lookAtMat;

fn_vec3 th_getDecalTangentVector(fn_vec3 look)
{
  fn_vec3 up_decal = fn_createVec3(0,-1,0);
  if (fn_almostequalVec3(fn_createVec3(0,-1,0),look,0.001) || fn_almostequalVec3(fn_createVec3(0,1,0),look,0.001))
  {
    up_decal = fn_createVec3(1,0,0);
  }
  return fn_normalizeVec3(fn_cross(up_decal,look));
}

fn_mat4 th_makeDecalMatrix(fn_vec3 pos,fn_vec3 scale,fn_vec3 normal,fn_vec3 tangent)
{
  fn_vec3 zAxis = fn_normalizeVec3(normal);
  fn_vec3 yAxis = fn_normalizeVec3(tangent);
  fn_vec3 xAxis = fn_cross(yAxis, zAxis);

  //-- 4. compute data for the decal view matrix.

  fn_mat4 lookAtMat = fn_makeMat4Rows(fn_createVec4(xAxis.x,            yAxis.x,          zAxis.x,           0),
  fn_createVec4(xAxis.y,            yAxis.y,          zAxis.y,           0),
  fn_createVec4(xAxis.z,            yAxis.z,          zAxis.z,           0),
  fn_createVec4(-fn_dot(xAxis, pos),  -fn_dot(yAxis, pos), -fn_dot(zAxis, pos),   1));


  //-- 5. compute data for the decal proj matrix.
  fn_mat4 projMat =
  fn_makeMat4Rows(
    fn_createVec4(2.0f / scale.x, 0,              0, 0),
    fn_createVec4(0,              2.0f / scale.y, 0, 0),
    fn_createVec4(0,              0,              -2.0/scale.z, 0),
    fn_createVec4(0,              0,              0, 1)
  );
  //-- 6. caclulate final view-projection decal matrix.
  fn_mat4 viewProjMat = fn_multMat4(lookAtMat,projMat);
  return viewProjMat;
}

fn_mat4 th_makeDecalMatrixWorld(fn_vec3 pos,fn_vec3 scale,fn_vec3 normal,fn_vec3 tangent)
{
  fn_vec3 zAxis = fn_normalizeVec3(normal);
  fn_vec3 yAxis = fn_normalizeVec3(tangent);
  fn_vec3 xAxis = fn_cross(yAxis, zAxis);
  fn_mat4 scaleMat = fn_makescale(scale);
  // mat4 scaleMat =
  // {
  //   vec4(scale.x, 0, 0, 0),vec4(0, scale.y, 0, 0),vec4(0, 0, scale.z, 0),vec4(0, 0, 0, 1)
  // };
  fn_mat4 worldMat = fn_makeMat4Rows(fn_createVec4Vec3(xAxis, 0),fn_createVec4Vec3(yAxis, 0),fn_createVec4Vec3(zAxis, 0),fn_createVec4Vec3(pos,   1));

  //-- 3. final world matrix.
  worldMat = fn_multMat4(scaleMat,worldMat);

  return worldMat;
}

static th_DecalOrientation* decals_list = NULL;
static int decals_count = 0;
static int decals_used = 0;



th_DecalOrientation* th_getDecalOrientations()
{
  return decals_list;
}

void th_addDecal(th_DecalOrientation new_decal)
{
  if (decals_list == NULL)
  {
    decals_list = malloc(sizeof(th_DecalOrientation)*800);
  }
  decals_list[decals_count] = new_decal;
  decals_list[decals_count].viewproj = th_makeDecalMatrix(new_decal.pos,new_decal.scale,new_decal.normal,new_decal.tangent);
  decals_list[decals_count].world = th_makeDecalMatrixWorld(new_decal.pos,new_decal.scale,new_decal.normal,new_decal.tangent);
  decals_count++;
  if (decals_used < DECALS_LIMIT)
  {
    decals_used = decals_count;
  }
//  printf("%i\n",decals_count );
  if (decals_count >= DECALS_LIMIT)
  {
    decals_count = 0;
    decals_used = DECALS_LIMIT;
  //  printf("%s\n","800!" );
  }
}

void th_resetDecals()
{
  if (decals_list != NULL)
  {
    decals_list = NULL;
    free(decals_list);
    decals_count = 0;
    decals_used = 0;
  }


}

void th_getDecals(th_Decal* out,int* count)
{
//  th_printlnDevConsole("%i",decals_used);
  *count = decals_used;
  for (int i = 0 ; i < decals_used;i++)
  {
    th_DecalOrientation this_decal = decals_list[i];
    fn_mat4 decal_matrix = this_decal.viewproj;//th_makeDecalMatrix(this_decal.pos,this_decal.scale,this_decal.normal,this_decal.tangent);
    out[i].decal_matrix = decal_matrix;
    out[i].decal_properties = fn_createVec4(this_decal.material_handle.x,0,0,0);
    out[i].decal_properties.yzw = this_decal.normal;
    // printf("Decal %i\n",i );
    // fn_printVec3(this_decal.tangent);
    // fn_printVec3(this_decal.normal);
    // fn_printVec3(this_decal.pos);
    // fn_printVec3(this_decal.scale);
  }
}
