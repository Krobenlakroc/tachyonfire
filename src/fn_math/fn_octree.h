#pragma once
#include "../fn_engine/th_collision.h"

struct fn_OctreeNode
{
  int* indices;
  int indiceCount;
  fn_AABB box;
  struct fn_OctreeNode* children[8]; //8 of them at max
  bool leaf;
};

typedef struct
{
  struct fn_OctreeNode* root;
  fn_AABB* aabbs;
  int aabbCount;
  char* usedIndices;

  fn_vec3 min_position;
  fn_vec3 max_position;
}fn_Octree;

fn_Octree fn_createOctree(fn_AABB* aabbs,int aabbcount);

// fn_Octree fn_createOctreeParam(fn_AABB* aabbs,int aabbcount,int max_elm,int max_depth);

int* fn_getAABBSTouch(fn_AABB aabb,fn_Octree* otree,int* indiceCount);

void fn_getAABBSTouchv(fn_AABB aabb,fn_Octree* otree,int* indiceCount,int* ret,char* memory);

void fn_exportOctree(fn_Octree* octree,const char* demoName);

fn_Octree fn_importOctree(const char* demoName,fn_AABB* aabbs,int aabbCount);

void fn_freeOctree(fn_Octree* octree);
