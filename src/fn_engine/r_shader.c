#include "r_shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void r_setShaderUniformName(int ID,r_Shader* shader,const char* name)
{
  shader->textureUniforms[ID].name = name;
}
void errorCheck(r_Shader* shader,bool frag)
{
  int successg,successg2;
  if (!frag)
  {
    glGetShaderiv(shader->vertexShader, GL_COMPILE_STATUS, &successg);

    if(!successg)
    {
      GLchar infoLog[512];
      glGetShaderInfoLog(shader->vertexShader, 512, NULL, infoLog);
      printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s",infoLog);
    }
  }
  else
  {
    glGetShaderiv(shader->fragmentShader, GL_COMPILE_STATUS, &successg2);

    if(!successg2)
    {

      GLchar infoLog[512];
      glGetShaderInfoLog(shader->fragmentShader, 512, NULL, infoLog);
      printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s",infoLog);
    }
  }
}

static char* getFileString(const char* filename)
{
  char * buffer = 0;
  long length;
  FILE * f = fopen(filename, "rb");

  if (f)
  {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    if (length == -1)
    {
      buffer = NULL;
      printf("%s %s\n","Error reading Shader File!",filename);
      return NULL;
    }

    fseek (f, 0, SEEK_SET);
    buffer = malloc (length + 1);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
    buffer[length] = '\0';
  }
  if (!buffer)
  {
    printf("%s %s\n","Error reading Shader File!",filename );
  }
  return buffer;

}

void r_initShaderCompute(r_Shader* shader,const char* filepath,const char* id,const char* extra)
{
  const char* file = getFileString(filepath);

  const char* file_sources[2] = {extra,file};
  // printf("%s\n",filepath );
  // printf("%i\n",'\n' );
  // printf("%i\n",vert[strlen(vert)] );
  GLint i;
  shader->vertexShader = glCreateShader(GL_COMPUTE_SHADER);

  glShaderSource(shader->vertexShader, 2, file_sources , NULL);
  glCompileShader(shader->vertexShader);


   glGetShaderiv(shader->vertexShader, GL_COMPILE_STATUS, &i);
  //
  if(!i)
  {
    GLchar infoLog[512];
    glGetShaderInfoLog(shader->vertexShader, 512, NULL, infoLog);
    printf("%s ERROR::SHADER::COMPUTE::COMPILATION_FAILED\n%s",id,infoLog);
  }


  shader->shaderProgram = glCreateProgram();
  glAttachShader(shader->shaderProgram, shader->vertexShader);
  glLinkProgram(shader->shaderProgram);
  glDeleteShader(shader->vertexShader);
}


void r_initShader(r_Shader* shader,const char* vertpath,const char* fragpath,const char* id,const char* extra)
{
  const char* frag = getFileString(fragpath);
  const char* vert = getFileString(vertpath);

  const char* frag_sources[2] = {extra,frag};
  // printf("%i\n",'\n' );
  // printf("%i\n",vert[strlen(vert)] );
  GLint i;
  shader->vertexShader = glCreateShader(GL_VERTEX_SHADER);

  glShaderSource(shader->vertexShader, 1, &vert , NULL);
  glCompileShader(shader->vertexShader);


   glGetShaderiv(shader->vertexShader, GL_COMPILE_STATUS, &i);
  //
  if(!i)
  {
    GLchar infoLog[512];
    glGetShaderInfoLog(shader->vertexShader, 512, NULL, infoLog);
    printf("%s ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s",id,infoLog);
  }
  // else
  // {
  //   GLchar infoLog[512];
  //   glGetShaderInfoLog(shader->vertexShader, 512, NULL, infoLog);
  //   printf("%s SHADER::COMPILATION::VERTEX\n%s",id,infoLog);
  // }


  shader->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(shader->fragmentShader, 2, frag_sources, NULL);
  glCompileShader(shader->fragmentShader);


  glGetShaderiv(shader->fragmentShader, GL_COMPILE_STATUS, &i);

  if(!i)
  {
    GLchar infoLog[4000];
    glGetShaderInfoLog(shader->fragmentShader, 4000, NULL, infoLog);
    printf("%s ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s",id,infoLog);
  }
  // else
  // {
  //   GLchar infoLog[512];
  //   glGetShaderInfoLog(shader->fragmentShader, 512, NULL, infoLog);
  //   printf("%s SHADER::COMPILATION::FRAGMENT\n%s",id,infoLog);
  // }

//  errorCheck(shader,true);

  shader->shaderProgram = glCreateProgram();
  glAttachShader(shader->shaderProgram, shader->vertexShader);
  glAttachShader(shader->shaderProgram, shader->fragmentShader);
  glLinkProgram(shader->shaderProgram);
  glDeleteShader(shader->vertexShader);
  glDeleteShader(shader->fragmentShader);
}

void r_initShaderTess(r_Shader* shader,const char* vertpath,const char* fragpath,const char* tesscontrol_path,const char* tesseval_path,const char* id)
{
  const char* frag = getFileString(fragpath);
  const char* vert = getFileString(vertpath);
  const char* tesseval = getFileString(tesseval_path);
  const char* tesscontrol = getFileString(tesscontrol_path);
  // printf("%i\n",'\n' );
  // printf("%i\n",vert[strlen(vert)] );
  GLint i;
  shader->vertexShader = glCreateShader(GL_VERTEX_SHADER);

  glShaderSource(shader->vertexShader, 1, &vert , NULL);
  glCompileShader(shader->vertexShader);


   glGetShaderiv(shader->vertexShader, GL_COMPILE_STATUS, &i);
  //
  if(!i)
  {
    GLchar infoLog[512];
    glGetShaderInfoLog(shader->vertexShader, 512, NULL, infoLog);
    printf("%s ERROR::SHADER::VERTEX::COMPILATION_FAILED\n%s",id,infoLog);
  }


  shader->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(shader->fragmentShader, 1, &frag, NULL);
  glCompileShader(shader->fragmentShader);


  glGetShaderiv(shader->fragmentShader, GL_COMPILE_STATUS, &i);

  if(!i)
  {
    GLchar infoLog[512];
    glGetShaderInfoLog(shader->fragmentShader, 512, NULL, infoLog);
    printf("%s ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n%s",id,infoLog);
  }

  GLuint tess_shader1 = glCreateShader(GL_TESS_CONTROL_SHADER);

  glShaderSource(tess_shader1, 1, &tesscontrol, NULL);
  glCompileShader(tess_shader1);


  glGetShaderiv(tess_shader1, GL_COMPILE_STATUS, &i);

  if(!i)
  {
    GLchar infoLog[512];
    glGetShaderInfoLog(tess_shader1, 512, NULL, infoLog);
    printf("%s ERROR::SHADER::TESSELATION_CONTROL:COMPILATION_FAILED\n%s",id,infoLog);
  }

  GLuint tess_shader2 = glCreateShader(GL_TESS_EVALUATION_SHADER);

  glShaderSource(tess_shader2, 1, &tesseval, NULL);
  glCompileShader(tess_shader2);


  glGetShaderiv(tess_shader2, GL_COMPILE_STATUS, &i);

  if(!i)
  {
    GLchar infoLog[512];
    glGetShaderInfoLog(tess_shader2, 512, NULL, infoLog);
    printf("%s ERROR::SHADER::TESSELATION_EVALUATION:COMPILATION_FAILED\n%s",id,infoLog);
  }
//  errorCheck(shader,true);

  shader->shaderProgram = glCreateProgram();
  glAttachShader(shader->shaderProgram, shader->vertexShader);
  glAttachShader(shader->shaderProgram, shader->fragmentShader);
  glAttachShader(shader->shaderProgram, tess_shader1);
  glAttachShader(shader->shaderProgram, tess_shader2);
  glLinkProgram(shader->shaderProgram);

  GLint isLinked = 0;
  glGetProgramiv(shader->shaderProgram, GL_LINK_STATUS, &isLinked);
  if (isLinked == GL_FALSE)
  {
    GLint maxLength = 0;
    glGetProgramiv(shader->shaderProgram, GL_INFO_LOG_LENGTH, &maxLength);

    GLchar* infoLog = malloc(sizeof(GLchar)*maxLength);//[maxLength];
    glGetProgramInfoLog(shader->shaderProgram, maxLength, &maxLength, &infoLog[0]);
    printf("%s ERROR::SHADER::LINKING_FAILED\n%s",id,infoLog);
    free(infoLog);
  }


  glDeleteShader(shader->vertexShader);
  glDeleteShader(shader->fragmentShader);
  glDeleteShader(tess_shader2);
  glDeleteShader(tess_shader1);
}

static r_Shader* boundshader = NULL;
void r_bindShader(r_Shader* shader)
{
  if (boundshader == shader)
  {
    return;
  }
  boundshader = shader;
  glUseProgram(shader->shaderProgram);
}

void r_unbindShader()
{
//  glUseProgram(0);
}

void r_sendTextureUniform(r_Shader* shader,int unit,const char* name)
{
  GLuint t1Location = glGetUniformLocation(shader->shaderProgram, name);
  glUniform1i(t1Location, unit);
}

void th_sendMaterials(r_Shader* shader)
{
  GLuint t1Location = glGetUniformLocation(shader->shaderProgram, "materials");
  GLint vals[3];
  vals[0] = 6;
  vals[1] = 7;
  vals[2] = 8;
  glUniform1iv(t1Location,3,vals);
}


void r_sendVec3(r_Shader* shader,fn_vec3* pos,int poscount,const char* name)
{
  GLint loc = glGetUniformLocation(shader->shaderProgram, name);
  if (loc == -1) {return;}
  glUniform3fv(glGetUniformLocation(loc, name), poscount,&pos->x);
}

void r_sendi(r_Shader* shader,int data,const char* name)
{
    glUniform1i(glGetUniformLocation(shader->shaderProgram, name), data);
}

void r_sendui(r_Shader* shader,unsigned int data,const char* name)
{
  glUniform1ui(glGetUniformLocation(shader->shaderProgram, name), data);
}

void r_sendiv(r_Shader* shader,int* data,int dataCount,const char* name)
{
  glUniform1iv(glGetUniformLocation(shader->shaderProgram, name),dataCount, data);
}

void r_sendfv(r_Shader* shader,float* data,int dataCount,const char* name)
{
  glUniform1fv(glGetUniformLocation(shader->shaderProgram, name),dataCount, data);
}

void r_sendf(r_Shader* shader,float data,const char* name)
{
  GLint loc = glGetUniformLocation(shader->shaderProgram, name);
  if (loc == -1) {return;}
  glUniform1f(loc, data);
}

void r_send3f(r_Shader* shader,fn_vec3 data,const char* name)
{
  GLint loc = glGetUniformLocation(shader->shaderProgram, name);
  if (loc == -1) {return;}
  glUniform3f(loc, data.x,data.y,data.z);
}

void r_send4f(r_Shader* shader,fn_vec4 data,const char* name)
{
  glUniform4f(glGetUniformLocation(shader->shaderProgram, name), data.x,data.y,data.z,data.w);
}

void r_send2f(r_Shader* shader,fn_vec2 data,const char* name)
{
  glUniform2f(glGetUniformLocation(shader->shaderProgram, name), data.x,data.y);
}

void r_sendmat4(r_Shader* shader,fn_mat4 m,const char* name)
{

  glUniformMatrix4fv(glGetUniformLocation(shader->shaderProgram, name),1,GL_FALSE,&m.m[0]);
}

void r_sendmat4v(r_Shader* shader,fn_mat4* m,int count,const char* name)
{
  glUniformMatrix4fv(glGetUniformLocation(shader->shaderProgram, name),count,GL_FALSE,&m[0].m[0]);
}

void r_sendmat3(r_Shader* shader,fn_mat3 m,const char* name)
{
  glUniformMatrix3fv(glGetUniformLocation(shader->shaderProgram, name),1,GL_FALSE,&m.m[0]);
}
