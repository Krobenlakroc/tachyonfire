#pragma once



typedef struct
{
  //rendering
  th_GpuDataOffsets static_geometry;
  th_GpuDataOffsets tesselated_static_geometry;

  //collision
  th_World static_colliders;

  //lighting
  GLuint lightTex;
  GLuint cubemapDepth;
  GLuint cubemapColor;
  fn_vec3* cubeMins;
  fn_vec3* cubeMaxs;
}th_Level;
