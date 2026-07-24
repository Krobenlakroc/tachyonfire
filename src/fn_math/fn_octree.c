#include "fn_octree.h"
#include <stdlib.h>
#include <string.h>
#include "../fn_engine/fn_arrayutils.h"
#include "../fn_engine/fn_profile.h"
// #include <zlib.h>
#include <stdio.h>
static int MAX_ELEMENTS = 50;//5
static int MAX_DEPTH = 4;

static void free_r(struct fn_OctreeNode* root)
{

  int i;

  if (root->indiceCount != 0)
    free(root->indices);

  if (!root->leaf)
  {

    for (i =0;i < 8;i++)
    {
      if (root->children[i] != NULL)
      free_r(root->children[i]);
    }
  }

  free(root);
}

static void getAABBPoints(fn_AABB aabb,fn_vec3* points)
{

  points[0] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,aabb.hwidth.y,aabb.hwidth.z)); // 1 1 1
  points[1] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,-aabb.hwidth.y,-aabb.hwidth.z));// -1 -1 -1
  points[2] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,aabb.hwidth.y,aabb.hwidth.z)); // -1 1 1
  points[3] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,-aabb.hwidth.y,aabb.hwidth.z)); // -1 -1 1
  points[4] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,aabb.hwidth.y,-aabb.hwidth.z)); // -1 1 -1
  points[5] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,aabb.hwidth.y,-aabb.hwidth.z)); // 1 1 -1
  points[6] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,-aabb.hwidth.y,aabb.hwidth.z)); // 1 -1 1
  points[7] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,-aabb.hwidth.y,-aabb.hwidth.z)); // 1 -1 -1
}

static fn_AABB makeAABBMinMax(fn_vec3 min,fn_vec3 max)
{
  fn_vec3 new_min = fn_minVec3(min,max);
  fn_vec3 new_max = fn_maxVec3(min,max);
  fn_AABB box;//default here
  box.position = fn_multVec3s(fn_addVec3(new_max,new_min),0.5);

  box.hwidth = fn_multVec3s(fn_subVec3(new_max,new_min),0.5);

  return box;
}

static void makeChildren(struct fn_OctreeNode* root)
{
  fn_vec3 points[8];

  getAABBPoints(root->box,points);
  int i;
  for (i = 0;i < 8;i++)
  {
    root->children[i] = (struct fn_OctreeNode*)malloc(sizeof(struct fn_OctreeNode));
    root->children[i]->box = makeAABBMinMax(points[i],root->box.position);

    root->children[i]->indices = NULL;
    root->children[i]->indiceCount = 0;
    root->children[i]->leaf = true;
    memset(root->children[i]->children, 0, 8*sizeof(struct fn_OctreeNode*));
    // root->children[i]->aabbs = root->aabbs;
    // root->children[i]->aabbCount = root->aabbCount;
  }
}

static int findIndex(int* numbers,int count,int s)
{
  for (int i = 0 ; i < count;i++)
  {
    if (numbers[i] == s)
    {
      return i;
    }
  }
  return -1;
}

typedef struct
{
  th_Collider aabb;
  int id;
}th_AABBPair;

static void createTree_r(struct fn_OctreeNode* root,int* flatCount,th_AABBPair** aabbs,int* aabbcount,int depth,bool multithread_tree)
{

  if (root->indices != NULL)
    free(root->indices);
  root->indiceCount = 0;
  root->indices = NULL;

 // printf("%i\n",*aabbcount );
  int i,j,k;

  makeChildren(root);
  root->leaf = false;


  *flatCount = *flatCount+8;

  th_AABBPair* subsets[8] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
  int subsets_counts[8] = {0,0,0,0,0,0,0,0};
  bool splits[8] = {false,false,false,false,false,false,false,false};
  for (j =0 ;j < 8; j++)
  {
      subsets[j] = malloc(sizeof(th_AABBPair)*(*aabbcount));
  for (i = *aabbcount-1;i >=0 ;i--)
  {
    // printf("%i\n",i );
    // int usedIndices[*aabbcount];
   // #pragma omp parallel for private(j) if(multithread_tree)


      if (fn_aabbCheck((*aabbs)[i].aabb,root->children[j]->box))
      {
        subsets[j][subsets_counts[j]] = (*aabbs)[i];
        subsets_counts[j]++;

        if (root->children[j]->indiceCount < MAX_ELEMENTS || depth > MAX_DEPTH )
        {
          root->children[j]->indiceCount++;

          root->children[j]->indices = realloc(root->children[j]->indices,sizeof(int)*root->children[j]->indiceCount);

          root->children[j]->indices[root->children[j]->indiceCount-1] = (*aabbs)[i].id;

        }
        else
        {
          if (root->children[j]->indices != NULL)
            free(root->children[j]->indices);
          root->children[j]->indices = NULL;
          root->children[j]->indiceCount = 0;
          splits[j] = true;
        }
      }

    }

  }

  for (j =0 ;j < 8; j++)
  {
    if (splits[j])
    {
      createTree_r(root->children[j],flatCount,&subsets[j],&subsets_counts[j],depth + 1,false);
    }
    free(subsets[j]);
  }


  // for (i = *aabbcount-1;i >=0 ;i--)
  // {
  //   // printf("%i\n",i );
  //   // int usedIndices[*aabbcount];
  //  // #pragma omp parallel for private(j) if(multithread_tree)
  //   for (j =0 ;j < 8; j++)
  //   {
  //
  //     if (fn_aabbCheck((*aabbs)[i].aabb,root->children[j]->box))
  //     {
  //
  //       if (root->children[j]->indiceCount < MAX_ELEMENTS || depth > MAX_DEPTH )
  //       {
  //         root->children[j]->indiceCount++;
  //       //  printf("%i\n",root->children[j]->indiceCount );
  //
  //         root->children[j]->indices = realloc(root->children[j]->indices,sizeof(int)*root->children[j]->indiceCount);
  //
  //         root->children[j]->indices[root->children[j]->indiceCount-1] = (*aabbs)[i].id;
  //         // int toremove = findIndex(root->indices,root->indiceCount,i);
  //         // if (toremove != -1)
  //         // {
  //         //   root->indices[toremove] = root->indices[root->indiceCount - 1];
  //         //   root->indiceCount = root->indiceCount - 1;
  //         // }
  //       }
  //       else
  //       {
  //
  //
  //         createTree_r(root->children[j],flatCount,aabbs,aabbcount,depth + 1,false);
  //       }
  //     }
  //   }
  // }
   int remcount = 0;
  for (k =0 ;k < 8; k++)//prune
  {
    if (root->children[k]->indiceCount == 0 && root->children[k]->leaf)
    {
      free_r(root->children[k]);
      root->children[k] = NULL;
      remcount++;
    }
  }
  if (remcount == 8)
  {
    root->leaf = true;
  }




}



fn_Octree fn_createOctree(fn_AABB* aabbs,int aabbcount)
{
  //printf("%i\n",aabbcount );
  fn_Octree ret;

  int i,j;
  struct fn_OctreeNode* root = malloc(sizeof(struct fn_OctreeNode));

  //get size of root node
  fn_vec3 min = fn_subVec3(aabbs[0].position,aabbs[0].hwidth);
  fn_vec3 max = fn_addVec3(aabbs[0].position,aabbs[0].hwidth);
  //#pragma omp parallel for private(i)
  for (i = 0;i < aabbcount;i++)
  {
    fn_vec3 points[8];
    getAABBPoints(aabbs[i],points);

    for (j = 0;j < 8;j ++)
    {

      if (points[j].x < min.x)
      {
        min.x = points[j].x;
      }
      if (points[j].y < min.y)
      {
        min.y = points[j].y;
      }
      if (points[j].z < min.z)
      {
        min.z = points[j].z;
      }

      if (points[j].x > max.x)
      {
        max.x = points[j].x;
      }
      if (points[j].y > max.y)
      {
        max.y = points[j].y;
      }
      if (points[j].z > max.z)
      {
        max.z = points[j].z;
      }

    }
  }
  max = fn_addVec3(max,fn_createVec3s(10));
  min = fn_subVec3(min,fn_createVec3s(10));
  root->box.position = fn_multVec3s(fn_addVec3(max,min),0.5);
  root->box.hwidth = fn_multVec3s(fn_subVec3(max,min),0.5);

  ret.min_position = min;
  ret.max_position = max;


  root->indices = NULL;
  root->indiceCount = 0;
  root->leaf = false;
  memset(root->children, 0, 8*sizeof(int));

  ret.root = root;
  ret.aabbs = aabbs;
  ret.aabbCount = aabbcount;
  int flatCount = 1;
  ret.usedIndices = malloc(sizeof(char)*aabbcount);

  th_AABBPair* pairs = malloc(sizeof(th_AABBPair)*aabbcount);
  for (int i = 0; i < aabbcount; i++) {
    pairs[i].aabb = aabbs[i];
    pairs[i].id = i;
  }

  int paircount = aabbcount;


  createTree_r(ret.root,&flatCount, &pairs, &paircount,0,false);

  free(pairs);
  return ret;
}

// fn_Octree fn_createOctreeParam(fn_AABB* aabbs,int aabbcount,int max_elm,int max_depth)
// {
//   int temp1 = MAX_DEPTH,temp2 = MAX_ELEMENTS;
//   MAX_ELEMENTS = max_elm;
//   MAX_DEPTH = max_depth;
//     fn_Octree ret;
//
//   int i,j;
//   struct fn_OctreeNode* root = malloc(sizeof(struct fn_OctreeNode));
//
//   //get size of root node
//   fn_vec3 min = aabbs[0].position;
//   fn_vec3 max = aabbs[0].position;
//   //#pragma omp parallel for private(i)
//   for (i = 0;i < aabbcount;i++)
//   {
//     fn_vec3 points[8];
//     getAABBPoints(aabbs[i],points);
//
//     for (j = 0;j < 8;j ++)
//     {
//
//       if (points[j].x < min.x)
//       {
//         min.x = points[j].x;
//       }
//       if (points[j].y < min.y)
//       {
//         min.y = points[j].y;
//       }
//       if (points[j].z < min.z)
//       {
//         min.z = points[j].z;
//       }
//
//       if (points[j].x > max.x)
//       {
//         max.x = points[j].x;
//       }
//       if (points[j].y > max.y)
//       {
//         max.y = points[j].y;
//       }
//       if (points[j].z > max.z)
//       {
//         max.z = points[j].z;
//       }
//
//     }
//   }
//   root->box.position = fn_multVec3s(fn_addVec3(max,min),0.5);
//   root->box.hwidth = fn_abs(fn_subVec3(root->box.position,min));
//
//   root->indices = NULL;
//   root->indiceCount = 0;
//   root->leaf = false;
//   memset(root->children, 0, 8*sizeof(int));
//
//   ret.root = root;
//   ret.aabbs = aabbs;
//   ret.aabbCount = aabbcount;
//   int flatCount = 1;
//
//
//
//   createTree_r(ret.root,&flatCount, &aabbs, &aabbcount,0,fn_getProfileVar("multithread") == 1);
//
//   MAX_ELEMENTS = temp2;
//   MAX_DEPTH = temp1;
//   return ret;
// }



static void getAABBTouch_r(fn_AABB aabb,struct fn_OctreeNode* node,int* indiceCount,int** ret,char* usedIndices)
{
  int i,j;

  for (i =0;i< 8;i++)
  {
    if(node->children[i] != NULL && fn_aabbCheck(aabb,node->children[i]->box))
    {

      if (node->children[i]->leaf && node->children[i]->indiceCount > 0)
      {
        //   memcpy(&((*ret)[*indiceCount]),&node->children[i]->indices[0],node->children[i]->indiceCount * sizeof(int));
        // *indiceCount = *indiceCount+node->children[i]->indiceCount;
        for (j = 0;j < node->children[i]->indiceCount;j++)
        {
          if (usedIndices[node->children[i]->indices[j]] == 0)
          {
            *indiceCount = *indiceCount+1;

            (*ret)[*indiceCount - 1] = node->children[i]->indices[j];
            usedIndices[node->children[i]->indices[j]] = 1;
          }

        }
      }

      if (!node->children[i]->leaf)
        getAABBTouch_r(aabb,node->children[i],indiceCount,ret,usedIndices);

    }
  }

}


int* fn_getAABBSTouch(fn_AABB aabb,fn_Octree* otree,int* indiceCount)
{
  // int* usedIndices = malloc(otree->aabbCount*sizeof(int));
  int i;
  *indiceCount = 0;
  memset(otree->usedIndices,0,sizeof(char)*otree->aabbCount);
  // for (i = 0; i < otree->aabbCount;i++)
  //   otree->usedIndices[i] = 0;

  int* ret = malloc(sizeof(int)*(otree->aabbCount));
  if (fn_aabbCheck(aabb,otree->root->box))
    getAABBTouch_r(aabb,otree->root,indiceCount,&ret,otree->usedIndices);

  // free(usedIndices);
//  ret = realloc(ret,sizeof(int)*(*indiceCount));
  return ret;
}

void fn_getAABBSTouchv(fn_AABB aabb,fn_Octree* otree,int* indiceCount,int* ret,char* memory)
{
  // int* usedIndices = NULL;

  *indiceCount = 0;
  memset(memory,0,sizeof(char)*otree->aabbCount);
  // usedIndices = calloc(otree->aabbCount,sizeof(int));

  if (fn_aabbCheck(aabb,otree->root->box))
    getAABBTouch_r(aabb,otree->root,indiceCount,&ret,memory);

  // free(usedIndices);

}


//static void export_r(gzFile f,struct fn_OctreeNode* root)
//{
//  bool tval = (root == NULL);
//  gzwrite(f,&tval,sizeof(bool));
//
//  if (tval)
//  {
//    return;
//  }
//
//  int i;
//  gzwrite(f,&root->indiceCount,sizeof(int)*1);
//  if (root->indiceCount != 0)
//    gzwrite(f,root->indices,sizeof(int)*root->indiceCount);
//  gzwrite(f,&root->box,sizeof(fn_AABB));
//  gzwrite(f,&root->leaf,sizeof(bool));
//  if (!root->leaf)
//  {
//    for (i =0;i < 8;i++)
//    {
//      export_r(f,root->children[i]);
//    }
//  }
//
//}
//
//void fn_exportOctree(fn_Octree* octree,const char* demoName)
//{
//  char filename[512] = "fn1/octrees/";
//  strcat(filename,demoName);
//  gzFile f = gzopen(filename, "wb");
//  export_r(f,octree->root);
//  gzclose(f);
//}

//static bool import_r(gzFile f,struct fn_OctreeNode* root)
//{
//  bool tval;
//  gzread(f,&tval,sizeof(bool));
//
//  if (tval)
//  {
//    return false;
//  }
//
//  int i;
//  gzread(f,&root->indiceCount,sizeof(int)*1);
//  if (root->indiceCount != 0)
//  {
//    root->indices = malloc(sizeof(int)*root->indiceCount);
//    gzread(f,root->indices,sizeof(int)*root->indiceCount);
//  }
//  gzread(f,&root->box,sizeof(fn_AABB));
//  gzread(f,&root->leaf,sizeof(bool));
//  if (!root->leaf)
//  {
//    for (i =0;i < 8;i++)
//    {
//      root->children[i] = (struct fn_OctreeNode*)malloc(sizeof(struct fn_OctreeNode));
//      root->children[i]->indices = NULL;
//      root->children[i]->indiceCount = 0;
//      root->children[i]->leaf = true;
//      memset(root->children[i]->children, 0, 8*sizeof(int));
//      bool  b = import_r(f,root->children[i]);
//      if (!b)
//      {
//        free(root->children[i]);
//        root->children[i] = NULL;
//      }
//
//    }
//  }
//  return true;
//
//}

//fn_Octree fn_importOctree(const char* demoName,fn_AABB* aabbs,int aabbCount)
//{
//  fn_Octree ret;
//  ret.aabbs = aabbs;
//  ret.aabbCount = aabbCount;
//  ret.root = malloc(sizeof(struct fn_OctreeNode));
//  char filename[512] = "fn1/octrees/";
//  strcat(filename,demoName);
//  gzFile f = gzopen(filename, "rb");
//
//  import_r(f,ret.root);
//
//  gzclose(f);
//  return ret;
//}



void fn_freeOctree(fn_Octree* octree)
{
  free(octree->usedIndices);
  free_r(octree->root);
}
