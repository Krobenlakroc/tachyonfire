#include "fn_grid.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../fn_engine/th_threads.h"

#include "../fn_engine/th_system.h"

//types here
static float padding = 0;
static bool supress_warning = true;
static int MAX_COLLIDABLE = 1000;
// static void getAABBPoints(fn_AABB aabb,fn_vec3* points)
// {
//   points[0] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,aabb.hwidth.y,aabb.hwidth.z)); // 1 1 1
//   points[1] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,-aabb.hwidth.y,-aabb.hwidth.z));// -1 -1 -1
//   points[2] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,aabb.hwidth.y,aabb.hwidth.z)); // -1 1 1
//   points[3] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,-aabb.hwidth.y,aabb.hwidth.z)); // -1 -1 1
//   points[4] = fn_addVec3(aabb.position,fn_createVec3(-aabb.hwidth.x,aabb.hwidth.y,-aabb.hwidth.z)); // -1 1 -1
//   points[5] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,aabb.hwidth.y,-aabb.hwidth.z)); // 1 1 -1
//   points[6] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,-aabb.hwidth.y,aabb.hwidth.z)); // 1 -1 1
//   points[7] = fn_addVec3(aabb.position,fn_createVec3(aabb.hwidth.x,-aabb.hwidth.y,-aabb.hwidth.z)); // 1 -1 -1
// }

static bool contains(int* indices,int index)
{
  int i;
  bool ret = false;

  for (i = 1 ;i <= indices[0];i++)
  {
    if (indices[i] == index)
    {
      ret = true;
    }
  }

  return ret;
}

static bool containsv(void** indices,void* index,int range)
{
  intptr_t i;
  bool ret = false;

  for (i = 0 ;i < range;i++)
  {
    if (indices[i] == index)
    {
      ret = true;
    }
  }

  return ret;
}

int fn_getGridID (int x,int y,int z,int w, int h)
{
  return (z * w * h) + (y * w) + x;
}
fn_vec3 fn_getGridPos(int idx,int w,int h)
{
  int z = floor(idx / (w * h));
  idx -= (z * w * h);
  return fn_createVec3(idx % w,idx / w,z);
}

int fn_worldToGrid(fn_Grid* grid,fn_vec3 pos)
{
  pos = fn_addVec3(pos,grid->offset);
  fn_vec3 gpos = fn_createVec3(floor(pos.x / grid->cellsize.x) ,floor(pos.y / grid->cellsize.y) ,floor(pos.z / grid->cellsize.z) );

  if (gpos.x > grid->cellcount || gpos.y > grid->cellcount || gpos.z > grid->cellcount || gpos.x < 0 || gpos.y < 0 || gpos.z < 0 )
  {
    return grid->sectionCount + 1;//purposefully overflow
  }
  return fn_getGridID(gpos.x,gpos.y,gpos.z,grid->cellcount,grid->cellcount);
}

fn_vec3 fn_getGridIndices(fn_Grid* grid,fn_vec3 pos)
{
  pos = fn_addVec3(pos,grid->offset);
  fn_vec3 gpos = fn_createVec3(floor(pos.x / grid->cellsize.x) ,floor(pos.y / grid->cellsize.y) ,floor(pos.z / grid->cellsize.z) );
  return gpos;
}

// static th_Entity** usedIndices = NULL;
// static th_Entity** ret = NULL;
// void fn_freeGrid(fn_Grid* grid)
// {
//   int i;
//   free(grid->ret);
//   free(grid->usedIndices);
//   for (i = 0;i < grid->entityCount;i++)
//     free(grid->sectionsTouched[i]);
//   free(grid->sectionsTouched);
//   for ( i =0;i < grid->sectionCount;i++)
//   {
//     if (grid->sections[i].entities != NULL)
//       free(grid->sections[i].entities);
//   }
//   free(grid->sections);
// }

void fn_createGrid(th_Allocator* alloc,fn_Grid* grid,th_Entity* entities,int entityCount,fn_vec3 cellsize,int cellcount)
{
  int i;
  // if (rebuild)
  // {
  //   free(grid->ret);
  //   free(grid->usedIndices);
  //   for (i = 0;i < entityCount;i++)
  //   {
  //     free(grid->sectionsTouched[i]);
  //   }
  //   free(grid->sectionsTouched);
  //   for ( i =0;i < grid->sectionCount;i++)
  //   {
  //     if (grid->sections[i].entities != NULL)
  //       free(grid->sections[i].entities);
  //   }
  //   free(grid->sections);
  // }
  MAX_COLLIDABLE = 0;
  grid->alloc = alloc;
  grid->entities = entities;
  grid->entityCount = entityCount;
  grid->created = true;

  grid->cellsize = cellsize;
  grid->cellcount = cellcount;
  grid->sectionsTouched = calloc(entityCount,sizeof(int*));
  grid->sectionsTouched = th_arenaManage(alloc,grid->sectionsTouched,sizeof(int*)*entityCount);
  for (i = 0;i < entityCount;i++)
  {
    grid->sectionsTouched[i] = calloc(31,sizeof(int));
    grid->sectionsTouched[i] = th_arenaManage(alloc,grid->sectionsTouched[i],sizeof(int)*31);
    // 0 is sections touched, 1-8 is ID of nth touched section
  }
  grid->sectionCount = cellcount*cellcount*cellcount;
  grid->sections = calloc(grid->sectionCount,sizeof(fn_GridSection));
  grid->sections = th_arenaManage(alloc,grid->sections,grid->sectionCount*sizeof(fn_GridSection));

  fn_vec3 hsize = fn_multVec3s(cellsize,0.5);
  fn_vec3 offset = fn_multVec3s(hsize,cellcount);
  grid->offset = offset;

  for ( i =0;i < grid->sectionCount;i++)
  {
    grid->sections[i].alloced = 0;
    grid->sections[i].entityCount = 0;
    grid->sections[i].entities = NULL;//th_reallocCleanup(alloc,NULL,sizeof(th_Entity)*100);
    // printf("RA %i\n",i);
  }

  for ( i =0;i < entityCount;i++)
  {
    fn_updateEntityGrid(grid,i,1.f);
  }

  grid->usedIndices = th_alloc(alloc,sizeof(th_Entity*)*grid->entityCount);
  grid->ret = th_alloc(alloc,sizeof(th_Entity*)*grid->entityCount);

  grid->grid_mem = th_alloc(alloc,sizeof(fn_GridMemory)*th_getNumThreads());
  for (int i = 0 ; i < th_getNumThreads();i++)
  {
    grid->grid_mem[i].usedIndices = th_alloc(alloc,sizeof(th_Entity*)*grid->entityCount);
    grid->grid_mem[i].ret = th_alloc(alloc,sizeof(th_Entity*)*grid->entityCount);
    grid->grid_mem[i].sections_touched = th_alloc(alloc,sizeof(int)*GRID_MAX_SECTIONS_TOUCHED);
  }
}

//get list of entities that an entity in the grid can collide with
// th_Entity** fn_getCollidableGrid(fn_Grid* grid,int entity,int* entities)
// {
//
//   th_Entity** ret = malloc(sizeof(th_Entity*)*MAX_COLLIDABLE*8);
//   *entities = 0;
//   int i;
//   // printf("O %i\n",grid->sectionsTouched[entity][0] );
//   if ( grid->sectionsTouched[entity][0] != 0)
//   {
//   for (i = 1 ;i <= grid->sectionsTouched[entity][0];i++)
//   {
//     // printf("I %p\n",grid->sections[grid->sectionsTouched[entity][i]].entities );
//     if (grid->sections[grid->sectionsTouched[entity][i]].entityCount != 0)
//     {
//     memcpy(&ret[*entities],grid->sections[grid->sectionsTouched[entity][i]].entities,grid->sections[grid->sectionsTouched[entity][i]].entityCount*sizeof(th_Entity*));//grid->sectionsTouched[entity][i]
//     *entities = *entities + grid->sections[grid->sectionsTouched[entity][i]].entityCount;
//     }
//   }
//   }
//
//   return ret;
// }

th_Entity** fn_getCollidableAABB(fn_Grid* grid,fn_AABB aabb,int* entities,fn_GridMemory* memory)
{

  // usedIndices = malloc(sizeof(th_Entity*)*grid->entityCount);
  // ret = malloc(sizeof(th_Entity*)*grid->entityCount);
  int* sectionsTouched = memory->sections_touched;//[1000] = {0};
  memset(sectionsTouched,0,sizeof(int)*GRID_MAX_SECTIONS_TOUCHED);
  *entities = 0;
  int j,i;
  int i1,j1,k1;
  int range = 0;
  aabb.hwidth = fn_addVec3(aabb.hwidth,fn_createVec3s(padding));
  fn_vec3 min = fn_subVec3(aabb.position,aabb.hwidth);
  fn_vec3 size = fn_multVec3s(aabb.hwidth,2.f);
  fn_vec3 qsize = fn_createVec3(fmax(ceil(size.x/grid->cellsize.x),1),fmax(ceil(size.y/grid->cellsize.y),1),fmax(ceil(size.z/grid->cellsize.z),1));

  for (i1 =0;i1 < qsize.x;i1+=1)
  {
    for (j1 =0;j1 < qsize.y;j1+=1)
    {
      for (k1 =0;k1 < qsize.z;k1+=1)
      {
          int gridIndex = fn_worldToGrid(grid,fn_addVec3(min,fn_multVec3s(fn_createVec3(i1,j1,k1),grid->cellsize.y)));

          if (gridIndex < 0 || gridIndex >= grid->sectionCount)
          {
            if (!supress_warning)
            printf("%s\n","Spatial hash Not big enough!" );
            continue;
          }
          if (!contains(sectionsTouched,gridIndex))
            {
            sectionsTouched[0]++;

            if (sectionsTouched[0] >= GRID_MAX_SECTIONS_TOUCHED)
            {
              //if (!supress_warning)
              sectionsTouched[0]--;
              fn_printVec3(size);
                printf("%s %i %i %i\n","Too many sections touched",(int)qsize.x,(int)qsize.y,(int)qsize.z );
                print_stacktrace();
              return memory->ret;
            }

            sectionsTouched[sectionsTouched[0]] = gridIndex;
              for (i = 0; i < grid->sections[gridIndex].entityCount;i++)
              {
                if (!containsv((void**)memory->usedIndices,grid->sections[gridIndex].entities[i],range))
                {
                  range++;
                  intptr_t temp = range - 1;
                  memory->usedIndices[temp] = grid->sections[gridIndex].entities[i];
                  memory->ret[*entities] = grid->sections[gridIndex].entities[i];
                  *entities = *entities+1;
                }
              }
            }
      }
    }
  }
  // free(usedIndices);

  return memory->ret;
}

void checkcell(fn_Grid* grid,int i1,int j1,int k1,int* sectionsTouched,fn_GridMemory* memory,int* entities,int* range)
{
  int gridIndex = fn_worldToGrid(grid,fn_multVec3s(fn_createVec3(i1,j1,k1),grid->cellsize.y));

  if (gridIndex < 0 || gridIndex >= grid->sectionCount)
  {
    if (!supress_warning)
    printf("%s\n","Spatial hash Not big enough!" );
    return;
  }
  if (!contains(sectionsTouched,gridIndex))
  {
    sectionsTouched[0]++;

    if (sectionsTouched[0] >= 1000)
    {
      sectionsTouched[0]--;

      printf("%s","Too many sections touched" );

      return;
    }

    sectionsTouched[sectionsTouched[0]] = gridIndex;
    for (int i = 0; i < grid->sections[gridIndex].entityCount;i++)
    {
      if (!containsv((void**)memory->usedIndices,grid->sections[gridIndex].entities[i],*range))
      {
        *range = *range + 1;
        intptr_t temp = *range - 1;
        memory->usedIndices[temp] = grid->sections[gridIndex].entities[i];
        memory->ret[*entities] = grid->sections[gridIndex].entities[i];
        *entities = *entities+1;
      }
    }
  }
}

th_Entity** th_getCollidableLine(fn_Grid* grid,fn_vec3 start,fn_vec3 end,int* entities,fn_GridMemory* memory)
{

  // usedIndices = malloc(sizeof(th_Entity*)*grid->entityCount);
  // ret = malloc(sizeof(th_Entity*)*grid->entityCount);
  int sectionsTouched[1000] = {0};
  *entities = 0;
  int j,i;
  int i1,j1,k1;
  int range = 0;
  // aabb.hwidth = fn_addVec3(aabb.hwidth,fn_createVec3s(padding));
  // fn_vec3 min = fn_subVec3(aabb.position,aabb.hwidth);
  // fn_vec3 size = fn_multVec3s(aabb.hwidth,2.f);
  // fn_vec3 qsize = fn_createVec3(fmax(ceil(size.x/grid->cellsize.x),1),fmax(ceil(size.y/grid->cellsize.y),1),fmax(ceil(size.z/grid->cellsize.z),1));

  fn_vec3 start_q = fn_getGridIndices(grid,start);

  fn_vec3 end_q = fn_getGridIndices(grid,end);
  float x1,x2,y1,y2,z1,z2;
  x1 = start_q.x;y1 = start_q.y;z1 = start_q.z;
  x2 = end_q.x;y2 = end_q.y;z2 = end_q.z;

float dx = fabs(x2 - x1);
float dy = fabs(y2 - y1);
float dz = fabs(z2 - z1);
float xs,ys,zs;

if (x2 > x1)
{
  xs = 1;
}
else
{
  xs = -1;
}

if (y2 > y1)
{
  ys = 1;
}
else
{
  ys = -1;
}

if (z2 > z1)
{
  zs = 1;
}
else
{
  zs = -1;
}


//x axis
if (dx >= dy && dx >= dz)
{
  float p1 = 2 * dy - dx;
  float p2 = 2 * dz - dx;
  while (x1 != x2)
  {
    x1 += xs;
    if (p1 >= 0)
    {
      y1 += ys;
      p1 -= 2 * dx;
    }

    if (p2 >= 0)
    {
      z1 += zs;
      p2 -= 2 * dx;
    }

    p1 += 2 * dy;
    p2 += 2 * dz;
    checkcell(grid,x1,y1,z1,sectionsTouched,memory,entities,&range);
  }

}
else if (dy >= dx && dy >= dz)
{
  float p1 = 2 * dx - dy;
  float p2 = 2 * dz - dy;
  while (y1 != y2)
  {
    y1 += ys;
    if (p1 >= 0)
    {
      x1 += xs;
      p1 -= 2 * dy;
    }

    if (p2 >= 0)
    {
      z1 += zs;
      p2 -= 2 * dy;
    }

    p1 += 2 * dx;
    p2 += 2 * dz;
    checkcell(grid,x1,y1,z1,sectionsTouched,memory,entities,&range);
  }

}
else
{
  float p1 = 2 * dy - dz;
  float p2 = 2 * dx - dz;
  while (z1 != z2)
  {
    z1 += zs;
    if (p1 >= 0)
    {
      y1 += ys;
      p1 -= 2 * dz;
    }

    if (p2 >= 0)
    {
      x1 += xs;
      p2 -= 2 * dz;
    }
    p1 += 2 * dy;
    p2 += 2 * dx;
    checkcell(grid,x1,y1,z1,sectionsTouched,memory,entities,&range);
  }

}


  return memory->ret;
}

void fn_getCollidableAABBSections(fn_Grid* grid,fn_AABB aabb,int* sectionsTouched)
{

  // int i,j,k;
  int i,j,k;
  aabb.hwidth = fn_addVec3(aabb.hwidth,fn_createVec3s(padding));
  fn_vec3 min = fn_subVec3(aabb.position,aabb.hwidth);
  fn_vec3 size = fn_multVec3s(aabb.hwidth,2.f);
  fn_vec3 qsize = fn_createVec3(fmax(ceil(size.x/grid->cellsize.x),1),fmax(ceil(size.y/grid->cellsize.y),1),fmax(ceil(size.z/grid->cellsize.z),1));
  // int qsizep = qsize.x * qsize.y * qsize.z;

  for (i =0;i < qsize.x;i+=1)
  {
    for (j =0;j < qsize.y;j+=1)
    {
      for (k =0;k < qsize.z;k+=1)
      {
          int gridIndex = fn_worldToGrid(grid,fn_addVec3(min,fn_multVec3s(fn_createVec3(i,j,k),grid->cellsize.y)));

          if (gridIndex < 0 || gridIndex >= grid->sectionCount)
          {
            if (!supress_warning)
            printf("%s\n","Spatial hash Not big enough!" );
            continue;
          }
          if (sectionsTouched[0] >= 30)
          {
            // printf("%i\n",gridIndex );
            // fn_printVec3(aabb.position);
            // printf("%s\n","Too many sectiosn touched" );
            continue;
          }
          if (!contains(sectionsTouched,gridIndex))
          {
          sectionsTouched[0]++;
          sectionsTouched[sectionsTouched[0]] = gridIndex;
          }
      }
    }
  }


}

void fn_removeEntityGrid(fn_Grid* grid,int entity)
{
  int i,j;
  //printf("%i\n",entity );
  for (i = 1 ;i <= grid->sectionsTouched[entity][0];i++)
  {

    for (j = 0;j < grid->sections[grid->sectionsTouched[entity][i]].entityCount;j++)
    {
      if (grid->sections[grid->sectionsTouched[entity][i]].entities[j] == &grid->entities[entity])
      {
      //moves top of the entity stack to the current position
        grid->sections[grid->sectionsTouched[entity][i]].entities[j] = grid->sections[grid->sectionsTouched[entity][i]].entities[grid->sections[grid->sectionsTouched[entity][i]].entityCount - 1];

        grid->sections[grid->sectionsTouched[entity][i]].entityCount--;
         break;
      }
    }
  }



  grid->sectionsTouched[entity][0] = 0;
}

void fn_updateEntityGrid(fn_Grid* grid,int entity,float dt)
{
  int i,j;
  //printf("%i\n",entity );
  for (i = 1 ;i <= grid->sectionsTouched[entity][0];i++)
  {
    if (grid->sectionsTouched[entity][i] < 0 || grid->sectionsTouched[entity][i] >= grid->sectionCount)
    {
      printf("%i %i\n",grid->sectionsTouched[entity][i],grid->sectionCount );
    }
    else
    {
      for (j = 0;j < grid->sections[grid->sectionsTouched[entity][i]].entityCount;j++)
      {
        if (grid->sections[grid->sectionsTouched[entity][i]].entities[j] == &grid->entities[entity])
        {

        //moves top of the entity stack to the current position
        if (grid->sections[grid->sectionsTouched[entity][i]].entityCount > 0)
        {
          grid->sections[grid->sectionsTouched[entity][i]].entities[j] = grid->sections[grid->sectionsTouched[entity][i]].entities[grid->sections[grid->sectionsTouched[entity][i]].entityCount - 1];

          grid->sections[grid->sectionsTouched[entity][i]].entityCount--;
           break;
        }

        }
      }
    }


  }



  grid->sectionsTouched[entity][0] = 0;

  // if (!grid->entities[entity].enabled)
  //   return;

  fn_AABB broadphase = fn_getSweptBroadphaseBox(grid->entities[entity].aabb,fn_multVec3s(grid->entities[entity].velocity,dt));
  fn_getCollidableAABBSections(grid,broadphase,grid->sectionsTouched[entity]);

  for (j = 1;j <= grid->sectionsTouched[entity][0];j++)
  {

    int gridIndex = grid->sectionsTouched[entity][j];//fn_worldToGrid(grid,points[j]);

    grid->sections[gridIndex].entityCount++;
    if (  grid->sections[gridIndex].entityCount > grid->sections[gridIndex].alloced)
    {

    grid->sections[gridIndex].alloced++;

    grid->sections[gridIndex].entities = th_reallocCleanup(grid->alloc,grid->sections[gridIndex].entities,sizeof(th_Entity*)*grid->sections[gridIndex].alloced);
    }
    grid->sections[gridIndex].entities[grid->sections[gridIndex].entityCount - 1] = &grid->entities[entity];
   // }

  }


}

static th_EntityCollisionEdict* edicts = NULL;
static int edict_count = 0;

void th_freeEntityGroups()
{
  free(edicts);
  edicts = NULL;
  edict_count = 0;
}

void th_registerEntityGroup(th_EntityCollisionEdict e)
{
  edicts = realloc(edicts,sizeof(th_EntityCollisionEdict)*(edict_count + 1));
  edicts[edict_count] = e;
  edict_count++;
}

th_Entity* th_collideWithEntities(th_EntityEdictFlags flags,th_Entity* e,float worldtime,fn_vec3* normal,bool* is_hit,int thread_id,float dt,fn_vec3* pos,float* time)
{
  float time_least = 1000;
  bool hit_something = false;
  th_Entity* closest_entity = NULL;

  for (int i = 0 ; i < edict_count;i++)
  {
    th_EntityCollisionEdict edict = edicts[i];
    if (!(flags & edict.flags) )
    {
      continue;
    }


    bool useray = false;
    if (flags & TH_USE_RAY)
    {
      useray = true;
    }
    if (edict.grid == NULL)
    {
      bool collided = false;
      th_Collision ret;
      ret.time = 2;
      int cindex = 0;
      for (int j =0 ; j < *edict.entityCount ; j ++ )
      {
        if (!edict.entities[j].alive || (!edict.entities[j].playercollideable && (flags & TH_CAN_KILL_PLAYER)))
        {
          continue;
        }
        th_Collision col = th_sweepTestAABB(e->aabb,fn_multVec3s(fn_subVec3(e->velocity,edict.entities[j].velocity),dt),edict.entities[j].aabb);
        if (col.collided && col.time < worldtime)
        {
          if (col.time < ret.time)
          {
            cindex = j;
            ret = col;
            collided = true;


          }
          else if (!collided)
          {
            cindex = j;
            ret = col;
            collided = true;
          }
        }

      }


      if (ret.time < time_least)
      {
        *time = ret.time;
        *normal = ret.normal;
        *is_hit = collided ;
        *pos = fn_addVec3(e->aabb.position,fn_multVec3s(e->velocity,dt*fn_clamp(ret.time,0,1)));

        hit_something = true;
        closest_entity = &edict.entities[cindex];
        time_least = ret.time;
      }

      //return &edict.entities[cindex];
    }
    else
    {
      int colCountPossible = 0;
      th_AABB neighborhood = fn_getSweptBroadphaseBox(e->aabb,fn_multVec3s(e->velocity,dt));
      th_Entity** colsPossible;
      if (useray)
      {
        colsPossible = th_getCollidableLine(edict.grid,e->aabb.position,fn_addVec3(e->aabb.position,e->velocity),&colCountPossible,&edict.grid->grid_mem[thread_id]);
      }
      else
      {
        colsPossible = fn_getCollidableAABB(edict.grid,neighborhood,&colCountPossible,&edict.grid->grid_mem[thread_id]);
      }



      bool collided = false;
      th_Collision ret;
      ret.time = 2;
      int cindex = 0;
      for (int j =0 ; j < colCountPossible ; j ++ )
      {
        if (!colsPossible[j]->alive || (!colsPossible[j]->playercollideable && (flags & TH_CAN_KILL_PLAYER)))
        {
          continue;
        }
        th_Collision col = th_sweepTestAABB(e->aabb,fn_multVec3s(fn_subVec3(e->velocity,colsPossible[j]->velocity),dt),colsPossible[j]->aabb);
        if (col.collided && col.time < worldtime)
        {
          if (col.time < ret.time)
          {
            ret = col;
            collided = true;
            cindex = j;
          }
          else if (!collided)
          {
            ret = col;
            collided = true;
            cindex = j;
          }
        }

      }

      if (ret.time < time_least)
      {
      *time = ret.time;
      *normal = ret.normal;
      *is_hit = collided ;
      *pos = fn_addVec3(e->aabb.position,fn_multVec3s(e->velocity,dt*fn_clamp(ret.time,0,1)));

      hit_something = true;
      closest_entity = colsPossible[cindex];
      time_least = ret.time;
    //  return colsPossible[cindex];
      }
    }

  }

  if (!hit_something)
  {
    *time = 0 ;
    *normal = fn_createVec3(0,0,0);
    *is_hit = false;
    *pos = fn_createVec3(0,0,0);
    return NULL;
  }
  else {
    return closest_entity;
  }

}

th_Entity* th_traceWithEntitites(th_EntityEdictFlags flags,fn_vec3 start,fn_vec3 end,fn_vec3* normal,bool* is_hit,int thread_id,float dt,fn_vec3* pos,float* time,float radius)
{
  th_Entity e;
  e.aabb.position = start;
  e.aabb.hwidth = fn_createVec3(radius,radius,radius);
  e.aabb.mode = SPHERE;
  e.velocity = fn_subVec3(end,start);
  return th_collideWithEntities(flags ,&e,1,normal,is_hit,thread_id,1,pos,time);
}

th_Entity* th_collideWithEntitiesExclusionary(th_EntityEdictFlags flags,th_EntityEdictFlags eflags,th_Entity* e,float worldtime,fn_vec3* normal,bool* is_hit,int thread_id,float dt,fn_vec3* pos,float* time)
{
  float time_least = 1000;
  bool hit_something = false;
  th_Entity* closest_entity = NULL;

  for (int i = 0 ; i < edict_count;i++)
  {
    th_EntityCollisionEdict edict = edicts[i];
    if (!(flags & edict.flags) )
    {
      continue;
    }

    if ((eflags & edict.flags) )
    {
      continue;
    }


    bool useray = false;
    if (flags & TH_USE_RAY)
    {
      useray = true;
    }
    if (edict.grid == NULL)
    {
      bool collided = false;
      th_Collision ret;
      ret.time = 2;
      int cindex = 0;
      for (int j =0 ; j < *edict.entityCount ; j ++ )
      {
        if (!edict.entities[j].alive || (!edict.entities[j].playercollideable && (flags & TH_CAN_KILL_PLAYER)))
        {
          continue;
        }
        th_Collision col = th_sweepTestAABB(e->aabb,fn_multVec3s(fn_subVec3(e->velocity,edict.entities[j].velocity),dt),edict.entities[j].aabb);
        if (col.collided && col.time < worldtime)
        {
          if (col.time < ret.time)
          {
            cindex = j;
            ret = col;
            collided = true;


          }
          else if (!collided)
          {
            cindex = j;
            ret = col;
            collided = true;
          }
        }

      }


      if (ret.time < time_least)
      {
        *time = ret.time;
        *normal = ret.normal;
        *is_hit = collided ;
        *pos = fn_addVec3(e->aabb.position,fn_multVec3s(e->velocity,dt*fn_clamp(ret.time,0,1)));

        hit_something = true;
        closest_entity = &edict.entities[cindex];
        time_least = ret.time;
      }

      //return &edict.entities[cindex];
    }
    else
    {
      int colCountPossible = 0;
      th_AABB neighborhood = fn_getSweptBroadphaseBox(e->aabb,fn_multVec3s(e->velocity,dt));
      th_Entity** colsPossible;
      if (useray)
      {
        colsPossible = th_getCollidableLine(edict.grid,e->aabb.position,fn_addVec3(e->aabb.position,e->velocity),&colCountPossible,&edict.grid->grid_mem[thread_id]);
      }
      else
      {
        colsPossible = fn_getCollidableAABB(edict.grid,neighborhood,&colCountPossible,&edict.grid->grid_mem[thread_id]);
      }



      bool collided = false;
      th_Collision ret;
      ret.time = 2;
      int cindex = 0;
      for (int j =0 ; j < colCountPossible ; j ++ )
      {
        if (!colsPossible[j]->alive || (!colsPossible[j]->playercollideable && (flags & TH_CAN_KILL_PLAYER)))
        {
          continue;
        }
        th_Collision col = th_sweepTestAABB(e->aabb,fn_multVec3s(fn_subVec3(e->velocity,colsPossible[j]->velocity),dt),colsPossible[j]->aabb);
        if (col.collided && col.time < worldtime)
        {
          if (col.time < ret.time)
          {
            ret = col;
            collided = true;
            cindex = j;
          }
          else if (!collided)
          {
            ret = col;
            collided = true;
            cindex = j;
          }
        }

      }

      if (ret.time < time_least)
      {
      *time = ret.time;
      *normal = ret.normal;
      *is_hit = collided ;
      *pos = fn_addVec3(e->aabb.position,fn_multVec3s(e->velocity,dt*fn_clamp(ret.time,0,1)));

      hit_something = true;
      closest_entity = colsPossible[cindex];
      time_least = ret.time;
    //  return colsPossible[cindex];
      }
    }

  }

  if (!hit_something)
  {
    *time = 0 ;
    *normal = fn_createVec3(0,0,0);
    *is_hit = false;
    *pos = fn_createVec3(0,0,0);
    return NULL;
  }
  else {
    return closest_entity;
  }

}

void th_getEntitiesInRadius(th_EntityEdictFlags flags,fn_vec3 pos,float radius,int thread_id,float dt,int* count,th_Entity** eptr)
{
  // float time_least = 1000;
  // bool hit_something = false;
  // th_Entity* closest_entity = NULL;

  *count = 0;

  th_Entity** list = eptr;

  for (int i = 0 ; i < edict_count;i++)
  {
    th_EntityCollisionEdict edict = edicts[i];
    if (!(flags & edict.flags) )
    {
      continue;
    }


    if (edict.grid == NULL)
    {
      bool collided = false;
      // th_Collision ret;
      // ret.time = 2;
      int cindex = 0;
      for (int j =0 ; j < *edict.entityCount ; j ++ )
      {
        if (!edict.entities[j].alive)
        {
          continue;
        }


        th_Collision col = th_sweepTestSphere(pos,radius,fn_multVec3s(fn_subVec3(fn_createVec3s(0),edict.entities[j].velocity),dt),edict.entities[j].aabb);
        if (col.collided)
        {

          eptr[*count] = &edict.entities[j];
          *count = *count + 1;
          if (*count >= TH_MAX_ENTITY_PTRS)
          {
            return;
          }
        }

      }


    }
    else
    {
      int colCountPossible = 0;
      th_AABB neighborhood;// = fn_getSweptBroadphaseBox(e->aabb,fn_multVec3s(e->velocity,dt));
      neighborhood.position = pos;
      neighborhood.hwidth = fn_createVec3s(radius);
      neighborhood.mode = BOX;
      th_Entity** colsPossible;

      colsPossible = fn_getCollidableAABB(edict.grid,neighborhood,&colCountPossible,&edict.grid->grid_mem[thread_id]);




      bool collided = false;
      // th_Collision ret;
      // ret.time = 2;
      int cindex = 0;
      for (int j =0 ; j < colCountPossible ; j ++ )
      {
        if (!colsPossible[j]->alive)
        {
          continue;
        }
        th_Collision col = th_sweepTestSphere(pos,radius,fn_multVec3s(fn_subVec3(fn_createVec3s(0),colsPossible[j]->velocity),dt),colsPossible[j]->aabb);
        if (col.collided )
        {
          eptr[*count] = colsPossible[j];
          *count = *count + 1;
          if (*count >= TH_MAX_ENTITY_PTRS)
          {
            return;
          }
        }

      }

    }

  }

}
