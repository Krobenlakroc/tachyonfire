#pragma once
#include "../fn_math/fn_vec3.h"
#include "../fn_math/fn_vec4.h"
#include "th_allocator.h"

#define MAX_SNAME_LEN 100

typedef struct
{
  char name[MAX_SNAME_LEN]; // type of monster "skull" or "shambler" or "horse", or name of course "horseCourse1"
  char courseName[MAX_SNAME_LEN]; //name of the course to follow
  fn_vec3 position; //3d position point
  int index;//entity index or index within the course
  float time;//time at which to spawn
  float duration;//used for centipedes to determine time spent spawning in
}th_SpawnsetPair;



typedef struct
{
  float flt_value;
  char str_value[MAX_SNAME_LEN];
}th_ValueType;

typedef struct
{
  char key[MAX_SNAME_LEN];
  th_ValueType values[16];
  int num_values;
  int token_index;
}th_KeyValuePair;


typedef struct
{
  char* name;
  char* spawnset_name;
  char* display_name;
  char* thumbnail_path;
  char* difficulty_name;
}th_LevelFile;

typedef struct
{
  th_LevelFile* levelfiles;
  int count;
}th_LevelManifest;

// 0 - unplayed
// 1 - no medal (completed)
// 2 - bronze tier 1
// 3 - silver tier 2
// 4 - gold tier 3
typedef struct
{
  int level_speed;
  int level_airtime;
  int level_damagetaken;
  int flawless;
}th_LevelVictoryState;

typedef struct
{
  th_LevelVictoryState* victorystates;
  int count;
}th_VictoryManifest;
//file entry:
//name px py pz time
//name coursename px py pz time

th_SpawnsetPair* th_spawnSetLoad(th_Allocator* allocator,const char* filename,int* entries);


th_SpawnsetPair* th_spawnSetFind(th_SpawnsetPair* spawnset,int count,const char* name,int* num_spawns);


th_KeyValuePair* th_keyValueLoad(th_Allocator* allocator,const char* filename,int* entries);


th_KeyValuePair* th_keyValueFind(th_KeyValuePair* keyvalues,int count,const char* name);

fn_vec3 th_keyValueGetVec3(th_KeyValuePair* keyvalues,int count,const char* name);

fn_vec3 th_keyValueGetVec3Default(th_KeyValuePair* keyvalues,int count,const char* name,fn_vec3 def);

fn_vec4 th_keyValueGetVec4(th_KeyValuePair* keyvalues,int count,const char* name);

float th_keyValueGetFloat(th_KeyValuePair* keyvalues,int count,const char* name);

float th_keyValueGetFloatDefault(th_KeyValuePair* keyvalues,int count,const char* name,float def);

bool th_keyValueGetBool(th_KeyValuePair* keyvalues,int count,const char* name);

const char* th_keyValueGetStrDefault(th_KeyValuePair* keyvalues,int count,const char* name,const char* def);

th_LevelManifest th_getLevelManifest(const char* filename);

th_VictoryManifest th_getVictoryManifest(const char* filename,int minimum_count);
