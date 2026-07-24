#include "../fn_math/fn_math.h"

#include "th_splinegen.h"
#include <string.h>


static fn_vec3 vec3_reflect(fn_vec3 v, fn_vec3 n)
{
    double k = 2.0 * fn_dot(v, n) / fn_dot(n, n);
    return fn_subVec3(v, fn_multVec3s(n, k));
}




typedef struct {
    fn_vec3 p[4];
} CubicBSpline;


static fn_vec3 bspline_eval(const CubicBSpline *s, double t)
{
    double u  = 1.0 - t;
    double b0 = u*u*u;
    double b1 = 3.0*t*u*u;
    double b2 = 3.0*t*t*u;
    double b3 = t*t*t;
    return fn_createVec3(
        b0*s->p[0].x + b1*s->p[1].x + b2*s->p[2].x + b3*s->p[3].x,
        b0*s->p[0].y + b1*s->p[1].y + b2*s->p[2].y + b3*s->p[3].y,
        b0*s->p[0].z + b1*s->p[1].z + b2*s->p[2].z + b3*s->p[3].z);
}

static fn_vec3 bspline_deriv(const CubicBSpline *s, double t)
{
    double u   = 1.0 - t;
    double db0 = -3.0*u*u;
    double db1 =  3.0*u*u - 6.0*t*u;
    double db2 =  6.0*t*u - 3.0*t*t;
    double db3 =  3.0*t*t;
    return fn_createVec3(
        db0*s->p[0].x + db1*s->p[1].x + db2*s->p[2].x + db3*s->p[3].x,
        db0*s->p[0].y + db1*s->p[1].y + db2*s->p[2].y + db3*s->p[3].y,
        db0*s->p[0].z + db1*s->p[1].z + db2*s->p[2].z + db3*s->p[3].z);
}

static const double GL_NODES[8] = {
    -0.9602898564975363, -0.7966664774136267,
    -0.5255324099163290, -0.1834346424956498,
    0.1834346424956498,  0.5255324099163290,
    0.7966664774136267,  0.9602898564975363
};
static const double GL_WEIGHTS[8] = {
    0.1012285362903763,  0.2223810344533745,
    0.3137066458778873,  0.3626837833783620,
    0.3626837833783620,  0.3137066458778873,
    0.2223810344533745,  0.1012285362903763
};

static double arc_length_segment(const CubicBSpline *s, double a, double b)
{
    double half = 0.5*(b-a), mid = 0.5*(b+a), sum = 0.0;
    for (int i = 0; i < 8; i++) {
        fn_vec3 d = bspline_deriv(s, mid + half*GL_NODES[i]);
        sum += GL_WEIGHTS[i] * fn_length(d);
    }
    return half * sum;
}

static double arc_length_full(const CubicBSpline *s, int n_seg)
{
    double total = 0.0, dt = 1.0/n_seg;
    for (int i = 0; i < n_seg; i++)
        total += arc_length_segment(s, i*dt, (i+1)*dt);
    return total;
}


static CubicBSpline build_spline_hermite(fn_vec3 start,     fn_vec3 end,
                                         fn_vec3 tan_start, fn_vec3 tan_end,
                                         fn_vec3 bias_dir,  double  bias_offset)
{
    CubicBSpline s;
    fn_vec3 bias = fn_multVec3s(bias_dir, bias_offset);
    s.p[0] = start;
    s.p[1] = fn_addVec3(fn_addVec3(start, fn_multVec3s(tan_start, 1.0/3.0)), bias);
    s.p[2] = fn_addVec3(fn_subVec3(end,   fn_multVec3s(tan_end,   1.0/3.0)), bias);
    s.p[3] = end;
    return s;
}

#define ARC_SEGMENTS  64
#define BISECT_ITERS  64
#define BISECT_TOL    1e-9


static double solve_bias_offset(fn_vec3 start,     fn_vec3 end,
                                fn_vec3 tan_start, fn_vec3 tan_end,
                                fn_vec3 bias_dir,  double  desired_length)
{
    double straight = fn_length(fn_subVec3(end, start));
    if (desired_length <= straight + 1e-10) return 0.0;

    /* Check arc length with zero bias as the lower bound */
    double upper = (straight > 1e-6) ? straight : 1.0;
    for (int i = 0; i < 60; i++) {
        CubicBSpline tmp = build_spline_hermite(start, end,
                                                tan_start, tan_end,
                                                bias_dir, upper);
        if (arc_length_full(&tmp, ARC_SEGMENTS) >= desired_length) break;
        upper *= 2.0;
    }

    double lo = 0.0, hi = upper;
    for (int i = 0; i < BISECT_ITERS; i++) {
        double mid = 0.5*(lo + hi);
        CubicBSpline tmp = build_spline_hermite(start, end,
                                                tan_start, tan_end,
                                                bias_dir, mid);
        double L = arc_length_full(&tmp, ARC_SEGMENTS);
        if (fabs(L - desired_length) < BISECT_TOL) return mid;
        if (L < desired_length) lo = mid; else hi = mid;
    }
    return 0.5*(lo + hi);
}


static fn_vec3 rmf_propagate_right(fn_vec3 ri,  fn_vec3 xi,  fn_vec3 ti,
                                   fn_vec3 xi1, fn_vec3 ti1)
{
    fn_vec3 v1 = fn_subVec3(xi1, xi);
    fn_vec3 rL, tL;
    if (fn_dot(v1, v1) > 1e-28) {
        rL = vec3_reflect(ri, v1);
        tL = vec3_reflect(ti, v1);
    } else {
        rL = ri;
        tL = ti;
    }
    fn_vec3 v2  = fn_subVec3(ti1, tL);
    fn_vec3 ri1 = (fn_dot(v2, v2) > 1e-28) ? vec3_reflect(rL, v2) : rL;
    return fn_normalizeVec3(ri1);
}


typedef struct {
    th_SplineFrame *frames;
    int          n_points;        /* total frames across all segments */
    double       computed_length; /* total arc length                 */
} MultiSegmentResult;



static fn_vec3 interior_tangent(fn_vec3 prev, fn_vec3 curr, fn_vec3 next)
{
    fn_vec3 d_in  = fn_normalizeVec3(fn_subVec3(curr, prev));
    fn_vec3 d_out = fn_normalizeVec3(fn_subVec3(next, curr));
    return fn_normalizeVec3(fn_addVec3(d_in, d_out));
}


static fn_vec3 resolve_bias(fn_vec3 bias_dir, fn_vec3 start, fn_vec3 end)
{
    fn_vec3 bias = fn_normalizeVec3(bias_dir);
    if (fn_length(bias) < 0.5) {
        fn_vec3 chord_n = fn_normalizeVec3(fn_subVec3(end, start));
        fn_vec3 ref = (fabs(fn_dot(chord_n, fn_createVec3(0,0,1))) > 0.9)
        ? fn_createVec3(0,1,0) : fn_createVec3(0,0,1);
        bias = fn_normalizeVec3(fn_cross(chord_n, ref));
    }
    return bias;
}



static MultiSegmentResult generate_wire_frames(
    th_Allocator      *alloc,
    const th_WireKeypoint *keypoints,
    int                n_keypoints,
    fn_vec3            tangent_start,
    fn_vec3            tangent_end,
    int                points_per_seg,
    float alpha)
{

    fn_vec3 *pos = th_alloc(alloc, points_per_seg * sizeof(fn_vec3));
    fn_vec3 *tan = th_alloc(alloc, points_per_seg * sizeof(fn_vec3));
    double  *sal = th_alloc(alloc, points_per_seg * sizeof(double));

    MultiSegmentResult result;
    memset(&result, 0, sizeof(result));

    int n_segs = n_keypoints - 1;
    if (n_segs < 1 || points_per_seg < 2) return result;

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    double global_t_max = alpha * (double)n_segs;


    int total_points = points_per_seg + (n_segs - 1) * (points_per_seg - 1);

    th_SplineFrame *frames = th_alloc(alloc, total_points * sizeof(th_SplineFrame));
    memset(frames, 0, total_points * sizeof(th_SplineFrame));


    fn_vec3 *seg_dirs = th_alloc(alloc, n_keypoints * sizeof(fn_vec3));
    seg_dirs[0]              = fn_normalizeVec3(tangent_start);
    seg_dirs[n_keypoints-1]  = fn_normalizeVec3(tangent_end);
    for (int i = 1; i < n_keypoints - 1; i++) {
        seg_dirs[i] = interior_tangent(keypoints[i-1].position,
                                       keypoints[i  ].position,
                                       keypoints[i+1].position);
    }


    fn_vec3 first_bias = resolve_bias(keypoints[0].bias_dir,
                                      keypoints[0].position,
                                      keypoints[1].position);


    int frame_cursor = 0;
    double cumulative_arc = 0.0;



    for (int seg_ext = 0; seg_ext < n_segs; seg_ext++) {
        int seg = seg_ext;

        double t_hi = global_t_max - (double)seg;
        if (t_hi < 0.0) t_hi = 0.0;
        if (t_hi > 1.0) t_hi = 1.0;

        if ((double)seg >= global_t_max)
        {
            if (frame_cursor == 0) {
                fn_vec3 fwd     = fn_normalizeVec3(tangent_start);
                fn_vec3 up_hint = fn_subVec3(first_bias,
                                             fn_multVec3s(fwd, fn_dot(first_bias, fwd)));
                if (fn_length(up_hint) < 1e-6) {
                    fn_vec3 ref = (fabs(fn_dot(fwd, fn_createVec3(0,0,1))) > 0.9)
                    ? fn_createVec3(0,1,0) : fn_createVec3(0,0,1);
                    up_hint = fn_subVec3(ref, fn_multVec3s(fwd, fn_dot(ref, fwd)));
                }
                up_hint = fn_normalizeVec3(up_hint);
                frames[0].position   = keypoints[0].position;
                frames[0].forward    = fwd;
                frames[0].right      = fn_normalizeVec3(fn_cross(up_hint, fwd));
                frames[0].up         = fn_cross(fwd, frames[0].right);
                frames[0].arc_length = 0.0;
                frame_cursor = 1;
            }
            int start_i = (seg == 0) ? 1 : 1; /* always skip index 0 after seeding */
            for (int i = start_i; i < points_per_seg; i++) {
                frames[frame_cursor] = frames[frame_cursor - 1];
                frame_cursor++;
            }
            continue;
        }

        fn_vec3 p0 = keypoints[seg  ].position;
        fn_vec3 p1 = keypoints[seg+1].position;
        fn_vec3 d0 = seg_dirs[seg  ];
        fn_vec3 d1 = seg_dirs[seg+1];

        fn_vec3 bias = resolve_bias(keypoints[seg].bias_dir, p0, p1);
        double straight = fn_length(fn_subVec3(p1, p0));

        fn_vec3 tan_start = fn_multVec3s(d0, straight);
        fn_vec3 tan_end   = fn_multVec3s(d1, straight);

        /* Clamp wire length for this segment */
        double seg_wire_len = keypoints[seg].wire_length_to_next;
        if (seg_wire_len < straight) seg_wire_len = straight;

        /* Solve only the perpendicular bias offset to match the arc length */
        double bias_offset = solve_bias_offset(p0, p1,
                                               tan_start, tan_end,
                                               bias, seg_wire_len);

        CubicBSpline spline = build_spline_hermite(p0, p1,
                                                   tan_start, tan_end,
                                                   bias, bias_offset);





        int start_i = (seg == 0) ? 0 : 1;



        sal[0] = cumulative_arc;
        for (int i = 0; i < points_per_seg; i++) {
            double t = (double)i / (double)(points_per_seg - 1);

            if (t > t_hi) t = t_hi;   /* clamp to tip */

            pos[i]   = bspline_eval (&spline, t);
            tan[i]   = fn_normalizeVec3(bspline_deriv(&spline, t));
        }
        for (int i = 1; i < points_per_seg; i++) {
            double ta = (double)(i-1) / (double)(points_per_seg - 1);
            double tb = (double)  i   / (double)(points_per_seg - 1);

            if (ta > t_hi) ta = t_hi;
            if (tb > t_hi) tb = t_hi;

            sal[i] = sal[i-1] + arc_length_segment(&spline, ta, tb);
        }


        if (seg == 0) {
            fn_vec3 fwd      = tan[0];
            fn_vec3 up_hint  = fn_subVec3(first_bias,
                                          fn_multVec3s(fwd, fn_dot(first_bias, fwd)));
            if (fn_length(up_hint) < 1e-6) {
                fn_vec3 ref = (fabs(fn_dot(fwd, fn_createVec3(0,0,1))) > 0.9)
                ? fn_createVec3(0,1,0) : fn_createVec3(0,0,1);
                up_hint = fn_subVec3(ref, fn_multVec3s(fwd, fn_dot(ref, fwd)));
            }
            up_hint = fn_normalizeVec3(up_hint);

            frames[0].position   = pos[0];
            frames[0].forward    = fwd;
            frames[0].right      = fn_normalizeVec3(fn_cross(up_hint, fwd));
            frames[0].up         = fn_cross(fwd, frames[0].right);
            frames[0].arc_length = sal[0];
            frame_cursor = 1;
            start_i = 1;
        }


        for (int i = start_i; i < points_per_seg; i++) {
            int fc = frame_cursor;
            int fp = fc - 1;
            frames[fc].position   = pos[i];
            frames[fc].forward    = tan[i];
            frames[fc].arc_length = sal[i];
            frames[fc].right      = rmf_propagate_right(
                frames[fp].right,
                frames[fp].position, frames[fp].forward,
                pos[i],              tan[i]);
            frames[fc].up = fn_cross(frames[fc].forward, frames[fc].right);
            frame_cursor++;
        }


        cumulative_arc = sal[points_per_seg - 1];
    }

    result.frames          = frames;
    result.n_points        = frame_cursor;
    result.computed_length = cumulative_arc;
    return result;
}


th_GpuData th_generateWire(th_Allocator          *alloc,
                           const th_WireKeypoint *keypoints,
                           int                    n_keypoints,
                           fn_vec3                tangent_start,
                           fn_vec3                tangent_end,
                           int                    points_per_seg,
                           float                  radius,
                           float alpha,
                           th_SplineFrame* out_frame)
{
    const float len_per_uv  = 500.0f;
    const float rads_per_uv = 5.0f;
    const int   ring_res    = 16;

    MultiSegmentResult spline = generate_wire_frames(alloc,
                                                     keypoints, n_keypoints,
                                                     tangent_start, tangent_end,
                                                     points_per_seg,alpha);


    if (out_frame != NULL)
    {
        *out_frame = spline.frames[spline.n_points - 1] ;
    }

    int n_points = spline.n_points;
    int verts    = n_points * ring_res;
    int inds     = (n_points - 1) * ring_res * 2 * 3;

    th_Vertex *data    = th_alloc(alloc, verts * sizeof(th_Vertex));
    GLuint    *indices = th_alloc(alloc, inds  * sizeof(GLuint));


    for (int i = 0; i < n_points; i++) {
        //fn_printVec3(spline.frames[i].right);
        for (int j = 0; j < ring_res; j++) {
            double theta = (double)j / (double)(ring_res - 1) * 2.0 * 3.141596;

            data[i*ring_res + j].texCoord =
            fn_createVec2((float)(spline.frames[i].arc_length / len_per_uv),
                          (float)(theta / rads_per_uv));

            fn_vec3 base            = spline.frames[i].position;

            fn_vec3 right_component = fn_multVec3s(spline.frames[i].right, cos(theta) * radius);
            fn_vec3 up_component    = fn_multVec3s(spline.frames[i].up,    sin(theta) * radius);

            data[i*ring_res + j].position  = fn_addVec3(base, fn_addVec3(right_component, up_component));
            data[i*ring_res + j].normal    = fn_normalizeVec3(fn_addVec3(right_component, up_component));
            data[i*ring_res + j].tangent   = spline.frames[i].forward;
            data[i*ring_res + j].bitangent = fn_cross(data[i*ring_res + j].tangent,
                                                      data[i*ring_res + j].normal);
        }
    }


    int indxs = 0;
    for (int i = 0; i < n_points - 1; i++) {
        for (int j = 0; j < ring_res; j++) {
            int j1 = (j + 1) % ring_res;
            indices[indxs++] = i     * ring_res + j;
            indices[indxs++] = i     * ring_res + j1;
            indices[indxs++] = (i+1) * ring_res + j1;

            indices[indxs++] = i     * ring_res + j;
            indices[indxs++] = (i+1) * ring_res + j1;
            indices[indxs++] = (i+1) * ring_res + j;
        }
    }

    th_GpuData ret;
    ret.verts         = data;
    ret.indices       = indices;
    ret.instances     = th_alloc(alloc, sizeof(fn_mat4));
    ret.instances[0]  = fn_identityMat4();
    ret.vertcount     = verts;
    ret.indicecount   = indxs;
    ret.instancecount = 1;
    return ret;
}


th_GpuData* th_generateWireBundle(th_Allocator* alloc,fn_vec3* starts,fn_vec3* mids,fn_vec3* mid2s,fn_vec3* ends,int n_wires,fn_vec3 tangent_start,fn_vec3 tangent_end,fn_vec3 bias_dir,float alpha,th_SplineFrame* out_frames)
{
    th_GpuData* ret = th_alloc(alloc,sizeof(th_GpuData)*n_wires);

    for (int i = 0 ; i < n_wires;i++)
    {
        //fn_printVec3(starts[i]);
        th_WireKeypoint wire_keys[4];
        wire_keys[0].position = starts[i];
        wire_keys[0].bias_dir = bias_dir;
        wire_keys[0].wire_length_to_next = 1;

        wire_keys[1].position = mids[i];
        wire_keys[1].bias_dir = bias_dir;
        wire_keys[1].wire_length_to_next = 1;

        wire_keys[2].position = mid2s[i];
        wire_keys[2].bias_dir = bias_dir;
        wire_keys[2].wire_length_to_next = 1;

        wire_keys[3].position = ends[i];
        wire_keys[3].bias_dir = bias_dir;
        wire_keys[3].wire_length_to_next = 1;

        th_SplineFrame* sp_ptr = NULL;
        if (out_frames != NULL)
        {
            sp_ptr = &out_frames[i];
        }

        th_GpuData wire_data = th_generateWire(alloc,wire_keys,4,tangent_start,tangent_end,8,14,alpha,sp_ptr);

        ret[i] = wire_data;
    }

    return ret;
}
