#pragma once
#define defaultHit {1,fn_createVec3s(0),fn_createVec3s(0),fn_createVec3s(0),false,fn_createVec2(0,0)}
#define defaultSweep {defaultHit,defaultAABB,defaultAABB,fn_createVec3s(0),0,1,INFINITY,INFINITY,0,false,defaultPlane,NULL,0,false}
typedef struct  {
  float time;
  fn_vec3 pos;
  fn_vec3 delta;
  fn_vec3 normal;
  bool collided;
  fn_vec2 uvs;
}fn_Hit;



typedef struct  {
  fn_Hit hit;
  fn_AABB item;
  fn_AABB other;
  fn_vec3 pos;
  int id;
  float time;
  float distance;
  float dot;
  int id2;
  bool failbrush;
  fn_Plane plane;
  fn_Plane* planes;
  int planeCount;
  bool resetPosition;
}fn_Sweep;

typedef struct
{
  fn_vec3 pos;
  fn_vec3 normals[16];//honestly how many normals can you really collide with
  int normalCount;
  bool useCross;
  fn_Sweep least;
  bool reset;
  int leastID;
}fn_ColisionEdict;

typedef enum
{
  SLIDE = 0,
  BOUNCE = 1,
  TOUCH = 2,
  NONE = 3,
  DONT_CARE = 4
}fn_CollisionMode_t;
