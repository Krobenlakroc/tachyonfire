#include "th_occlusion.h"
#include <string.h>

static const int max_occluders = 2048;
static fn_vec4* new_occluders = NULL;
static float* squared_distances = NULL;

static int occluder_idx = 0;

static fn_vec3 eyepos_frame;
static fn_vec4* frustum_planes_frame = NULL;

static int fn_sphereInFrustum( fn_vec3 pos, float radius ,fn_vec4 frustum [6])
{
    int p;

    bool res = true;

    for( p = 0; p < 6; p++ )
    {

        if (frustum[p].x * pos.x + frustum[p].y * pos.y + frustum[p].z * pos.z + frustum[p].w <= -radius)
        {
            res = false;
            p = 6;
        }

    }


    if (!res)
        return 0;
    return 1;//d + radius;
}

void th_initOcclusion()
{
    squared_distances = malloc(sizeof(float)*max_occluders);
    new_occluders = malloc(sizeof(fn_vec4)*max_occluders);
    occluder_idx = 0;

    frustum_planes_frame = malloc(sizeof(fn_vec4)*6);
    frustum_planes_frame[0] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[1] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[2] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[3] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[4] = fn_createVec4(0,0,0,0);
    frustum_planes_frame[5] = fn_createVec4(0,0,0,0);
    eyepos_frame = fn_createVec3(0,0,0);
}


void th_beginOccluderFrame(fn_vec3 eyepos,fn_vec4* frustum_planes)
{
    occluder_idx = 0;
    eyepos_frame = eyepos;
    memcpy(frustum_planes_frame,frustum_planes,sizeof(fn_vec4)*6);
    for (int i = 0 ; i < max_occluders;i++)
    {
        new_occluders[i] = fn_createVec4(0,0,0,0);
    }

}

static void heapifyUp(int index)
{
    while (index > 0){
        int parentIndex = (index - 1) / 2;
        if (squared_distances[index] <= squared_distances[parentIndex])
        {
            break;
        }
        float temp_dist;
        fn_vec4 temp_occluder;
        temp_dist = squared_distances[index];
        squared_distances[index] = squared_distances[parentIndex];
        squared_distances[parentIndex] = temp_dist;

        temp_occluder = new_occluders[index];
        new_occluders[index] = new_occluders[parentIndex];
        new_occluders[parentIndex] = temp_occluder;

        index = parentIndex;
    }

}

static void heapifyDown(int index)
{
    while (true)
    {
        int leftChild  = 2 * index + 1;
        int rightChild = 2 * index + 2;
        int largest = index;

        if (leftChild < occluder_idx && squared_distances[leftChild] > squared_distances[largest])
        {
            largest = leftChild;
        }

        if (rightChild < occluder_idx && squared_distances[rightChild] > squared_distances[largest])
        {
            largest = rightChild;
        }


        if (largest == index)
        {
            break;
        }

        float temp_dist;
        fn_vec4 temp_occluder;
        temp_dist = squared_distances[index];
        squared_distances[index] = squared_distances[largest];
        squared_distances[largest] = temp_dist;

        temp_occluder = new_occluders[index];
        new_occluders[index] = new_occluders[largest];
        new_occluders[largest] = temp_occluder;

        index = largest;
    }

}

void th_pushOccluderFrame(fn_vec4 occluder)
{
    if (!fn_sphereInFrustum(occluder.xyz,occluder.w,frustum_planes_frame))
    {
        return;
    }

    float squared_dist = fn_distance2(occluder.xyz,eyepos_frame);

    if (occluder_idx < max_occluders)
    {
        new_occluders[occluder_idx] = occluder;
        squared_distances[occluder_idx] = squared_dist;
        heapifyUp(occluder_idx);
        occluder_idx++;
    }
    else if (squared_dist < squared_distances[0])
    {
        new_occluders[0] = occluder;
        squared_distances[0] = squared_dist;
        heapifyDown(0);
    }
}

void th_getOccluders(fn_vec4* dest,int* count)
{
    *count = occluder_idx;
    memcpy(dest,new_occluders,sizeof(fn_vec4)*max_occluders);
}

