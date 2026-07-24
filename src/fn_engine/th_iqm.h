#pragma once

#include <stdint.h>
#include "th_gpu.h"
#include "th_allocator.h"
#include "th_ragdoll.h"

#define uint uint32_t
#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 2

//https://github.com/solenum/exengine
typedef struct {
  char magic[16];
  uint version;
  uint filesize;
  uint flags;
  uint num_text, ofs_text;
  uint num_meshes, ofs_meshes;
  uint num_vertexarrays, num_vertexes, ofs_vertexarrays;
  uint num_triangles, ofs_triangles, ofs_adjacency;
  uint num_joints, ofs_joints;
  uint num_poses, ofs_poses;
  uint num_anims, ofs_anims;
  uint num_frames, num_framechannels, ofs_frames, ofs_bounds;
  uint num_comment, ofs_comment;
  uint num_extensions, ofs_extensions;
} th_IQMHeader;

enum {
  IQM_POSITION     = 0,
  IQM_TEXCOORD     = 1,
  IQM_NORMAL       = 2,
  IQM_TANGENT      = 3,
  IQM_BLENDINDEXES = 4,
  IQM_BLENDWEIGHTS = 5,
  IQM_COLOR        = 6,
  IQM_CUSTOM       = 0x10,
  IQM_BYTE   = 0,
  IQM_UBYTE  = 1,
  IQM_SHORT  = 2,
  IQM_USHORT = 3,
  IQM_INT    = 4,
  IQM_UINT   = 5,
  IQM_HALF   = 6,
  IQM_FLOAT  = 7,
  IQM_DOUBLE = 8,
  IQM_LOOP   = 1<<0
};

typedef struct {
  uint name;
  int parent;
  float translate[3], rotate[4], scale[3];
} th_IQMJoint;

typedef struct {
  int parent;
  uint channelmask;
  float channeloffset[10];
  float channelscale[10];
} th_IQMPose;

typedef struct {
  uint name;
  uint first_frame, num_frames;
  float framerate;
  uint flags;
} th_IQMAnim;

typedef struct {
  uint type;
  uint flags;
  uint format;
  uint size;
  uint offset;
} th_IQMVertexArray;

typedef struct {
  float bbmin[3], bbmax[3];
  float xyradius, radius;
} th_IQMbounds;

typedef struct {
  uint name;
  uint material;
  uint first_vertex, num_vertexes;
  uint first_triangle, num_triangles;
} th_IQMMesh;

typedef struct {
  char name[64];
  int parent;
  fn_vec3 position, scale;
  fn_vec4 rotation;
  fn_mat4 transform;
} th_Bone;

typedef struct {
  fn_vec3 translate, scale;
  fn_vec4 rotate;
} th_PoseElement;

typedef struct {
  char *name;
  uint32_t first, last;
  float rate;
  uint8_t loop;
  uint id;
} th_Animation;


// NEW: Animation blend state structure
typedef struct {
  bool is_blending;           // Whether we're currently blending
  th_Animation *source_anim;  // Animation we're blending from
  th_Animation *target_anim;  // Animation we're blending to
  float blend_time;           // Current blend progress (0-1)
  float blend_duration;       // How long the blend takes
  th_PoseElement *source_pose; // Snapshot of pose when blend started
  float source_time;          // Time in source animation when blend started
  uint32_t source_frame;      // Frame in source animation when blend started
  uint32_t source_next_frame; // Next frame in source animation
  float source_position;      // Position within source frame

  th_PoseElement* target_pose;
} th_AnimBlendState;

typedef struct
{
th_Animation *current_anim;
float     current_time;
uint32_t  current_frame;
uint32_t  next_frame;
float position;
int anim_finished;

th_Bone* bones;
th_PoseElement* pose;
fn_mat4 *skeleton;
fn_mat4 *skeleton_world;
float timescale;

bool ragdoll;
fn_mat4 ragdoll_mat;
bool ragdoll_setpose;
th_Ragdoll ragdoll_data;
fn_vec3 ragdoll_velocity;
fn_vec3 ragdoll_velocity_center;

th_AnimBlendState blend_state;
}th_ModelInstance;

typedef struct
{
  th_GpuData* meshes;
  GLuint meshcount;
  th_ModelInstance* instances;
  GLuint instanceCount;
  fn_mat4* inverse_base;
  th_Animation* anims;
  th_PoseElement** frames;
  th_PoseElement* bind_pose;
  GLuint bones_count,anim_count,frame_count;
  th_Allocator* alloc;

  fn_mat4* transform_cache;
}th_Model;

th_Model th_loadIQM(th_Allocator* alloc,const char *path, uint8_t flags,GLuint instances,fn_vec3 escale);

void th_setTimeScale(th_Model *m, float ts,GLuint instance);

void th_setAnim(th_Model *m,const char *id,GLuint instance);

void th_setAnimID(th_Model *m, int id,GLuint instance);

void th_updateModel(th_Model *m, float delta_time,th_World* world);

void th_updateModelInstance(th_Model *m, float delta_time,th_World* world,int idx);

void th_captureModel(th_Model *m,float* current_times,uint32_t* current_frames,uint32_t* next_frames,char* is_ragdoll,fn_vec3* vel_center,fn_vec3* vel);

void th_updateModelPlayback(th_Model *m, float* current_time,uint32_t* current_frame,uint32_t* next_frame,float* current_time_b,uint32_t* current_frame_b,uint32_t* next_frame_b,float f,char* is_ragdoll,fn_vec3* vel_center,fn_vec3* vel,float delta_time,th_World* world);

void th_setModelPose(th_Model *m, th_PoseElement* frame,GLuint instance);

void th_transformModel(th_Model* m,fn_mat4 tr);

void th_resetModelRagdoll(th_Model *m,int idx);

fn_mat4 th_getSkeletonElement(th_Model *m,const char *id,GLuint instance);

// NEW: Set animation with smooth blending
void th_setAnimBlend(th_Model *m, const char *id, float blend_duration, GLuint instance);
void th_setAnimIDBlend(th_Model *m, int id, float blend_duration, GLuint instance);

void th_pushModelOccluders(th_Model* m,int idx,float radius,fn_mat4 a);
