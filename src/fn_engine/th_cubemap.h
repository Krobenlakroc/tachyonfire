#pragma once
#include "../fn_gl.h"
#include "../fn_math/fn_vec3.h"
#include "r_shader.h"
#include "th_gpu.h"

#define TH_MAX_CUBEMAPS 55

GLuint th_createCubemap(GLfloat** faces,int dimension);
void th_saveCubemap(GLfloat** faces,int dimension,const char* filename);
GLfloat** th_loadCubemap(int dimension,const char* filename);

void th_loadCubemapWAD(int resolution,int mips,GLuint* depthtex,GLuint* colortex,fn_vec3* min,fn_vec3* max,int cubecount,const char* wadlocation);
void th_blankCubemap(int resolution,int mips,GLuint* depthtex,GLuint* colortex,fn_vec3* min,fn_vec3* max,int cubecount);
void th_createSkyCubemap(int resolution,r_Shader* shader,GLuint* skytex,fn_vec3 sunpos,r_Shader* prefiltershader,fn_vec3 atm_rayleigh,float sun_intensity,fn_vec3 sun_color,bool send_cloud_info,float cloud_enable,float horizon_height);


void th_loadCubemapCompressed(int resolution,int mips,GLuint* depthtex,GLuint* colortex,fn_vec3* min,fn_vec3* max,int cubecount,const char* wadlocation);
