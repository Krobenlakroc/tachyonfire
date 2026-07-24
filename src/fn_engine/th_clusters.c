#include "th_clusters.h"
#include "th_collision.h"

#include <pthread.h>
#include <stdatomic.h>
#include "th_time.h"
#include "th_threads.h"
#include "th_system.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdio.h>
#include <assert.h>

#include <SDL.h>

#define MAX_LIGHTS 256


static const uint early_cutoff_slice  = CLUSTER_Z_OFFSET;
static const float numSlices = CLUSTER_Z_COUNT;
static const uint num_tiles_x = 16;
static const uint num_tiles_y = 8;
static const uint num_tiles_z = CLUSTER_Z_COUNT;



static float screen_x = 1920;
static float screen_y = 1080;



// static float Nearz = 1.0;
// static float Farz = 5000;

static float Nearz = 5.0;
static float Farz = 20000;



float SqDistPointAABB( fn_vec3 p, th_AABB b )
{
    float sqDist = 0.0f;
    fn_vec3 min = fn_subVec3(b.position,b.hwidth);
    fn_vec3 max = fn_addVec3(b.position,b.hwidth);
    for( int i = 0; i < 3; i++ ){
        // for each axis count any excess distance outside box extents
        float v = p.v[i];
        if( v < min.v[i] ) sqDist += (min.v[i] - v) * (min.v[i] - v);
        if( v > max.v[i] ) sqDist += (v - max.v[i]) * (v - max.v[i]);
    }
    return sqDist;
}

bool testSphereAABB(float radius,fn_vec3 center,th_AABB tile){

    float squaredDistance = SqDistPointAABB(center, tile);

    return squaredDistance <= (radius * radius);
}

// bool pointLightIntersectsCluster(float radius,fn_vec3 center,th_AABB b)
// {
//
//
//     fn_vec3 min = b.position;//fn_subVec3(b.position,b.hwidth);
//     fn_vec3 max = b.hwidth;//fn_addVec3(b.position,b.hwidth);
//
//     if (center.x < max.x && center.x > min.x &&
//     center.y < max.y && center.y > min.y &&
//   center.z < max.z && center.z > min.z  )
//   {
//     return true;
//   }
//
//     // get closest point to sphere center
//     fn_vec3 closest = fn_maxVec3(min, fn_minVec3(center, max));
//
//     // check if point is inside the sphere
//     fn_vec3 dist = fn_subVec3(closest , center);
//     return fn_dot(dist, dist) <= (radius * radius);
// }

static inline bool pointLightIntersectsCluster(
  float cx, float cy, float cz, float r2,
  float minx, float miny, float minz,
  float maxx, float maxy, float maxz)
{

  if (cx < maxx && cx > minx &&
    cy < maxy && cy > miny &&
    cz < maxz && cz > minz  )
  {
    return true;
  }
  // Clamp center to box
  float qx = (cx < minx ? minx : (cx > maxx ? maxx : cx));
  float qy = (cy < miny ? miny : (cy > maxy ? maxy : cy));
  float qz = (cz < minz ? minz : (cz > maxz ? maxz : cz));
  float dx = qx - cx, dy = qy - cy, dz = qz - cz;
  return dx*dx + dy*dy + dz*dz <= r2;
}

static uint hashCount;
static int* hashTable = NULL;

static uint** lightIDList = NULL;
static uint* lightCountList = NULL;
static int* offsets_temp = NULL;

static uint* globally_unique_list=NULL;
static int* aliases=NULL;

static th_AABB* clusterAABBs = NULL;
static float* clusterAABBs_minx = NULL;
static float* clusterAABBs_miny = NULL;
static float* clusterAABBs_minz = NULL;
static float* clusterAABBs_maxx = NULL;
static float* clusterAABBs_maxy = NULL;
static float* clusterAABBs_maxz = NULL;

static uint* usedHashes = 0;

static th_Plane* planes = NULL;
static int* hash_counts = NULL;

static uint* access_unpacked = NULL;

int cmpuint (const void * a, const void * b) {
   const uint* x = a;
   const uint* y = b;
   if (*x > *y)
   {
     return 1;
   }
   else
   {
     return(*x < *y) ? -1: 0;
   }
}


fn_vec4 screen2Eye(fn_vec4 coord,fn_mat4 invProj)
{

    fn_vec3 ndc = fn_createVec3(
        2.0 * (coord.x / screen_x)  - 1.0,
        2.0 * ( (coord.y/ screen_y) )  - 1.0,
        2.0 * coord.z - 1.0 // -> [-1, 1]
    );

    fn_vec4 eye = fn_multVec4Mat4(invProj, fn_createVec4(ndc.x,ndc.y,ndc.z, 1.0));
    eye = fn_multVec4s(eye,1.0/eye.w);

    return eye;

}

fn_vec2 Eye2Screen(fn_vec4 eye,fn_mat4 proj)
{
    fn_vec4 clip = fn_multVec4Mat4(proj,eye);
    clip = fn_multVec4s(clip,1.0/clip.w);
    fn_vec2 screen = fn_createVec2(((clip.x + 1.0)/2.0)*screen_x ,((clip.y + 1.0)/2.0)*screen_y );
    return screen;
}

fn_vec3 lineIntersectionToZPlane(fn_vec3 A, fn_vec3 B, float zDistance){
    //all clusters planes are aligned in the same z direction
    fn_vec3 normal = fn_createVec3(0.0, 0.0, 1.0);
    //getting the line from the eye to the tile
    fn_vec3 ab =  fn_subVec3(B , A);
    //Computing the intersection length for the line and the plane
    float t = (zDistance - fn_dot(normal, A)) / fn_dot(normal, ab);
    //Computing the actual xyz position of the point along the line
    fn_vec3 result = fn_addVec3(A ,fn_multVec3s( ab,t));
    return result;
}

th_AABB clusterAABB(float i,float j,float k,fn_mat4 invProj,fn_vec3 eyePos)
{

  float xslicesize = screen_x/16.f;
  float yslicesize = screen_y/8.f;

  fn_vec4 minScreen = fn_createVec4( i*xslicesize,j*yslicesize, 1.0, 1.0);
  fn_vec4 maxScreen = fn_createVec4((i+1)*xslicesize,(j+1)*yslicesize, 1.0, 1.0);

   // -> eye coordinates
   // z is the camera far plane (1 in screen coordinates)
   fn_vec3 minEye = fn_multVec3s(screen2Eye(minScreen,invProj).xyz,-1.0);
   fn_vec3 maxEye = fn_multVec3s(screen2Eye(maxScreen,invProj).xyz,-1.0);


   float clusterNear;
   float clusterFar;
   if (k == 0)
   {
    clusterNear = -Nearz;
    clusterFar = -Nearz * powf(Farz / Nearz,  (k + early_cutoff_slice + 1) / (CLUSTER_Z_VIRTUAL));
   }
   else
   {
    clusterNear = -Nearz * powf(Farz / Nearz, (k + early_cutoff_slice ) / (CLUSTER_Z_VIRTUAL));
    clusterFar  = -Nearz * powf(Farz / Nearz, (k + early_cutoff_slice + 1) / (CLUSTER_Z_VIRTUAL));
   }

   // calculate near and far depth edges of the cluster
   // float clusterNear = -Nearz * powf(Farz / Nearz,  k      / (numSlices));
   // float clusterFar  = -Nearz * powf(Farz / Nearz, (k + 1) / (numSlices));
  // printf("%f\n",clusterFar - clusterNear );

   // this calculates the intersection between:
   // - a line from the camera (origin) to the eye point (at the camera's far plane)
   // - the cluster's z-planes (near + far)
   // we could divide by u_zFar as well
   fn_vec3 minNear = (lineIntersectionToZPlane(eyePos,minEye,clusterNear));//fn_transformVec3(fn_multVec3s(minEye , clusterNear / minEye.z),invView);
   fn_vec3 minFar  = (lineIntersectionToZPlane(eyePos,minEye,clusterFar));//fn_transformVec3(fn_multVec3s(minEye , clusterFar  / minEye.z),invView);
   fn_vec3 maxNear = (lineIntersectionToZPlane(eyePos,maxEye,clusterNear));//fn_transformVec3(fn_multVec3s(maxEye , clusterNear / maxEye.z),invView);
   fn_vec3 maxFar  = (lineIntersectionToZPlane(eyePos,maxEye,clusterFar));//fn_transformVec3(fn_multVec3s(maxEye , clusterFar  / maxEye.z),invView);

   // get extent of the cluster in all dimensions (axis-aligned bounding box)
   // there is some overlap here but it's easier to calculate intersections with AABB
   fn_vec3 minBounds = fn_minVec3(fn_minVec3(minNear, minFar), fn_minVec3(maxNear, maxFar));
   fn_vec3 maxBounds = fn_maxVec3(fn_maxVec3(minNear, minFar), fn_maxVec3(maxNear, maxFar));

   th_AABB ret;
   ret.position = minBounds;//fn_multVec3s(fn_addVec3(minBounds,maxBounds),0.5);
   ret.hwidth = maxBounds;//fn_multVec3s(fn_subVec3(maxBounds,minBounds),0.5);
   return ret;
}

// uint getClusterZIndex_eye(float eyeDepth)
// {
//     eyeDepth = fn_clamp(eyeDepth,Nearz,Farz);
//     uint zIndex = fn_maxi((int)((float)(CLUSTER_Z_VIRTUAL)*(log(eyeDepth/Nearz)/log(Farz/Nearz))) - CLUSTER_Z_OFFSET,0);
//     // uint zIndex = uint(max(log(eyeDepth) * scale + bias, 0.0));
//     return zIndex;
// }

uint getClusterZIndex_eye(float eyeDepth)
{

  uint zIndex = fn_maxi((int)( fn_maxi(0,(float)(CLUSTER_Z_VIRTUAL)*(log((eyeDepth/Nearz))/log(Farz/Nearz)) ) ) - CLUSTER_Z_OFFSET,0);
  // uint zIndex = uint(max(log(eyeDepth) * scale + bias, 0.0));
  return zIndex;
}

void GetProjectedBounds(fn_vec3 center, float radius,  fn_vec2* boxMin,  fn_vec2* boxMax,fn_mat4 Projection)
{

    float d2 = fn_dot(center,center);

    float a = sqrtf(d2 - radius * radius);

    /// view-aligned "right" vector (right angle to the view plane from the center of the sphere. Since  "up" is always (0,n,0), replaced cross product with vec3(-c.z, 0, c.x)
    fn_vec3 right = fn_multVec3s(fn_createVec3(-center.z, 0, center.x),(radius / a));
    fn_vec3 up = fn_createVec3(0,radius,0);

    fn_vec4 projectedRight  = fn_multVec4Mat4(Projection , fn_createVec4Vec3(right,0));
    fn_vec4 projectedUp     = fn_multVec4Mat4(Projection , fn_createVec4Vec3(up,0));

    fn_vec4 projectedCenter = fn_multVec4Mat4(Projection , fn_createVec4Vec3(center,1));

    fn_vec4 north  = fn_addVec4(projectedCenter , projectedUp);
    fn_vec4 east   = fn_addVec4(projectedCenter , projectedRight);
    fn_vec4 south  = fn_subVec4(projectedCenter , projectedUp);
    fn_vec4 west   = fn_subVec4(projectedCenter , projectedRight);
    // printf("%f\n",east.w );
    float oldeastw = east.w;
    north = fn_multVec4s(  north ,1.0/north.w) ;
    east  = fn_multVec4s(  east ,1.0/east.w ) ;
    west  = fn_multVec4s(  west ,1.0/west.w ) ;
    south = fn_multVec4s(  south ,1.0/south.w );

    fn_vec3 boxMin_clip = fn_minVec3(fn_minVec3(fn_minVec3(east.xyz,west.xyz),north.xyz),south.xyz);
    fn_vec3 boxMax_clip = fn_maxVec3(fn_maxVec3(fn_maxVec3(east.xyz,west.xyz),north.xyz),south.xyz);

    // fn_printVec3(east.xyz);
    // fn_printVec3(west.xyz);
    // fn_printVec3(north.xyz);
    // fn_printVec3(south.xyz);


    *boxMin = fn_createVec2(((boxMin_clip.x + 1.0)/2.0)*screen_x ,((boxMin_clip.y + 1.0)/2.0)*screen_y );
    *boxMax = fn_createVec2(((boxMax_clip.x + 1.0)/2.0)*screen_x ,((boxMax_clip.y + 1.0)/2.0)*screen_y );
    //isnan(east.w)
    if ( d2 <= radius * radius || oldeastw == 0 || east.z > 1 || east.x < -1 || east.x > 1 || west.x < -1 || west.x > 1 )
    {
      boxMax->x = screen_x;
      boxMin->x = 0;
    }

    if ( d2 <= radius * radius || oldeastw == 0  )
    {
      boxMax->y = screen_y;
      boxMin->y = 0;
    }

}

void th_initClusterMemory(fn_mat4 invProj)
{
   planes = malloc(sizeof(th_Plane)*6);

     printf("%s\n","allocted hash table" );
     clusterAABBs = malloc(sizeof(th_AABB)*num_tiles_x*num_tiles_y*numSlices);
     clusterAABBs_minx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
     clusterAABBs_miny = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
     clusterAABBs_minz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
     clusterAABBs_maxx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
     clusterAABBs_maxy = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
     clusterAABBs_maxz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);

     int invocs = num_tiles_x*num_tiles_y*numSlices;
   //  #pragma omp parallel for
     for (int u = 0; u < invocs;u++)
     {
       uint xMax = num_tiles_x;
       uint yMax = num_tiles_y;
       uint idx = u;
       int i = idx / (xMax * yMax) ;
       idx -= (i * xMax * yMax);
       int k = idx / xMax ;
       int j = idx % xMax ;
      //  printf("%i %i %i %i\n",j,k,i,u);
       clusterAABBs[u] = clusterAABB(j,k,i,invProj,fn_createVec3(0,0,0));

       clusterAABBs_minx[u] = clusterAABBs[u].position.x;
       clusterAABBs_miny[u] = clusterAABBs[u].position.y;
       clusterAABBs_minz[u] = clusterAABBs[u].position.z;
       clusterAABBs_maxx[u] = clusterAABBs[u].hwidth.x;
       clusterAABBs_maxy[u] = clusterAABBs[u].hwidth.y;
       clusterAABBs_maxz[u] = clusterAABBs[u].hwidth.z;
     }

     globally_unique_list=malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
     aliases=malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
     offsets_temp = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
     hashTable = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
     lightCountList = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
     lightIDList = malloc(sizeof(uint*)*num_tiles_x*num_tiles_y*numSlices);

     for (int i = 0 ; i < num_tiles_x*num_tiles_y*numSlices;i++)
     {
       lightIDList[i] = malloc(sizeof(uint)*MAX_LIGHTS);
     }

     usedHashes = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);

}



typedef struct
{
  int start;
  int range;
  uint* usedHashes;
  uint id;
}th_threadCompactData;

void thread_compact( void* id)
{

  th_threadCompactData* data_ptr = (th_threadCompactData*)(id);
  th_threadCompactData data = *data_ptr;


  for (int j = data.start ; j < data.start + data.range;j++)
  {
    uint jd = data.usedHashes[j];
    if (lightCountList[data.id] == lightCountList[jd])
    {
     // bool equiv = compare_2_lists(lightIDList[data.id],lightIDList[jd],lightCountList[data.id]);
      bool equiv = true;
      for (uint k = 0 ; k < lightCountList[data.id];k++)
      {
        if (lightIDList[data.id][k] != lightIDList[jd][k])
        {
          equiv = false;
          break;
        }
      }
      if (equiv)
      {
        aliases[jd] = data.id;
        lightCountList[jd] = 0;
      }
    }
  }
}


typedef struct
{
    long id;
    int max_x;
    int max_y;
    int min_y;
    int min_x;
    int zIndex_min;
    int u;
    int range;
    int* hashCounts;
    uint* offsets;
    int l;
    float radius;
    fn_vec3 pos;
}th_intersectThreadDataPointLight;



static void thread_intersectPointLight(void* id)
{

  th_intersectThreadDataPointLight* data_ptr = (th_intersectThreadDataPointLight*)(id);
  th_intersectThreadDataPointLight data = *data_ptr;
  int max_x = data.max_x;
  int max_y = data.max_y;
  int min_x = data.min_x;
  int min_y = data.min_y;
  int zIndex_min = data.zIndex_min;
  int u = data.u;
  uint* offsets = data.offsets;
  float radius = data.radius;
  float radius2 = data.radius*data.radius;
  fn_vec3 pos = data.pos;
  int l = data.l;
  uint xMax = (max_x - min_x);
  uint yMax = (max_y - min_y);

  const int xyMax = xMax * yMax;
  const int tilesXY = num_tiles_x * num_tiles_y;

  uint idx = data.u;
  uint ii = 0, jj = 0, kk = 0; // local coords within [0..xMax), [0..yMax), [0..zMax)
  uint i = zIndex_min, j = min_x, k = min_y;

  // Initialize ii,jj,kk from idx if u doesn’t start at 0:
  {
    uint t = idx;
    ii = t % xMax; t /= xMax;
    jj = t % yMax; t /= yMax;
    kk = t;            // if range never spans z, kk may be 0
    j += ii; k += jj; i += kk;
  }

//  bool hit_one = false;
  // printf("%i %i %i\n",i,j,k);
  for (int index =  0; index < data.range ; index++)
  {
    // uint xMax = (max_x - min_x);
    // uint yMax = (max_y - min_y);
    // uint idx = u + index;
    // int i = idx / (xMax * yMax) ;
    // idx -= (i * xMax * yMax);
    // int k = idx / xMax + min_y;
    // int j = idx % xMax + min_x;
    // i+= zIndex_min;

    int id2 = j + k*num_tiles_x + (i)*num_tiles_x*num_tiles_y;
    // if (pointLightIntersectsCluster(radius,pos,clusterAABBs[id2]))

    bool intersect = pointLightIntersectsCluster(pos.x,pos.y,pos.z,radius2,clusterAABBs_minx[id2],clusterAABBs_miny[id2],clusterAABBs_minz[id2],clusterAABBs_maxx[id2],clusterAABBs_maxy[id2],clusterAABBs_maxz[id2]   );
    //bool intersect = true;
    if (intersect)
    {

     // hit_one = true;
      //printf("%i %i %i %i\n",j,k,i, data.range);

      if (offsets[id2*2 + 1] < MAX_LIGHTS && id2 < num_tiles_x*num_tiles_y*numSlices)
      {
        if (hashTable[id2] == -1)
        {
          *data.hashCounts = *data.hashCounts + 1;
        }


        hashTable[id2] = id2;

        uint hash =  hashTable[id2];

        uint count = offsets[id2*2 + 1] + 1;
        offsets[id2*2 + 1] = count ;//count

        lightIDList[hash][count - 1] = l;
        lightCountList[hash] = count;
      }


    }

    if (++j == min_x + xMax) { j = min_x; if (++k == min_y + yMax) { k = min_y; ++i; } }
    //   }
    // }
  }

  // if (!hit_one)
  // {
  //   printf("%f %f %f %f\n",pos.x,pos.y,pos.z,radius2);
  // }
}

static int fn_sphereInFrustum( fn_vec3 pos, float radius ,fn_vec4 frustum [6])
{
   int p;

   bool res = true;

   for( p = 0; p < 6; p++ )
   {

      if (frustum[p].x * pos.x + frustum[p].y * pos.y + frustum[p].z * pos.z + frustum[p].w <= -radius)
      {
        res = false;
        p = 6;
      }

    }


  if (!res)
      return 0;
   return 1;//d + radius;
}


uint th_binLights(fn_vec4* planes_frust,fn_mat4 viewmatrix,uint* access,uint* offsets,th_PointLight* lights,uint lights_to_bin,fn_mat4 invProj,fn_vec2 screen,float zn,float zf)
{
  Uint32 start_time = SDL_GetTicks();
  Nearz = zn;
  Farz = zf;
  screen_x = screen.x;
  screen_y = screen.y;
  //allocate memory
  if (hashTable == NULL)
  {
    planes = malloc(sizeof(th_Plane)*6);

      printf("%s\n","allocted hash table" );
      clusterAABBs = malloc(sizeof(th_AABB)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_minx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_miny = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_minz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxy = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);

      int invocs = num_tiles_x*num_tiles_y*numSlices;
    //  #pragma omp parallel for
      for (int u = 0; u < invocs;u++)
      {
        uint xMax = num_tiles_x;
        uint yMax = num_tiles_y;
        uint idx = u;
        int i = idx / (xMax * yMax) ;
        idx -= (i * xMax * yMax);
        int k = idx / xMax ;
        int j = idx % xMax ;
       //  printf("%i %i %i %i\n",j,k,i,u);
        clusterAABBs[u] = clusterAABB(j,k,i,invProj,fn_createVec3(0,0,0));


        clusterAABBs_minx[u] = clusterAABBs[u].position.x;
        clusterAABBs_miny[u] = clusterAABBs[u].position.y;
        clusterAABBs_minz[u] = clusterAABBs[u].position.z;
        clusterAABBs_maxx[u] = clusterAABBs[u].hwidth.x;
        clusterAABBs_maxy[u] = clusterAABBs[u].hwidth.y;
        clusterAABBs_maxz[u] = clusterAABBs[u].hwidth.z;

        // if (i == 9)
        //   printf("%f %f %f %f %f %f\n",clusterAABBs_minx[u],clusterAABBs_miny[u],clusterAABBs_minz[u],clusterAABBs_maxx[u],clusterAABBs_maxy[u],clusterAABBs_maxz[u]);
      }

      globally_unique_list=malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      aliases=malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      offsets_temp = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      hashTable = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      lightCountList = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      lightIDList = malloc(sizeof(uint*)*num_tiles_x*num_tiles_y*numSlices);

      for (int i = 0 ; i < num_tiles_x*num_tiles_y*numSlices;i++)
      {
        lightIDList[i] = malloc(sizeof(uint)*MAX_LIGHTS);
      }

      usedHashes = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      hash_counts = malloc(sizeof(int)*th_getNumThreads());
  }

  if (access_unpacked == NULL)
  {
    access_unpacked = malloc(sizeof(uint)*4*4096*2);
  }



  //reset hash table
  int hashCount_a = 0;
  // int hash_counts = [th_getNumThreads()];
  memset(hash_counts,0,sizeof(int)*th_getNumThreads());
  // memset(hashTable,-1,sizeof(int)*);
  for (int i = 0 ; i < num_tiles_x*num_tiles_y*numSlices;i++)
  {
    hashTable[i] = -1;
  }

  //reset offfset table
  memset(offsets,0,sizeof(uint)*num_tiles_x*num_tiles_y*numSlices*2);

//  th_AABB a = clusterAABB(6,3,14,invProj);
  // fn_printVec3(a.position);
  // fn_printVec3(a.hwidth);

  //bin lights
  Uint32 timeBegin = SDL_GetTicks();
  fn_mat4 proj = fn_inverse(invProj);
  for (uint l = 0 ; l < lights_to_bin;l++)
  {
    int cells = 0;
    //voxelize light
  //

  float thresh = 0.001;
  if (lights[l].pos2.w != 0.0)
  {
    thresh = 0.0001;
  }
  if (lights[l].color.w != 0.0)
  {
    thresh = fmax(lights[l].color.w,0.0000001);
  }
  float radius = sqrtf(fmax(lights[l].color.x,fmax(lights[l].color.y,lights[l].color.z))/thresh);

  if (lights[l].pos2.w > 1.5 )
  {
    if (fmax(lights[l].color.x,fmax(lights[l].color.y,lights[l].color.z)) > 0.001)
    {
      radius = (lights[l].pos2.w - 2.0)*(1.0 + 1.5 + 0.001); //fake emissive radius with 1.3333 knee
      //printf("%f!!!!\n",radius);
    }
    else
    {
      radius = 0.0;
    }

  }


  if (radius < 0.01)
  {
    continue;
  }

  fn_vec3 pos = fn_transformVec3(lights[l].pos.xyz,viewmatrix);

  // fn_vec2 max_screen = Eye2Screen(fn_createVec4(pos.x + radius,pos.y + radius,pos.z -radius   ,1.0),fn_inverse(invProj));
  // fn_vec2 min_screen = Eye2Screen(fn_createVec4(pos.x - radius,pos.y - radius,pos.z +radius  ,1.0),fn_inverse(invProj));
  fn_vec2 min_screen,max_screen;
  GetProjectedBounds(pos,radius,&min_screen,&max_screen,proj);

  // printf("%f %f\n",pos.z + radius,pos.z - radius );
  // fn_printVec2(min_screen);
  // fn_printVec2(max_screen);



  max_screen.x = fn_clamp(max_screen.x/(screen_x/16),0,16);
  min_screen.x = fn_clamp(min_screen.x/(screen_x/16),0,16);

  max_screen.y = fn_clamp(max_screen.y/(screen_y/8),0,8);
  min_screen.y = fn_clamp(min_screen.y/(screen_y/8),0,8);


  uint min_x = (uint)(fn_clamp(fmin(min_screen.x,max_screen.x)-1,0,16));
  uint max_x = (uint)(fn_clamp(fmax(min_screen.x,max_screen.x)+1,0,16));

  uint min_y = (uint)(fn_clamp(fmin(min_screen.y,max_screen.y)-1,0,8));

  uint max_y = (uint)(fn_clamp(fmax(min_screen.y,max_screen.y)+1,0,8));

  int zIndex_max = fn_clampi(getClusterZIndex_eye(-(pos.z - radius)) + 1,0,CLUSTER_Z_COUNT);//fn_clampi((int)(CLUSTER_Z_COUNT*(log(-(pos.z - radius)/0.1)/log(zf/zn))) +1 ,0,CLUSTER_Z_COUNT );
  int zIndex_min = fn_clampi(getClusterZIndex_eye(-(pos.z + radius)) - 1,0,CLUSTER_Z_COUNT); //fn_clampi((int)(CLUSTER_Z_COUNT*(log(-(pos.z + radius)/0.1)/log(zf/zn))) - 1 ,0,CLUSTER_Z_COUNT );
  //prevent overflow
  if (-(pos.z - radius) > zf)
  {
    zIndex_max = CLUSTER_Z_COUNT;
  }
  if (-(pos.z + radius) < zn)
  {
    zIndex_min = 0;
  }

  // zIndex_min = 0;
  //  zIndex_max = CLUSTER_Z_COUNT;

  if (zIndex_min == 0 && zIndex_max == 0)
  {
    continue;
  }
  //
  if (!fn_sphereInFrustum(lights[l].pos.xyz,radius,planes_frust))
  {
    continue;
  }

//   min_x = 0;
// max_x = 16;
// min_y = 0;
// max_y = 8;
// zIndex_max = CLUSTER_Z_COUNT;
// zIndex_min = 0;


  // uint num_comps = (zIndex_max - zIndex_min)*(max_x - min_x)*(max_y- min_y);
  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_intersectThreadDataPointLight,((zIndex_max - zIndex_min)*(max_x - min_x)*(max_y- min_y)))
  data[th_thread_id].u = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].id = 0;
  data[th_thread_id].max_x = max_x;
  data[th_thread_id].max_y = max_y;
  data[th_thread_id].min_y = min_y;
  data[th_thread_id].min_x = min_x;
  data[th_thread_id].zIndex_min = zIndex_min;
  data[th_thread_id].hashCounts = &hash_counts[th_thread_id];
  data[th_thread_id].offsets = offsets;
  data[th_thread_id].l = l;
  data[th_thread_id].pos = pos;
  data[th_thread_id].radius = radius;
  TH_SCHEDULING_FUNC
  th_setThread(thread_intersectPointLight,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING



}

  // printf("%i %i\n",hc,hashCount );
  // printf("VOXELIZATION %i\n",SDL_GetTicks() - timeBegin );
  // timeBegin = SDL_GetTicks();

  for (int i = 0 ; i < th_getNumThreads();i++)
  {
      hashCount_a += hash_counts[i];
  }


  int hc = 0;
  for (int i = 0 ; i < 16*8*CLUSTER_Z_COUNT;i++)
  {
    if (hashTable[i] != -1)
    {
      usedHashes[hc] = i;
      hc++;
    }
    if (hc == hashCount_a)
    {
      break;
    }
  }

uint globally_unique_count = 0;



for (int i = 0 ; i < hashCount_a;i++)
{
  aliases[usedHashes[i]] = -1;
}

//compact the list
for (int i = 0 ; i < hashCount_a;i++)
{
  uint id = usedHashes[i];

  if (lightCountList[id] != 0 && aliases[id] == -1)
  {
    globally_unique_list[globally_unique_count] = id;
    aliases[id] = id;

    int loops_num =hashCount_a - (i + 1);

    if ( hashCount_a  > (i + 1))
    {
      TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_threadCompactData,loops_num)
      data[th_thread_id].start = start_pos + (i + 1);
      data[th_thread_id].range = add ;
      data[th_thread_id].usedHashes = usedHashes;
      data[th_thread_id].id = id;

     TH_SCHEDULING_FUNC
     th_setThread(thread_compact,(void*)&data[th_thread_id],th_thread_id);
     TH_END_SCHEDULING
    }


    globally_unique_count++;

  }

}



// printf("COMPACTION %i\n",SDL_GetTicks() - timeBegin );
// timeBegin = SDL_GetTicks();
  uint total_uints = 0;
  // printf("%i ------\n",globally_unique_count );
  // for (uint i = 0 ; i < globally_unique_count;i++)
  // {
  //   uint id = globally_unique_list[i];
  //   printf("[");
  //   for (uint j = 0 ; j < lightCountList[id];j++ )
  //   {
  //     printf("%i ",lightIDList[id][j]);
  //   }
  //   printf("]\n");
  //   total_uints += lightCountList[id];
  // }
  // printf("Acc %i uints\n",total_uints);

  uint* ptr = &access_unpacked[0];
  uint total_count = 0;
  //printf("------\n" );
  for (uint i = 0 ; i < globally_unique_count;i++)
  {


    uint id = globally_unique_list[i];

    if (total_count + lightCountList[id] > 16384*2)
    {
      //printf("%s\n","!!!" );
      break;
    }
   //printf("LIST: " );
    // qsort(lightIDList[id], lightCountList[id], sizeof(uint), cmpuint);
    // for (int j = 0 ; j < lightCountList[id];j++)
    // {
    //   printf("%i ",lightIDList[id][j] );
    // }
  //  printf("\n");
    memcpy(ptr,lightIDList[id],sizeof(uint)*lightCountList[id]);
   ptr += lightCountList[id];
   uint temp = lightCountList[id];
   lightCountList[id] = total_count;

   total_count += temp;
  }
  // printf("%s\n","-----" );
  // for (int i = 0 ; i < total_count;i++ )
  // {
  //   printf("%i ",access[i] );
  // }
  // printf("\n%s\n","-----" );
  // if (total_count > 16384)
  // {
  //   printf("%s\n","!!!" );
  //   total_count = 16383;
  // }
  //printf("%i\n", total_count);


  //#pragma omp parallel for
  for (uint i = 0 ; i < num_tiles_x*num_tiles_y*num_tiles_z;i++)
  {
    if (hashTable[i] >= 0)
    {
      offsets[i*2 + 0] = lightCountList[aliases[hashTable[i]]];//lightCountList[aliases[hashTable[i]]];
    }
  }

  //pack into 2x16 format in a uint
  uint num_packed = 0;
  for (uint i = 0 ; i < total_count;i++)
  {
    uint packed_idx = i / 2;
    uint bitfield_idx = i % 2;

    if (bitfield_idx == 0)
    {
      access[packed_idx] = 0;

      access[packed_idx] |= (access_unpacked[i] & 0xFFFF);
      num_packed++;
    }
    else
    {
      access[packed_idx] |= ((access_unpacked[i] ) & 0xFFFF) << 16;
    }
  }

  // printf("WRITING %i\n",SDL_GetTicks() - timeBegin );
  // timeBegin = SDL_GetTicks();
  //th_printlnDevConsole("Light Bin Time %i",SDL_GetTicks() - start_time);
  return num_packed;

}



bool intersectAABB(th_AABB a,th_AABB b)
{
  fn_vec3 min1 = a.position;
  fn_vec3 max1 = a.hwidth;

  fn_vec3 min2 = b.position;
  fn_vec3 max2 = b.hwidth;


  return min1.x < max2.x &&
         min2.x < max1.x &&
         min1.y < max2.y &&
         min2.y < max1.y &&
         min1.z < max2.z &&
         min2.z < max1.z ;
}

bool intersectOBBAABB(th_Plane* planes,th_AABB b,int count)
{



  fn_vec3 position = fn_multVec3s(fn_addVec3(b.position,b.hwidth),0.5);
  fn_vec3 extens = fn_multVec3s(fn_subVec3(b.hwidth,b.position),0.5);
  bool ret = true;
  for (int i = 0 ; i<count;i++)
  {
    th_Plane plane = planes[i];
    float offset = (float)(fabs( extens.x * plane.normal.x ) +
                         fabs( extens.y* plane.normal.y ) +
                         fabs( extens.z * plane.normal.z ) );
    float d = fn_pointInPlane(position,plane.position,plane.normal) - offset ;

    ret =ret && (d <= 0);
  }
  return ret;
}

fn_mat4 matrix_abs (fn_mat4 matrix) {
  fn_mat4 m;
  for (int i = 0 ; i <16;i++)
  {
    m.m[i] = fabs(matrix.m[i]);
  }
  return m;
}

th_AABB transformAABB(th_AABB a,fn_mat4 matrix)
{
  fn_mat4 absmat = matrix_abs(matrix);

    fn_vec3 center = fn_multVec3s(fn_addVec3(a.position , a.hwidth) , 1.0/2.f);
    fn_vec3 extent = fn_multVec3s(fn_subVec3(a.position , a.hwidth) , 1.0/2.f);

    fn_vec3 new_center = fn_transformVec3(center, matrix);
    fn_vec3 new_extent = fn_transformNormal(extent, matrix_abs(matrix));

    th_AABB ret;
    ret.position = fn_subVec3(new_center,new_extent);
    ret.hwidth = fn_addVec3(new_center,new_extent);
    return ret;
}




bool intersectAABBAABB(th_AABB a,th_AABB b)
{
  return (a.position.x < b.hwidth.x &&
    b.position.x < a.hwidth.x &&
    a.position.y < b.hwidth.y &&
    b.position.y < a.hwidth.y &&
    a.position.z < b.hwidth.z &&
    b.position.z < a.hwidth.z);
}

typedef struct
{
    long id;
    th_Plane* planes;
    int max_x;
    int max_y;
    int min_y;
    int min_x;
    int zIndex_min;
    int u;
    int range;
    int* hashCounts;
    uint* offsets;
    int l;
}th_intersectThreadData;

void thread_intersect( void *id)
{

    th_intersectThreadData* data_ptr = (th_intersectThreadData*)(id);
    th_intersectThreadData data = *data_ptr;
    th_Plane* planes = data.planes;
    int max_x = data.max_x;
    int max_y = data.max_y;
    int min_x = data.min_x;
    int min_y = data.min_y;
    int zIndex_min = data.zIndex_min;
    int u = data.u;
    uint* offsets = data.offsets;

    uint xMax = (max_x - min_x);
    uint yMax = (max_y - min_y);

    uint idx = data.u;
    uint ii = 0, jj = 0, kk = 0; // local coords within [0..xMax), [0..yMax), [0..zMax)
    uint i = zIndex_min, j = min_x, k = min_y;

    // Initialize ii,jj,kk from idx if u doesn’t start at 0:
    {
      uint t = idx;
      ii = t % xMax; t /= xMax;
      jj = t % yMax; t /= yMax;
      kk = t;            // if range never spans z, kk may be 0
      j += ii; k += jj; i += kk;
    }


    for (int index = 0; index < data.range;index++)
    {
    //      uint idx = u + index;
    // int i = idx / (xMax * yMax) ;
    // idx -= (i * xMax * yMax);
    // int k = idx / xMax + min_y;
    // int j = idx % xMax + min_x;
    // i+= zIndex_min;



    unsigned int id2 = j + k*num_tiles_x + (i)*num_tiles_x*num_tiles_y;

    if (intersectOBBAABB(planes,clusterAABBs[id2],6))
    {



      if (offsets[id2*2 + 1] < MAX_LIGHTS && id2 < num_tiles_x*num_tiles_y*num_tiles_z)
      {
        if (hashTable[id2] == -1)
        {
          *data.hashCounts = *data.hashCounts + 1;
        }

        hashTable[id2] = id2;

        uint hash =  hashTable[id2];

        uint count = offsets[id2*2 + 1] + 1;
        offsets[id2*2 + 1] = count ;//count

        lightIDList[hash][count - 1] = data.l;
        lightCountList[hash] = count;
      }
      else
      {
      //  printf("%s\n","OVERLFW" );
      }


    }

    if (++j == min_x + xMax) { j = min_x; if (++k == min_y + yMax) { k = min_y; ++i; } }

    }
  //  pthread_exit(NULL);

}

void thread_intersectCube( void *id)
{

  th_intersectThreadData* data_ptr = (th_intersectThreadData*)(id);
  th_intersectThreadData data = *data_ptr;
  th_Plane* planes = data.planes;
  int max_x = data.max_x;
  int max_y = data.max_y;
  int min_x = data.min_x;
  int min_y = data.min_y;
  int zIndex_min = data.zIndex_min;
  int u = data.u;
  uint* offsets = data.offsets;

  uint xMax = (max_x - min_x);
  uint yMax = (max_y - min_y);


  uint idx = data.u;
  uint ii = 0, jj = 0, kk = 0; // local coords within [0..xMax), [0..yMax), [0..zMax)
  uint i = zIndex_min, j = min_x, k = min_y;

  // Initialize ii,jj,kk from idx if u doesn’t start at 0:
  {
    uint t = idx;
    ii = t % xMax; t /= xMax;
    jj = t % yMax; t /= yMax;
    kk = t;            // if range never spans z, kk may be 0
    j += ii; k += jj; i += kk;
  }

  // uint xMax = (max_x - min_x);
  // uint yMax = (max_y - min_y);
  for (int index = 0; index < data.range;index++)
  {
    // uint idx = u + index;
    // int i = idx / (xMax * yMax) ;
    // idx -= (i * xMax * yMax);
    // int k = idx / xMax + min_y;
    // int j = idx % xMax + min_x;
    // i+= zIndex_min;

    unsigned int id2 = j + k*num_tiles_x + (i)*num_tiles_x*num_tiles_y;
    if (intersectOBBAABB(planes,clusterAABBs[id2],6))
    {



      if (offsets[id2*2 + 1] < MAX_LIGHTS && id2 < num_tiles_x*num_tiles_y*num_tiles_z)
      {
        if (hashTable[id2] == -1)
        {
          *data.hashCounts = *data.hashCounts + 1;
        }

        hashTable[id2] = id2;

        uint hash =  hashTable[id2];

        uint count = offsets[id2*2 + 1] + 1;
        offsets[id2*2 + 1] = count ;//count

        lightIDList[hash][count - 1] = data.l;
        lightCountList[hash] = count;
      }

    }

    if (++j == min_x + xMax) { j = min_x; if (++k == min_y + yMax) { k = min_y; ++i; } }
  }
}


static int fn_aabbInFrustum(fn_vec3 mins,fn_vec3 maxs,fn_vec4 frustum [6])
{
  int ret = 1;
  fn_vec3 vmax;
  for( int i = 0; i < 6; i++ )
  {
    // X axis
    if(frustum[i].x > 0)
    {
    // vmin.x = mins.x;
    vmax.x = maxs.x;
    }
    else
    {
    // vmin.x = maxs.x;
    vmax.x = mins.x;
    }

    // Y axis
    if(frustum[i].y > 0)
    {
    // vmin.y = mins.y;
    vmax.y = maxs.y;
    }
    else
    {
    // vmin.y = maxs.y;
    vmax.y = mins.y;
    }

    // Z axis
    if(frustum[i].z > 0)
    {
    // vmin.z = mins.z;
    vmax.z = maxs.z;
    }
    else
    {
    // vmin.z = maxs.z;
    vmax.z = mins.z;
    }

    // if(fn_dot(frustum[i].xyz, vmin) +
    // frustum[i].w > 0)
    // return 0;
    // if(fn_dot(frustum[i].xyz, vmax) +
    // frustum[i].w >= 0)
    // return 1;
    if (fn_dot(vmax,frustum[i].xyz) + frustum[i].w < 0)
    {
      return 0;
    }

  }

  return 1;
}


uint th_binDecals(fn_vec4* planes_frust,fn_mat4 viewmatrix,uint* access,uint* offsets,th_DecalOrientation* decals,uint decals_to_bin,fn_mat4 invProj,fn_vec2 screen,float zn,float zf,fn_vec3 camera_pos)
{
  // uint datatesta[32];
  // uint datatestb[32];
  // memset(datatesta,1,sizeof(uint)*32);
  // memset(datatestb,0,sizeof(uint)*32);
  //
  // datatesta[5] = 0;
  // uint count_datatest = 32;
  //
  // printf("EQ test %i\n",compare_2_lists(datatesta,datatestb,count_datatest) );
  //printf("COMPACTION %i\n",SDL_GetTicks() - timeBegin );
  Uint32 timeBegin = SDL_GetTicks();
  Uint32 start_time = SDL_GetTicks();
  Nearz = zn;
  Farz = zf;
  screen_x = screen.x;
  screen_y = screen.y;

  //allocate memory
  if (hashTable == NULL)
  {
    planes = malloc(sizeof(th_Plane)*6);

      printf("%s\n","allocted hash table" );
      clusterAABBs = malloc(sizeof(th_AABB)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_minx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_miny = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_minz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxy = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      int invocs = num_tiles_x*num_tiles_y*numSlices;
    //  #pragma omp parallel for
      for (int u = 0; u < invocs;u++)
      {
        uint xMax = num_tiles_x;
        uint yMax = num_tiles_y;
        uint idx = u;
        int i = idx / (xMax * yMax) ;
        idx -= (i * xMax * yMax);
        int k = idx / xMax ;
        int j = idx % xMax ;
       //  printf("%i %i %i %i\n",j,k,i,u);
        clusterAABBs[u] = clusterAABB(j,k,i,invProj,fn_createVec3(0,0,0));


        clusterAABBs_minx[u] = clusterAABBs[u].position.x;
        clusterAABBs_miny[u] = clusterAABBs[u].position.y;
        clusterAABBs_minz[u] = clusterAABBs[u].position.z;
        clusterAABBs_maxx[u] = clusterAABBs[u].hwidth.x;
        clusterAABBs_maxy[u] = clusterAABBs[u].hwidth.y;
        clusterAABBs_maxz[u] = clusterAABBs[u].hwidth.z;
      }

      globally_unique_list=malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      aliases=malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      offsets_temp = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      hashTable = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      lightCountList = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      lightIDList = malloc(sizeof(uint*)*num_tiles_x*num_tiles_y*numSlices);

      for (int i = 0 ; i < num_tiles_x*num_tiles_y*numSlices;i++)
      {
        lightIDList[i] = malloc(sizeof(uint)*MAX_LIGHTS);
      }

      usedHashes = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      hash_counts = malloc(sizeof(int)*th_getNumThreads());
  }


  // if (usedHashes == 0)
  // {
  //
  //   printf("USED HASHES %p\n",usedHashes );
  //   assert(false);
  // }


  //reset hash table
 // hashCount = 0;
  int hashCount_a = 0;
  //int hash_counts[th_getNumThreads()];
  memset(hash_counts,0,sizeof(int)*th_getNumThreads());
 // printf("Test\n");
  //atomic_init(&hashCount_a,0);
for (int i = 0 ; i < num_tiles_x*num_tiles_y*numSlices;i++)
{
    hashTable[i] = -1;
}


  //reset offfset table
  memset(offsets,0,sizeof(uint)*num_tiles_x*num_tiles_y*numSlices*2);

  fn_mat4 proj = fn_inverse(invProj);


  uint total_invocs = 0;
  for (uint l = 0 ; l < decals_to_bin;l++)
  {
    th_DecalOrientation this_decal = decals[l];
    fn_mat4 world = this_decal.world;//th_makeDecalMatrixWorld(this_decal.pos,this_decal.scale,this_decal.normal,this_decal.tangent);
    fn_mat4 cubematrix = fn_multMat4(world,viewmatrix);
    int cells = 0;
    th_AABB cubeAABB;
    cubeAABB.position = fn_createVec3(-0.5,-0.5,-0.5);//(mins[l]);
    cubeAABB.hwidth = fn_createVec3(0.5,0.5,0.5);

    //th_AABB transformed_aabb = transformAABB(cubeAABB,cubematrix);

    // fn_vec3 center = fn_multVec3s(fn_addVec3(a.position,a.hwidth),0.5);
  //  th_Plane planes[12];
    // th_Plane* planes = planes_meta[l % 2];


    planes[0].position = cubeAABB.position;

       planes[0].normal = fn_createVec3(0,0,-1);
    planes[1].position = cubeAABB.position;planes[1].normal = fn_createVec3(0,-1,0);
    planes[2].position = cubeAABB.position;planes[2].normal = fn_createVec3(-1,0,0);

    planes[3].position = cubeAABB.hwidth;planes[3].normal = fn_createVec3(0,0,1);
    planes[4].position = cubeAABB.hwidth;planes[4].normal = fn_createVec3(0,1,0);
    planes[5].position = cubeAABB.hwidth;planes[5].normal = fn_createVec3(1,0,0);

    th_transformCollider(planes,6,cubematrix);
    fn_vec3 max_pos = fn_createVec3s(0);
    fn_vec3 min_pos = fn_createVec3s(0);

    fn_mat4 total_mat = fn_multMat4(cubematrix,proj);
    fn_vec4 verts[8];
    fn_vec4 world_verts[8];

    float dimension_box = 0.5;


    verts[0] = fn_createVec4(-dimension_box,-dimension_box,-dimension_box,1.0);
    verts[1] = fn_createVec4(-dimension_box,-dimension_box,dimension_box,1.0) ;
    verts[2] = fn_createVec4(-dimension_box,dimension_box,-dimension_box,1.0);
    verts[3] = fn_createVec4(-dimension_box,dimension_box,dimension_box,1.0) ;
    verts[4] = fn_createVec4(dimension_box,-dimension_box,-dimension_box,1.0);
    verts[5] = fn_createVec4(dimension_box,-dimension_box,dimension_box,1.0) ;
    verts[6] = fn_createVec4(dimension_box,dimension_box,-dimension_box,1.0);
    verts[7] = fn_createVec4(dimension_box,dimension_box,dimension_box,1.0) ;


    float zmin,zmax;

    fn_vec3 boxMin_clip;//fn_minVec3(fn_minVec3(fn_minVec3(east.xyz,west.xyz),north.xyz),south.xyz);
    fn_vec3 boxMax_clip;//fn_maxVec3(fn_maxVec3(fn_maxVec3(east.xyz,west.xyz),north.xyz),south.xyz);
    bool xflag = false;
    bool yflag = false;
    bool zflag = false;

    fn_vec3 min_aabb;
    fn_vec3 max_aabb;

    bool valid = false;

    for (int i = 0 ; i <8;i++)
    {
      world_verts[i] = fn_multVec4Mat4(world,verts[i]);
    //  fn_vec4 tr_vert = fn_multVec4Mat4(cubematrix,verts[i]);
      verts[i] = fn_multVec4Mat4(total_mat,verts[i]);

      if (verts[i].w > 0)
      {
        valid = true;
      }
      // fn_vec4 prediv = verts[i];
      verts[i] = fn_multVec4s(  verts[i] ,1.0/verts[i].w) ;

      if (!i)
      {

        boxMin_clip = verts[i].xyz;
        boxMax_clip = verts[i].xyz;
        min_aabb = world_verts[i].xyz;
        max_aabb = world_verts[i].xyz;

      }
      else
      {
        boxMin_clip = fn_minVec3(verts[i].xyz,boxMin_clip);
        boxMax_clip = fn_maxVec3(verts[i].xyz,boxMax_clip);

        min_aabb = fn_minVec3(world_verts[i].xyz,min_aabb);
        max_aabb = fn_maxVec3(world_verts[i].xyz,max_aabb);

      }
    }

    if (!fn_aabbInFrustum(min_aabb,max_aabb,planes_frust))
    {
      continue;
    }

    if (!valid)
    {
      continue;
    }


    float ndc =  fn_clamp(boxMin_clip.z,-1.0,1.0 );//- 1.0;
    float eye = 2.0 * zf * zn / (zf + zn + ndc * (zn - zf));
    uint zIndex_a = getClusterZIndex_eye(eye);//(uint)floor((float)(CLUSTER_Z_COUNT)*(log(eye/zn)/log(zf/zn)));

     ndc = fn_clamp(boxMax_clip.z,-1.0,1.0 );
     eye = 2.0 * zf * zn / (zf + zn + ndc * (zn - zf));
    uint zIndex_b = getClusterZIndex_eye(eye);//(uint)ceil((float)(CLUSTER_Z_COUNT)*(log(eye/zn)/log(zf/zn)));


    int ofre = 0;


    fn_vec2 min_screen = fn_createVec2(((boxMin_clip.x + 1.0)/2.0)*screen.x ,((boxMin_clip.y + 1.0)/2.0)*screen.y );
    fn_vec2 max_screen = fn_createVec2(((boxMax_clip.x + 1.0)/2.0)*screen.x ,((boxMax_clip.y + 1.0)/2.0)*screen.y );



    max_screen.x = fn_clamp(ceil(16.0*(max_screen.x/(screen.x))),0,16);
    min_screen.x = fn_clamp(floor(16.0*(min_screen.x/(screen.x))),0,16);

    max_screen.y = fn_clamp(ceil(8.0*(max_screen.y/(screen.y))),0,8);
    min_screen.y = fn_clamp(floor(8.0*(min_screen.y/(screen.y))),0,8);



    uint min_x = (uint)(fn_clamp(fmin(min_screen.x,max_screen.x)-1,0,16));
    uint max_x = (uint)(fn_clamp(fmax(min_screen.x,max_screen.x)+1,0,16));

    uint min_y = (uint)(fn_clamp(fmin(min_screen.y,max_screen.y)-1,0,8));
    uint max_y = (uint)(fn_clamp(fmax(min_screen.y,max_screen.y)+1,0,8));

    int zIndex_max = fn_clampi(zIndex_b+1,0,CLUSTER_Z_COUNT);//fn_clampi(ceil(CLUSTER_Z_COUNT*(log(-(zmin)/0.1)/log(zf/zn))) +1 ,0,CLUSTER_Z_COUNT );
    int zIndex_min =  fn_clampi(zIndex_a-1,0,CLUSTER_Z_COUNT);//fn_clampi(floor(CLUSTER_Z_COUNT*(log(-(zmax)/0.1)/log(zf/zn))) - 1 ,0,CLUSTER_Z_COUNT );

    if (zIndex_min > zIndex_max)
    {
      zIndex_max = 0;
      zIndex_min = 0;
    }

    if (zIndex_min == 0 && zIndex_max == 0)
    {
      continue;
    }




    if (camera_pos.x > min_aabb.x && camera_pos.x < max_aabb.x\
     && camera_pos.y > min_aabb.y && camera_pos.y < max_aabb.y\
    && camera_pos.z > min_aabb.z && camera_pos.z < max_aabb.z)
    {
      min_x = 0;
      max_x = 16;
      min_y = 0;
      max_y = 8;
       zIndex_min = 0;
       // zIndex_max = 3;
    }
    total_invocs = total_invocs + ((zIndex_max - zIndex_min)*(max_x - min_x)*(max_y- min_y));

    // if (!valid)
    // {

    // }
    //
    // if (((zIndex_max - zIndex_min)*(max_x - min_x)*(max_y- min_y)) > 2560)
    // {
    //   continue;
    // }
    TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_intersectThreadData,((zIndex_max - zIndex_min)*(max_x - min_x)*(max_y- min_y)))
    data[th_thread_id].u = start_pos;
    data[th_thread_id].range = add;
    data[th_thread_id].id = 0;
    data[th_thread_id].planes = planes;
    data[th_thread_id].max_x = max_x;
    data[th_thread_id].max_y = max_y;
    data[th_thread_id].min_y = min_y;
    data[th_thread_id].min_x = min_x;
    data[th_thread_id].zIndex_min = zIndex_min;
    data[th_thread_id].hashCounts = &hash_counts[th_thread_id];
    data[th_thread_id].offsets = offsets;
    data[th_thread_id].l = l;
    TH_SCHEDULING_FUNC
    th_setThread(thread_intersect,(void*)&data[th_thread_id],th_thread_id);
    TH_END_SCHEDULING


  }



for (int i = 0 ; i < th_getNumThreads();i++)
{

    hashCount_a += hash_counts[i];
}


//   printf("%i\n",hashCount_a );
  //  printf("VOXELIZATION %i\n",SDL_GetTicks() - timeBegin );
  // timeBegin = SDL_GetTicks();

//  uint usedHashes[hashCount_a];

  int hc = 0;
  for (int i = 0 ; i < 16*8*CLUSTER_Z_COUNT;i++)
  {

    if (hashTable[i] != -1)
    {

      usedHashes[hc] = i;
      hc++;
    }
    if (hc == hashCount_a)
    {
      break;
    }
  }

uint globally_unique_count = 0;

uint all_total_count = 0;

for (int i = 0 ; i < hashCount_a;i++)
{
  aliases[usedHashes[i]] = -1;
//  all_total_count += lightCountList[usedHashes[i]];
}

// int compression_needed = 0;
//
// if (all_total_count >= 16383)
// {
//   compression_needed = all_total_count - 16383;
// }
//
//
// //compact the list
//
// if (compression_needed > 0)
// {
//
// }
for (int i = 0 ; i < hashCount_a;i++)
{
  uint id = usedHashes[i];

  if (lightCountList[id] != 0 && aliases[id] == -1)
  {
    globally_unique_list[globally_unique_count] = id;
    aliases[id] = id;

    int loops_num =hashCount_a - (i + 1);

    if ( hashCount_a  > (i + 1))
    {
      TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_threadCompactData,loops_num)
      data[th_thread_id].start = start_pos + (i + 1);
      data[th_thread_id].range = add ;
      data[th_thread_id].usedHashes = usedHashes;
      data[th_thread_id].id = id;

     TH_SCHEDULING_FUNC
     th_setThread(thread_compact,(void*)&data[th_thread_id],th_thread_id);
     TH_END_SCHEDULING
    }


    globally_unique_count++;

  }

}

//printf("%i %i\n",hashCount_a,globally_unique_count );

  uint* ptr = &access[0];
  uint total_count = 0;

  for (uint i = 0 ; i < globally_unique_count;i++)
  {
    uint id = globally_unique_list[i];

    if (total_count + lightCountList[id] > 16384)
    {
      break;
    }
  //  printf("%i\n",id );

    memcpy(ptr,lightIDList[id],sizeof(uint)*lightCountList[id]);
   ptr += lightCountList[id];
   uint temp = lightCountList[id];
   lightCountList[id] = total_count;

   total_count += temp;
  }
  if (total_count > 16384)
  {
    printf("%s\n","!!!" );
    total_count = 16383;
  }



 // #pragma omp parallel for
  for (uint i = 0 ; i < num_tiles_x*num_tiles_y*num_tiles_z;i++)
  {
    if (hashTable[i] >= 0)
    {
      offsets[i*2 + 0] = lightCountList[aliases[hashTable[i]]];//lightCountList[aliases[hashTable[i]]];
    }
  }//SDL_GetTicks() - start_time
  //th_printlnDevConsole("Decal Bin Time %i %i",SDL_GetTicks() - start_time,total_invocs);
  return total_count;

 //printf("COMPACTION %i\n",SDL_GetTicks() - timeBegin );
 // timeBegin = SDL_GetTicks();

}




uint th_binCubes(fn_mat4 viewmatrix,uint* access,uint* offsets,fn_vec3* mins,fn_vec3* maxs,uint cubes_to_bin,fn_mat4 invProj,fn_vec2 screen,float zn,float zf,bool keypressed,fn_mat4 proj,fn_vec3 camera_pos)
{
  Uint32 start_time = SDL_GetTicks();
  Nearz = zn;
  Farz = zf;
  screen_x = screen.x;
  screen_y = screen.y;

  // if (usedHashes == NULL)
  // {
  //   usedHashes = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
  // }

  //allocate memory
  if (hashTable == NULL)
  {
    planes = malloc(sizeof(th_Plane)*6);

      printf("%s\n","allocted hash table" );
      clusterAABBs = malloc(sizeof(th_AABB)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_minx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_miny = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_minz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxx = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxy = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      clusterAABBs_maxz = malloc(sizeof(float)*num_tiles_x*num_tiles_y*numSlices);
      int invocs = num_tiles_x*num_tiles_y*numSlices;
    //  #pragma omp parallel for
      for (int u = 0; u < invocs;u++)
      {
        uint xMax = num_tiles_x;
        uint yMax = num_tiles_y;
        uint idx = u;
        int i = idx / (xMax * yMax) ;
        idx -= (i * xMax * yMax);
        int k = idx / xMax ;
        int j = idx % xMax ;
       //  printf("%i %i %i %i\n",j,k,i,u);
        clusterAABBs[u] = clusterAABB(j,k,i,invProj,fn_createVec3(0,0,0));




        clusterAABBs_minx[u] = clusterAABBs[u].position.x;
        clusterAABBs_miny[u] = clusterAABBs[u].position.y;
        clusterAABBs_minz[u] = clusterAABBs[u].position.z;
        clusterAABBs_maxx[u] = clusterAABBs[u].hwidth.x;
        clusterAABBs_maxy[u] = clusterAABBs[u].hwidth.y;
        clusterAABBs_maxz[u] = clusterAABBs[u].hwidth.z;
      }

      globally_unique_list=malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      aliases=malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      offsets_temp = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      hashTable = malloc(sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
      lightCountList = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      lightIDList = malloc(sizeof(uint*)*num_tiles_x*num_tiles_y*numSlices);

      for (int i = 0 ; i < num_tiles_x*num_tiles_y*numSlices;i++)
      {
        lightIDList[i] = malloc(sizeof(uint)*MAX_LIGHTS);
      }

      usedHashes = malloc(sizeof(uint)*num_tiles_x*num_tiles_y*numSlices);
      hash_counts = malloc(sizeof(int)*th_getNumThreads());
  }


  int hashCount_a = 0;
  //int hash_counts[th_getNumThreads()];
  memset(hash_counts,0,sizeof(int)*th_getNumThreads());
  //atomic_init(&hashCount_a,0);

  for (int i = 0 ; i < num_tiles_x*num_tiles_y*numSlices;i++)
  {
      hashTable[i] = -1;
  }

  //reset offfset table
  memset(offsets,0,sizeof(uint)*num_tiles_x*num_tiles_y*numSlices*2);

  // //reset hash table
  // hashCount = 0;
  // memset(hashTable,-1,sizeof(int)*num_tiles_x*num_tiles_y*numSlices);
  //
  // //reset offfset table
  // memset(offsets,0,sizeof(uint)*num_tiles_x*num_tiles_y*numSlices*2);

//  th_AABB a = clusterAABB(6,3,14,invProj);
  // fn_printVec3(a.position);
  // fn_printVec3(a.hwidth);

  //bin lights
  Uint32 timeBegin = SDL_GetTicks();
    // fn_mat4 proj = fn_inverse(invProj);
  for (uint l = 0 ; l < cubes_to_bin;l++)
  {

    int cells = 0;
    th_AABB cubeAABB;
    cubeAABB.position = (mins[l]);
    cubeAABB.hwidth = (maxs[l]);

    th_AABB transformed_aabb = transformAABB(cubeAABB,viewmatrix);

    // fn_vec3 center = fn_multVec3s(fn_addVec3(a.position,a.hwidth),0.5);
    // th_Plane planes[6];
    planes[0].position = cubeAABB.position;planes[0].normal = fn_createVec3(0,0,-1);
    planes[1].position = cubeAABB.position;planes[1].normal = fn_createVec3(0,-1,0);
    planes[2].position = cubeAABB.position;planes[2].normal = fn_createVec3(-1,0,0);

    planes[3].position = cubeAABB.hwidth;planes[3].normal = fn_createVec3(0,0,1);
    planes[4].position = cubeAABB.hwidth;planes[4].normal = fn_createVec3(0,1,0);
    planes[5].position = cubeAABB.hwidth;planes[5].normal = fn_createVec3(1,0,0);
    th_transformCollider(planes,6,viewmatrix);
    fn_vec4 verts[8];

    float dimension_box = 0.5;

    fn_mat4 total_mat = fn_multMat4(viewmatrix,proj);
    verts[0] = fn_createVec4(cubeAABB.position.x,cubeAABB.position.y,cubeAABB.position.z,1.0);
    verts[1] = fn_createVec4(cubeAABB.position.x,cubeAABB.position.y,cubeAABB.hwidth.z,1.0) ;
    verts[2] = fn_createVec4(cubeAABB.position.x,cubeAABB.hwidth.y,cubeAABB.position.z,1.0);
    verts[3] = fn_createVec4(cubeAABB.position.x,cubeAABB.hwidth.y,cubeAABB.hwidth.z,1.0) ;
    verts[4] = fn_createVec4(cubeAABB.hwidth.x,cubeAABB.position.y,cubeAABB.position.z,1.0);
    verts[5] = fn_createVec4(cubeAABB.hwidth.x,cubeAABB.position.y,cubeAABB.hwidth.z,1.0) ;
    verts[6] = fn_createVec4(cubeAABB.hwidth.x,cubeAABB.hwidth.y,cubeAABB.position.z,1.0);
    verts[7] = fn_createVec4(cubeAABB.hwidth.x,cubeAABB.hwidth.y,cubeAABB.hwidth.z,1.0) ;


    float zmin,zmax;

    fn_vec3 boxMin_clip;//fn_minVec3(fn_minVec3(fn_minVec3(east.xyz,west.xyz),north.xyz),south.xyz);
    fn_vec3 boxMax_clip;//fn_maxVec3(fn_maxVec3(fn_maxVec3(east.xyz,west.xyz),north.xyz),south.xyz);
    bool xflag = false;
    bool yflag = false;
    bool zflag = false;

    uint x_max = 0;
    uint y_max = 0;
    uint z_max = 0;

    uint x_min = 0;
    uint y_min = 0;
    uint z_min = 0;

    int behind_count = 0;
    bool x_gt_flag = false;
    bool x_lt_flag = false;
    bool y_gt_flag = false;
    bool y_lt_flag = false;
    bool z_lt_flag = false;
    for (int i = 0 ; i <8;i++)
    {
    //  fn_vec4 tr_vert = fn_multVec4Mat4(cubematrix,verts[i]);
      verts[i] = fn_multVec4Mat4(total_mat,verts[i]);

      //do homogenous clipping
      // float dist_to_clip =fn_dot(verts[i].xyz,fn_createVec3(0,0,1));
      // if (dist_to_clip < 0)
      // {
      //   verts[i].xyz = fn_addVec3(verts[i].xyz,fn_createVec3(0,0,-dist_to_clip));
      // }
    //  th_printlnDevConsole("%i",i );
    if (verts[i].w < 0)
    {
      behind_count++;
      z_lt_flag = true;
      if (verts[i].x < 0)
      {
        x_lt_flag = true;
      }
      if (verts[i].x > 0)
      {
        x_gt_flag = true;
      }
      if (verts[i].y < 0)
      {
        y_lt_flag = true;
      }
      if (verts[i].y > 0)
      {
        y_gt_flag = true;
      }
    }



      // fn_vec4 prediv = verts[i];

      // if (verts[i].w  == 0.0)
      // {
      //   printf("%s\n","DZ" );
      // }
      verts[i] = fn_multVec4s(  verts[i] ,1.0/verts[i].w) ;

      //  th_printlnDevConsole("%f %f %f",verts[i].x,verts[i].y,verts[i].z);
      //  th_printlnDevConsole("%f %f %f",verts[i].x,verts[i].y,verts[i].z);


      // float ndc =  fn_clamp(verts[i].z,-1.0,1.0 );//- 1.0;
      // float eye = 2.0 * zf * zn / (zf + zn + ndc * (zn - zf));
      // float zval = ((float)(CLUSTER_Z_COUNT)*(log(eye/zn)/log(zf/zn)));
      //
      // fn_vec2 screenval = fn_createVec2(((verts[i].x + 1.0)/2.0)*screen.x ,((verts[i].y + 1.0)/2.0)*screen.y );
      //
      // float xval = fn_clamp((16.0*(screenval.x/(screen.x))),0,16);
      // float yval = fn_clamp((8.0*(screenval.y/(screen.y))),0,8);

      // uint zIndex_f = (uint)floor(zval);
      // uint zIndex_c = (uint)ceil(zval);
      //
      // uint xIndex_f = (uint)floor(xval);
      // uint xIndex_c = (uint)ceil(xval);
      //
      // uint yIndex_f = (uint)floor(yval);
      // uint yIndex_c = (uint)ceil(yval);


      if (!i)
      {

        boxMin_clip = verts[i].xyz;
        boxMax_clip = verts[i].xyz;
        // x_max = xIndex_c;
        // y_max = yIndex_c;
        // z_max = zIndex_c;
        //
        // x_min = xIndex_f;
        // y_min = yIndex_f;
        // z_min = zIndex_f;


      }
      else
      {
        boxMin_clip = fn_minVec3(verts[i].xyz,boxMin_clip);
        boxMax_clip = fn_maxVec3(verts[i].xyz,boxMax_clip);
        // x_max = xIndex_c > x_max ? xIndex_c : x_max;
        // y_max = yIndex_c > y_max ? yIndex_c : y_max;
        // z_max = zIndex_c > z_max ? zIndex_c : z_max;
        //
        // x_min = xIndex_f < x_min ? xIndex_f : x_min;
        // y_min = yIndex_f < y_min ? yIndex_f : y_min;
        // z_min = zIndex_f < z_min ? zIndex_f : z_min;

      }
    }

    if (behind_count == 8)
    {
      continue;
    }

    fn_vec3 boxMax_clip_primal = boxMax_clip;
    fn_vec3 boxMin_clip_primal = boxMin_clip;

    // if (keypressed)
    // {
    //   fn_printVec3(boxMax_clip_primal);
    //   fn_printVec3(boxMin_clip_primal);
    // }



    float ndc =  fn_clamp(boxMin_clip.z,-1.0,1.0 );//- 1.0;
    float eye = 2.0 * zf * zn / (zf + zn + ndc * (zn - zf));
    float fp_za = getClusterZIndex_eye(eye);//((float)(CLUSTER_Z_COUNT)*(log(eye/zn)/log(zf/zn)));


     ndc = fn_clamp(boxMax_clip.z,-1.0,1.0 );
     eye = 2.0 * zf * zn / (zf + zn + ndc * (zn - zf));
     float fp_zb = getClusterZIndex_eye(eye);//(float)(CLUSTER_Z_COUNT)*(log(eye/zn)/log(zf/zn));
     if (fp_za > fp_zb)
     {
       float temp = fp_za;
       fp_za = fp_zb;
       fp_zb = temp;
     }
    uint zIndex_b = (uint)ceil(fp_zb);
    uint zIndex_a = (uint)floor(fp_za);

    int ofre = 0;

    boxMin_clip.x = fn_clamp(boxMin_clip.x,-1.0,1.0);
    boxMin_clip.y = fn_clamp(boxMin_clip.y,-1.0,1.0);
    boxMin_clip.z = fn_clamp(boxMin_clip.z,-1.0,1.0);

    boxMax_clip.x = fn_clamp(boxMax_clip.x,-1.0,1.0);
    boxMax_clip.y = fn_clamp(boxMax_clip.y,-1.0,1.0);
    boxMax_clip.z = fn_clamp(boxMax_clip.z,-1.0,1.0);

    fn_vec2 min_screen = fn_createVec2(((boxMin_clip.x + 1.0)/2.0)*screen.x ,((boxMin_clip.y + 1.0)/2.0)*screen.y );
    fn_vec2 max_screen = fn_createVec2(((boxMax_clip.x + 1.0)/2.0)*screen.x ,((boxMax_clip.y + 1.0)/2.0)*screen.y );



    max_screen.x = fn_clamp(ceil(16.0*(max_screen.x/(screen.x))),0,16);
    min_screen.x = fn_clamp(floor(16.0*(min_screen.x/(screen.x))),0,16);

    max_screen.y = fn_clamp(ceil(8.0*(max_screen.y/(screen.y))),0,8);
    min_screen.y = fn_clamp(floor(8.0*(min_screen.y/(screen.y))),0,8);



    uint min_x = (uint)(fn_clamp(fmin(min_screen.x,max_screen.x)-1,0,16));
    uint max_x = (uint)(fn_clamp(fmax(min_screen.x,max_screen.x)+1,0,16));

    uint min_y = (uint)(fn_clamp(fmin(min_screen.y,max_screen.y)-1,0,8));
    uint max_y = (uint)(fn_clamp(fmax(min_screen.y,max_screen.y)+1,0,8));

    if (zIndex_a > zIndex_b)
    {
      uint temp = zIndex_b;
      zIndex_b = zIndex_a;
      zIndex_a = temp;
    }


    int zIndex_max = fn_clampi(zIndex_b+1,0,CLUSTER_Z_COUNT);//fn_clampi(ceil(CLUSTER_Z_COUNT*(log(-(zmin)/0.1)/log(zf/zn))) +1 ,0,CLUSTER_Z_COUNT );
    int zIndex_min =  fn_clampi(zIndex_a-1,0,CLUSTER_Z_COUNT);//fn_clampi(floor(CLUSTER_Z_COUNT*(log(-(zmax)/0.1)/log(zf/zn))) - 1 ,0,CLUSTER_Z_COUNT );

    // if (keypressed)
    // {
    //   printf("%i %i %i\n",max_x,max_y,zIndex_max );
    //   printf("%i %i %i\n",min_x,min_y,zIndex_min );
    //
    //   printf("%i %i %i\n",x_max,y_max,z_max );
    //   printf("%i %i %i\n",x_min,y_min,z_min );
    // }
    // th_printlnDevConsole("%s","START" );
    // th_printlnDevConsole("%f %f %f",boxMin_clip_primal.x,boxMin_clip_primal.y,boxMin_clip_primal.z);
    // th_printlnDevConsole("%f %f %f",boxMax_clip_primal.x,boxMax_clip_primal.y,boxMax_clip_primal.z);
    //
    // th_printlnDevConsole("%i %i %i %i %i", x_lt_flag,y_lt_flag,z_lt_flag,x_gt_flag,y_gt_flag);
     if ( boxMin_clip_primal.x <= -1 || x_lt_flag)
     {
        min_x = 0;
     }
     if ( boxMin_clip_primal.y <= -1 || y_lt_flag)
     {
        min_y = 0;
     }
     if ( boxMin_clip_primal.z <= -1 || z_lt_flag)
     {
        zIndex_min = 0;
     }

     if (boxMax_clip_primal.x >= 1 || x_gt_flag)
     {
        max_x = 16;
     }
     if (boxMax_clip_primal.y >= 1 || y_gt_flag )
     {
        max_y = 8;
     }
     if (boxMax_clip_primal.z >= 1)
     {
        zIndex_max = CLUSTER_Z_COUNT;
     }

     // if (boxMax_clip_primal.x >= 1 || boxMin_clip_primal.x <= -1)
     // {
     //    min_x = 0;
     //    max_x = 16;
     // }
     // if (boxMax_clip_primal.y >= 1 || boxMin_clip_primal.y <= -1)
     // {
     //    min_y = 0;
     //    max_y = 8;
     // }
     // if (boxMax_clip_primal.z >= 1 || boxMin_clip_primal.z <= -1)
     // {
     //    zIndex_min = 0;
     //    zIndex_max = CLUSTER_Z_COUNT;
     // }

     //TODO CHECK IF CLIPSPACE POS in CLIPSPACE VOLUME

     if (camera_pos.x > mins[l].x && camera_pos.x < maxs[l].x\
      && camera_pos.y > mins[l].y && camera_pos.y < maxs[l].y\
     && camera_pos.z > mins[l].z && camera_pos.z < maxs[l].z)
     {
       min_x = 0;
       max_x = 16;
       min_y = 0;
       max_y = 8;
        zIndex_min = 0;
      //  printf("%i\n",l );
       // zIndex_max = CLUSTER_Z_COUNT;
     }


    // if (zIndex_min == 0 && zIndex_max == 0)
    // {
    //   continue;
    // }

    // printf("%i\n",((zIndex_max - zIndex_min)*(max_x - min_x)*(max_y- min_y)) );
  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_intersectThreadData,((zIndex_max - zIndex_min)*(max_x - min_x)*(max_y- min_y)))
  data[th_thread_id].u = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].id = 0;
  data[th_thread_id].planes = planes;
  data[th_thread_id].max_x = max_x;
  data[th_thread_id].max_y = max_y;
  data[th_thread_id].min_y = min_y;
  data[th_thread_id].min_x = min_x;
  data[th_thread_id].zIndex_min = zIndex_min;
  data[th_thread_id].hashCounts = &hash_counts[th_thread_id];
  data[th_thread_id].offsets = offsets;
  data[th_thread_id].l = l;
  TH_SCHEDULING_FUNC
  th_setThread(thread_intersectCube,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING

}

//   printf("%i\n",hashCount );
  // printf("VOXELIZATION %i\n",SDL_GetTicks() - timeBegin );
  // timeBegin = SDL_GetTicks();

  for (int i = 0 ; i < th_getNumThreads();i++)
  {

      hashCount_a += hash_counts[i];
  }


  // uint usedHashes[hashCount];
  int hc = 0;
  for (int i = 0 ; i < 16*8*CLUSTER_Z_COUNT;i++)
  {
    if (hashTable[i] != -1)
    {
      usedHashes[hc] = i;
      hc++;
    }
    if (hc == hashCount_a)
    {
      break;
    }
  }

uint globally_unique_count = 0;


for (int i = 0 ; i < hashCount_a;i++)
{
  aliases[usedHashes[i]] = -1;
}


//compact the list
for (int i = 0 ; i < hashCount_a;i++)
{
  uint id = usedHashes[i];

  if (lightCountList[id] != 0 && aliases[id] == -1)
  {
    globally_unique_list[globally_unique_count] = id;
    aliases[id] = id;

    int loops_num =hashCount_a - (i + 1);

    if ( hashCount_a  > (i + 1))
    {
      TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_threadCompactData,loops_num)
      data[th_thread_id].start = start_pos + (i + 1);
      data[th_thread_id].range = add ;
      data[th_thread_id].usedHashes = usedHashes;
      data[th_thread_id].id = id;

     TH_SCHEDULING_FUNC
     th_setThread(thread_compact,(void*)&data[th_thread_id],th_thread_id);
     TH_END_SCHEDULING
    }


    globally_unique_count++;

  }

}

// printf("COMPACTION %i\n",SDL_GetTicks() - timeBegin );
// timeBegin = SDL_GetTicks();

  uint* ptr = &access[0];
  uint total_count = 0;

  for (uint i = 0 ; i < globally_unique_count;i++)
  {
    uint id = globally_unique_list[i];

    if (total_count + lightCountList[id] > 16384)
    {
      break;
    }
  //  printf("%i\n",id );

    memcpy(ptr,lightIDList[id],sizeof(uint)*lightCountList[id]);
   ptr += lightCountList[id];
   uint temp = lightCountList[id];
   lightCountList[id] = total_count;

   total_count += temp;
  }
  if (total_count > 16384)
  {
    printf("%s\n","!!!" );
    total_count = 16383;
  }
//  printf("%i\n", total_count);


  //#pragma omp parallel for
  for (uint i = 0 ; i < num_tiles_x*num_tiles_y*num_tiles_z;i++)
  {
    if (hashTable[i] >= 0)
    {
      offsets[i*2 + 0] = lightCountList[aliases[hashTable[i]]];
    }
  }


  //th_printlnDevConsole("Cube Bin Time %i",SDL_GetTicks() - start_time);
  return total_count;

}

void th_updateClustersFOV(fn_mat4 invProj)
{
  int invocs = num_tiles_x*num_tiles_y*numSlices;
  //  #pragma omp parallel for
  for (int u = 0; u < invocs;u++)
  {
    uint xMax = num_tiles_x;
    uint yMax = num_tiles_y;
    uint idx = u;
    int i = idx / (xMax * yMax) ;
    idx -= (i * xMax * yMax);
    int k = idx / xMax ;
    int j = idx % xMax ;
    //  printf("%i %i %i %i\n",j,k,i,u);
    clusterAABBs[u] = clusterAABB(j,k,i,invProj,fn_createVec3(0,0,0));



    clusterAABBs_minx[u] = clusterAABBs[u].position.x;
    clusterAABBs_miny[u] = clusterAABBs[u].position.y;
    clusterAABBs_minz[u] = clusterAABBs[u].position.z;
    clusterAABBs_maxx[u] = clusterAABBs[u].hwidth.x;
    clusterAABBs_maxy[u] = clusterAABBs[u].hwidth.y;
    clusterAABBs_maxz[u] = clusterAABBs[u].hwidth.z;
  }
}

