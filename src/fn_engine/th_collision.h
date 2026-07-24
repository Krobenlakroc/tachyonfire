#pragma once
#include "../fn_math/fn_common.h"
#include "../fn_math/fn_mat4.h"
#include "../fn_math/fn_vec4.h"



typedef enum
{
  TH_STATIC_OBJECT,
  TH_DYNAMIC_OBJECT,
  TH_UNCULLED_OBJECT,
}th_ObjectClass;


typedef enum
{
  CAPSULE = 1,
  SPHERE = 2,
  BOX = 4,
  RAY = 8,
  OBB = 16,
  EDGE_MODE = 32,
}th_ColliderType;

typedef struct
{
  fn_vec3 pos;
  fn_vec3 ndir;
}th_Edge;

typedef struct
{
  fn_vec3 position;
  fn_vec3 normal;
}th_Plane;
#define DEFAULTPLANE (th_Plane){fn_createVec3s(0),fn_createVec3s(0)}



typedef struct
{
  fn_vec3 position;
  fn_vec3 hwidth;
  th_ColliderType mode;
}th_Collider;

typedef th_Collider th_AABB;
typedef th_Collider fn_AABB;
typedef struct
{
  // r3 space
  fn_vec3 r3_velocity, r3_position;

  // ellipsoid space
  fn_vec3 e_radius;
  fn_vec3 e_velocity;
  fn_vec3 e_norm_velocity;
  fn_vec3 e_base_point;

  // original tri points
  fn_vec3 a,b,c;

  // hit information
  int found_collision;
  float nearest_distance;
  double t;
  fn_vec3 intersect_point;
  th_Plane plane;

  // iteration depth
  int depth;

  bool robust;
}th_CollisionPacket;


typedef struct
{
  th_Plane* planes;
  int planeCount;
  int usablePlaneCount;
  th_Collider aabb;
  fn_vec3* points;
  int pointCount;

  // bool trisoup;
  // fn_vec3* triangles; // stored t0p0,t0p1, t0p2 , t1p0,t1p1, t1p2

  // th_Edge** planeEdges;
  // int* planeEdgesCount;

  // int originalPlaneCount;

  // unsigned int* indices;
  // int indexCount;

  int id;
  const char* tag;

}th_CollidableVolume;

typedef struct
{
  th_Plane plane;
  fn_vec3 normal;
  fn_vec3 pos;
  float time;
  bool collided;

  // int volume_id;
  // int plane_id;
  // bool found;
  fn_vec3 touch;
  th_CollidableVolume* volume;
}th_Collision;

#define NOCOLLISION {DEFAULTPLANE,fn_createVec3s(0),fn_createVec3s(0),0,false,fn_createVec3s(0),NULL}

typedef struct
{
  float* min_x;
  float* max_x;
  float* min_y;
  float* max_y;
  float* min_z;
  float* max_z;
  bool* skip_culling_flag;
  th_ObjectClass* oc;
  int aabbCount;
}th_FrustumCullData;

bool th_box_tri_intersect(fn_vec3 pos,fn_vec3 boxhalf,fn_vec3 t0,fn_vec3 t1,fn_vec3 t2);

//https://github.com/solenum/exengine
bool th_ray_in_tri(fn_vec3 from, fn_vec3 to, fn_vec3 v0, fn_vec3 v1, fn_vec3 v2, fn_vec3* intersect);

void th_collideTriangle(th_CollisionPacket* packet,th_CollidableVolume* v);

th_Plane th_makePlaneTriangle(fn_vec3 a,fn_vec3 b,fn_vec3 c);

float th_signedDistanceToPlane(fn_vec3 p,th_Plane* plane);

th_Plane th_makePlane(fn_vec3 p,fn_vec3 n);

fn_vec3 th_calculateSurfaceNormal (fn_vec3 points[3]);

th_AABB th_getBoundingBox(fn_vec3* points,int count,fn_mat4 mat);
bool fn_aabbCheck(fn_AABB a,fn_AABB  b);
fn_AABB fn_getSweptBroadphaseBox(fn_AABB b,fn_vec3 v);


th_Plane* th_getVolumeFromTris(fn_vec3* verts,unsigned int* indices,int tricount,int* outPlanesCount);



void th_translateCollider(th_Plane* planes,int planeCount,fn_vec3 translate);
void th_scaleCollider(th_Plane* planes,int planeCount,fn_vec3 scale);
// void th_rotateCollider(th_Plane* planes,int planeCount,fn_quat quaternion);
void th_transformCollider(th_Plane* planes,int planeCount,fn_mat4 m);
void th_copyCollider(th_Plane* out,th_Plane* in,int planeCount);


void th_getCollision(th_Collision* collision,int* collisioncount,th_CollidableVolume* volumes,int volumecount,th_Collider edict,fn_vec3 delta);
// th_Collision th_CollidePlanes(th_Collider edict,fn_vec3 delta,th_CollidableVolume* v);

th_Collision th_sweepTestAABB(th_Collider box,fn_vec3 delta,th_Collider box2);

void th_frustumCull(th_FrustumCullData* fdata,char* visibility,fn_vec4* frustumPlanes);

th_Collision th_sweepTestSphere(fn_vec3 pos,float radius,fn_vec3 delta,th_Collider box2);
