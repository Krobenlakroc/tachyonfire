#pragma once
#include "../fn_gl.h"
#include "../fn_math/fn_vec3.h"
#include "../fn_math/fn_vec4.h"
#include "../fn_math/fn_common.h"
#include "th_allocator.h"


#define TH_HARMONICS_MAX_COUNT 1200000

typedef struct
{
  fn_vec3 c0;
  fn_vec3 c1;
  fn_vec3 c2;
  fn_vec3 c3;
  fn_vec3 c4;
  fn_vec3 c5;
  fn_vec3 c6;
  fn_vec3 c7;
  fn_vec3 c8;
}th_Harmonic;

GLuint th_createLightTexture(th_Harmonic* harmonics,int count,fn_vec2* outdims);
th_Harmonic th_CubemaptoHarmonic(GLfloat** faces,int dimension);
th_Harmonic* th_blankHarmonics(th_Allocator* alloc,int* count,fn_vec3 griddims);
th_Harmonic* th_loadHarmonicsBinary(th_Allocator* alloc,char* filename, int* count,fn_vec3 griddims,fn_vec3 gridpos,float gridsize);
void th_exportHarmonicsBinary(th_Harmonic* harmonics,int count,const char* filename);


void th_CuebmapToHarmonicsBatch(th_Harmonic* harmonics,int batch_counter,int index_start,GLfloat* faces,int dimension);
