#pragma once

#include "../fn_gl.h"
#include "r_vertex.h"
#include "th_gpu.h"
#include "th_allocator.h"

typedef struct
{
    float height;
    float flow[4];
}th_SimCell;

typedef struct
{
    th_SimCell* grid;
    int dim_w;
    int dim_h;
    float* smoothed;
    float* smoothed_temp;
}th_LiquidHeights;


void th_initLiquidSim(th_Allocator *alloc,th_LiquidHeights *heights);

void th_updateLiquidSim(th_LiquidHeights *heights,float dt);

void th_addNoiseLiquidSim(th_LiquidHeights *s,float strength);

void th_addCentalLiftLiquidSim(th_LiquidHeights *s,float strength,int rad);


th_GpuData th_generateLiquidMesh(th_Allocator          *alloc,
                         th_LiquidHeights *heights,fn_mat4 transform);



