#pragma once
#include "../fn_gl.h"
#include "r_vertex.h"
typedef struct
{
  fn_vec3 min;
  fn_vec3 max;
  float s;
}fn_NormalizeTransform;

typedef enum
{
  TH_NORMALIZEMESH = 1,
  TH_IGNORETEXCOORD = 2
}th_MeshFlag;

// fn_Vertex* r_reindex(fn_Vertex* in,int* inIndices,int indiceCountIN,int* verts,GLuint** result,int* inds);

// fn_AnimVertex* r_reindexAnim(fn_AnimVertex* in,int* inIndices,int indiceCountIN,int* verts,GLuint** result,int* inds,int* perFrame,int oldPerFrame);
// fn_Vertex* r_loadMesh_PreserveNormals(const char* filename,GLuint** result,int* verts,int* inds,fn_vec3* dif,fn_NormalizeTransform* t);
// fn_Vertex* r_loadMesh(const char* filename,GLuint** result,int* verts,int* inds,fn_vec3* dif,fn_NormalizeTransform* t);
// fn_Vertex* r_loadMeshVerts(const char* filename,fn_vec3 center,fn_NormalizeTransform t);

// void r_scaleMesh(fn_Vertex* verts,int vertcount,fn_vec3 s);
// void r_translateMesh(fn_Vertex* verts,int vertcount,fn_vec3 t);

fn_vec3* th_loadMeshVerts(const char* filename,unsigned int** indices,int* tricount,bool normalize,int* vcount,bool center_mesh);

th_GpuData r_loadThorMesh_multiple(const char* filename,bool normalize,bool ignoreTexCoord);
th_GpuData r_loadThorMesh(const char* filename,bool normalize,bool ignoreTexCoord,bool center_mesh);
void r_scaleMeshUVs(th_GpuData* data,fn_vec2 uv);
void r_scaleThorMesh(th_GpuData* data,fn_vec3 scale);
void r_translateThorMesh(th_GpuData* data,fn_vec3 translate);

void r_transformThorMesh(th_GpuData* data,fn_mat4 t);

// void th_loadMeshes(char** files,int count,th_GpuData* out,th_MeshFlag* flags);
void th_exportMWADs(th_GpuData* data,int count,const char* directory,const char* prefix,uint8_t* hash);
void th_importMWADs(th_GpuData* data,int count,const char* directory,const char* prefix);

fn_vec3 th_getCenterVerts(th_GpuData* data);

void th_getMinMaxVerts(th_GpuData* data,fn_vec3* minout,fn_vec3* maxout);
