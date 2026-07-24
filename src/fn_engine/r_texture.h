#pragma once
#include "../fn_math/fn_vec2.h"
#include "../fn_gl.h"
#include <stdbool.h>
#include "th_allocator.h"

//used to be 5
//albedometal BC7, normal BC5, roughness BC4, optionally displacement/alpha mask BC4
#define TEX_PER_MATERIAL 4

//engine limit for tex groups
#define TEX_GROUP_IMG_SIZE 55

void r_allocateTextureHandles();

GLuint fn_loadTexture(const char* file);
GLuint fn_loadTextureArray(const char** files,int numfiles);
GLuint fn_loadWatermark(unsigned int width,unsigned int height,GLubyte* data);
void th_loadMaterials(char** files,int filecount);
void th_unloadMaterials();


fn_vec2 th_getMaterialHandle(int index);

void r_markTextureRemapping(int texID,GLuint* remapping);
GLuint* r_isTextureRemapped(int texID);

void r_bindTexture(GLuint texture,GLenum type,int unit);
void r_bindTextureForce(GLuint texture,GLenum type,int unit);
void r_resetTextureSubSystem();

int r_getTextureUnit();
char* th_getTextureFromMtl(const char* file);
