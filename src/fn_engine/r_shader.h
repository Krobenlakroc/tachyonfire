#pragma once
#include "../fn_gl.h"
#include <stdbool.h>
#include "../fn_math/fn_math.h"
#define T_ALBTEX 0
#define T_DECALTEX 1
#define T_LIGHTMAPTEX 2
#define T_ALTTEX 3

typedef struct
{
  const char* name;
  int unit;
  GLuint id;
  GLenum type;
}fn_TextureUniform;

typedef struct
{
  GLuint vertexShader;
  GLuint fragmentShader;
  GLuint shaderProgram;
  fn_TextureUniform textureUniforms[4];
}r_Shader;
void th_sendMaterials(r_Shader* shader);

void r_initShaderCompute(r_Shader* shader,const char* filepath,const char* id,const char* extra);
void r_initShader(r_Shader* shader,const char* vertpath,const char* fragpath,const char* id,const char* extra);
void r_initShaderTess(r_Shader* shader,const char* vertpath,const char* fragpath,const char* tesscontrol_path,const char* tesseval_path,const char* id);
void r_setShaderUniformName(int ID,r_Shader* shader,const char* name);
void r_bindShader(r_Shader* shader);
void r_unbindShader();
void r_sendTextureUniform(r_Shader* shader,int unit,const char* name);

void r_sendVec3(r_Shader* shader,fn_vec3* pos,int poscount,const char* name);

void r_sendmat4v(r_Shader* shader,fn_mat4* m,int count,const char* name);
void r_sendmat4(r_Shader* shader,fn_mat4 m,const char* name);
void r_sendmat3(r_Shader* shader,fn_mat3 m,const char* name);
void r_sendi(r_Shader* shader,int data,const char* name);
void r_sendui(r_Shader* shader,unsigned int data,const char* name);
void r_sendiv(r_Shader* shader,int* data,int dataCount,const char* name);
void r_sendf(r_Shader* shader,float data,const char* name);
void r_sendfv(r_Shader* shader,float* data,int dataCount,const char* name);
void r_send3f(r_Shader* shader,fn_vec3 data,const char* name);
void r_send4f(r_Shader* shader,fn_vec4 data,const char* name);
void r_send2f(r_Shader* shader,fn_vec2 data,const char* name);
