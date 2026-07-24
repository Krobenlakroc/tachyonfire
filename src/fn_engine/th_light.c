#include "th_light.h"
#include "th_time.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

th_LightIdTuple th_makeLightIDTuple(int id,int count)
{
  th_LightIdTuple ret;
  ret.id = id;
  ret.count = count;
  return ret;
}

th_LightProperties th_getDefaultLight()
{
  th_LightProperties p;
  p.life = 10000000;
  p.fadeout = false;
  p.alive = true;
  p.start_time = th_time();
  p.base_color = fn_createVec3(0,0,0);
  p.tube = false;
  p.tube_endcap = fn_createVec3(0,0,0);
  p.self_destruct = false;
  p.lightthresh = 0.0;
  return p;
}

th_LightProperties th_getLightProperties(th_LightQuery* l,th_LightIdTuple i)
{
  return l->properties[i.id];
}

static th_LightIdTuple lcall(th_LightQuery* l,th_LightProperties p,fn_vec3 color,fn_vec3 position)
{


  pthread_mutex_lock(&l->light_lock);

  if (l->free_stack.stack_count > 0)
  {
    l->current_light = th_stackPop(&l->free_stack);
  }


  p.base_color = color;
  l->properties[l->current_light] = p;
  l->pointlights[l->current_light].pos = fn_createVec4Vec3(position,0);
  l->pointlights[l->current_light].color = fn_createVec4Vec3(color,p.lightthresh);
  l->count_array[l->current_light] = l->count_array[l->current_light] + 1;
  int r = l->current_light;
  int r2 = l->count_array[l->current_light];
  l->current_light++;
  if (l->current_light >= l->num_lights)
  {
    l->current_light = l->light_offset_start;
  }
  th_LightIdTuple ret;
  ret.id = r;
  ret.count = r2;
  pthread_mutex_unlock(&l->light_lock);
  return ret;
}

void th_makeLight(th_LightQuery* l,th_LightProperties p,fn_vec3 color,fn_vec3 position)
{
  lcall(l,p,color,position);
}

th_LightIdTuple th_getLight(th_LightQuery* l,th_LightProperties p,fn_vec3 color,fn_vec3 position)
{
  return lcall(l,p,color,position);
}

void th_killLight(th_LightQuery* l,th_LightIdTuple i)
{
  pthread_mutex_lock(&l->light_lock);
  if (i.count == l->count_array[i.id])
  {
    if (l->properties[i.id].alive)
    {
      th_stackPush(&l->free_stack,i.id);
    }
    l->properties[i.id].alive = false;
    l->pointlights[i.id].pos = fn_createVec4(100000,10000000,10000000,0);
    l->pointlights[i.id].pos2 = fn_createVec4(100000,10000000,10000000,0);
    l->pointlights[i.id].color = fn_createVec4(0,0,0,0);
  }
  else
  {
    printf("Light cannot kill, too old\n");
  }
  pthread_mutex_unlock(&l->light_lock);
}

void th_killLightAfterRender(th_LightQuery* l,th_LightIdTuple i)
{
  if (i.count == l->count_array[i.id])
  {
    //printf("%i\n",l->properties[i.id].tube );
    l->properties[i.id].self_destruct = true;

    // l->pointlights[i.id].pos = fn_createVec4(100000,10000000,10000000,0);
    // l->pointlights[i.id].color = fn_createVec4(0,0,0,0);
  }
}

void th_setLight(th_LightQuery* l,th_LightIdTuple i,fn_vec3 color,fn_vec3 position)
{
  if (i.count == l->count_array[i.id])
  {
    l->pointlights[i.id].pos2 = fn_createVec4Vec3(fn_createVec3s(0),0);
    l->pointlights[i.id].pos = fn_createVec4Vec3(position,0);
    l->pointlights[i.id].color = fn_createVec4Vec3(color,l->properties[i.id].lightthresh);
  }
}

void th_setLightTube(th_LightQuery* l,th_LightIdTuple i,fn_vec3 color,fn_vec3 position,fn_vec4 tube)
{
  if (i.count == l->count_array[i.id])
  {
    l->pointlights[i.id].pos2 = tube;
    l->pointlights[i.id].pos = fn_createVec4Vec3(position,0);
    l->pointlights[i.id].color = fn_createVec4Vec3(color,0);
  }
}

void th_setLightProperties(th_LightQuery* l,th_LightIdTuple i,fn_vec3 color,fn_vec3 position,th_LightProperties prop)
{
  if (i.count == l->count_array[i.id])
  {
    l->properties[i.id] = prop;
    l->pointlights[i.id].pos = fn_createVec4Vec3(position,0);
    l->pointlights[i.id].color = fn_createVec4Vec3(color,prop.lightthresh);
  }
}

void th_initLightQuery(th_LightQuery* l,th_PointLight* pointlights,int useable,int offset,th_Allocator* alloc)
{
  if (alloc == NULL)
  {
    l->count_array = malloc(sizeof(int)*VIRTUAL_LIGHTCOUNT);
  }
  else
  {
    l->count_array = th_alloc(alloc,sizeof(int)*VIRTUAL_LIGHTCOUNT);
  }

  if (alloc == NULL)
  {
    l->pointlights = malloc(sizeof(th_PointLight)*VIRTUAL_LIGHTCOUNT);
  }
  else
  {
    l->pointlights = th_alloc(alloc,sizeof(th_PointLight)*VIRTUAL_LIGHTCOUNT);
  }




  for (int i = 0 ; i < VIRTUAL_LIGHTCOUNT;i++)
  {
    l->pointlights[i].pos = fn_createVec4(100000,0,0,0);
    l->pointlights[i].color = fn_createVec4(0,0,0,0);
    l->pointlights[i].lightmat = fn_identityMat4();
    l->pointlights[i].shadowindex = fn_createVec4(0,0,0,0);
    l->pointlights[i].pos2 = fn_createVec4(0,0,0,0);
  }
  l->physical_used = 0;
  l->num_lights = VIRTUAL_LIGHTCOUNT;
  l->current_light = 0;
  l->light_offset_start = 0;

  if (alloc == NULL)
  {
      l->properties = malloc(sizeof(th_LightProperties)*VIRTUAL_LIGHTCOUNT);

      l->frustum_test = malloc(sizeof(int)*VIRTUAL_LIGHTCOUNT);
      l->distances = malloc(sizeof(float)*VIRTUAL_LIGHTCOUNT);

      th_Allocator alloc_temp;
      th_createAllocator(&alloc_temp);
      th_stackInit(&alloc_temp,&l->free_stack,VIRTUAL_LIGHTCOUNT);
  }
  else
  {
      l->properties = th_alloc(alloc,sizeof(th_LightProperties)*VIRTUAL_LIGHTCOUNT);

      l->frustum_test = th_alloc(alloc,sizeof(int)*VIRTUAL_LIGHTCOUNT);
      l->distances = th_alloc(alloc,sizeof(float)*VIRTUAL_LIGHTCOUNT);

      th_stackInit(alloc,&l->free_stack,VIRTUAL_LIGHTCOUNT);
  }

  pthread_mutex_init(&l->light_lock, NULL);

  for (int i = 0; i < VIRTUAL_LIGHTCOUNT; i++) {
    l->properties[i] = th_getDefaultLight();
    l->properties[i].life = 0.0;
    l->properties[i].start_time = 0.0;
    l->properties[i].fadeout = false;
    l->properties[i].alive = false;
    l->properties[i].base_color = fn_createVec3(0,0,0);
    l->count_array[i] = 0;

    th_stackPush(&l->free_stack,i);
  }

  l->lights_physical_count = useable;
  l->light_offset_start_physical = offset;
  l->pointlights_physical = pointlights;

  int n_select = l->lights_physical_count - l->light_offset_start_physical;
  if (alloc == NULL)
  {
    l->indices_times = malloc(sizeof(int)*n_select*2);
    l->start_times = malloc(sizeof(th_timer_t)*n_select*2);
  }
  else
  {
    l->indices_times = th_alloc(alloc,sizeof(int)*n_select*2);
    l->start_times = th_alloc(alloc,sizeof(th_timer_t)*n_select*2);

  }
}

void th_updateLightSingle(th_LightQuery* l,th_LightIdTuple idx)
{
  if (idx.count == l->count_array[idx.id])
  {
    int i = idx.id;
    if (l->properties[i].self_destruct)
    {
      if (l->properties[i].alive)
      {
        pthread_mutex_lock(&l->light_lock);
        th_stackPush(&l->free_stack,i);
        pthread_mutex_unlock(&l->light_lock);
      }
      l->properties[i].alive = false;

    }

    if (l->properties[i].alive)
    {


      if (!l->properties[i].tube)
      {
        l->pointlights[i].pos2 = fn_createVec4(0,0,0,0);
      }
      else
      {
        fn_vec3 endcap = l->properties[i].tube_endcap;
        l->pointlights[i].pos2 = fn_createVec4(endcap.x,endcap.y,endcap.z,1);
      }


      l->pointlights[i].color.w = l->properties[i].lightthresh;
      if (th_time() > l->properties[i].start_time + l->properties[i].life)
      {
        if (l->properties[i].alive)
        {
          pthread_mutex_lock(&l->light_lock);
          th_stackPush(&l->free_stack,i);
          pthread_mutex_unlock(&l->light_lock);
        }
        l->properties[i].alive = false;
        l->pointlights[i].pos = fn_createVec4(100000,10000000,10000000,0);
        l->pointlights[i].color = fn_createVec4(0,0,0,0);
      }
      else if (l->properties[i].fadeout)
      {

        float alpha = ((th_time() - l->properties[i].start_time)/l->properties[i].life);
        l->pointlights[i].color.xyz = fn_lerpVec3(l->properties[i].base_color,fn_createVec3(0,0,0),alpha);


        if(fn_max(l->pointlights[i].color.x,fn_max(l->pointlights[i].color.y,l->pointlights[i].color.z)) < 0.1)
        {
          if (l->properties[i].alive)
          {
            pthread_mutex_lock(&l->light_lock);
            th_stackPush(&l->free_stack,i);
            pthread_mutex_unlock(&l->light_lock);
          }
          l->properties[i].alive = false;
        }
      }
    }
    else
    {
      l->pointlights[i].pos = fn_createVec4(100000,10000000,10000000,0);
      l->pointlights[i].color = fn_createVec4(0,0,0,0);
    }
  }

}

void th_updateLights(th_LightQuery* l)
{
  for (int i = l->light_offset_start;i < l->num_lights;i++)
  {

    if (l->properties[i].self_destruct)
    {
      if (l->properties[i].alive)
      {
        th_stackPush(&l->free_stack,i);
      }
      l->properties[i].alive = false;
    }

    if (l->properties[i].alive)
    {


      if (!l->properties[i].tube)
      {
        l->pointlights[i].pos2 = fn_createVec4(0,0,0,0);
      }
      else
      {
        fn_vec3 endcap = l->properties[i].tube_endcap;
        l->pointlights[i].pos2 = fn_createVec4(endcap.x,endcap.y,endcap.z,1);
      }


      l->pointlights[i].color.w = l->properties[i].lightthresh;
      if (th_time() > l->properties[i].start_time + l->properties[i].life)
      {
        if (l->properties[i].alive)
        {
          th_stackPush(&l->free_stack,i);
        }
        l->properties[i].alive = false;
        l->pointlights[i].pos = fn_createVec4(100000,10000000,10000000,0);
        l->pointlights[i].color = fn_createVec4(0,0,0,0);
      }
      else if (l->properties[i].fadeout)
      {
        float alpha = ((th_time() - l->properties[i].start_time)/l->properties[i].life);
        l->pointlights[i].color.xyz = fn_lerpVec3(l->properties[i].base_color,fn_createVec3(0,0,0),alpha);

        if(fn_max(l->pointlights[i].color.x,fn_max(l->pointlights[i].color.y,l->pointlights[i].color.z)) < 0.1)
        {
          if (l->properties[i].alive)
          {
            th_stackPush(&l->free_stack,i);
          }
          l->properties[i].alive = false;
        }
      }
    }
    else
    {
      l->pointlights[i].pos2 = fn_createVec4(100000,10000000,10000000,0);
      l->pointlights[i].pos = fn_createVec4(100000,10000000,10000000,0);
      l->pointlights[i].color = fn_createVec4(0,0,0,0);
    }
  }
}

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

static int* indices_times = NULL;
static th_timer_t* start_times = NULL;
static int time_idx = 0;

static float* dists = NULL;


static void heapifyUp(int index)
{
  while (index > 0){
    int parentIndex = (index - 1) / 2;
    if (start_times[index] <= start_times[parentIndex])
    {
      break;
    }
    float temp_dist;
    int temp_occluder;
    temp_dist = start_times[index];
    start_times[index] = start_times[parentIndex];
    start_times[parentIndex] = temp_dist;

    temp_occluder = indices_times[index];
    indices_times[index] = indices_times[parentIndex];
    indices_times[parentIndex] = temp_occluder;

    index = parentIndex;
  }

}





int compare_lights(const void* a, const void* b) {
  float dist_a = dists[*((const int*)a)];
  float dist_b = dists[*((const int*)b)];
  return (dist_a > dist_b) - (dist_a < dist_b);
}

void th_assignPhysicalLights(th_LightQuery* l,fn_vec4* planes_frust,fn_vec3 viewpos)
{
  //of the lights in the frustum, get the 256 - offset closest, and copy them to the physical light buffer

  th_PointLight* lights = l->pointlights;

  int frst_count = 0;

  for (int i = 0 ; i < l->num_lights;i++)
  {
    l->distances[i] = 10000000000000.0;
    float thresh = 0.001;
    if (lights[i].pos2.w != 0.0)
    {
      thresh = 0.0001;
    }
    float radius = sqrtf(fmax(lights[i].color.x,fmax(lights[i].color.y,lights[i].color.z))/thresh);
    if (radius < 0.01)
    {
      continue;
    }

    if (!fn_sphereInFrustum(lights[i].pos.xyz,radius,planes_frust))
    {
      continue;
    }

    l->frustum_test[frst_count] = i;
    l->distances[i] = fn_max(fn_distance2(lights[i].pos.xyz,viewpos) - radius,0.0);
    frst_count++;
  }


  int n_select = l->lights_physical_count - l->light_offset_start_physical;


  l->physical_used = frst_count;

  if (frst_count > n_select)
  {
    l->physical_used = n_select;
  }

  //TODO create indices array
  //TODO create start times array
  time_idx = 0;
  // if (indices_times == NULL)
  // {
  //   indices_times = malloc(sizeof(int)*n_select*2);
  //   start_times = malloc(sizeof(th_timer_t)*n_select*2);
  // }
  indices_times = l->indices_times;
  start_times = l->start_times;


  if (frst_count <= n_select)
  {
    for (int i = 0 ; i < n_select;i++)
    {
      if (i < frst_count)
      {
        l->pointlights_physical[l->light_offset_start_physical + i] = lights[l->frustum_test[i]];

        int lidx = l->frustum_test[i];
        float start_time = l->properties[lidx].start_time;

        indices_times[time_idx] = i;
        start_times[time_idx] = start_time;
        heapifyUp(time_idx);
        time_idx++;

      }
      else
      {
        l->pointlights_physical[l->light_offset_start_physical + i].pos2 = fn_createVec4(100000,10000000,10000000,0);
        l->pointlights_physical[l->light_offset_start_physical + i].pos = fn_createVec4(100000,10000000,10000000,0);
        l->pointlights_physical[l->light_offset_start_physical + i].color = fn_createVec4(0,0,0,0);
      }

    }
  }
  else
  {
    dists = l->distances;
    qsort(l->frustum_test, frst_count, sizeof(int), compare_lights);

    for (int i = 0 ; i < n_select;i++)
    {
      l->pointlights_physical[l->light_offset_start_physical + i] = lights[l->frustum_test[i]];

      int lidx = l->frustum_test[i];
      float start_time = l->properties[lidx].start_time;

      indices_times[time_idx] = i;
      start_times[time_idx] = start_time;
      heapifyUp(time_idx);
      time_idx++;


    }
  }




}

void th_mergePhysicalLights(th_LightQuery* l,th_PointLight* merge,int num_merge)
{
  if (num_merge == 0)
  {
    return;
  }
  //slots
  int n_select = l->lights_physical_count - l->light_offset_start_physical;
  //used
  //l->physical_used

  //3 cases
  //enough to fit num_merge into unused light slots
  //enough to fit a few of the num_merge into unused light slots but not all
  //no free light slots

  if (n_select - l->physical_used >= num_merge )
  {
    for (int i = 0 ; i < num_merge;i++)
    {
      l->pointlights_physical[l->light_offset_start_physical + l->physical_used + i] = merge[i];

    }
  }
  else
  {
    //use the free light slots if any
    int fit_in = 0;
    if (n_select - l->physical_used > 0)
    {
      fit_in = n_select - l->physical_used;
      for (int i = 0 ; i < fit_in;i++)
      {
        l->pointlights_physical[l->light_offset_start_physical + l->physical_used + i] = merge[i];

      }
    }

    //iterate over remaining light slots and clobber the oldest one
    int clobber_idx = 0;
    for (int i = fit_in ; i < num_merge ; i++)
    {
      //i is merge index
      int idx_in_physical = indices_times[clobber_idx];
      clobber_idx++;
      l->pointlights_physical[l->light_offset_start_physical + idx_in_physical] = merge[i];

    }
  }


}
