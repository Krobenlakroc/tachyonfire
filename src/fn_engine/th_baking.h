#pragma once
#include "th_gpu.h"
#include "th_renderer.h"

typedef struct
{
    GLfloat* wad_data;
    GLfloat** color_data;
    GLfloat* minmaxdata;
    int cube_count;
}th_CubemapGenData;


th_CubemapGenData th_generateCubemaps(int resolution,SDL_Window* window,th_RendererState* state);
void th_generateHarmonics(int resolution,SDL_Window* window,char* wadlocation,th_RendererState* state);
