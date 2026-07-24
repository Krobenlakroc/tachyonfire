#include "th_defines.h"
#include "r_shader.h"
#include "th_gpu.h"
#include <string.h>
#include "../fn_window.h"
#include "th_time.h"
#include <stdio.h>
#define NOSYNC
static size_t pinned = 0;

static void* mapCommandBuffer(size_t datasize)
{
  pinned += datasize;
  GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage(GL_DRAW_INDIRECT_BUFFER, datasize , 0, flags);
  return glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, datasize, flags);
}

static void* vboMapInstanceBuffer(size_t datasize,int count)
{
  pinned += datasize*count;
  GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage(GL_ARRAY_BUFFER, datasize * count, 0, flags);
  return glMapBufferRange(GL_ARRAY_BUFFER, 0, datasize * count, flags);
}

// static void* vboMapVertexBuffer(size_t datasize,int count)
// {
//   pinned += datasize*count;
//   GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
//   glBufferStorage(GL_ARRAY_BUFFER, datasize * count, 0, flags);
//   return glMapBufferRange(GL_ARRAY_BUFFER, 0, datasize * count, flags);
// }


size_t th_getPinnedMemory()
{
    return pinned;
}

void r_waitVboBuffer(r_VboSync* syncs)
{
  if (syncs[th_frame()%3].used)
  {

    while (1)
    {
    GLenum waitReturn = glClientWaitSync(syncs[th_frame()%3].sync,
                                       GL_SYNC_FLUSH_COMMANDS_BIT, 0);//
      if (waitReturn == GL_ALREADY_SIGNALED ||
          waitReturn == GL_CONDITION_SATISFIED)
          {

            return;
          }


    }
  }
}

void th_setInstance(th_ArrayObject* array,int index,fn_mat4 m)
{
  r_waitVboBuffer(array->instancesSync);
  array->mappedInstances[index + array->max_instances*(th_frame()%3)] = m;
}

void th_setInstances(th_ArrayObject* array,int index,fn_mat4* m,int mats,int stride)
{
  //r_waitVboBuffer(array->instancesSync);
  if (stride == 1)
  {
    memcpy(&array->mappedInstances[index + array->max_instances*(th_frame()%3)],m,sizeof(fn_mat4)*mats);
  }
  else
  {
    for (int i = 0 ; i < stride;i++)
    {
      memcpy(&array->mappedInstances[index + i*mats + array->max_instances*(th_frame()%3)],&m[0],sizeof(fn_mat4)*mats);
    }
  }


}

void th_syncTripleSync(r_VboSync* syncs)
{
  if (syncs[th_frame()%3].used)
    glDeleteSync(syncs[th_frame()%3].sync);
  syncs[th_frame()%3].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
  syncs[th_frame()%3].used = true;
}

void th_waitTripleSync(r_VboSync* syncs)
{
  r_waitVboBuffer(syncs);
}

void r_syncAfterRender(th_ArrayObject* array)
{
  #ifdef NOSYNC
  return;
  #endif
  if (array->instancesSync[th_frame()%3].used)
    glDeleteSync(array->instancesSync[th_frame()%3].sync);
  array->instancesSync[th_frame()%3].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
  array->instancesSync[th_frame()%3].used = true;
}

th_CommandBuffer th_createCommandBuffer(int commands)
{
  GLuint ret;
  r_DrawElementsIndirectCommand* mapped;
    glGenBuffers(1, &ret);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,ret);
    mapped = mapCommandBuffer(sizeof(r_DrawElementsIndirectCommand)*commands*3);
  th_CommandBuffer r;
  r.buffer= ret;
  r.mapped = mapped;
  r.commandcount_max = commands;
  return r;
}

void th_deleteCommandBuffer(th_CommandBuffer* cmd)
{
  //glUnmapNamedBuffer(cmd->buffer);
  glDeleteBuffers(1, &cmd->buffer);
}

void th_createVbo(th_ArrayObject* array,th_GpuData data,th_RenderEnum flags)
{
  array->flags = flags;
  bool commanded = !(flags & TH_NOCOMMAND);
  bool instanced = !(flags & TH_NOINSTANCE);
  bool usetextureID = (flags & TH_TEXTUREID);
  bool mapped_verts = (flags & TH_MAPPED_VERTS);

  array->instanced = instanced;
  for (int i =0;i < 3;i++)
  array->instancesSync[i].used = false;

  glGenVertexArrays(1, &array->arrayObject);
  glGenBuffers( 1, &array->indices );

  glGenBuffers( 1, &array->verts );

  // glGenBuffers( 1, &array->ids );

  glGenBuffers(1, &array->instances);

  glGenBuffers(1, &array->cmds.drawCommandBuffer);
  glGenBuffers(1, &array->ids);


  glBindVertexArray(array->arrayObject);

  if (usetextureID)
  {
    glBindBuffer(GL_ARRAY_BUFFER, array->ids);
    glBufferData( GL_ARRAY_BUFFER, sizeof(fn_vec2)*data.instancecount*3, NULL, GL_STATIC_DRAW );
    glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(fn_vec2)*data.instancecount,data.ids);
    glBufferSubData(GL_ARRAY_BUFFER,sizeof(fn_vec2)*data.instancecount,sizeof(fn_vec2)*data.instancecount,data.ids);
    glBufferSubData(GL_ARRAY_BUFFER,sizeof(fn_vec2)*data.instancecount*2,sizeof(fn_vec2)*data.instancecount,data.ids);
  }

  if (instanced)
  {

    glBindBuffer(GL_ARRAY_BUFFER, array->instances);
    array->max_instances = data.instancecount;
      array->mappedInstances = (fn_mat4*)vboMapInstanceBuffer(sizeof(fn_mat4),data.instancecount*FN_BUFFER_COUNT);
      memcpy(array->mappedInstances,data.instances,sizeof(fn_mat4)*data.instancecount);
      memcpy(&array->mappedInstances[data.instancecount],data.instances,sizeof(fn_mat4)*data.instancecount);
      memcpy(&array->mappedInstances[data.instancecount*2],data.instances,sizeof(fn_mat4)*data.instancecount);
  }
  if (commanded)
  {
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,array->cmds.drawCommandBuffer);
    array->cmds.commandBufferMapped = mapCommandBuffer(sizeof(r_DrawElementsIndirectCommand)*256*3);
  }



  // glBindBuffer (GL_ARRAY_BUFFER, vbo->IIOID);
  //
  //   vbo->mappedInstancedBufferID = (float*)vboMapInstanceBuffer(sizeof(float),vbo->instances);
  //   memcpy(vbo->mappedInstancedBufferID,vbo->id,sizeof(float)*vbo->instances);


  glBindBuffer( GL_ARRAY_BUFFER, array->verts );
  if (flags & TH_ANIMATED)
  {
    glBufferData( GL_ARRAY_BUFFER, data.vertcount * sizeof(th_AnimVertex), data.animverts, GL_STATIC_DRAW );
  }
  else
  {
    if (mapped_verts)
    {
      //glBufferData( GL_ARRAY_BUFFER, data.vertcount * sizeof(th_Vertex), data.verts, GL_STATIC_DRAW );
      array->mappedVerts = (th_Vertex*)vboMapInstanceBuffer(sizeof(th_Vertex),data.vertcount*FN_BUFFER_COUNT);

      memcpy(array->mappedVerts,data.verts,sizeof(th_Vertex)*data.vertcount);
      memcpy(&array->mappedVerts[data.vertcount],data.verts,sizeof(th_Vertex)*data.vertcount);
      memcpy(&array->mappedVerts[data.vertcount*2],data.verts,sizeof(th_Vertex)*data.vertcount);
    }
    else
    {
      glBufferData( GL_ARRAY_BUFFER, data.vertcount * sizeof(th_Vertex), data.verts, GL_STATIC_DRAW );
    }

  }

  //glBufferSubData( GL_ARRAY_BUFFER, 0, verts * sizeof(Vertex), vData );
  //Create IBO
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, array->indices);
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, data.indicecount * sizeof(GLuint), data.indices, GL_STATIC_DRAW );


  GLint posloc,texloc,norloc,tanloc,btanloc,trloc,idloc;
  posloc = 0;//glGetAttribLocation(shader->shaderProgram,"inPosition");
  texloc = 1;//glGetAttribLocation(shader->shaderProgram,"inTexcoord");
  norloc = 2;//glGetAttribLocation(shader->shaderProgram,"inNormal");
  tanloc = 3;//glGetAttribLocation(shader->shaderProgram,"inTangent");
  btanloc = 4;//glGetAttribLocation(shader->shaderProgram,"inBiTangent");
  trloc = 5;//glGetAttribLocation(shader->shaderProgram,"inTransform");
  idloc = 9;
  GLint weightloc = 10;
  GLint indexloc = 11;

  GLsizei vertex_size = (flags & TH_ANIMATED) ? sizeof(th_AnimVertex) : sizeof(th_Vertex);
  //might cause problems
  size_t offsets1[7] = {offsetof(th_AnimVertex,boneweights),offsetof(th_AnimVertex,boneindex),
    offsetof(th_AnimVertex,position),offsetof(th_AnimVertex,texCoord),offsetof(th_AnimVertex,normal),
  offsetof(th_AnimVertex,tangent),offsetof(th_AnimVertex,bitangent)};

  size_t offsets2[7] = {offsetof(th_AnimVertex,boneweights),offsetof(th_AnimVertex,boneindex),
    offsetof(th_Vertex,position),offsetof(th_Vertex,texCoord),offsetof(th_Vertex,normal),
  offsetof(th_Vertex,tangent),offsetof(th_Vertex,bitangent)};

  size_t offsets[7];
  if (flags & TH_ANIMATED)
  {
    memcpy(offsets,offsets1,sizeof(size_t)*7);
  }
  else
  {
    memcpy(offsets,offsets2,sizeof(size_t)*7);
  }

  if (flags & TH_ANIMATED)
  {
    glVertexAttribPointer(weightloc, 4, GL_UNSIGNED_BYTE, GL_TRUE,vertex_size , (GLvoid*)offsets[0]);
    glEnableVertexAttribArray(weightloc);
    glVertexAttribPointer(indexloc, 4, GL_UNSIGNED_BYTE, GL_TRUE,vertex_size , (GLvoid*)offsets[1]);
    glEnableVertexAttribArray(indexloc);
  }

  glVertexAttribPointer(posloc, 3, GL_FLOAT, GL_FALSE,vertex_size , (GLvoid*)offsets[2]);
  glEnableVertexAttribArray(posloc);


  if (texloc != -1)
  {
  glVertexAttribPointer(texloc, 2, GL_FLOAT, GL_FALSE, vertex_size, (GLvoid*)offsets[3]);
  glEnableVertexAttribArray(texloc);
  }
  if (norloc != -1 && !(flags & TH_NONORMALTANGENT))
  {
  glVertexAttribPointer(norloc, 3, GL_FLOAT, GL_FALSE, vertex_size, (GLvoid*)offsets[4]);
  glEnableVertexAttribArray(norloc);
  }
  if (tanloc != -1 && !(flags & TH_NONORMALTANGENT))
  {
  glVertexAttribPointer(tanloc, 3, GL_FLOAT, GL_FALSE, vertex_size, (GLvoid*)offsets[5]);
  glEnableVertexAttribArray(tanloc);
  }
  if (btanloc != -1 && !(flags & TH_NONORMALTANGENT))
  {
  glVertexAttribPointer(btanloc, 3, GL_FLOAT, GL_FALSE, vertex_size, (GLvoid*)offsets[6]);
  glEnableVertexAttribArray(btanloc);
  }

  if (instanced && trloc != -1)
  {
    glBindBuffer(GL_ARRAY_BUFFER, array->instances); // this attribute comes from a different vertex buffer

    glVertexAttribPointer(trloc, 4, GL_FLOAT, GL_FALSE, sizeof(fn_mat4) , (void*)(0));
    glEnableVertexAttribArray(trloc);
    glVertexAttribPointer(trloc+1, 4, GL_FLOAT, GL_FALSE, sizeof(fn_mat4) , (void*)(sizeof(fn_vec4) * 1));
    glEnableVertexAttribArray(trloc+1);
    glVertexAttribPointer(trloc+2, 4, GL_FLOAT, GL_FALSE, sizeof(fn_mat4), (void*)(sizeof(fn_vec4) * 2));
    glEnableVertexAttribArray(trloc+2);
    glVertexAttribPointer(trloc+3, 4, GL_FLOAT, GL_FALSE, sizeof(fn_mat4), (void*)(sizeof(fn_vec4) * 3));
    glEnableVertexAttribArray(trloc+3);


    glVertexAttribDivisor(trloc, 1);
    glVertexAttribDivisor(trloc+1, 1);
    glVertexAttribDivisor(trloc+2, 1);
    glVertexAttribDivisor(trloc+3, 1);
  }

  if (usetextureID)
  {
      glBindBuffer(GL_ARRAY_BUFFER, array->ids);
      glVertexAttribPointer(idloc, 2, GL_FLOAT, GL_FALSE, sizeof(fn_vec2), (GLvoid*)0);
      glEnableVertexAttribArray(idloc);
      glVertexAttribDivisor(idloc, 1);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);



  // glBindBuffer(GL_ARRAY_BUFFER, vbo->IIOID);
  // glVertexAttribPointer(iiloc, 1, GL_FLOAT, GL_FALSE, sizeof(float), (GLvoid*)0);
  // glEnableVertexAttribArray(idloc);
  // glBindBuffer(GL_ARRAY_BUFFER, 0);
  // glVertexAttribDivisor(idloc, 1);



  glBindVertexArray(0);

}

void th_freeVbo(th_ArrayObject* array)
{
  bool commanded = !(array->flags & TH_NOCOMMAND);

  // if (commanded)
  // {
  //   glUnmapNamedBuffer(array->cmds.drawCommandBuffer);
  // }
  //
  // if (array->instanced)
  // {
  //   glUnmapNamedBuffer(array->instances);
  // }
  glDeleteVertexArrays(1, &array->arrayObject);
  glDeleteBuffers( 1, &array->indices );

  glDeleteBuffers( 1, &array->verts );

  glDeleteBuffers(1, &array->instances);

  glDeleteBuffers(1, &array->cmds.drawCommandBuffer);
  glDeleteBuffers(1, &array->ids);
}


void th_renderArray(th_ArrayObject* array,GLuint numVerts,GLuint numInstances,GLuint vertOffset,GLuint instanceOffset)
{
  glBindVertexArray(array->arrayObject);
  if (array->instanced)
  {
    glDrawElementsInstancedBaseVertexBaseInstance( GL_TRIANGLES, numVerts, GL_UNSIGNED_INT, (void*)0 ,numInstances,vertOffset,instanceOffset );
    r_syncAfterRender(array);
  }
  else
  {
    glDrawElementsBaseVertex( GL_TRIANGLES, numVerts, GL_UNSIGNED_INT, (void*)0 ,vertOffset);
  }

  //glBindVertexArray(0);
}

void th_setCommandBuffer(th_ArrayObject* array,r_DrawElementsIndirectCommand* cmds,GLuint count)
{
  memcpy(&array->cmds.commandBufferMapped[256*(th_frame()%3)],cmds,sizeof(r_DrawElementsIndirectCommand)*count);
}

void th_renderArrayCmdBuffer(th_ArrayObject* array,GLuint count)
{

  glBindVertexArray(array->arrayObject);
  glMultiDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_INT,(void*)(256*(th_frame()%3)*sizeof(r_DrawElementsIndirectCommand)),count,0);
  r_syncAfterRender(array);
//  glBindVertexArray(0);
}

void th_renderArrayCmdBufferPatches(th_ArrayObject* array,GLuint count)
{

  glBindVertexArray(array->arrayObject);
  glPatchParameteri(GL_PATCH_VERTICES, 3);
  glMultiDrawElementsIndirect(GL_PATCHES,GL_UNSIGNED_INT,(void*)(256*(th_frame()%3)*sizeof(r_DrawElementsIndirectCommand)),count,0);
  r_syncAfterRender(array);
//  glBindVertexArray(0);
}

void th_createFramebuffer(th_FrameBuffer* framebuffer,int width,int height)
{
framebuffer->flags = 0;
glGenFramebuffers(1, &framebuffer->framebuffer);
glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);

// - color + specular color buffer
unsigned int attachments[4] = { GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2,GL_COLOR_ATTACHMENT3};
glDrawBuffers(4, attachments);

framebuffer->textures = malloc(sizeof(GLuint)*4);
framebuffer->textureCount = 4;

//irradiance
glGenTextures(1, &framebuffer->textures[0]);
glBindTexture(GL_TEXTURE_2D, framebuffer->textures[0]);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);//rgba
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer->textures[0], 0);
//normal-roughness
glGenTextures(1, &framebuffer->textures[1]);
glBindTexture(GL_TEXTURE_2D, framebuffer->textures[1]);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_FLOAT, NULL);//rgba
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, framebuffer->textures[1], 0);
//cubemaps (will also be used for directional occluson)
glGenTextures(1, &framebuffer->textures[2]);
glBindTexture(GL_TEXTURE_2D, framebuffer->textures[2]);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, framebuffer->textures[2], 0);
//albedo-metallic
glGenTextures(1, &framebuffer->textures[3]);
glBindTexture(GL_TEXTURE_2D, framebuffer->textures[3]);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, framebuffer->textures[3], 0);

glGenTextures(1, &framebuffer->depthTexture);
glBindTexture(GL_TEXTURE_2D, framebuffer->depthTexture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer->depthTexture, 0);

if(!(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE))
{
  printf("%s\n","Incomplete Framebuffer" );
}

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void th_createFramebufferPrefilter(th_FrameBuffer* framebuffer,int width,int height)
{
  framebuffer->flags = 0;
  glGenFramebuffers(1, &framebuffer->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);

  // - color + specular color buffer
  unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, attachments);

  framebuffer->textures = malloc(sizeof(GLuint)*1);
  framebuffer->textureCount = 1;

  //irradiance
  glGenTextures(1, &framebuffer->textures[0]);
  glBindTexture(GL_TEXTURE_2D, framebuffer->textures[0]);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer->textures[0], 0);


  if(!(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE))
  {
    printf("%s\n","Incomplete Framebuffer" );
  }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void th_createFramebufferDepthOnly(th_FrameBuffer* framebuffer,int width,int height)
{
  framebuffer->flags = 0;
  glGenFramebuffers(1, &framebuffer->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);

  // // - color + specular color buffer
   unsigned int attachments[1] = { GL_NONE};
  glDrawBuffers(1, attachments);

  framebuffer->textures = NULL;
  framebuffer->textureCount = 0;

  glGenTextures(1, &framebuffer->depthTexture);
  glBindTexture(GL_TEXTURE_2D, framebuffer->depthTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

  GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer->depthTexture, 0);



  if(!(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE))
  {
    printf("%s\n","Incomplete Framebuffer" );
  }

  glClear(GL_DEPTH_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void th_createFramebufferGeneral(th_FrameBuffer* framebuffer,int width,int height,th_RenderEnum buffers_desc,int num_buffers)
{
  glGenFramebuffers(1, &framebuffer->framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
  framebuffer->flags = buffers_desc;
  // - color + specular color buffer
  unsigned int* attachments = malloc(sizeof(unsigned int)*num_buffers);//[num_buffers];
  for (int i = 0 ; i < num_buffers;i++)
  {
    attachments[i] = GL_COLOR_ATTACHMENT0 + i;
  }
  glDrawBuffers(num_buffers, attachments);

  framebuffer->textures = malloc(sizeof(GLuint)*num_buffers);
  framebuffer->textureCount = num_buffers;

  for (int i = 0 ; i < num_buffers;i++)
  {
    glGenTextures(1, &framebuffer->textures[i]);
    glBindTexture(GL_TEXTURE_2D, framebuffer->textures[i]);
    GLint internalformat = GL_RGB8;
    GLenum format = GL_RGB;
    if (buffers_desc & TH_VEC3 && buffers_desc & TH_VEC4)
    {
      if (i == 0)
      {
        format = GL_RGB;
      }
      else
      {
        format = GL_RGBA;
      }
    }
    else if (buffers_desc & TH_VEC4 && buffers_desc & TH_VEC1)
    {
      if (i == 0 && num_buffers > 1)
      {
        format = GL_RGBA;
      }
      else
      {
        format = GL_RED;
      }
    }
    else if (buffers_desc & TH_VEC3)
    {
      format = GL_RGB;
    }
    else if (buffers_desc & TH_VEC4)
    {
      format = GL_RGBA;
    }

    if (format == GL_RGB)
    {
      if (buffers_desc & TH_8BIT)
      {
        internalformat = GL_RGB8;
      }
      else if (buffers_desc & TH_16BIT)
      {
        internalformat = GL_RGB16F;
      }
      else
      {
        internalformat = GL_RGB32F;
      }

      if (i == 0 && (buffers_desc & TH_32BITRGB))
      {
        internalformat = GL_RGB32F;
      }
    }
    else if (format == GL_RGBA)
    {
      if (buffers_desc & TH_8BIT)
      {
        internalformat = GL_RGBA8;
      }
      else if (buffers_desc & TH_16BIT)
      {
        internalformat = GL_RGBA16F;
      }
      else
      {
        internalformat = GL_RGBA32F;
      }
    }
    else if (format == GL_RED)
    {
      if (buffers_desc & TH_8BIT)
      {
        internalformat = GL_R8;
      }
      else if (buffers_desc & TH_16BIT)
      {
        internalformat = GL_R16F;
      }
      else
      {
        internalformat = GL_R32F;
      }
    }

    if ((buffers_desc & TH_RG16F) && num_buffers == 3 && i == 2)
    {
      internalformat = GL_RG16F;
      format = GL_RG;
    }
    else if ((buffers_desc & TH_RGBA16F) && num_buffers == 3 && i == 2)
    {
      internalformat = GL_RGBA16F;
      format = GL_RGBA;
    }


    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_FLOAT, NULL);
    if (buffers_desc & TH_FILTERED)
    {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, framebuffer->textures[i], 0);
  }


  if (buffers_desc & TH_DEPTHBUFFER)
  {
    if (buffers_desc & TH_DEPTHBUFFER_RENDERBUFFER)
    {
      unsigned int rbo;
      glGenRenderbuffers(1, &rbo);
      glBindRenderbuffer(GL_RENDERBUFFER, rbo);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
    }
    else
    {
      glGenTextures(1, &framebuffer->depthTexture);
      glBindTexture(GL_TEXTURE_2D, framebuffer->depthTexture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer->depthTexture, 0);
    }

  }


  if(!(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE))
  {
    printf("%s\n","Incomplete Framebuffer" );
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    free(attachments);
}

void th_swapTextureFBO(GLuint* old,GLuint* new,GLenum index,th_FrameBuffer* fb )
{
  GLuint temp = *new;
  *new = *old;
  *old = temp;
  glBindFramebuffer(GL_FRAMEBUFFER, fb->framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, index, GL_TEXTURE_2D, *old, 0);
  // glNamedFramebufferTexture(fb->framebuffer, index,*old,0);
}

void th_createFrameTextureGeneral(th_FrameBuffer* framebuffer,int width,int height,th_RenderEnum buffers_desc,int num_buffers)
{

  framebuffer->flags = buffers_desc;


  framebuffer->textures = malloc(sizeof(GLuint)*num_buffers);
  framebuffer->textureCount = num_buffers;

  for (int i = 0 ; i < num_buffers;i++)
  {
    glGenTextures(1, &framebuffer->textures[i]);
    glBindTexture(GL_TEXTURE_2D, framebuffer->textures[i]);
    GLint internalformat = GL_RGB8;
    GLenum format = GL_RGB;
    if (buffers_desc & TH_VEC3 && buffers_desc & TH_VEC4)
    {
      if (i == 0)
      {
        format = GL_RGB;
      }
      else
      {
        format = GL_RGBA;
      }
    }
    else if (buffers_desc & TH_VEC4 && buffers_desc & TH_VEC1)
    {
      if (i == 0)
      {
        format = GL_RGBA;
      }
      else
      {
        format = GL_RED;
      }
    }
    else if (buffers_desc & TH_VEC3)
    {
      format = GL_RGB;
    }
    else if (buffers_desc & TH_VEC4)
    {
      format = GL_RGBA;
    }

    if (format == GL_RGB)
    {
      if (buffers_desc & TH_8BIT)
      {
        internalformat = GL_RGB8;
      }
      else if (buffers_desc & TH_16BIT)
      {
        internalformat = GL_RGB16F;
      }
      else
      {
        internalformat = GL_RGB32F;
      }

      if (i == 0 && (buffers_desc & TH_32BITRGB))
      {
        internalformat = GL_RGB32F;
      }
    }
    else if (format == GL_RGBA)
    {
      if (buffers_desc & TH_8BIT)
      {
        internalformat = GL_RGBA8;
      }
      else if (buffers_desc & TH_16BIT)
      {
        internalformat = GL_RGBA16F;
      }
      else
      {
        internalformat = GL_RGBA32F;
      }
    }
    else if (format == GL_RED)
    {
      if (buffers_desc & TH_8BIT)
      {
        internalformat = GL_R8;
      }
      else if (buffers_desc & TH_16BIT)
      {
        internalformat = GL_R16F;
      }
      else
      {
        internalformat = GL_R32F;
      }
    }

    if ((buffers_desc & TH_RG16F) && num_buffers == 3 && i == 2)
    {
      internalformat = GL_RG16F;
      format = GL_RG;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_FLOAT, NULL);
    if (buffers_desc & TH_FILTERED)
    {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }


  if (buffers_desc & TH_DEPTHBUFFER)
  {
    glGenTextures(1, &framebuffer->depthTexture);
    glBindTexture(GL_TEXTURE_2D, framebuffer->depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }


}

void th_bindFrameBuffer(th_FrameBuffer* framebuffer)
{
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
}

void th_freeFramebuffer(th_FrameBuffer* framebuffer)
{
  glDeleteFramebuffers(1,&framebuffer->framebuffer);
  if (framebuffer->flags & TH_DEPTHBUFFER)
  {
    glDeleteTextures(1,&framebuffer->depthTexture);
  }
  if (framebuffer->textureCount > 0 )
  {
    glDeleteTextures(framebuffer->textureCount,framebuffer->textures);
  }
}


void* pboMapBuffer(size_t datasize)
{

  GLenum flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage(GL_PIXEL_PACK_BUFFER, datasize, 0, flags);
  return glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, datasize, flags);

}

th_PixelPackBuffer th_createPixelPackBuffers(size_t datasize,int num_bufs)
{
  th_PixelPackBuffer out;
  glGenBuffers(num_bufs, out.pbos);
  for (int i = 0 ; i < num_bufs;i++)
  {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, out.pbos[i]);
    glBufferData(GL_PIXEL_PACK_BUFFER, datasize, 0, GL_STREAM_READ);
    out.pbodata[i] = pboMapBuffer(datasize);
  }

  // glBindBuffer(GL_PIXEL_PACK_BUFFER, out.pbos[1]);
  // glBufferData(GL_PIXEL_PACK_BUFFER, datasize, 0, GL_STREAM_READ);
  // out.pbodata[1] = pboMapBuffer(datasize);

  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

  // out.current = malloc(datasize);
  return out;
}

void th_capturePixelBuffer(th_PixelPackBuffer* buffer,GLuint texture,size_t datasize)
{
  //tetxure must be binded
//  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer->pbos[th_frame() % 2]);
  glGetTextureImage(	texture,	0,GL_RGBA,	GL_FLOAT,	datasize,0);
  //captured = ((fn_frame() + 1 )% 2);
}

GLuint th_createUbo(GLuint size,GLuint binding_point)
{
  unsigned int ret;
glGenBuffers(1, &ret);

glBindBuffer(GL_UNIFORM_BUFFER, ret);
glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
glBindBuffer(GL_UNIFORM_BUFFER, 0);

glBindBufferRange(GL_UNIFORM_BUFFER, binding_point, ret, 0, size);
glBindBuffer(GL_UNIFORM_BUFFER, 0);
return ret;
}

void* uboMapBuffer(size_t datasize)
{

  GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage(GL_UNIFORM_BUFFER, datasize, 0, flags);
  return glMapBufferRange(GL_UNIFORM_BUFFER, 0, datasize, flags);

}

GLuint th_createMappedUbo(GLuint size,GLuint binding_point,void** mem)
{
  unsigned int ret;
glGenBuffers(1, &ret);

glBindBuffer(GL_UNIFORM_BUFFER, ret);
*mem = uboMapBuffer(size*3);
glBindBuffer(GL_UNIFORM_BUFFER, 0);

glBindBufferRange(GL_UNIFORM_BUFFER, binding_point, ret, 0, size);
glBindBuffer(GL_UNIFORM_BUFFER, 0);
return ret;
}

void* ssboMapBuffer(size_t datasize)
{

  GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, datasize, 0, flags);
  return glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, datasize, flags);

}

GLuint th_createSSBO(GLuint size,GLuint binding_point)
{
  unsigned int ret;
glGenBuffers(1, &ret);

glBindBuffer(GL_SHADER_STORAGE_BUFFER, ret);
glBufferData(GL_SHADER_STORAGE_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding_point, ret, 0, size);
glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
return ret;
}

GLuint th_createMappedSSBO(GLuint size,GLuint binding_point,void** mem,r_VboSync* syncs)
{
  unsigned int ret;
glGenBuffers(1, &ret);

glBindBuffer(GL_SHADER_STORAGE_BUFFER, ret);
*mem = ssboMapBuffer(size*3);
glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding_point, ret, 0, size*3);
glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

for (int i =0;i < 3;i++)
  syncs[i].used = false;


return ret;
}

void th_makeQuery(th_QueryBuffer* q,const char* name)
{
  glGenQueries(3,q->query);
  q->name = name;

}
void th_startQuery(th_QueryBuffer* q)
{
  glBeginQuery(GL_TIME_ELAPSED,q->query[th_frame()%3]);

}
void th_endQuery(th_QueryBuffer* q)
{
  glEndQuery(GL_TIME_ELAPSED);
}
void th_printQuery(th_QueryBuffer* q)
{
  if (!(th_frame() > 3))
  {
    return;
  }
  GLint64 result;
  glGetQueryObjecti64v(q->query[(th_frame() - 2)%3],GL_QUERY_RESULT,&result);

  float final = result*0.000001;
  printf("%s: %f\n",q->name,final );


}


static char toUpper(char x)
{
  return x > 90 ? x - 32 : x;
}

void th_renderText(char* string,fn_vec2 pos_tx,float size,float font,r_Shader* text_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data)
{
  r_bindShader(text_shader);
  r_sendmat4(text_shader,postmatrix,"modelViewprojection");
  r_sendf(text_shader,36,"charset");

  float uvs[100];
  int i;
  int uvCount = 0;
  float chwidth = 1.f/36;
  // char string[100];
  // sprintf(string,"HEALTH 4/4 |,,,,|");
  int len = strlen(string);
  for (i =0;i <len;i++)
  {
    int num =  string[i] < 58 ? (int)string[i] - ((int)'0') : (int)toUpper(string[i]) - ((int)'A') + 10;
    float uv0 = num*chwidth;
    float uv1 = num*chwidth + chwidth;
    if (num == -1)
    {
      uv0 = -1;
    }
    if (num == -2)
    {
      uv0 = -2;
    }
    if (string[i] == '_')
    {
      uv0 = -3;
    }
    if (string[i] == (char)44)
    {
      uv0 = -4;
    }
    if (string[i] == ' ')
    {
      uv0 = 0;
      uv1 = 0;
    }
    if (string[i] == '|')
    {
      uv0 = -5;
    }
    if (string[i] == '+')
    {
      uv0 = -6;
    }
    uvs[uvCount] = uv0;
    uvs[uvCount + 1] = uv1;

    uvCount = uvCount + 2;
  }
  r_sendfv(text_shader,uvs,uvCount,"inUvs");
  r_sendf(text_shader,len,"charCount");
  r_sendf(text_shader,font,"font_id");
  r_sendf(text_shader,th_time()*0.01,"time");



  // fn_vec2 pos_tx = fn_createVec2(32,32);
  float width_tx = len*size;
  float height_tx = size;
  fn_vec2 p0 = pos_tx;
  fn_vec2 p1 = fn_addVec2(pos_tx,fn_createVec2(width_tx,height_tx));

  text_data->verts[0].position = fn_createVec3(p1.x,p1.y,0);
  text_data->verts[1].position = fn_createVec3(p1.x,p0.y,0);
  text_data->verts[2].position = fn_createVec3(p0.x,p0.y,0);
  text_data->verts[3].position = fn_createVec3(p0.x,p1.y,0);

  glBindVertexArray(text_gpu->arrayObject);
  glBindBuffer( GL_ARRAY_BUFFER, text_gpu->verts );
  glBufferSubData( GL_ARRAY_BUFFER, 0, 4 * sizeof(th_Vertex), text_data->verts );

  th_renderArray(text_gpu,6,0,0,0);
}

static void renderChar(th_Character ch,th_ArrayObject* text_gpu,th_GpuData* text_data,r_Shader* text_shader,float x,float y,float size,fn_vec3 color)
{
  float xpos = x + ch.bearing.x * size;
  float ypos = y - (ch.size.y - ch.bearing.y) * size;

  float w = ch.size.x * size;
  float h = ch.size.y * size;

  fn_vec2 p0 = fn_createVec2(xpos,ypos);
  fn_vec2 p1 = fn_addVec2(p0,fn_createVec2(w,h));

  text_data->verts[0].position = fn_createVec3(p1.x,p1.y,0);
  text_data->verts[1].position = fn_createVec3(p1.x,p0.y,0);
  text_data->verts[2].position = fn_createVec3(p0.x,p0.y,0);
  text_data->verts[3].position = fn_createVec3(p0.x,p1.y,0);


  glBindTexture( GL_TEXTURE_2D, ch.textureID );

  glBindVertexArray(text_gpu->arrayObject);
  glBindBuffer( GL_ARRAY_BUFFER, text_gpu->verts );
  glBufferSubData( GL_ARRAY_BUFFER, 0, 4 * sizeof(th_Vertex), text_data->verts );



  r_send3f(text_shader,color,"textColor");
  th_renderArray(text_gpu,6,0,0,0);
}

static float magnitude_local = 35.0;
void th_setTextGlyphMagnitude(float mag)
{
  magnitude_local = mag;
}

static const float max_chars_offsets = 100;
static float* vertical_offset_chars = NULL;
void th_setTextGlyphOffsets(float* o,int count)
{
  if (vertical_offset_chars == NULL)
  {
    vertical_offset_chars = malloc(sizeof(float)*max_chars_offsets);
  }
  if (count > max_chars_offsets)
  {
    count = max_chars_offsets;
  }

  if (o == NULL)
  {
    for (int i = 0 ; i < max_chars_offsets;i++)
    {
      vertical_offset_chars[i] = 0.0;
    }
  }
  else
  {
      memcpy(vertical_offset_chars,o,sizeof(float)*count);
  }

}

float th_renderTextGlyph(const char* string,fn_vec2 pos_tx,float size,float font,r_Shader* text_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data,th_Character* cmap,fn_vec3 color)
{
  r_bindShader(text_shader);
  r_sendmat4(text_shader,postmatrix,"modelViewprojection");
  r_sendf(text_shader,(float)th_time_global(),"time");
  r_sendf(text_shader,magnitude_local,"magnitude");

  int len = strlen(string);
  float x = pos_tx.x;


  float y = pos_tx.y;
  glActiveTexture(GL_TEXTURE0 + 35);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  float w = 0;

  for (int i =0;i <len;i++)
  {

    float yoff = (vertical_offset_chars == NULL || i >= max_chars_offsets) ? 0.0 : vertical_offset_chars[i];

    th_Character ch = cmap[(unsigned char)string[i]];
    th_Character ch_outline = cmap[(unsigned char)string[i] + 128];

    renderChar(ch_outline,text_gpu,text_data,text_shader,x,y + yoff,size,fn_createVec3(0,0,0));
    renderChar(ch,text_gpu,text_data,text_shader,x,y + yoff,size,color);


    x += (ch.advance >> 6) * size;
    w += (ch.advance >> 6) * size;
  }


  glDisable(GL_BLEND);
  // r_sendfv(text_shader,uvs,uvCount,"inUvs");
  // r_sendf(text_shader,len,"charCount");
  // r_sendf(text_shader,font,"font_id");
  // r_sendf(text_shader,th_time()*0.01,"time");
  //
  //
  //
  // // fn_vec2 pos_tx = fn_createVec2(32,32);
  // float width_tx = len*size;
  // float height_tx = size;
  return w;


}


void th_renderImage(GLuint texture,fn_vec2 pos_tx,fn_vec2 dimensions,r_Shader* image_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data,float alpha)
{

  r_bindShader(image_shader);
  r_sendmat4(image_shader,postmatrix,"modelViewprojection");
  r_sendf(image_shader,alpha,"alpha_blend");


  float x = pos_tx.x;
  float y = pos_tx.y;
  glActiveTexture(GL_TEXTURE0 + 35);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



  float w = dimensions.x;
  float h = dimensions.y;

  fn_vec2 p0 = fn_createVec2(x,y);
  fn_vec2 p1 = fn_addVec2(p0,fn_createVec2(w,h));

  text_data->verts[0].position = fn_createVec3(p1.x,p1.y,0);
  text_data->verts[1].position = fn_createVec3(p1.x,p0.y,0);
  text_data->verts[2].position = fn_createVec3(p0.x,p0.y,0);
  text_data->verts[3].position = fn_createVec3(p0.x,p1.y,0);


  glBindTexture( GL_TEXTURE_2D, texture );

  glBindVertexArray(text_gpu->arrayObject);
  glBindBuffer( GL_ARRAY_BUFFER, text_gpu->verts );
  glBufferSubData( GL_ARRAY_BUFFER, 0, 4 * sizeof(th_Vertex), text_data->verts );


  th_renderArray(text_gpu,6,0,0,0);

  glDisable(GL_BLEND);
}

void th_renderShaderQuad(fn_vec2 pos_tx,fn_vec2 dimensions,r_Shader* image_shader,fn_mat4 postmatrix,th_ArrayObject* text_gpu,th_GpuData* text_data,float alpha,fn_vec3 tint)
{
  r_bindShader(image_shader);
  r_sendmat4(image_shader,postmatrix,"modelViewprojection");
  r_sendf(image_shader,alpha,"alpha_blend");
  r_sendf(image_shader,(float)th_time_global(),"time");
  r_send3f(image_shader,tint,"tint");


  float x = pos_tx.x;
  float y = pos_tx.y;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



  float w = dimensions.x;
  float h = dimensions.y;

  fn_vec2 p0 = fn_createVec2(x,y);
  fn_vec2 p1 = fn_addVec2(p0,fn_createVec2(w,h));

  text_data->verts[0].position = fn_createVec3(p1.x,p1.y,0);
  text_data->verts[1].position = fn_createVec3(p1.x,p0.y,0);
  text_data->verts[2].position = fn_createVec3(p0.x,p0.y,0);
  text_data->verts[3].position = fn_createVec3(p0.x,p1.y,0);


  glBindVertexArray(text_gpu->arrayObject);
  glBindBuffer( GL_ARRAY_BUFFER, text_gpu->verts );
  glBufferSubData( GL_ARRAY_BUFFER, 0, 4 * sizeof(th_Vertex), text_data->verts );


  th_renderArray(text_gpu,6,0,0,0);

  glDisable(GL_BLEND);
}

void th_createVboOccluder(th_ArrayObject* array,th_GpuOccluderData data,th_RenderEnum flags)
{


  array->flags = flags;



  array->instanced = true;
  for (int i =0;i < 3;i++)
    array->instancesSync[i].used = false;

  glGenVertexArrays(1, &array->arrayObject);
  glGenBuffers( 1, &array->indices );

  glGenBuffers( 1, &array->verts );

  // glGenBuffers( 1, &array->ids );

  glGenBuffers(1, &array->instances);

  glGenBuffers(1, &array->ids);


  glBindVertexArray(array->arrayObject);




    glBindBuffer(GL_ARRAY_BUFFER, array->instances);
    array->max_instances = data.instancecount;
    array->mappedInstancesOccluder = (fn_vec4*)vboMapInstanceBuffer(sizeof(fn_vec4),data.instancecount*FN_BUFFER_COUNT);
    memcpy(array->mappedInstancesOccluder,data.instances,sizeof(fn_vec4)*data.instancecount);
    memcpy(&array->mappedInstancesOccluder[data.instancecount],data.instances,sizeof(fn_vec4)*data.instancecount);
    memcpy(&array->mappedInstancesOccluder[data.instancecount*2],data.instances,sizeof(fn_vec4)*data.instancecount);




  // glBindBuffer (GL_ARRAY_BUFFER, vbo->IIOID);
  //
  //   vbo->mappedInstancedBufferID = (float*)vboMapInstanceBuffer(sizeof(float),vbo->instances);
  //   memcpy(vbo->mappedInstancedBufferID,vbo->id,sizeof(float)*vbo->instances);


  glBindBuffer( GL_ARRAY_BUFFER, array->verts );

  glBufferData( GL_ARRAY_BUFFER, data.vertcount * sizeof(th_OccluderVertex), data.verts, GL_STATIC_DRAW );



  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, array->indices);
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, data.indicecount * sizeof(GLuint), data.indices, GL_STATIC_DRAW );


  GLint posloc,trloc;
  posloc = 0;//glGetAttribLocation(shader->shaderProgram,"inPosition");
  trloc = 1;


  GLsizei vertex_size = sizeof(th_OccluderVertex);



  glVertexAttribPointer(posloc, 3, GL_FLOAT, GL_FALSE,vertex_size , (GLvoid*)offsetof(th_OccluderVertex,position));
  glEnableVertexAttribArray(posloc);




  glBindBuffer(GL_ARRAY_BUFFER, array->instances); // this attribute comes from a different vertex buffer

  glVertexAttribPointer(trloc, 4, GL_FLOAT, GL_FALSE, sizeof(fn_vec4) , (void*)(0));
  glEnableVertexAttribArray(trloc);

  glVertexAttribDivisor(trloc, 1);


  glBindBuffer(GL_ARRAY_BUFFER, 0);


  glBindVertexArray(0);

}
