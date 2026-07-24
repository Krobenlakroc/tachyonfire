#include "th_particle.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include <string.h>
#include "th_time.h"
#include "th_threads.h"
#include "th_system.h"
//prevent race condition
static pthread_mutex_t particle_lock = PTHREAD_MUTEX_INITIALIZER;

static fn_vec2 particle_ids_list[256];
void th_registerParticle(fn_vec2 id,th_ParticleName name)
{
  particle_ids_list[(int)name] = id;
}
fn_vec2 th_getParticleTexture(th_ParticleName name)
{
  return particle_ids_list[(int)name];
}

static int max_particles = 0;
static th_Particle* particles_list = NULL;

static th_Particle* particles_list_copy = NULL;

static fn_mat4* pmats = NULL;
static int pmat_count = 0;
static int particle_index = 0;
static uint64_t particles_created = 0;

static th_xoshiro128p_state rand_state = {{0x12345678, 0x9ABCDEF0, 0x13579BDF, 0x2468ACE0}};

static uint64_t num_lights = 0;

void th_initParticles(th_Allocator* alloc,int count)
{
  //pthread_mutex_init(&particle_lock, NULL);
  th_xoshiro128p_init(&rand_state,5);

  pmats = th_alloc(alloc,sizeof(fn_mat4)*count);
  for (int i = 0 ; i < count;i++)
  {
    pmats[i] = fn_maketranslate(fn_createVec3s(1000000));
  }
  pmat_count = 0;
  particles_list = th_alloc(alloc,sizeof(th_Particle)*count);
  particles_list_copy = th_alloc(alloc,sizeof(th_Particle)*count);
  for (int i = 0 ; i < count;i++)
  {
    particles_list[i] = th_defaultParticle();
    particles_list[i].tubelight = false;
    particles_list[i].hasphysics = false;
    particles_list[i].animated = false;
    particles_list[i].alive = false;
    particles_list[i].alphascale = 1.0;
    //th_Particle th_defaultParticle()
  }
  max_particles = count;
  particle_index = 0;
  num_lights = 0;
}

fn_mat4* th_particle_matrices()
{
  return pmats;
}

int th_particle_count()
{
  return pmat_count;
}

static fn_vec3 camera_pos;
static int compare_partcle(const void* a,const void* b)
{

  const th_Particle* ap = (const th_Particle*)a;
  const th_Particle* bp = (const th_Particle*)b;

  float d1 = ap->distance_to_camera;
  float d2 = bp->distance_to_camera;
  if (d1 > d2)
  {
    return -1;
  }
  else
  {
    return 1;
  }
}

static fn_vec3 rotate_tangent(fn_vec3 tangent , fn_vec3 normal, float angle)
{
  float c = cosf(angle);
  float s = sinf(angle);
  fn_vec3 cross = fn_cross(normal,tangent);
  return fn_addVec3(fn_multVec3s(tangent,c) , fn_multVec3s(cross,s));
}


typedef struct
{
  int start;
  int range;
  float dt;
  th_DecalOrientation** decals_list2d;
  int* counts2d;
  int thread_id;
  fn_vec3 pos;
  fn_vec3 look;
  th_World* w;
}th_particleComputeData;

void thread_computeparticle( void* id)
{
  th_particleComputeData data = *((th_particleComputeData*)id);
  float dt = data.dt;
  fn_vec3 pos= data.pos;
  fn_vec3 look = data.look;
  th_DecalOrientation** decals_list2d = data.decals_list2d;
  int* counts2d = data.counts2d;
  int thread_id = data.thread_id;
  th_World* w = data.w;

  for (int i = data.start ; i < data.start + data.range;i++)
  {

    th_Particle* p = &particles_list[i];

    if (p->alive)
    {
      p->num_frames_alive = p->num_frames_alive + 1;
      if (fn_equalVec2(p->texture_handle , th_getParticleTexture(TH_BLOOD)) && !p->hasphysics)
      {
        //printf("%s\n","ERROR OVERWRITE" );
      }


      p->distance_to_camera = fn_pointInPlane(p->position,pos,look);
      if (p->fade_out)
      {
        p->alphascale = fn_clamp(1.0 - ((th_time() - p->start_time)/p->life),0.0,1.0);
      }
      else if (p->fade_out_sharp)
      {
        //use last 25%
        float major = p->life*0.75;
        float minor = p->life*0.25;
        p->alphascale = fn_clamp(1.0 - (((th_time() - p->start_time) - major )/minor),0.0,1.0);
      }

      p->alphascale = fn_clamp(p->alphascale*p->alphascale_base,0.0,1.0);

      if (p->scale_end != -1.0)
      {
        float alpha = fn_clamp(1.0 - ((th_time() - p->start_time)/p->life),0.0,1.0);
        p->scale_internal = fn_lerp(p->scale_end,p->scale,alpha);
      }
      else
      {
        p->scale_internal = p->scale;
      }

      //((p->fade_out && p->num_frames_alive > 0) || !p->fade_out)
      //&& p->num_frames_alive > 1
      if (th_time() - p->start_time > p->life  )
      {
        p->alphascale = 0;
        p->alive = false;
        continue;
      }

      if (p->animated)
      {
        if (th_time() - p->last_frame_start > p->time_per_frame)
        {
          p->texture_handle.y += p->offset_per_frame_y;
          p->texture_handle.x += p->offset_per_frame_x;
          p->frame_id = p->frame_id + 1;
          p->last_frame_start = th_time();
        }
        p->blend = ((th_time() - p->last_frame_start)/p->time_per_frame);

        if (p->frame_id >= p->num_frames)
        {
          p->blend = 0;
          p->alphascale = 0;
          p->alive = false;
          continue;
        }
      }

      if (p->hasphysics )
      {
        fn_vec3 n;
        bool col_hit;
        float t = 0;
        fn_vec3 col_pos =  th_trace(w,p->position,fn_addVec3(p->position,fn_multVec3s(p->velocity,dt)),&n,&col_hit,th_getPhysicsMemory(w,thread_id),&t);

        if (t < 0.0 || t > 1.0)
        {
          // printf("%f\n",t );
          col_hit = false;
        }
        // if (fn_almostEqualf(fn_length(fn_multVec3s(p->velocity,dt)),0,0.01))
        // {
        //   col_hit = false;
        // }
        // fn_printVec3(col)
        if (col_hit )
        {

          if (p->makedecal)
          {
            th_DecalOrientation testdecal = p->decal;
            testdecal.normal = fn_multVec3(n,fn_createVec3(-1,-1,-1));
            testdecal.pos = fn_addVec3(col_pos,fn_multVec3s(testdecal.normal,2));
            //testdecal.scale = fn_createVec3(70,70,50);

            // fn_quat q;
            // if (fn_equalVec3(testdecal.normal,fn_createVec3(-1.000000 ,-0.000000, -0.000000)))
            // {
            //   q = fn_getRotationQuaternion(fn_createVec3(-1,0,0),testdecal.normal);
            // }
            // else
            // {
            //   q = fn_getRotationQuaternion(fn_createVec3(1,0,0),testdecal.normal);
            //}


            testdecal.tangent = th_getDecalTangentVector(testdecal.normal);//fn_rotatePointQuat(fn_createVec3(0,-1,0),q);
            pthread_mutex_lock(&particle_lock);
            uint32_t angle_rand = th_xoshiro128p_next(&rand_state);
            pthread_mutex_unlock(&particle_lock);
            float angl =  (angle_rand >> 8) * 0x1.0p-24f * (2.0f * 3.1415927);

            testdecal.tangent  =rotate_tangent(testdecal.tangent,testdecal.normal,angl);
          //  testdecal.material_handle = fn_createVec2(0,22);//14

            if (counts2d[thread_id]< 50)
            {
            decals_list2d[thread_id][counts2d[thread_id]] = testdecal;
            counts2d[thread_id] = counts2d[thread_id] + 1;
            }
          }

          p->alive = false;
        }
      }

      if (p->has_velocity)
      {
        p->position = fn_addVec3(p->position,fn_multVec3s(p->velocity,dt));
      }


        if (p->has_gravity)
        {
          p->velocity = fn_addVec3(p->velocity,fn_createVec3(0,dt*0.001,0));
        }


      if (p->haslight && p->lightq != NULL)
      {
        if (!p->tubelight)
        {
          th_setLight(p->lightq,p->light_id,p->lightcolor,p->position);
        }
        else
        {
          float extra = 50.0;
          fn_vec3 l0 = fn_addVec3(p->position,fn_multVec3s(p->velocity,p->stretch*p->scale + extra));
          fn_vec3 l1 = fn_addVec3(p->position,fn_multVec3s(p->velocity,-p->stretch*p->scale - extra));
          th_LightProperties lightprop = th_getDefaultLight();
          lightprop.tube = true;
          lightprop.tube_endcap = l1;
          th_setLightProperties(p->lightq,p->light_id,p->lightcolor,l0,lightprop);

          th_updateLightSingle(p->lightq,p->light_id);

          if (p->self_destruct)
          {
            th_killLightAfterRender(p->lightq,p->light_id);
          }
        }

      }


    }

  }
}

static float packBools(float a,float b,float c,float d)
{
  int retf = 0;
  if (a == 1.0)
  {
    retf = retf | 1;
  }

  if (b == 1.0)
  {
    retf = retf | 2;
  }

  if (c == 1.0)
  {
    retf = retf | 4;
  }

  if (d == 1.0)
  {
    retf = retf | 8;
  }

  return (float)retf;

}

static th_DecalOrientation** decals_list2d = NULL;
static int* counts2d = NULL;
void th_simulateParticles(float dt,fn_vec3 pos,fn_vec3 look,th_World* w)
{
  //printf("%d \n",num_lights);
  uint num_threads = (uint)th_getNumThreads();
  camera_pos = pos;
  if (decals_list2d == NULL)
  {
    decals_list2d = malloc(sizeof(th_DecalOrientation*)*num_threads);
    for (uint i = 0 ; i < num_threads;i++)
    {
      decals_list2d[i] = malloc(sizeof(th_DecalOrientation)*50);
    }
    counts2d = malloc(sizeof(int)*num_threads);
  }

  memset(counts2d,0,sizeof(int)*num_threads);


  TH_BEGIN_SCHEDULING(num_threads,th_particleComputeData,pmat_count)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].thread_id = th_thread_id;
  data[th_thread_id].dt = dt;
  data[th_thread_id].decals_list2d = decals_list2d;
  data[th_thread_id].counts2d = counts2d;
  data[th_thread_id].pos = pos;
  data[th_thread_id].look = look;
  data[th_thread_id].w = w;
  TH_SCHEDULING_FUNC
  th_setThread(thread_computeparticle,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING


  for (uint i = 0 ; i < num_threads ;i++)
  {
    for (int j = 0 ; j < counts2d[i];j++)
    {
      th_addDecal(decals_list2d[i][j]);
    }
  }

  // int iterations = pmat_count - 1;
  // for (int i = iterations ; i >= 0 ;i--)
  // {
  //   th_Particle* p = &particles_list[i];
  //   if (!p->alive)
  //   {
  //     if (p->haslight && p->lightq != NULL)
  //     {
  //       th_killLight(p->lightq,p->light_id);
  //     }
  //
  //     if (i == pmat_count - 1)
  //     {
  //       particles_list[i] = th_defaultParticle();
  //       particles_list[i].hasphysics = false;
  //       particles_list[i].tubelight = false;
  //       particles_list[i].animated = false;
  //       particles_list[i].alive = false;
  //       pmat_count--;
  //     }
  //     else if (particles_list[pmat_count - 1].alive)
  //     {
  //       particles_list[i] = particles_list[pmat_count - 1];
  //       particles_list[pmat_count - 1] = th_defaultParticle();
  //       particles_list[pmat_count - 1].hasphysics = false;
  //       particles_list[pmat_count - 1].animated = false;
  //       particles_list[pmat_count - 1].tubelight = false;
  //       particles_list[pmat_count - 1].alive = false;
  //       pmat_count--;
  //
  //     }
  //   }
  //
  // }


  for (int i = 0; i < pmat_count; )
  {
    //move top to current
    if (!particles_list[i].alive)
    {
      if (particles_list[i].haslight && particles_list[i].lightq != NULL)
      {
        th_killLight(particles_list[i].lightq,particles_list[i].light_id);
        num_lights--;
        particles_list[i].haslight = false;
        particles_list[i].lightq = NULL;
      }
      particles_list[i] = particles_list[--pmat_count];

      particles_list[pmat_count] = th_defaultParticle();
      particles_list[pmat_count].hasphysics = false;
      particles_list[pmat_count].animated = false;
      particles_list[pmat_count].tubelight = false;
      particles_list[pmat_count].alive = false;
    }
    else
    {
      i++;
    }
  }

  if (pmat_count >= 1 )
  {
    if (pmat_count < max_particles)
    {
      particle_index = pmat_count;
      if ( particles_list[pmat_count].haslight && particles_list[pmat_count].lightq != NULL)
      {
        th_killLight(particles_list[pmat_count].lightq,particles_list[pmat_count].light_id);
        num_lights--;
        particles_list[pmat_count].haslight = false;
        particles_list[pmat_count].lightq = NULL;
      }
      particles_list[particle_index] = th_defaultParticle();
      particles_list[particle_index].animated = false;
      particles_list[particle_index].alive= false;
      particles_list[particle_index].hasphysics = false;
      particles_list[particle_index].tubelight = false;

      // if (particle_index >= max_particles)
      // {
      //   particle_index = 0;
      //   pmat_count = max_particles;
      // }
    }
    // if (particle_index >= pmat_count)
    // {
    //   particle_index = pmat_count;
    // }

  }
  else
  {
    particle_index = 0;
    if ( particles_list[particle_index].haslight && particles_list[particle_index].lightq != NULL)
    {
      th_killLight(particles_list[particle_index].lightq,particles_list[particle_index].light_id);
      num_lights--;
      particles_list[particle_index].haslight = false;
      particles_list[particle_index].lightq = NULL;
    }
    particles_list[particle_index] = th_defaultParticle();
    particles_list[particle_index].animated = false;
    particles_list[particle_index].alive= false;
    particles_list[particle_index].hasphysics = false;
    particles_list[particle_index].tubelight = false;
  }



  // if (pmat_count >= 1)
  // {
  //   particle_index = pmat_count - 1;
  // }
  // else
  // {
  //   particle_index = 0;
  // }





  qsort(&particles_list[0],pmat_count,sizeof(th_Particle),compare_partcle);

  int pc = 0;
  for (int i = 0 ; i < pmat_count;i++)
  {
    th_Particle* p = &particles_list[i];
    fn_mat4 particle_mat;

    float billboard_factor = 0.0;
    if (p->constrained_billboard)
    {
      billboard_factor = 1.0;
    }

    if (p->use_add_blending )
    {
      pc++;
    }

    if (p->scale_end == -1.0)
    {
      p->scale_internal = p->scale;
    }

    float g_mul = packBools(p->use_geom_normal,p->use_add_blending,p->use_reflections,p->use_premultiplied);
    // if (p->use_geom_normal == 0.0 && p->use_add_blending == 1.0)
    // {
    //   g_mul = 2.0;
    // }
    // else if (p->use_geom_normal == 1.0 && p->use_add_blending == 1.0)
    // {
    //   g_mul = 3.0;
    // }
    // else if (p->use_geom_normal == 0.0 && p->use_add_blending == 0.0)
    // {
    //   g_mul = 0.0;
    // }
    // else if (p->use_geom_normal == 1.0 && p->use_add_blending == 0.0)
    // {
    //   g_mul = 1.0;
    // }

    particle_mat = fn_createMat4(p->position.x,p->position.y,p->position.z,p->theta,p->velocity.x,p->velocity.y,p->velocity.z,p->scale_internal,p->stretch,p->overbright,p->texture_handle.y,p->alphascale,p->texture_handle.x,p->blend,billboard_factor,g_mul);

    pmats[i] = particle_mat;
    p->index_in_array = i;

    if (p->self_destruct)
    {
      p->alphascale = 0;
      p->alive = false;
    }
  }

  // if (pc > 19)
  // {
  //   printf("%s %i\n","PRBLM",pc );
  // }

}

static void addParticleLight(th_Particle* p)
{
  if (p->haslight && p->lightq != NULL)
  {
    th_LightProperties lprop = th_getDefaultLight();
    lprop.lightthresh = p->lightthresh;
    p->light_id = th_getLight(p->lightq,lprop,p->lightcolor,p->position);
    num_lights++;
    if (p->self_destruct)
    {

      p->haslight = false;
      num_lights--;

      if (p->tubelight)
      {
        float extra = 50.0;
        fn_vec3 l0 = fn_addVec3(p->position,fn_multVec3s(p->velocity,p->stretch*p->scale + extra));
        fn_vec3 l1 = fn_addVec3(p->position,fn_multVec3s(p->velocity,-p->stretch*p->scale - extra));
        th_LightProperties lightprop = th_getLightProperties(p->lightq,p->light_id);
        lightprop.tube = true;
        lightprop.tube_endcap = l1;
        th_setLightProperties(p->lightq,p->light_id,p->lightcolor,l0,lightprop);

        th_updateLightSingle(p->lightq,p->light_id);
      }

      th_killLightAfterRender(p->lightq,p->light_id);
    }

  }
}

bool th_addParticle(th_Particle p)
{
  p.scale_internal = p.scale;
  fn_vec2 pid_explosion = particle_ids_list[TH_EXPLOSION];
  p.creation_id =  __atomic_fetch_add(&particles_created,1,__ATOMIC_RELAXED);

  //
  bool found = false;

  th_timer_t highest_life = -1.0;
  int highest_life_index = -1;

  if ( !(particles_list[particle_index].hasphysics || particles_list[particle_index].tubelight || particles_list[particle_index].animated ) )
  {

    if ( particles_list[particle_index].haslight && particles_list[particle_index].lightq != NULL)
    {
      th_killLight(particles_list[particle_index].lightq,particles_list[particle_index].light_id);
      particles_list[particle_index].haslight = false;
      particles_list[particle_index].lightq = NULL;
      num_lights--;
    }
    addParticleLight(&p);
    particles_list[particle_index] = p;
    found = true;

    particle_index++;

    if (pmat_count < particle_index && particle_index < max_particles)
    {
      if ( particles_list[particle_index].haslight && particles_list[particle_index].lightq != NULL)
      {
        th_killLight(particles_list[particle_index].lightq,particles_list[particle_index].light_id);
        num_lights--;
        particles_list[particle_index].haslight = false;
        particles_list[particle_index].lightq = NULL;
      }
        particles_list[particle_index] = th_defaultParticle();
        particles_list[particle_index].alive = false;
        particles_list[particle_index].hasphysics = false;
        particles_list[particle_index].tubelight = false;
        particles_list[particle_index].animated = false;
        pmat_count++;
    }



    if (particle_index >= max_particles)
    {
      particle_index = 0;
      pmat_count = max_particles;
    }
  }
  else
  {
    // printf("%s\n","STALL" );
    if (pmat_count != max_particles)
    {
      printf("%s\n","ERROR" );

      printf("%i %i %i\n",pmat_count,max_particles,particle_index); //0 256 0
    }

    for (int i = 0; i < pmat_count; i++) {
      if (!(particles_list[i].hasphysics || particles_list[i].tubelight || particles_list[i].animated  ))
      {
        if (particles_list[i].haslight && particles_list[i].lightq != NULL)
        {
          th_killLight(particles_list[i].lightq,particles_list[i].light_id);
          num_lights--;
          particles_list[i].haslight = false;
          particles_list[i].lightq = NULL;
        }
        addParticleLight(&p);
        particles_list[i] = p;
        found = true;
        return true;
      }
      else
      {
        if (th_time() - particles_list[i].start_time > highest_life && (!particles_list[i].animated || (highest_life_index == -1 && i == pmat_count - 1)  ))
        {
          highest_life = th_time() - particles_list[i].start_time;
          highest_life_index = i;
        }
      }
    }
  }

  if (!found && (p.hasphysics || p.tubelight || p.animated ) && highest_life_index != -1)
  {
    if (particles_list[highest_life_index].haslight && particles_list[highest_life_index].lightq != NULL)
    {
      th_killLight(particles_list[highest_life_index].lightq,particles_list[highest_life_index].light_id);
      num_lights--;

      particles_list[highest_life_index].haslight = false;
      particles_list[highest_life_index].lightq = NULL;
    }
    addParticleLight(&p);
    particles_list[highest_life_index] = p;
    found = true;
  }

  return found;



  // if (!found && p.hasphysics)
  // {
  //   printf("%s %i\n","FAIL" ,th_frame());
  // }

  // if ( p.hasphysics)
  // {
  //   particles_list[particle_index] = p;
  //
  //
  //   particle_index++;
  //
  //   if (pmat_count < particle_index)
  //   {
  //       pmat_count++;
  //   }
  //
  //
  //
  //   if (particle_index >= max_particles)
  //   {
  //     particle_index = 0;
  //     pmat_count = max_particles;
  //   }
  // }



}



th_Particle th_defaultParticle()
{
 th_Particle p;
 p.theta = 0;
 p.alive = true;
 p.life = 10000;
 p.start_time = th_time();
 p.stretch = 4;
 p.scale = 15;
 p.overbright = 1;
 p.texture_handle = th_getParticleTexture(TH_BLOOD);
 p.makedecal = false;
 p.hasphysics = false;
 p.alphascale = 1.0;
 p.alphascale_base = 1.0;
 p.has_gravity = true;
 p.animated = false;
 p.blend = 0;
 p.haslight = false;
 p.lightcolor = fn_createVec3(0,0,0);
 p.light_id = th_makeLightIDTuple(0,0);
 p.lightq = NULL;
 p.has_velocity = true;
 p.constrained_billboard = false;
 p.use_add_blending = 0.0;
 p.fade_out = true;
 p.num_frames_alive = 0;
 p.self_destruct = false;
 p.tubelight = false;
 p.creation_id = 0;
 p.index_in_array = 0;
 p.scale_end = -1.0;
 p.scale_internal = p.scale;
 p.use_geom_normal = 1.0;
 p.offset_per_frame_x = 1;
 p.offset_per_frame_y = 2;
 p.lightthresh = 0.0;
 p.use_reflections = 0.0;
 p.use_premultiplied = 0.0;
 p.fade_out_sharp = false;
 return p;
}
//
static int order_by_creation(const void* a,const void* b)
{

  const th_Particle* ap = (const th_Particle*)a;
  const th_Particle* bp = (const th_Particle*)b;

  uint64_t d1 = ap->creation_id;
  uint64_t d2 = bp->creation_id;
  if (d1 < d2)
  {
    return -1;
  }
  else
  {
    return 1;
  }
}

void th_particle_framecap(uint64_t* out_ids,fn_mat4* out_mats, int* out_count)
{
  *out_count = pmat_count;

  memcpy(particles_list_copy,particles_list,sizeof(th_Particle)*pmat_count);

  qsort(&particles_list_copy[0],pmat_count,sizeof(th_Particle),order_by_creation);


  for (int i = 0 ; i < pmat_count;i++)
  {
    th_Particle* p = &particles_list_copy[i];
    fn_mat4 particle_mat;

    // float billboard_factor = 0.0;
    // if (p->constrained_billboard)
    // {
    //   billboard_factor = 1.0;
    // }

    particle_mat = pmats[p->index_in_array];//fn_createMat4(p->position.x,p->position.y,p->position.z,p->theta,p->velocity.x,p->velocity.y,p->velocity.z,p->scale,p->stretch,p->overbright,p->texture_handle.y,p->alphascale,p->texture_handle.x,p->blend,billboard_factor,p->use_add_blending);

    out_mats[i] = particle_mat;

    out_ids[i] = p->creation_id;
  }

}

static fn_vec3 pos_static;
static fn_vec3 look_static;

static int compare_matrix(const void* a,const void* b)
{

  const fn_mat4* ap = (const fn_mat4*)a;
  const fn_mat4* bp = (const fn_mat4*)b;
  float d1 = fn_pointInPlane(fn_createVec3(ap->m[0],ap->m[1],ap->m[2]),pos_static,look_static);
  float d2 = fn_pointInPlane(fn_createVec3(bp->m[0],bp->m[1],bp->m[2]),pos_static,look_static);

  if (d1 > d2)
  {
    return -1;
  }
  else
  {
    return 1;
  }
}


fn_mat4* th_particle_playback(fn_vec3 pos,fn_vec3 look,uint64_t* a_ids,uint64_t* b_ids,fn_mat4* a_mats,fn_mat4* b_mats,int a_count,int b_count,int* out_count,float f)
{


  pos_static = pos;
  look_static = look;
  //find matching ids and interpolate between them
  int i = 0, j = 0;
  int index = 0;
  bool firstframe = f < 0.5;


  while (i < a_count && j < b_count) {
      if (a_ids[i] == b_ids[j]) {


          // printf("%d ", arr1[i]);
          //interp between a and b
          fn_vec3 pos_a = fn_createVec3(a_mats[i].m[0],a_mats[i].m[1],a_mats[i].m[2]);
          fn_vec3 pos_b = fn_createVec3(b_mats[j].m[0],b_mats[j].m[1],b_mats[j].m[2]);
          fn_vec3 pos = fn_lerpVec3(pos_a,pos_b,f);

          fn_vec3 vel_a = fn_createVec3(a_mats[i].m[4],a_mats[i].m[5],a_mats[i].m[6]);
          fn_vec3 vel_b = fn_createVec3(b_mats[j].m[4],b_mats[j].m[5],b_mats[j].m[6]);
          fn_vec3 vel = fn_lerpVec3(vel_a,vel_b,f);

          float alpha = fn_lerp(a_mats[i].m[11],b_mats[j].m[11],f);

          float blend = fn_lerp(a_mats[i].m[13],b_mats[j].m[13],f);

          float scale = fn_lerp(a_mats[i].m[7],b_mats[j].m[7],f);

          if (index < max_particles)
          {

            if (a_mats[i].m[14] == 1.0 || b_mats[i].m[14] == 1.0)
            {
              pmats[index] = a_mats[i];
            }
            else
            {
              pmats[index] = fn_createMat4(pos.x,pos.y,pos.z,a_mats[i].m[3],vel.x,vel.y,vel.z,scale,a_mats[i].m[8],a_mats[i].m[9],a_mats[i].m[10],alpha,a_mats[i].m[12],blend,a_mats[i].m[14],a_mats[i].m[15]);
            }

            index++;
          }
          else
          {
            goto finish;
          }

          i++;
          j++;
      } else if (a_ids[i] < b_ids[j]) {
          //uncommon element in a
          if (firstframe)
          {
            if (index < max_particles)
            {
              pmats[index] = a_mats[i];
              index++;
            }
            else
            {
              goto finish;
            }
          }


          i++;
      } else {
          //uncommon element in b
          if (!firstframe)
          {
            if (index < max_particles)
            {
              pmats[index] = b_mats[j];
              index++;
            }
            else
            {
              goto finish;
            }
          }

          j++;
      }
  }

  while (i < a_count) {
      //uncommon in a
      if (firstframe)
      {
        if (index < max_particles)
        {
          pmats[index] = a_mats[i];
          index++;
        }
        else
        {
          goto finish;
        }
      }

      i++;
  }

  while (j < b_count) {
      //uncommon in b
      if (!firstframe)
      {
        if (index < max_particles)
        {
          pmats[index] = b_mats[j];
          index++;
        }
        else
        {
          goto finish;
        }
      }

      j++;
  }

  finish:

  //sort by camera distance
  qsort(&pmats[0],index,sizeof(fn_mat4),compare_matrix);

  *out_count = index;
  return pmats;
}
