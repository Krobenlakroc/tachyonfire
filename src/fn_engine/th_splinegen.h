#pragma once

#include "../fn_gl.h"
#include "r_vertex.h"
#include "th_gpu.h"
#include "th_allocator.h"


typedef struct {
    fn_vec3 position;
    fn_vec3 forward;
    fn_vec3 right;
    fn_vec3 up;
    double  arc_length;
}th_SplineFrame;

typedef struct {
    fn_vec3 position;
    fn_vec3 bias_dir;
    float   wire_length_to_next;
} th_WireKeypoint;

// th_GpuData th_generateWire(th_Allocator* alloc,fn_vec3 start, fn_vec3 end,
//                      float wire_length,
//                      fn_vec3 bias_dir,
//                      int n_points,float radius);


th_GpuData th_generateWire(th_Allocator          *alloc,
                           const th_WireKeypoint *keypoints,
                           int                    n_keypoints,
                           fn_vec3                tangent_start,
                           fn_vec3                tangent_end,
                           int                    points_per_seg,
                           float                  radius,
                           float alpha,
                           th_SplineFrame* out_frame);


th_GpuData* th_generateWireBundle(th_Allocator* alloc,fn_vec3* starts,fn_vec3* mids,fn_vec3* mid2s,fn_vec3* ends,int n_wires,fn_vec3 tangent_start,fn_vec3 tangent_end,fn_vec3 bias_dir,float alpha,th_SplineFrame* out_frames);
