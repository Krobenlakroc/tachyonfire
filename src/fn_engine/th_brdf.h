#pragma once
#include "../fn_gl.h"
#include "../fn_math/fn_vec2.h"
#include "../fn_math/fn_vec3.h"
#include "../fn_math/fn_mat4.h"
#include "../fn_math/fn_common.h"

void th_loadLUT(int dimension,const char* filename,GLuint* brdfLUTTexture);

void th_generateBRDFLUT(GLuint* brdfLUTTexture);
