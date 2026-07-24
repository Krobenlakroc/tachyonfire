#include "th_collision.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include "th_threads.h"
#include "th_system.h"
#include "th_time.h"
//
#define SURFACE_CLIP_EPSILON 0.03125f
#define EPSILON 0

#include <stdint.h>
#include <immintrin.h> // for SSE4.1; safe even if you only compile with SSE2 (see guards below)
#include <stdalign.h>

//#define CULL_SCALAR

th_Plane th_makePlane(fn_vec3 p,fn_vec3 n)
{
  th_Plane pl;
  pl.position = p;
  pl.normal = n;
  return pl;
}

float th_signedDistanceToPlane(fn_vec3 p,th_Plane* plane)
{
  return fn_dot(p,plane->normal) - fn_dot(plane->position,plane->normal);
}

th_Plane th_makePlaneTriangle(fn_vec3 a,fn_vec3 b,fn_vec3 c)
{
  fn_vec3 U = fn_subVec3(b , a);
  fn_vec3 V = fn_subVec3(c , a);

  fn_vec3 normal;
  normal.x = (U.y * V.z) - (U.z * V.y);
  normal.y = (U.z * V.x) - (U.x * V.z);
  normal.z = (U.x * V.y) - (U.y * V.x);

  th_Plane p;
  p.normal = fn_normalizeVec3(normal);
  p.position = a;
  return p;
}


// static int fn_sphereInFrustum( fn_vec3 pos, float radius ,fn_vec4 frustum [6])
// {
//    int p;
//
//    bool res = true;
//
//    for( p = 0; p < 6; p++ )
//    {
//
//       if (frustum[p].x * pos.x + frustum[p].y * pos.y + frustum[p].z * pos.z + frustum[p].w <= -radius)
//       {
//         res = false;
//         p = 6;
//       }
//
//     }
//
//
//   if (!res)
//       return 0;
//    return 1;//d + radius;
// }

static int fn_aabbInFrustum(fn_vec3 mins,fn_vec3 maxs,fn_vec4 frustum [6])
{
  fn_vec3 vmax;
  for( int i = 0; i < 6; i++ )
  {
    // X axis
    if(frustum[i].x > 0)
    {
      vmax.x = maxs.x;
    }
    else
    {
      vmax.x = mins.x;
    }

    // Y axis
    if(frustum[i].y > 0)
    {
      vmax.y = maxs.y;
    }
    else
    {
      vmax.y = mins.y;
    }

    // Z axis
    if(frustum[i].z > 0)
    {
      vmax.z = maxs.z;
    }
    else
    {
      vmax.z = mins.z;
    }

    if (fn_dot(vmax,frustum[i].xyz) + frustum[i].w < 0)
    {
      return 0;
    }

  }

  return 1;
}


#ifndef CULL_SCALAR
typedef struct {
  __m256 nx, ny, nz, w;  // plane components broadcasted
  __m256i selMask;        // bit0=x, bit1=y, bit2=z; 0=use mins, 1=maxs (broadcasted to 8 lanes)
} fn_plane_avx;

static inline void fn_prepare_frustum_avx(const fn_vec4 frustum[6], fn_plane_avx out[6])
{
  for (int i = 0; i < 6; ++i) {
    out[i].nx = _mm256_set1_ps(frustum[i].x);
    out[i].ny = _mm256_set1_ps(frustum[i].y);
    out[i].nz = _mm256_set1_ps(frustum[i].z);
    out[i].w  = _mm256_set1_ps(frustum[i].w);

    uint8_t mask = 0;
    mask |= (frustum[i].x >= 0.0f) ? 1 : 0;
    mask |= (frustum[i].y >= 0.0f) ? 2 : 0;
    mask |= (frustum[i].z >= 0.0f) ? 4 : 0;

    // replicate 8 lanes
    out[i].selMask = _mm256_set1_epi32(mask);
  }
}

// Vectorized 8-AABB test
static void fn_aabbInFrustum_avx8(const float *min_x, const float *min_y, const float *min_z,
                                  const float *max_x, const float *max_y, const float *max_z,
                                  int aabbStart, int aabbCount,
                                  const fn_plane_avx planes[6],
                                  char *visibility,bool* skip_culling_flag)
{
  #define simdWidth 8

  int i;
  for (i = 0; i <= aabbCount - simdWidth; i += simdWidth) {
    // Load 8 mins/maxs
    __m256 vx_min = _mm256_loadu_ps(min_x + aabbStart + i);
    __m256 vy_min = _mm256_loadu_ps(min_y + aabbStart + i);
    __m256 vz_min = _mm256_loadu_ps(min_z + aabbStart + i);

    __m256 vx_max = _mm256_loadu_ps(max_x + aabbStart + i);
    __m256 vy_max = _mm256_loadu_ps(max_y + aabbStart + i);
    __m256 vz_max = _mm256_loadu_ps(max_z + aabbStart + i);

    //__m256 mask_inside = _mm256_castsi256_ps(_mm256_set1_epi32(-1)); // all inside initially
    __m256i mask_inside = _mm256_set1_epi32(-1);

    // __m128i flags8  = _mm_loadl_epi64((const __m128i*)&skip_culling_flag[aabbStart + i]); // load 8 bytes
    __m128i tmp;
    memcpy(&tmp, &skip_culling_flag[aabbStart + i], 8);
    __m128i flags8 = tmp;

    __m256i flags32 = _mm256_cvtepu8_epi32(flags8);                // widen to 8x int32
    // Convert flags into an AVX mask (nonzero = skip)
    __m256i skip_mask = _mm256_cmpeq_epi32(flags32, _mm256_setzero_si256());
    // skip_mask = 1 if skip_flag==0, else 0



    // Test 6 planes
    for (int p = 0; p < 6; ++p) {
      __m256i sel = planes[p].selMask;

      // Branchless select mins/maxs using mask
      //__m256 vx = _mm256_blendv_ps(vx_min, vx_max, _mm256_castsi256_ps(_mm256_and_si256(sel, _mm256_set1_epi32(1))));

      __m256i vx_mask = _mm256_cmpgt_epi32(_mm256_and_si256(sel, _mm256_set1_epi32(1)), _mm256_setzero_si256());
      __m256 vx = _mm256_blendv_ps(vx_min, vx_max, _mm256_castsi256_ps(vx_mask));

      __m256i vy_mask = _mm256_cmpgt_epi32(_mm256_and_si256(sel, _mm256_set1_epi32(2)), _mm256_setzero_si256());
      __m256 vy = _mm256_blendv_ps(vy_min, vy_max, _mm256_castsi256_ps(vy_mask));

      __m256i vz_mask = _mm256_cmpgt_epi32(_mm256_and_si256(sel, _mm256_set1_epi32(4)), _mm256_setzero_si256());
      __m256 vz = _mm256_blendv_ps(vz_min, vz_max, _mm256_castsi256_ps(vz_mask));


      // Dot product
      __m256 dp = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(vx, planes[p].nx),
                                              _mm256_mul_ps(vy, planes[p].ny)),
                                _mm256_mul_ps(vz, planes[p].nz));
      dp = _mm256_add_ps(dp, planes[p].w);

      // // Outside mask: dp < 0
      // __m256 outside = _mm256_cmp_ps(dp, _mm256_setzero_ps(), _CMP_LT_OQ);
      // mask_inside = _mm256_andnot_ps(outside, mask_inside); // zero-out boxes outside this plane
      __m256 outside_f = _mm256_cmp_ps(dp, _mm256_setzero_ps(), _CMP_LT_OQ);
      __m256i outside  = _mm256_castps_si256(outside_f);
      mask_inside = _mm256_andnot_si256(outside, mask_inside);
    }



    __m256i final_mask = _mm256_and_si256(mask_inside, skip_mask);

    // Store results (as ints)
    alignas(32) int result[simdWidth];
    _mm256_storeu_si256((__m256i*)result, final_mask);

    for (int j = 0; j < simdWidth; ++j) {
      visibility[aabbStart + i + j] = (result[j] != 0);
    }
  }

  // Handle remaining AABBs scalar
  for (; i < aabbCount; ++i) {
    int vis = 1;
    float vx_min = min_x[aabbStart + i], vy_min = min_y[aabbStart + i], vz_min = min_z[aabbStart + i];
    float vx_max = max_x[aabbStart + i], vy_max = max_y[aabbStart + i], vz_max = max_z[aabbStart + i];
    for (int p = 0; p < 6; ++p) {
      uint8_t sel = _mm256_extract_epi32(planes[p].selMask, 0);
      float vx = (sel & 1) ? vx_max : vx_min;
      float vy = (sel & 2) ? vy_max : vy_min;
      float vz = (sel & 4) ? vz_max : vz_min;
      float dp = vx*planes[p].nx[0] + vy*planes[p].ny[0] + vz*planes[p].nz[0] + planes[p].w[0];
      if (dp < 0.0f) { vis = 0; break; }
    }
    visibility[aabbStart + i] = vis && !skip_culling_flag[aabbStart + i];
  }

  #undef simdWidth
}
#endif


typedef struct
{
  int start;
  int range;
  th_FrustumCullData* fdata;
  fn_vec4* frustumPlanes;
  char* visibility;
#ifndef CULL_SCALAR
  const fn_plane_avx* avx_plane;
#endif
}th_threadFrustumData;

void thread_frustumcull(void* data)
{
  th_threadFrustumData* dat = (th_threadFrustumData*)data;
  th_FrustumCullData* fdata = dat->fdata;
  fn_vec4* frustumPlanes = dat->frustumPlanes;
  char* visibility = dat->visibility;


#ifdef CULL_SCALAR

  for (int i = dat->start ; i < dat->start + dat->range;i++)
  {
    bool cull = false;

    if (fdata->skip_culling_flag[i])
    {
      visibility[i] = 0;
      continue;
    }

    visibility[i] = fn_aabbInFrustum(fn_createVec3(fdata->min_x[i],fdata->min_y[i],fdata->min_z[i]),fn_createVec3(fdata->max_x[i],fdata->max_y[i],fdata->max_z[i]),frustumPlanes);

    // fn_vec3 mins = fn_createVec3(fdata->min_x[i], fdata->min_y[i], fdata->min_z[i]);
    // fn_vec3 maxs = fn_createVec3(fdata->max_x[i], fdata->max_y[i], fdata->max_z[i]);
    //
    // visibility[i] = fn_aabbInFrustum_fast(mins, maxs, pre);
  }
#else

  fn_aabbInFrustum_avx8(fdata->min_x,fdata->min_y,fdata->min_z,fdata->max_x,fdata->max_y,fdata->max_z,dat->start,dat->range,dat->avx_plane,visibility,fdata->skip_culling_flag);
#endif

}


void th_frustumCull(th_FrustumCullData* fdata,char* visibility,fn_vec4* frustumPlanes)
{

  #ifndef CULL_SCALAR
  fn_plane_avx pre_avx[6];
  fn_prepare_frustum_avx(frustumPlanes, pre_avx);
#endif

  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_threadFrustumData,fdata->aabbCount)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].fdata = fdata;
  data[th_thread_id].frustumPlanes = frustumPlanes;
  data[th_thread_id].visibility = visibility;
#ifndef CULL_SCALAR
  data[th_thread_id].avx_plane = pre_avx;
#endif
  TH_SCHEDULING_FUNC
  th_setThread(thread_frustumcull,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING

}

th_AABB th_getBoundingBox(fn_vec3* points,int count,fn_mat4 mat)
{
  th_AABB ret;
  fn_vec3 min = fn_transformVec3(points[0],mat);
  fn_vec3 max = fn_transformVec3(points[0],mat);
  for (int i =0 ; i < count;i++)
  {
    fn_vec3 trpoint = fn_transformVec3(points[i],mat);
    if (trpoint.x > max.x)
    {
      max.x = trpoint.x;
    }
    if (trpoint.y > max.y)
    {
      max.y = trpoint.y;
    }
    if (trpoint.z > max.z)
    {
      max.z = trpoint.z;
    }

    if (trpoint.x < min.x)
    {
      min.x = trpoint.x;
    }
    if (trpoint.y < min.y)
    {
      min.y = trpoint.y;
    }
    if (trpoint.z < min.z)
    {
      min.z = trpoint.z;
    }
  }
  ret.position = fn_multVec3s(fn_addVec3(min,max),0.5);
  ret.hwidth = fn_multVec3s(fn_subVec3(max,min),0.5);
  return ret;
}



bool fn_aabbCheck(fn_AABB a,fn_AABB  b)
{
  return (a.position.x-a.hwidth.x < b.position.x+b.hwidth.x &&
    b.position.x-b.hwidth.x < a.position.x+a.hwidth.x &&
    a.position.y-a.hwidth.y < b.position.y+b.hwidth.y &&
    b.position.y-b.hwidth.y < a.position.y+a.hwidth.y &&
    a.position.z-a.hwidth.z < b.position.z+b.hwidth.z &&
    b.position.z-b.hwidth.z < a.position.z+a.hwidth.z);
}



fn_AABB fn_getSweptBroadphaseBox(fn_AABB b,fn_vec3 v)
{
  fn_AABB broadphasebox;
  broadphasebox.position = fn_addVec3(b.position,fn_multVec3s(v,0.5));
  //
  //
  // broadphasebox.hwidth = fn_addVec3(b.hwidth,fn_multVec3s(fn_abs(v),0.5));
  // broadphasebox.position.x = v.x > 0 ? b.position.x : b.position.x + v.x;
  // broadphasebox.position.y = v.y > 0 ? b.position.y : b.position.y + v.y;
  // broadphasebox.position.z = v.z > 0 ? b.position.z : b.position.z + v.z;
  // broadphasebox.hwidth.x = v.x > 0 ? v.x + b.hwidth.x : b.hwidth.x - v.x;
  // broadphasebox.hwidth.y = v.y > 0 ? v.y + b.hwidth.y : b.hwidth.y - v.y;
  // broadphasebox.hwidth.z = v.z > 0 ? v.z + b.hwidth.z : b.hwidth.z - v.z;
  // if (b.mode == CAPSULE)
  // {
  //   b.hwidth.y *= 2;
  // }
  broadphasebox.hwidth = fn_addVec3(b.hwidth,fn_multVec3s(fn_abs(v),0.5));
  return broadphasebox;
}



int check_point_in_triangle(const fn_vec3 point, const fn_vec3 p1, const fn_vec3 p2, const fn_vec3 p3)
{
  fn_vec3 u, v, w, vw, vu, uw, uv;
  u = fn_subVec3( p2, p1);
  v = fn_subVec3( p3, p1);
  w = fn_subVec3( point, p1);

  vw = fn_cross( v, w);
  vu = fn_cross( v, u);

  if (fn_dot(vw, vu) < 0.0f) {
    return 0;
  }

  uw = fn_cross( u, w);
  uv = fn_cross( u, v);

  if (fn_dot(uw, uv) < 0.0f) {
    return 0;
  }

  float d = fn_length(uv);
  float r = fn_length(vw) / d;
  float t = fn_length(uw) / d;

  return ((r + t) <= 1.0f);
}

int get_lowest_root(float a, float b, float c, float max, float *root)
{
  // check if solution exists
  float determinant = b*b - 4.0f*a*c;

  // if negative there is no solution
  if (determinant < 0.0f)
    return 0;

  // calculate two roots
  float sqrtD = sqrtf(determinant);
  float r1 = (-b - sqrtD) / (2.0f*a);
  float r2 = (-b + sqrtD) / (2.0f*a);

  // set x1 <= x2
  if (r1 > r2) {
    float temp = r2;
    r2 = r1;
    r1 = temp;
  }

  // get lowest root
  if (r1 > 0 && r1 < max) {
    *root = r1;
    return 1;
  }

  if (r2 > 0 && r2 < max) {
    *root = r2;
    return 1;
  }

  // no solutions
  return 0;
}



int is_front_facing(th_Plane *plane, const fn_vec3 direction)
{
  double f = fn_dot(plane->normal, direction);

  if (f <= 0.0)
    return 1;

  return 0;
}

void th_collideTriangle(th_CollisionPacket* packet,th_CollidableVolume* v)
{
  // edict.position = fn_multVec3s(edict.position,1.0/edict.hwidth.x);
  // delta = fn_multVec3s(delta,1.0/edict.hwidth.x);

  fn_vec3 linestart = packet->e_base_point;
  fn_vec3 lineend = fn_addVec3(packet->e_base_point,packet->e_velocity);
  fn_vec3 p1 = v->points[0];
  fn_vec3 p2 = v->points[1];
  fn_vec3 p3 = v->points[2];
  p1 = fn_divVec3(p1,packet->e_radius);
  p2 = fn_divVec3(p2,packet->e_radius);
  p3 = fn_divVec3(p3,packet->e_radius);
  th_Plane plane;// = v->planes[0];//= ex_triangle_to_plane(p1, p2, p3);
  fn_vec3 ps[3];
  ps[0] = p1;
  ps[1] = p2;
  ps[2] = p3;
  plane.normal = th_calculateSurfaceNormal(ps);
  plane.position = p1;



  // only check front facing triangles
  //packet->e_norm_velocity
  //is_front_facing(&plane,fn_normalizeVec3(fn_subVec3(p1,packet->e_base_point)) ) ||
  double signed_dist_to_plane =  fn_dot(packet->e_base_point,plane.normal) - fn_dot(plane.position,plane.normal);
  if (!packet->robust && !( is_front_facing(&plane,packet->e_norm_velocity )  ))
  {
    return;
    //plane.normal = fn_multVec3s(plane.normal,-1);
  }
  // return;

  // get interval of plane intersection
  double t0, t1;
  int embedded_in_plane = 0;

  // signed distance from sphere to point on plane
  signed_dist_to_plane =  fn_dot(packet->e_base_point,plane.normal) - fn_dot(plane.position,plane.normal);

  // cache this as we will reuse
  float normal_dot_vel = fn_dot(plane.normal, packet->e_velocity);

  // if sphere is moving parrallel to plane
  if (normal_dot_vel == 0.0f) {
    if (fabs(signed_dist_to_plane) >= 1.0f) {
      // no collision possible
      return;
    } else {
      // sphere is in plane in whole range [0..1]
      embedded_in_plane = 1;
      t0 = 0.0;
      t1 = 1.0;
    }
  } else {
    // N dot D is not 0, calc intersect interval
    // float nvi = 1.0f / normal_dot_vel;
    t0=(-1.0 - signed_dist_to_plane) / normal_dot_vel;
    t1=( 1.0 - signed_dist_to_plane) / normal_dot_vel;

    // swap so t0 < t1
    if (t0 > t1) {
      double temp = t1;
      t1 = t0;
      t0 = temp;
    }

    // check that at least one result is within range
    if (t0 > 1.0 || t1 < 0.0) {
      // both values outside range [0,1] so no collision
      return ;
    }

    // clamp to [0,1]
    if (t0 < 0.0) t0 = 0.0;
    if (t1 < 0.0) t1 = 0.0;
    if (t0 > 1.0) t0 = 1.0;
    if (t1 > 1.0) t1 = 1.0;
  }

  // time to check for a collision
  fn_vec3 collision_point;
  int found_collision = 0;
  double t = 1.0;

  // first check collision with the inside of the triangle
  if (embedded_in_plane == 0) {
    fn_vec3 plane_intersect, temp;
    plane_intersect = fn_subVec3(packet->e_base_point, plane.normal);
    temp = fn_multVec3s(packet->e_velocity, t0);
    plane_intersect = fn_addVec3(plane_intersect, temp);

    if (check_point_in_triangle(plane_intersect, p1, p2, p3)) {
      found_collision = 1;
      t = t0;
      collision_point = plane_intersect;
    }
  }


  // no collision yet, check against points and edges
  if (found_collision == 0) {
    fn_vec3 velocity, base, temp;
    velocity = packet->e_velocity;
    base = packet->e_base_point;

    float velocity_sqrt_length = fn_length2(velocity);
    float a,b,c;
    float new_t;

    // equation is a*t^2 + b*t + c = 0
    // check against points
    a = velocity_sqrt_length;

    // p1
    temp = fn_subVec3( base, p1);
    b = 2.0f*(fn_dot(velocity, temp));
    temp = fn_subVec3( p1, base);
    c = fn_length2(temp) - 1.0;
    if (get_lowest_root(a, b, c, t, &new_t) == 1) {
      t = new_t;
      found_collision = 1;
      collision_point = p1;
    }

    // p2
    if (found_collision == 0) {
      temp = fn_subVec3( base, p2);
      b = 2.0f*(fn_dot(velocity, temp));
      temp = fn_subVec3( p2, base);
      c = fn_length2(temp) - 1.0;
      if (get_lowest_root(a, b, c, t, &new_t) == 1) {
        t = new_t;
        found_collision = 1;
        collision_point = p2;
      }
    }

    // p3
    if (found_collision == 0) {
      temp = fn_subVec3( base, p3);
      b = 2.0f*(fn_dot(velocity, temp));
      temp = fn_subVec3( p3, base);
      c = fn_length2(temp) - 1.0;
      if (get_lowest_root(a, b, c, t, &new_t) == 1) {
        t = new_t;
        found_collision = 1;
        collision_point = p3;
      }
    }

    // check against edges
    // p1 -> p2
    fn_vec3 edge, base_to_vertex;
    edge = fn_subVec3( p2, p1);
    base_to_vertex = fn_subVec3( p1, base);
    float edge_sqrt_length        = fn_length2(edge);
    float edge_dot_velocity       = fn_dot(edge, velocity);
    float edge_dot_base_to_vertex = fn_dot(edge, base_to_vertex);

    // calculate params for equation
    a = edge_sqrt_length * -velocity_sqrt_length + edge_dot_velocity * edge_dot_velocity;
    b = edge_sqrt_length * (2.0f * fn_dot(velocity, base_to_vertex)) -
    2.0f * edge_dot_velocity * edge_dot_base_to_vertex;
    c = edge_sqrt_length * (1.0f - fn_length2(base_to_vertex)) +
    edge_dot_base_to_vertex * edge_dot_base_to_vertex;

    // do we collide against infinite edge
    if (get_lowest_root(a, b, c, t, &new_t) == 1) {
      // check if intersect is within line segment
      float f = (edge_dot_velocity * new_t - edge_dot_base_to_vertex) / edge_sqrt_length;
      if (f >= 0.0f && f <= 1.0f) {
        t = new_t;
        found_collision = 1;
        temp = fn_multVec3s( edge, f);
        temp = fn_addVec3( p1, temp);
        collision_point = temp;
      }
    }


    // p2 -> p3
    edge = fn_subVec3( p3, p2);
    base_to_vertex = fn_subVec3( p2, base);
    edge_sqrt_length        = fn_length2(edge);
    edge_dot_velocity       = fn_dot(edge, velocity);
    edge_dot_base_to_vertex = fn_dot(edge, base_to_vertex);

    // calculate params for equation
    a = edge_sqrt_length * -velocity_sqrt_length + edge_dot_velocity * edge_dot_velocity;
    b = edge_sqrt_length * (2.0f * fn_dot(velocity, base_to_vertex)) -
    2.0f * edge_dot_velocity * edge_dot_base_to_vertex;
    c = edge_sqrt_length * (1.0f - fn_length2(base_to_vertex)) +
    edge_dot_base_to_vertex * edge_dot_base_to_vertex;

    // do we collide against infinite edge
    if (get_lowest_root(a, b, c, t, &new_t) == 1) {
      // check if intersect is within line segment
      float f = (edge_dot_velocity * new_t - edge_dot_base_to_vertex) / edge_sqrt_length;
      if (f >= 0.0f && f <= 1.0f) {
        t = new_t;
        found_collision = 1;
        temp = fn_multVec3s( edge, f);
        temp = fn_addVec3( p2, temp);
        collision_point = temp;
      }
    }


    // p3 -> p1
    edge = fn_subVec3( p1, p3);
    base_to_vertex = fn_subVec3( p3, base);
    edge_sqrt_length        = fn_length2(edge);
    edge_dot_velocity       = fn_dot(edge, velocity);
    edge_dot_base_to_vertex = fn_dot(edge, base_to_vertex);

    // calculate params for equation
    a = edge_sqrt_length * -velocity_sqrt_length + edge_dot_velocity * edge_dot_velocity;
    b = edge_sqrt_length * (2.0f * fn_dot(velocity, base_to_vertex)) -
    2.0f * edge_dot_velocity * edge_dot_base_to_vertex;
    c = edge_sqrt_length * (1.0f - fn_length2(base_to_vertex)) +
    edge_dot_base_to_vertex * edge_dot_base_to_vertex;

    // do we collide against infinite edge
    if (get_lowest_root(a, b, c, t, &new_t) == 1) {
      // check if intersect is within line segment
      float f = (edge_dot_velocity * new_t - edge_dot_base_to_vertex) / edge_sqrt_length;
      if (f >= 0.0f && f <= 1.0f) {
        t = new_t;
        found_collision = 1;
        temp = fn_multVec3s( edge, f);
        temp = fn_addVec3( p3, temp);
        collision_point = temp;
      }
    }
  }


  // set results
  if (found_collision == 1) {
    if (packet->robust &&  t == 0.0 && fn_dot(plane.normal, packet->e_velocity) > 0.0)
    {
      return;
    }
    // distance to collision, t is time of collision
    double dist_to_coll = t*fn_length(packet->e_velocity);
    double dist_to_intersect = fn_length2(fn_subVec3(packet->e_base_point,collision_point));
    // double packet_dist = 0;
    // if (packet->found_collision == 1)
    // {
    //    packet_dist = fn_length2(fn_subVec3(packet->e_base_point,packet->intersect_point));
    // }
    //packet->found_collision == 1 && dist_to_coll >=  packet->nearest_distance  && dist_to_intersect < packet_dist
    // if (t == 0 && packet->found_collision == 1 &&  dist_to_coll >= packet->nearest_distance)
    // {
    //   th_printlnDevConsole("ITS OVER %i",th_frame());
    // }
    if (packet->found_collision == 0 ||  dist_to_coll < packet->nearest_distance  ) {
      packet->nearest_distance = dist_to_coll;
      packet->intersect_point =  collision_point;
      packet->found_collision = 1;
      packet->t = t;
      packet->plane =  plane;
      packet->a = p1;// sizeof(vec3));
      packet->b = p2;// sizeof(vec3));
      packet->c = p3;// sizeof(vec3));
    }

  }

}


th_Collision th_CollidePlanes(th_Collider edict,fn_vec3 delta,th_CollidableVolume* v)
{
  // if (v->pointCount == 3)
  // {
  //   return th_CollideTriangle( edict, delta,v);
  // }
  th_ColliderType mode = edict.mode;//CAPSULE;
  th_Plane* planes = v->planes;
  int planeCount = v->planeCount;

  int i;
  int hPlane  = 0;
  th_Collision s = NOCOLLISION;
  th_Collision fail = s;
  fn_vec3 linestart = edict.position;
  fn_vec3 lineend = fn_addVec3(edict.position,delta);

  fn_vec3 linestart2 = edict.position;
  fn_vec3 lineend2 = fn_addVec3(edict.position,delta);
  float f;

  bool startout = false;
  float enterFrac = -1.0;
  float leaveFrac = 1.0;
  th_Plane surf;
  surf.normal = fn_createVec3(0,0,0);
  fn_vec3 extens = edict.hwidth;


  for (i = 0; i < planeCount; i++) {


    th_Plane plane = planes[i];
    if (fn_equalVec3(plane.normal,fn_createVec3s(0)))
    {
      continue;
    }
    float offset = 0;

  if (mode & CAPSULE)
  {
    fn_vec3 axis = fn_createVec3(0.0,extens.y ,0.0);


     float t = fn_dot( plane.normal, axis );
     // // printf("%s\n","AA" );
     // // printf("%f\n",t );
       if ( t > 0.0 )
       {
         linestart =  fn_subVec3(linestart2,axis);
         lineend = fn_subVec3( lineend2,axis );
       }
       else
       {
         linestart =  fn_addVec3(axis , linestart2);
         lineend = fn_addVec3(axis, lineend2 );
       }
       offset = extens.x;
  }
  else if (mode & BOX)
  {
    offset = (float)(fabs( extens.x * plane.normal.x ) +
                         fabs( extens.y* plane.normal.y ) +
                         fabs( extens.z * plane.normal.z ) );
  }
  else if (mode & SPHERE)
  {
    offset = extens.x;
  }



   // plane.position = fn_addVec3(plane.position,fn_multVec3(plane.normal,edict.hwidth));
   offset += fn_dot(plane.position,plane.normal);
    // adjust the plane distance apropriately for mins/maxs
    float d2 = fn_dot(lineend,plane.normal) - offset;//fn_pointInPlane(lineend,plane.position,plane.normal) - offset ;
    float d1 = fn_dot(linestart,plane.normal) - offset;//fn_pointInPlane(linestart,plane.position,plane.normal) - offset ;



    if (d2 > 0.0) {

    }
    if (d1 > 0.0) {
      startout = true;
    }

    // if completely in front of face, no intersection with the entire brush
    if (d1 > 0.0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 )  ) {
      return fail;
    }

    // if it doesn't cross the plane, the plane isn't relevent
    if (d1 <= 0 && d2 <= 0 ) {
      continue;
    }

    // crosses face
    if (d1 > d2) {	// enter
      f = (d1-SURFACE_CLIP_EPSILON) / (d1-d2);
      if ( f < 0.0 ) {
        f = 0.0;
      }

      if (f > enterFrac) {
        enterFrac = f;
        hPlane = i;
        surf = plane;
      }

    } else {	// leave
      f = (d1+SURFACE_CLIP_EPSILON) / (d1-d2);
      if ( f > 1.0 ) {
        f = 1.0;
      }
      if (f < leaveFrac) {
        leaveFrac = f;
      }
    }
  }


  //
  // all planes have been checked, and the trace was not
  // completely outside the brush
  //
  if (!startout) {	// original point was inside brush

    return fail;


  }

  if (enterFrac < leaveFrac) {
    if (enterFrac > -1.0 ) {
      if (enterFrac < 0.0) {
        enterFrac = 0.0;
      }
      s.plane = planes[hPlane];
      s.time = enterFrac;
      s.normal = surf.normal;
      s.collided = true;
      s.pos = fn_lerpVec3(linestart,lineend,s.time );
      // s.plane_id = hPlane;
      s.volume = v;
      fn_vec3 tpoint = fn_subVec3(s.pos , fn_multVec3s(surf.normal,fn_dot(s.pos,surf.normal)  - fn_dot(surf.position,surf.normal)));
      s.touch = tpoint;



      if ( hPlane >= v->usablePlaneCount)
      {
        return fail;
      }





    }
  }

  //determine overlap
  return s;
}

th_Collision th_sweepTestAABB(th_Collider box,fn_vec3 delta,th_Collider box2)
{
  if (fn_aabbCheck(box,box2))
  {
    th_Collision ret;
    ret.collided = true;
    ret.time = 0;
    ret.pos = box.position;
    ret.normal = fn_normalizeVec3(delta);
    ret.plane.position = box.position;
    ret.plane.normal = ret.normal;
    return ret;
  }

  fn_vec3 normals[6];
  normals[0] = fn_createVec3(1,0,0);
  normals[1] = fn_createVec3(-1,0,0);
  normals[2] = fn_createVec3(0,-1,0);
  normals[3] = fn_createVec3(0,1,0);
  normals[4] = fn_createVec3(0,0,-1);
  normals[5] = fn_createVec3(0,0,1);

  th_Plane colliders[6];
  int i;
  for (i = 0; i< 6;i ++)
  {

    colliders[i].normal = normals[i];
    colliders[i].position = fn_addVec3(box2.position,fn_multVec3(box2.hwidth,normals[i]));

  }


  th_CollidableVolume volume;
  volume.planes = colliders;
  volume.planeCount = 6;
  volume.usablePlaneCount = 6;

  th_ColliderType type = box.mode;
  if (type != RAY)
  box.mode = BOX;
  th_Collision ret = th_CollidePlanes(box,delta,&volume);
  return ret;

}

static float check(
  const float pn,
  const float bmin,
  const float bmax )
{
  float out = 0;
  float v = pn;

  if ( v < bmin )
  {
      float val = (bmin - v);
      out += val * val;
  }

  if ( v > bmax )
  {
      float val = (v - bmax);
      out += val * val;
  }

  return out;
}

th_Collision th_sweepTestSphere(fn_vec3 pos,float radius,fn_vec3 delta,th_Collider box2)
{


  // do aabb v SPHERE



  // Squared distance
  float sq = 0.0;

  sq += check( pos.x, box2.position.x - box2.hwidth.x, box2.position.x + box2.hwidth.x );
  sq += check( pos.y, box2.position.y - box2.hwidth.y, box2.position.y + box2.hwidth.y  );
  sq += check( pos.z, box2.position.z - box2.hwidth.z, box2.position.z + box2.hwidth.z  );

  if (sq <= radius*radius)
  {
    th_Collision ret;
    ret.collided = true;
    ret.time = 0;
    ret.pos = pos;
    ret.normal = fn_normalizeVec3(delta);
    ret.plane.position = pos;
    ret.plane.normal = ret.normal;
    return ret;
  }



  fn_vec3 normals[6];
  normals[0] = fn_createVec3(1,0,0);
  normals[1] = fn_createVec3(-1,0,0);
  normals[2] = fn_createVec3(0,-1,0);
  normals[3] = fn_createVec3(0,1,0);
  normals[4] = fn_createVec3(0,0,-1);
  normals[5] = fn_createVec3(0,0,1);

  th_Plane colliders[6];
  int i;
  for (i = 0; i< 6;i ++)
  {

    colliders[i].normal = normals[i];
    colliders[i].position = fn_addVec3(box2.position,fn_multVec3(box2.hwidth,normals[i]));

  }


  th_CollidableVolume volume;
  volume.planes = colliders;
  volume.planeCount = 6;
  volume.usablePlaneCount = 6;

  // th_ColliderType type = box.mode;
  // if (type != RAY)
  // box.mode = BOX;

  th_Collider sphereCollider;
  sphereCollider.mode = SPHERE;
  sphereCollider.position = pos;
  sphereCollider.hwidth = fn_createVec3s(radius);



  th_Collision ret = th_CollidePlanes(sphereCollider,delta,&volume);
  return ret;

}

static int containsv3(fn_vec3* v,int count,fn_vec3 n)
{
  for (int i = 0 ; i < count;i++)
  {
    //00001
    if (fn_almostequalVec3(v[i],n,0.01))
    {
      return i;
    }
  }
  return -1;
}

static fn_vec3 calculateSurfaceNormal (fn_vec3 points[3])
{
	fn_vec3 U = fn_subVec3(points[1] , points[0]);
	fn_vec3 V = fn_subVec3(points[2] , points[0]);

  fn_vec3 normal;
	normal.x = (U.y * V.z) - (U.z * V.y);
	normal.y = (U.z * V.x) - (U.x * V.z);
	normal.z = (U.x * V.y) - (U.y * V.x);

	return fn_normalizeVec3(normal);

}

fn_vec3 th_calculateSurfaceNormal (fn_vec3 points[3])
{
  return calculateSurfaceNormal(points);

}




th_Plane* th_getVolumeFromTris(fn_vec3* verts,unsigned int* indices,int tricount,int* outPlanesCount)
{
  fn_vec3* normals = malloc(sizeof(fn_vec3)* (tricount/3));
  fn_vec3* positions = malloc(sizeof(fn_vec3)* (tricount/3));
  int pc = 0;

  for (int i = 0 ; i < tricount/3;i++)
  {
    fn_vec3 points[3];
     points[0] = verts[indices[i*3 + 0]];
     points[1] = verts[indices[i*3 + 1]];
     points[2] = verts[indices[i*3 + 2]];
     fn_vec3 n = calculateSurfaceNormal(points);
     fn_vec3 avg = fn_addVec3(fn_addVec3(points[0],points[2]),points[1]);
     avg = fn_multVec3s(avg,1.0/3.0);
     int c = containsv3(normals,pc,n);
    if (c > -1)
    {
      positions[c] = fn_multVec3s(fn_addVec3(positions[c],avg),0.5);
    }
    else
    {
      normals[pc] = n;
      positions[pc] = avg;
      pc++;
    }
  }
  th_Plane* ret = malloc(sizeof(th_Plane)*pc);
  for (int i = 0;i < pc;i++)
  {
    ret[i] = DEFAULTPLANE;
    ret[i].position = positions[i];
    ret[i].normal = normals[i];
  }
  *outPlanesCount = pc;
  free(normals);
  free(positions);
  return ret;
}





#define PLANE_EPSILON 0



void th_getAABB(th_Plane* out,th_CollidableVolume* volume,fn_mat4 tr)
{


  fn_vec3* points = volume->points;
  int pointCount = volume->pointCount;
  fn_vec3 max = points[0];
  fn_vec3 min = points[0];
  for (int i = 0 ; i < pointCount;i++)
  {
    points[i] = fn_transformVec3(points[i],tr);
    for (int j = 0 ; j < 3;j++)
    {
      if (points[i].v[j] > max.v[j])
      {
        max.v[j] = points[i].v[j];
      }
      if (points[i].v[j] < min.v[j])
      {
        min.v[j] = points[i].v[j];
      }
    }
  }
  fn_vec3 center = fn_multVec3s(fn_addVec3(max,min),0.5);
  fn_vec3 hwidth = fn_multVec3s(fn_createVec3(max.x - min.x,max.y - min.y,max.z - min.z),0.5);

  out[0].position = fn_createVec3(hwidth.x,0,0);
  out[0].normal = fn_createVec3(1,0,0);

  out[1].position = fn_createVec3(-hwidth.x,0,0);
  out[1].normal = fn_createVec3(-1,0,0);

  out[2].position = fn_createVec3(0,hwidth.y,0);
  out[2].normal = fn_createVec3(0,1,0);

  out[3].position = fn_createVec3(0,-hwidth.y,0);
  out[3].normal = fn_createVec3(0,-1,0);

  out[4].position = fn_createVec3(0,0,hwidth.z);
  out[4].normal = fn_createVec3(0,0,1);

  out[5].position = fn_createVec3(0,0,-hwidth.z);
  out[5].normal = fn_createVec3(0,0,-1);

  th_translateCollider(out,6,center);

}




void th_translateCollider(th_Plane* planes,int planeCount,fn_vec3 translate)
{
  int i;
  for (i=0;i < planeCount;i++)
  {
    planes[i].position = fn_addVec3(planes[i].position,translate);
  }
}
void th_scaleCollider(th_Plane* planes,int planeCount,fn_vec3 scale)
{
  fn_vec3 center = fn_createVec3s(0.f);
  int i;

  fn_mat4 matrix = fn_makescale(scale);
  fn_mat4 normalmatrix = fn_transpose(fn_inverse(matrix));

  for (i=0;i < planeCount;i++)
  {
    planes[i].normal = fn_normalizeVec3(fn_transformNormal(planes[i].normal,normalmatrix));
    planes[i].position = fn_multVec3(planes[i].position,scale);
  }

}

void th_transformCollider(th_Plane* planes,int planeCount,fn_mat4 m)
{
  int i;

  fn_mat4 matrix = m;
  fn_mat4 normalmatrix = fn_transpose(fn_inverse(matrix));

  for (i=0;i < planeCount;i++)
  {
    fn_vec3 old_normal = planes[i].normal;
    planes[i].normal = fn_normalizeVec3(fn_transformNormal(planes[i].normal,normalmatrix));
    if (old_normal.x == 1.0 || old_normal.x == -1.0 || old_normal.y == 1.0 || old_normal.y == -1.0 || old_normal.z == 1.0 || old_normal.z == -1.0)
    {
      planes[i].normal = fn_cleanUpNormal(planes[i].normal);
    }
    planes[i].position = fn_transformVec3(planes[i].position,matrix);
  }
}

void th_copyCollider(th_Plane* out,th_Plane* in,int planeCount)
{
  memcpy(out,in,sizeof(th_Plane)*planeCount);
}

// void th_rotateCollider(th_Plane* planes,int planeCount,fn_quat quaternion)
// {
//
//   th_Plane temp[planeCount];
//   memcpy(temp,planes,sizeof(th_Plane)*planeCount);
//
//   int i;
//   fn_mat4 matrix = fn_rotationMat(quaternion);
//   fn_mat4 normalmatrix = fn_transpose(fn_inverse(matrix));
//   for (i =0;i < planeCount;i++)
//   {
//     temp[i].position = fn_transformVec3(temp[i].position,matrix);
//     temp[i].normal = fn_normalizeVec3(fn_transformNormal(temp[i].normal,normalmatrix));
//   }
//
//   memcpy(planes,temp,sizeof(th_Plane)*planeCount);
//
// }

int th_cmpcol(const void * a, const void* b)
{
  const th_Collision* a_data = (const th_Collision*)a;
  const th_Collision* b_data = (const th_Collision*)b;
  if ((a_data)->time < (b_data)->time )
  {
    return -1;
  }
  else if ((a_data)->time > (b_data)->time )
  {
    return 1;
  }
  return 0;
}




void th_getCollision(th_Collision* collision,int* collisioncount,th_CollidableVolume* volumes,int volumecount,th_Collider edict,fn_vec3 delta)
{
  *collisioncount = 0;

  // th_Collision ret_3;
  // th_Collision ret_2;
  //th_Collision ret;
//  ret.time = 2;

  for (int i = 0;i < volumecount;i++)
  {
    th_Collision col = th_CollidePlanes(edict,delta,&volumes[i]);
    // col.volume_id = i;
    if (col.collided)
    {


      if (*collisioncount < 3)
      {
        collision[*collisioncount] = col;
        *collisioncount = *collisioncount + 1;
        if (*collisioncount > 1)
        {
          qsort(collision,*collisioncount,sizeof(th_Collision),th_cmpcol);
        }

      }
      else
      {
        collision[3] = col;
        qsort(collision,4,sizeof(th_Collision),th_cmpcol);
      }


      // *collisioncount = *collisioncount + 1;
    }
  }


  // *collision = ret;
}


// Möller–Trumbore intersection algorithm
bool th_ray_in_tri(fn_vec3 from, fn_vec3 to, fn_vec3 v0, fn_vec3 v1, fn_vec3 v2, fn_vec3* intersect)
{
  fn_vec3 vector;
  vector = fn_normalizeVec3(to);

  fn_vec3 edge1, edge2, h, s, q;
  edge1 = fn_subVec3(v1, v0);
  edge2 = fn_subVec3(v2, v0);

  h = fn_cross( vector, edge2);
  float a = fn_dot(edge1, h);

  if (a > -FLT_EPSILON && a < FLT_EPSILON)
    return 0;

  float f = 1.0/a;

  s = fn_subVec3( from, v0);

  float u = f * fn_dot(s, h);
  if (u < 0.0 || u > 1.0)
    return 0;

  q = fn_cross( s, edge1);
  float v = f * fn_dot(vector, q);
  if (v < 0.0 || u + v > 1.0)
    return 0;

  float t = f * fn_dot(edge2, q);
  if (t > FLT_EPSILON) {
    fn_vec3 tmp;
    tmp = fn_multVec3s(vector, t);
    *intersect = fn_addVec3( from, tmp);
    return true;
  }

  return false;
}




bool th_box_tri_intersect(fn_vec3 pos,fn_vec3 boxhalf,fn_vec3 t0,fn_vec3 t1,fn_vec3 t2)
{
  float min, max, p0, p1, p2, rad;

  /* Move triangle into box space */
  fn_vec3 v0 = fn_subVec3(t0, pos);
  fn_vec3 v1 = fn_subVec3(t1, pos);
  fn_vec3 v2 = fn_subVec3(t2, pos);

  /* Compute triangle edges */
  fn_vec3 e0 = fn_subVec3(v1, v0);
  fn_vec3 e1 = fn_subVec3(v2, v1);
  fn_vec3 e2 = fn_subVec3(v0, v2);

  float fex = fabs(e0.x);
  float fey = fabs(e0.y);
  float fez = fabs(e0.z);



  p0 = e0.z*v0.y - e0.y*v0.z;
  p2 = e0.z*v2.y - e0.y*v2.z;
  min = fmin(p0, p2);
  max = fmax(p0, p2);
  rad = fez*boxhalf.y + fey*boxhalf.z;
  if (min > rad || max < -rad) return 0;

  p0 = e0.z*v0.x - e0.x*v0.z;
  p2 = e0.z*v2.x - e0.x*v2.z;
  min = fmin(p0, p2); max = fmax(p0, p2);
  rad = fez*boxhalf.x + fex*boxhalf.z;
  if (min > rad || max < -rad) return 0;

  p1 = e0.x*v1.y - e0.y*v1.x;
  p2 = e0.x*v2.y - e0.y*v2.x;
  min = fmin(p1, p2); max = fmax(p1, p2);
  rad = fex*boxhalf.y + fey*boxhalf.x;
  if (min > rad || max < -rad) return 0;

  fex = fabs(e1.x); fey = fabs(e1.y); fez = fabs(e1.z);

  p0 = e1.z*v0.y - e1.y*v0.z;
  p2 = e1.z*v2.y - e1.y*v2.z;
  min = fmin(p0, p2); max = fmax(p0, p2);
  rad = fez*boxhalf.y + fey*boxhalf.z;
  if (min > rad || max < -rad) return 0;

  p0 = e1.z*v0.x - e1.x*v0.z;
  p2 = e1.z*v2.x - e1.x*v2.z;
  min = fmin(p0, p2); max = fmax(p0, p2);
  rad = fez*boxhalf.x + fex*boxhalf.z;
  if (min > rad || max < -rad) return 0;

  p0 = e1.x*v0.y - e1.y*v0.x;
  p1 = e1.x*v1.y - e1.y*v1.x;
  min = fmin(p0, p1); max = fmax(p0, p1);
  rad = fex*boxhalf.y + fey*boxhalf.x;
  if (min > rad || max < -rad) return 0;

  fex = fabs(e2.x); fey = fabs(e2.y); fez = fabs(e2.z);

  p0 = e2.z*v0.y - e2.y*v0.z;
  p1 = e2.z*v1.y - e2.y*v1.z;
  min = fmin(p0, p1); max = fmax(p0, p1);
  rad = fez*boxhalf.y + fey*boxhalf.z;
  if (min > rad || max < -rad) return 0;

  p0 = e2.z*v0.x - e2.x*v0.z;
  p1 = e2.z*v1.x - e2.x*v1.z;
  min = fmin(p0, p1); max = fmax(p0, p1);
  rad = fez*boxhalf.x + fex*boxhalf.z;
  if (min > rad || max < -rad) return 0;

  p1 = e2.x*v1.y - e2.y*v1.x;
  p2 = e2.x*v2.y - e2.y*v2.x;
  min = fmin(p1, p2); max = fmax(p1, p2);
  rad = fex*boxhalf.y + fey*boxhalf.x;
  if (min > rad || max < -rad) return 0;

  /* AABB face tests */
  min = fmin(v0.x, fmin(v1.x, v2.x));
  max = fmax(v0.x, fmax(v1.x, v2.x));
  if (min > boxhalf.x || max < -boxhalf.x) return 0;

  min = fmin(v0.y, fmin(v1.y, v2.y));
  max = fmax(v0.y, fmax(v1.y, v2.y));
  if (min > boxhalf.y || max < -boxhalf.y) return 0;

  min = fmin(v0.z, fmin(v1.z, v2.z));
  max = fmax(v0.z, fmax(v1.z, v2.z));
  if (min > boxhalf.z || max < -boxhalf.z) return 0;

  /* Triangle plane test */
  fn_vec3 normal = fn_cross(e0, e1);
  float d = -fn_dot(normal, v0);

  fn_vec3 vmin, vmax;
  for (int i = 0; i < 3; i++) {
    float n = (&normal.x)[i];
    float b = (&boxhalf.x)[i];
    if (n > 0.0f) {
      (&vmin.x)[i] = -b;
      (&vmax.x)[i] =  b;
    } else {
      (&vmin.x)[i] =  b;
      (&vmax.x)[i] = -b;
    }
  }

  if (fn_dot(normal, vmin) + d > 0.0f) return 0;
  if (fn_dot(normal, vmax) + d < 0.0f) return 0;

  return 1;
}
