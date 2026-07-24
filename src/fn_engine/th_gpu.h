#pragma once
#include "../fn_gl.h"
#include "../fn_math/fn_vec2.h"
#include "../fn_math/fn_vec3.h"
#include "../fn_math/fn_mat4.h"
#include "r_shader.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef enum
{
  TH_NOCOMMAND = 1,
  TH_NOINSTANCE = 2,
  TH_NONORMALTANGENT = 4,
  TH_TEXTUREID = 8,
  TH_ANIMATED = 16,
  TH_VEC4 = 32,
  TH_VEC3 = 64,
  TH_8BIT = 128,
  TH_16BIT = 256,
  TH_DEPTHBUFFER = 512,
  TH_32BIT = 1024,
  TH_FILTERED = 2048,
  TH_32BITRGB = 4096,
  TH_DEPTHBUFFER_RENDERBUFFER = 8192,
  TH_VEC1 = 16384,
  TH_RG16F = 32768,
  TH_RGBA16F = 65536,
  TH_MAPPED_VERTS = 131072,
  TH_INCREMENT_INDICES = 262144,
}th_RenderEnum;

typedef struct  {
    unsigned int textureID;  // ID handle of the glyph texture
    fn_ivec2   size;       // Size of glyph
    fn_ivec2   bearing;    // Offset from baseline to left/top of glyph
    unsigned int advance;    // Offset to advance to next glyph
}th_Character;

typedef struct
{
  GLuint query[3];
  const char* name;
}th_QueryBuffer;

typedef  struct {
  GLuint  count;
  GLuint  instanceCount;
  GLuint  firstIndex;
  GLuint  baseVertex;
  GLuint  baseInstance;
}r_DrawElementsIndirectCommand;

typedef struct
{
    fn_vec3 position;
    fn_vec2 texCoord;
    fn_vec3 normal;
    fn_vec3 tangent;
    fn_vec3 bitangent;
}th_Vertex;

typedef struct
{
  fn_vec3 position;
  fn_vec2 texCoord;
  fn_vec3 normal;
  fn_vec3 tangent;
  fn_vec3 bitangent;
  uint8_t boneindex[4];
  uint8_t boneweights[4];
}th_AnimVertex;

typedef struct
{
  GLsync sync;
  bool used;
}r_VboSync;

typedef struct
{
  GLuint drawCommandBuffer;
  r_DrawElementsIndirectCommand* commandBufferMapped;
}th_GpuCommandBuffer;

typedef struct
{
  GLuint arrayObject;
  GLuint instances;
  GLuint verts;
  GLuint indices;
  GLuint ids;
  // GLuint boneweights;
  // GLuint boneindex;
  th_GpuCommandBuffer cmds;
  fn_mat4* mappedInstances;
  th_Vertex* mappedVerts;

  fn_vec4* mappedInstancesOccluder;

  GLuint max_instances;//used to calc offset for mapping
  r_VboSync instancesSync[3];
  bool instanced;
  th_RenderEnum flags;
}th_ArrayObject;

typedef struct
{
  th_AnimVertex* animverts;
  th_Vertex* verts;
  GLuint* indices;
  fn_mat4* instances;
  fn_vec2* ids;
  GLuint vertcount;
  GLuint instancecount;
  GLuint indicecount;
}th_GpuData;

typedef struct
{
  th_GpuData data;
  GLuint* offsets_verts;//first vertex in a datagroup
  GLuint* offsets_indices;//first index
  GLuint* offsets_instances;//first instance in a datagroup
  GLuint* instance_counts;//per obj
  GLuint* element_counts;//per obj
}th_GpuDataOffsets;

typedef struct
{
  GLuint framebuffer;
  GLuint* textures;
  GLuint textureCount;
  GLuint depthTexture;
  th_RenderEnum flags;
}th_FrameBuffer;

typedef struct
{
  GLuint pbos[6];
  GLfloat* pbodata[6];
  GLfloat* current;
}th_PixelPackBuffer;

typedef struct
{
  GLuint buffer;
  r_DrawElementsIndirectCommand* mapped;
  int commandcount_max;
}th_CommandBuffer;
void r_syncAfterRender(th_ArrayObject* array);
void th_swapTextureFBO(GLuint* old,GLuint* new,GLenum index,th_FrameBuffer* fb );

void th_makeQuery(th_QueryBuffer* q,const char* name);
void th_startQuery(th_QueryBuffer* q);
void th_endQuery(th_QueryBuffer* q);
void th_printQuery(th_QueryBuffer* q);

void th_setInstance(th_ArrayObject* array,int index,fn_mat4 m);
void th_setInstances(th_ArrayObject* array,int index,fn_mat4* m,int mats,int stride);
void th_createVbo(th_ArrayObject* array,th_GpuData data,th_RenderEnum flags);
void th_renderArray(th_ArrayObject* array,GLuint numVerts,GLuint numInstances,GLuint vertOffset,GLuint instanceOffset);
void th_renderArrayCmdBuffer(th_ArrayObject* array,GLuint count);
void th_renderArrayCmdBufferPatches(th_ArrayObject* array,GLuint count);
void th_setCommandBuffer(th_ArrayObject* array,r_DrawElementsIndirectCommand* cmds,GLuint count);
void th_createFramebuffer(th_FrameBuffer* framebuffer,int width,int height);
void th_createFramebufferPrefilter(th_FrameBuffer* framebuffer,int width,int height);
void th_createFramebufferDepthOnly(th_FrameBuffer* framebuffer,int width,int height);
void th_createFramebufferGeneral(th_FrameBuffer* framebuffer,int width,int height,th_RenderEnum buffers_desc,int num_buffers);

void th_freeFramebuffer(th_FrameBuffer* framebuffer);

void th_createFrameTextureGeneral(th_FrameBuffer* framebuffer,int width,int height,th_RenderEnum buffers_desc,int num_buffers);
void th_bindFrameBuffer(th_FrameBuffer* framebuffer);

void th_freeVbo(th_ArrayObject* array);

th_CommandBuffer th_createCommandBuffer(int commands);
void th_deleteCommandBuffer(th_CommandBuffer* cmd);

GLuint th_createUbo(GLuint size,GLuint binding_point);

GLuint th_createSSBO(GLuint size,GLuint binding_point);

GLuint th_createMappedUbo(GLuint size,GLuint binding_point,void** mem);

GLuint th_createMappedSSBO(GLuint size,GLuint binding_point,void** mem,r_VboSync* syncs);

void th_syncTripleSync(r_VboSync* syncs);
void th_waitTripleSync(r_VboSync* syncs);

th_PixelPackBuffer th_createPixelPackBuffers(size_t datasize,int num_bufs);
void th_capturePixelBuffer(th_PixelPackBuffer* buffer,GLuint texture,size_t datasize);
void* pboMapBuffer(size_t datasize);

void th_renderText(char* string,fn_vec2 pos_tx,float size,float font,r_Shader* text_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data);

void th_setTextGlyphMagnitude(float mag);

void th_setTextGlyphOffsets(float* o,int count);

float th_renderTextGlyph(const char* string,fn_vec2 pos_tx,float size,float font,r_Shader* text_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data,th_Character* cmap,fn_vec3 color);

void th_renderImage(GLuint texture,fn_vec2 pos_tx,fn_vec2 dimensions,r_Shader* image_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data,float alpha);

void th_renderShaderQuad(fn_vec2 pos_tx,fn_vec2 dimensions,r_Shader* image_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data,float alpha,fn_vec3 tint);


size_t th_getPinnedMemory();


typedef struct
{
  fn_vec3 position;
}th_OccluderVertex;

typedef struct
{
  th_OccluderVertex* verts;
  GLuint* indices;
  fn_vec4* instances;
  GLuint vertcount;
  GLuint instancecount;
  GLuint indicecount;
}th_GpuOccluderData;



//use a vec4 (pos and radius) for instance data
void th_createVboOccluder(th_ArrayObject* array,th_GpuOccluderData data,th_RenderEnum flags);
