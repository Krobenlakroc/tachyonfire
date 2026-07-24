#include "th_eyeball.h"
#include  <math.h>
#include <stdio.h>
#include "../fn_engine/th_time.h"
#include <string.h>
#include "th_builtins.h"
#include "../fn_engine/th_threads.h"
#include "th_gems.h"
#include "../fn_engine/th_level.h"
#include "../fn_engine/th_occlusion.h"
#include "../fn_engine/th_hitmarker.h"

#define ROCKETWARMUPTIME 3500.0
#define ROCKETDWELLTIME 2000.0 //3500.0
#define ROCKET_INTERVAL_MS 7500.0
#define STRETCH_TIME_MS 2000.0

static const float EXPANSION_TIME = 750.0;
static const float SUPER_EYE_SIZE = 1.0;
static const float SUPER_EYE_SIZE_APPEARANCE = 1.3;
static const float RINGB_SUPER_SCALE = 1.1;
static const float SUPER_EYE_ROCKET_TIME = 0.5;

static fn_vec3 eyeballTarget(th_Entity* e,fn_vec3 goal,float speed,float weight)
{
  fn_vec3 target = fn_multVec3s(fn_subVec3(goal,e->aabb.position),weight);

  if (fn_length(target) > speed)
  {
    return fn_multVec3s(fn_multVec3s(target,1.f/fn_length(target)),speed);
  }
  return target;
}

static fn_vec3 eyeballLimitVelocity(th_Entity* e ,float limit)
{
  if (fn_length(e->velocity) > limit)
  {
    return fn_multVec3s(fn_multVec3s(e->velocity,1.f/fn_length(e->velocity)),limit);
  }
  return e->velocity;
}

static fn_vec3 sampleRandomSphere()
{
  float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
  float z =  (float)th_random()/(float)(RAND_MAX/2.0);
  z -= 1;
  float x = cos(theta);
  float y = sin(theta);
  fn_vec3 n = fn_normalizeVec3(fn_createVec3(x,y,z));
  return n;
}

static fn_vec3 eyeballPickTarget(fn_vec3 player,th_World* world)
{
  fn_vec3 n;
  bool is_hit = false;
  float t = 0;
  fn_vec3 d = sampleRandomSphere();
  d.z = -1;
  d = fn_normalizeVec3(d);
  fn_vec3 rayend = fn_addVec3(player,fn_multVec3s(d,1000));
  fn_vec3 p =  th_trace(world,player,rayend,&n,&is_hit,th_getPhysicsMemory(world,0),&t);
//  fn_printVec3(p);
  return p;
}

static fn_vec3 th_avoideyeballs(th_EyeballGroup* eyes,th_Entity* e,int i,float weight2,float weight3)
{

  th_EyeballData* data = eyes->data;

  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  fn_vec3 pc2 = fn_createVec3s(0.f);
  int colCount = 0;
  for (j = 0; j <eyes->count;j++)
  {

    if (j == i || !eyes->entities[j].alive ||  data[j].state == DEADEYEBALL ||  !data[j].enabled)
    {
      continue;
    }

    float mag2 = fn_length2(fn_subVec3(e->aabb.position,eyes->entities[j].aabb.position));

    fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,eyes->entities[j].aabb.position),weight2);

    //    float vel_mag = fn_length(e->velocity);

    if (mag2 < 350*350)
    {
      pc = fn_addVec3(delta,pc);

      pc2 = fn_addVec3(pc2,eyes->entities[j].velocity);
      colCount++;
    }



  }


  if (colCount > 0)
    pc2 = fn_multVec3s(pc2,1.f/colCount);


  return fn_addVec3(pc,fn_multVec3s(fn_subVec3(pc2,e->velocity),weight3));
}


static fn_vec3 avoidagents(th_CentipedeGroup* c,int count,th_Entity* e,float weight2,float weight3)
{

  th_AgentInfo* agents = c->agents;
  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  fn_vec3 pc2 = fn_createVec3s(0.f);
  int colCount = 0;
  for (j = 0; j <count;j++)
  {

    if (agents[j].hasphysics || !agents[j].spawned)
    {
      continue;
    }


    float mag2 = fn_length2(fn_subVec3(e->aabb.position,agents[j].position));

    fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,agents[j].position),weight2);

    //    float vel_mag = fn_length(e->velocity);

    if (mag2 < 350*350)
    {
      pc = fn_addVec3(delta,pc);

      pc2 = fn_addVec3(pc2,c->entities_gems[j].velocity);
      colCount++;
    }



  }


  if (colCount > 0)
    pc2 = fn_multVec3s(pc2,1.f/colCount);


  return fn_addVec3(pc,fn_multVec3s(fn_subVec3(pc2,e->velocity),weight3));
}

static fn_vec3 avoidwalkers(th_HorseGroup* horse,th_Entity* e,float weight2,float weight3)
{

  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  fn_vec3 pc2 = fn_createVec3s(0.f);
  int colCount = 0;
  for (j = 0; j <horse->count;j++)
  {
    if (!horse->data[j].spawn_finished || horse->data[j].gibbed)
    {
      continue;
    }

    float mag2 = fn_length2(fn_subVec3(e->aabb.position,horse->entities[j].aabb.position));

    fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,horse->entities[j].aabb.position),weight2);

    //    float vel_mag = fn_length(e->velocity);

    if (mag2 < 500*500)
    {
      pc = fn_addVec3(delta,pc);

      pc2 = fn_addVec3(pc2,horse->entities[j].velocity);
      colCount++;
    }



  }


  if (colCount > 0)
    pc2 = fn_multVec3s(pc2,1.f/colCount);


  return fn_addVec3(pc,fn_multVec3s(fn_subVec3(pc2,e->velocity),weight3));
}

static fn_vec3 avoidtricols(th_TricolumnGroup* tricol,th_Entity* e,float weight2,float weight3)
{

  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  fn_vec3 pc2 = fn_createVec3s(0.f);
  int colCount = 0;
  for (j = 0; j <tricol->count;j++)
  {
    if (!tricol->data[j].spawn_finished || tricol->data[j].state == TRICOL_GIBBED)
    {
      continue;
    }

    float mag2 = fn_length2(fn_subVec3(e->aabb.position,tricol->data[j].position));

    fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,tricol->data[j].position),weight2);

    //    float vel_mag = fn_length(e->velocity);

    if (mag2 < 700*700)
    {
      pc = fn_addVec3(delta,pc);
      colCount++;
    }

  }

  if (colCount > 0)
    pc2 = fn_multVec3s(pc2,1.f/colCount);

  return fn_addVec3(pc,fn_multVec3s(fn_subVec3(pc2,e->velocity),weight3));
}


void th_eyeballUpdate(th_EyeballGroup* c,float dt,fn_RawInput* input)
{
  // fn_vec3 unit = fn_createVec3(1,1,1);
  //
  // float stretch_amt = 1.45;
  // fn_vec3 stretch = fn_normalizeVec3(fn_createVec3(sqrt(1.0/stretch_amt),stretch_amt,sqrt(1.0/stretch_amt)));
  //
  // fn_vec3 squash = fn_normalizeVec3(fn_createVec3(sqrt(1.0/stretch_amt),sqrt(1.0/stretch_amt),stretch_amt));

  float warmupmult = c->super ? 0.333 : 1.0;
  float sizemult = c->super ? SUPER_EYE_SIZE : 1.0;
  fn_vec3 target_velocity = c->levelstate->player_e.velocity;
  fn_vec3 target = c->levelstate->player_e.aabb.position;
  th_World* world = c->levelstate->world;

  th_Entity* entities = c->entities;
  th_EyeballData* data = c->data;

  for (int i = 0; i < c->count; i++) {

    if (!data[i].enabled)
    {
      continue;
    }

    if (!data[i].ring_gib_a.alive &&
      !data[i].ring_gib_b.alive &&
      !data[i].eyeball_gib_a.alive &&
      !data[i].eyeball_gib_b.alive &&
      !entities[i].alive)
      {
        if(c->data[i].fadeout_ringa > 0.0 || c->data[i].fadeout_ringb > 0.0 || c->data[i].fadeout_giba > 0.0 || c->data[i].fadeout_gibb > 0.0)
        {
          c->transforms_ring_a[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_ringa,0.003*dt),c->data[i].old_ringa);
          c->transforms_ring_b[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_ringb,0.003*dt),c->data[i].old_ringb);
          c->transforms_eye_gib_a[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_giba,0.003*dt),c->data[i].old_giba);
          c->transforms_eye_gib_b[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_gibb,0.003*dt),c->data[i].old_gibb);

          c->transforms_pupil[i] = fn_makescale(fn_createVec3s(0));
        }

        continue;
      }

    if (entities[i].impact)
    {

      entities[i].impact = false;

      int particle_spawns = 0;
      for (int j = 0 ; j < entities[i].impact_count;j++)
      {
        th_Entity* projectile = (th_Entity*)entities[i].impacts[j].entity;

        if( !data[i].gibbed)
        {
          if (particle_spawns < 3)
          {
            th_spawnBlood(entities[i].impacts[j].pos,0);
            fn_vec3 direction = fn_normalizeVec3(fn_subVec3(entities[i].impacts[j].pos,entities[i].aabb.position));
            th_spawnBloodSpurt(direction,entities[i].impacts[j].pos,0);
            particle_spawns++;
          }

        }

        fn_vec3 b1 = fn_multVec3s(fn_createVec3(1,0,0),th_randomFloat(-1.0,1.0));
        fn_vec3 b2 = fn_multVec3s(fn_createVec3(0,1,0),th_randomFloat(-1.0,1.0));
        fn_vec3 b3 = fn_multVec3s(fn_createVec3(0,0,1),th_randomFloat(-1.0,1.0));
        if (c->data[i].jitter_time_a <= ((1.0/0.05)*3.14159)*5*0.5*2.0)
        {
          c->data[i].ring_jitter_a = fn_normalizeVec3(fn_addVec3(b1,fn_addVec3(b2,b3)));
          c->data[i].jitter_time_a = ((1.0/0.05)*3.14159)*5*2;
        }
        else
        {
          b1 = fn_multVec3s(fn_createVec3(1,0,0),th_randomFloat(-1.0,1.0));
          b2 = fn_multVec3s(fn_createVec3(0,1,0),th_randomFloat(-1.0,1.0));
          b3 = fn_multVec3s(fn_createVec3(0,0,1),th_randomFloat(-1.0,1.0));
          if (c->data[i].jitter_time_b <= ((1.0/0.05)*3.14159)*5*0.5*2.0)
          {
            c->data[i].ring_jitter_b = fn_normalizeVec3(fn_addVec3(b1,fn_addVec3(b2,b3)));
            c->data[i].jitter_time_b = ((1.0/0.05)*3.14159)*5*2;
          }
        }



        if (projectile->type == TH_MACHINEGUN_BULLET  )
        {
          if (data[i].state == DEADEYEBALL)
          {
            data[i].health -= 8;
          }
          else
          {
            data[i].health -= 4;
          }

          if (th_time() > data[i].stretch_timer - STRETCH_TIME_MS + 78 && data[i].stretch_permanent != 1.0)
          {
            data[i].stretch_amt = 1.25;
            data[i].stretch_axis = fn_createVec3(0,1,0);
            data[i].stretch_timer = th_time() + STRETCH_TIME_MS;
          }




        }
        else if (projectile->type == TH_SHOTGUN_SHELL )
        {
          if (data[i].state == DEADEYEBALL)
          {
            data[i].health -= 10;
          }
          else
          {
            data[i].health -= 5;
          }

          if (th_time() > data[i].stretch_timer - STRETCH_TIME_MS + 78 && data[i].stretch_permanent != 1.0)
          {
            data[i].stretch_amt = 2.0;
            data[i].stretch_axis = fn_createVec3(1,0,0);
            data[i].stretch_timer = th_time() + STRETCH_TIME_MS;
          }


        }
        else if (projectile->type == TH_HAMMER_PROJECTILE)
        {
          data[i].health -= 75;
          if( !data[i].gibbed)
          {
          th_spawnBlood(entities[i].impacts[j].pos,0);
          th_spawnBlood(entities[i].impacts[j].pos,0);
          fn_vec3 direction = fn_normalizeVec3(fn_subVec3(entities[i].impacts[j].pos,entities[i].aabb.position));
          th_spawnBloodSpurt(direction,entities[i].impacts[j].pos,0);
          th_spawnBloodSpurt(direction,entities[i].impacts[j].pos,0);
          }

          if (th_time() > data[i].stretch_timer - STRETCH_TIME_MS + 78 && data[i].stretch_permanent != 1.0)
          {
            data[i].stretch_amt = 3.0;
            data[i].stretch_axis = fn_createVec3(0,1,0);
            data[i].stretch_timer = th_time() + STRETCH_TIME_MS;
          }
        }

        th_Hitmarker hmarker;
        hmarker.entity_pos_ref = &entities[i].aabb.position;
        hmarker.entity_transform_ref = NULL;//&c->transforms_gems[i*7 + j];
        hmarker.alive_ref = &data[i].gibbed;
        hmarker.timer = th_time();
        hmarker.last_good_pos = fn_createVec3s(0.0);
        hmarker.is_alive = true;
        hmarker.radius = 100.0*sizemult*fn_clamp(data[i].stretch_amt,1.0,3.0) ;
        th_pushHitmarker(hmarker);

        if (data[i].health <= 0  && data[i].state != DEADEYEBALL)
        {
          th_markEnemyDeath(1);
          data[i].state = DEADEYEBALL;

          data[i].stretch_amt = 6.0;
          data[i].stretch_axis = fn_createVec3(0,1,0);
          data[i].stretch_timer = th_time() + STRETCH_TIME_MS*10000.0;
          data[i].stretch_permanent = 1.0;


         data[i].ring_gib_a.aabb.position = entities[i].aabb.position;
         data[i].ring_gib_b.aabb.position = entities[i].aabb.position;
         fn_vec3 nva = th_sampleRandomSphere();
         nva.y = -1;
         nva = fn_normalizeVec3(nva);

         fn_vec3 nvb = th_sampleRandomSphere();
         nvb.y = -1;
         nvb = fn_normalizeVec3(nvb);
         data[i].ring_gib_a.velocity = fn_addVec3(fn_normalizeVec3(entities[i].velocity),nva);
         data[i].ring_gib_a.velocity = fn_normalizeVec3(data[i].ring_gib_a.velocity);

         data[i].ring_gib_b.velocity = fn_addVec3(fn_normalizeVec3(entities[i].velocity),nvb);
         data[i].ring_gib_b.velocity = fn_normalizeVec3(data[i].ring_gib_b.velocity);

         entities[i].velocity.x = 0;
         entities[i].velocity.z = 0;
         entities[i].velocity.y = 0;


         th_setGameplayTimeScale(fn_createVec3(0.45,0.00000,0.0000002));

         {
           a_VirtualSource* s = a_playVirtualSource(41,0, entities[i].aabb.position,NULL);
           a_setVSLoop(s,false);
           a_setVSPos(s,entities[i].aabb.position);
           a_setVSVel(s,entities[i].velocity);
           a_setVSGain(s,1.0);
         }


        }
        else if (data[i].state == DEADEYEBALL && data[i].health <= -50 && !data[i].gibbed)
        {
          int n_gems = c->super ? 10 : 6;
          for (int k = 0 ; k < n_gems;k++)
          {
            fn_vec3 randir = th_sampleRandomSphere();
            randir.y = -fabs(randir.y)*3;
            randir = fn_multVec3s(fn_normalizeVec3(randir),0.75);

            th_gemSpawn(c->levelstate->gems,entities[i].aabb.position,randir);
          }

          th_setGameplayTimeScale(fn_createVec3(0.4,0.00000,0.0000002));

          {
            a_VirtualSource* s = a_playVirtualSource(34,0, entities[i].aabb.position,NULL);
            a_setVSLoop(s,false);
            a_setVSPos(s,entities[i].aabb.position);
            a_setVSVel(s,entities[i].velocity);
            a_setVSGain(s,1.0);
          }

          entities[i].alive = false;
          //spawn gibs, despawn main eyeball
          data[i].gibbed = true;

          data[i].eyeball_gib_a.aabb.position = entities[i].aabb.position;
          data[i].eyeball_gib_b.aabb.position = entities[i].aabb.position;


          // data[i].eyeball_gib_a.velocity = fn_createVec3s(0);
          // data[i].eyeball_gib_b.velocity = fn_createVec3s(0);
          fn_vec3 nva = th_sampleRandomSphere();
          nva.y = -1;
          nva = fn_normalizeVec3(nva);

          fn_vec3 nvb = th_sampleRandomSphere();
          nvb.y = -1;
          nvb = fn_normalizeVec3(nvb);

          fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,0,1),(fn_multVec3(data[i].gaze,fn_createVec3(1,1,-1))));
          fn_vec3 flydir = fn_rotatePointQuat(fn_createVec3(1,0,0),q_temp);

          data[i].eyeball_gib_a.velocity = fn_multVec3s(fn_normalizeVec3(flydir),0.5);//fn_addVec3(fn_normalizeVec3(entities[i].velocity),nva);
          // data[i].eyeball_gib_a.velocity = fn_normalizeVec3(data[i].eyeball_gib_a.velocity);

          data[i].eyeball_gib_b.velocity = fn_multVec3s(data[i].eyeball_gib_a.velocity,-1);//fn_addVec3(fn_normalizeVec3(entities[i].velocity),nvb);
          // data[i].eyeball_gib_b.velocity = fn_normalizeVec3(data[i].eyeball_gib_b.velocity);

        }



      }
      entities[i].impact_count = 0;

    }
    else if (data[i].state == DEADEYEBALL && data[i].health <= -50 && !data[i].gibbed)
    {
      int n_gems = c->super ? 10 : 6;
      for (int k = 0 ; k < n_gems;k++)
      {
        fn_vec3 randir = th_sampleRandomSphere();
        randir.y = -fabs(randir.y)*3;
        randir = fn_multVec3s(fn_normalizeVec3(randir),0.75);

        th_gemSpawn(c->levelstate->gems,entities[i].aabb.position,randir);
      }

      th_setGameplayTimeScale(fn_createVec3(0.4,0.00000,0.0000002));

      {
        a_VirtualSource* s = a_playVirtualSource(34,0, entities[i].aabb.position,NULL);
        a_setVSLoop(s,false);
        a_setVSPos(s,entities[i].aabb.position);
        a_setVSVel(s,entities[i].velocity);
        a_setVSGain(s,1.0);
      }

      entities[i].alive = false;
      //spawn gibs, despawn main eyeball
      data[i].gibbed = true;

      data[i].eyeball_gib_a.aabb.position = entities[i].aabb.position;
      data[i].eyeball_gib_b.aabb.position = entities[i].aabb.position;


      // data[i].eyeball_gib_a.velocity = fn_createVec3s(0);
      // data[i].eyeball_gib_b.velocity = fn_createVec3s(0);
      fn_vec3 nva = th_sampleRandomSphere();
      nva.y = -1;
      nva = fn_normalizeVec3(nva);

      fn_vec3 nvb = th_sampleRandomSphere();
      nvb.y = -1;
      nvb = fn_normalizeVec3(nvb);

      fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,0,1),(fn_multVec3(data[i].gaze,fn_createVec3(1,1,-1))));
      fn_vec3 flydir = fn_rotatePointQuat(fn_createVec3(1,0,0),q_temp);

      data[i].eyeball_gib_a.velocity = fn_multVec3s(fn_normalizeVec3(flydir),0.5);//fn_addVec3(fn_normalizeVec3(entities[i].velocity),nva);
      // data[i].eyeball_gib_a.velocity = fn_normalizeVec3(data[i].eyeball_gib_a.velocity);

      data[i].eyeball_gib_b.velocity = fn_multVec3s(data[i].eyeball_gib_a.velocity,-1);//fn_addVec3(fn_normalizeVec3(entities[i].velocity),nvb);
      // data[i].eyeball_gib_b.velocity = fn_normalizeVec3(data[i].eyeball_gib_b.velocity);

    }

    float rocker_time_offset = c->super ? ROCKET_INTERVAL_MS*SUPER_EYE_ROCKET_TIME : ROCKET_INTERVAL_MS;

    bool force_rocket = false;//input->currentKeyStates[SDL_SCANCODE_5] && !input->currentKeyStatesPrev[SDL_SCANCODE_5];

    if (data[i].state == BIRTH )
    {
      if (c->super)
      {
        data[i].target = target;
      }
      else
      {
        data[i].target = eyeballPickTarget(target,world);
      }

      //set state to traveling
      data[i].state = TRAVELING;
      data[i].last_time_targeted = th_time() + (float)th_random()/(float)(RAND_MAX/(2500));
    }
    else if ((th_time() >= data[i].last_time_targeted + rocker_time_offset && data[i].state != PREPARINGROCKET && data[i].state != ROCKETING && data[i].state != DEADEYEBALL && data[i].state != EYE_SPAWNING) || force_rocket)
    {
        data[i].state = PREPARINGROCKET;
        data[i].time_started_rocket = th_time();
        {

          if (  data[i].audio_source_scream == NULL)
          {
            data[i].audio_source_scream = a_playVirtualSource(35,0, entities[i].aabb.position,NULL);
            a_setVSLoop(data[i].audio_source_scream,false);
            a_setVSPos(data[i].audio_source_scream,entities[i].aabb.position);
            a_setVSVel(data[i].audio_source_scream,entities[i].velocity);
            a_setVSGain(data[i].audio_source_scream,1.0);
            a_setVSCleanup(data[i].audio_source_scream,&data[i].audio_source_scream,a_standardCleanup);
          }
          else
          {
            a_setVSLoop(data[i].audio_source_scream,false);
            a_setVSPos(data[i].audio_source_scream,entities[i].aabb.position);
            a_setVSVel(data[i].audio_source_scream,entities[i].velocity);
            a_setVSGain(data[i].audio_source_scream,1.0);
            a_setVSCleanup(data[i].audio_source_scream,&data[i].audio_source_scream,a_standardCleanup);
            a_setVSOffset(data[i].audio_source_scream,0.0);
            a_playVS(data[i].audio_source_scream);
          }




        }
    }

    fn_vec3 tpos = entities[i].velocity;
    float time_until = 0;
    float accel_coeff = 0.001*5;
    float max_speed = 0.8;
    float interp_alpha = 0.0;
    switch (data[i].state) {
      case EYE_SPAWNING:

      //float spd_scale = fn_min(1.0,(th_time() - data[i].spawn_time)/EXPANSION_TIME   );
      if ((th_time() - data[i].spawn_time) <= EXPANSION_TIME)
      {

      }
      else{
        data[i].state = BIRTH;
      }
      break;
      case BIRTH:
      //determine target
      if (c->super)
      {
        data[i].target = target;
      }
      else
      {
        data[i].target = eyeballPickTarget(target,world);
      }
      //set state to traveling
      data[i].state = TRAVELING;
      data[i].last_time_targeted = th_time();
      break;
      case WALLRIDING:
      //just side along the sliding direction for a little bit
      // if (!entities[i].collided)
      // {
      //   data[i].state = TRAVELING;
      //   data[i].last_time_wallrode = th_time();
      // }
       if ((th_time() < data[i].last_time + 1000) )
      {
        fn_vec3 tt = fn_addVec3(entities[i].aabb.position,fn_multVec3s(fn_normalizeVec3(entities[i].velocity),20));
        entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(eyeballTarget(&entities[i],tt,0.45,1.0),dt*accel_coeff));
          //float spd_scale = fn_min(1.0,(th_time() - data[i].spawn_time)/EXPANSION_TIME   );
        entities[i].velocity = eyeballLimitVelocity(&entities[i],max_speed);
      }
      else
      {
        data[i].state = TRAVELING;
        data[i].last_time_wallrode = th_time();
        {
          a_VirtualSource* s = a_playVirtualSource(37,-1,entities[i].aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entities[i].aabb.position);
          a_setVSVel(s,entities[i].velocity);
          a_setVSGain(s,0.4);
        }
      }
      //if its been like 3 seconds, switch to traveling

      break;
      case TRAVELING:
      //goto the target
      if (!(fn_length2(fn_subVec3(entities[i].aabb.position,data[i].target)) < pow(400,2)) )
      {
        //printf("%s\n","Seeking" );
        fn_vec3 olast = data[i].last_position;

        entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(eyeballTarget(&entities[i],data[i].target,0.45,1.0),dt*accel_coeff));

        //float spd_scale = fn_min(1.0,(th_time() - data[i].spawn_time)/EXPANSION_TIME   );
        entities[i].velocity = eyeballLimitVelocity(&entities[i],max_speed);

        fn_vec3 los = fn_normalizeVec3(fn_subVec3(data[i].target,entities[i].aabb.position));
        bool prog = fn_length2(fn_subVec3(data[i].target,entities[i].aabb.position)) < fn_length2(fn_subVec3(data[i].target,olast));
        if ((entities[i].collided && fn_dot(fn_normalizeVec3(entities[i].velocity),los) <= 0) || !prog )
        {
          // if ((th_time() >= data[i].last_time_wallrode + 200) )
          // {
            data[i].state = WALLRIDING;
            data[i].last_time = th_time();
            {
              a_VirtualSource* s = a_playVirtualSource(36,-1,entities[i].aabb.position,NULL );
              a_setVSLoop(s,false);
              a_setVSPos(s,entities[i].aabb.position);
              a_setVSVel(s,entities[i].velocity);
              a_setVSGain(s,1.0);
            }
        //  }
        //printf("%s\n","Walriding" );
        }


      }
      else
      {
        data[i].state = PREPARINGROCKET;
        data[i].time_started_rocket = th_time();
        if (  data[i].audio_source_scream == NULL)
        {
          data[i].audio_source_scream = a_playVirtualSource(35,0, entities[i].aabb.position,NULL);
          a_setVSLoop(data[i].audio_source_scream,false);
          a_setVSPos(data[i].audio_source_scream,entities[i].aabb.position);
          a_setVSVel(data[i].audio_source_scream,entities[i].velocity);
          a_setVSGain(data[i].audio_source_scream,1.0);
          a_setVSCleanup(data[i].audio_source_scream,&data[i].audio_source_scream,a_standardCleanup);
        }
        else
        {
          a_setVSLoop(data[i].audio_source_scream,false);
          a_setVSPos(data[i].audio_source_scream,entities[i].aabb.position);
          a_setVSVel(data[i].audio_source_scream,entities[i].velocity);
          a_setVSGain(data[i].audio_source_scream,1.0);
          a_setVSCleanup(data[i].audio_source_scream,&data[i].audio_source_scream,a_standardCleanup);
          a_setVSOffset(data[i].audio_source_scream,0.0);
          a_playVS(data[i].audio_source_scream);
        }
      }

      //if at target, switch to rocketing

      //if not any closer than last frame, switch to wallriding



      break;
      case PREPARINGROCKET:

      interp_alpha = (th_time() - data[i].time_started_rocket)/(ROCKETWARMUPTIME*warmupmult);
      interp_alpha = fn_clamp(interp_alpha,0.0,1.0);

      interp_alpha = 1.0 - exp((-interp_alpha)/0.2);
      data[i].pupilsize = fn_lerp(1.0,2.4,interp_alpha);

      if ((th_time() - data[i].time_started_rocket) > (ROCKETWARMUPTIME*warmupmult))
      {
        data[i].state = ROCKETING;
        {
          a_VirtualSource* s = a_playVirtualSource(39,-1,entities[i].aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entities[i].aabb.position);
          a_setVSVel(s,entities[i].velocity);
          a_setVSGain(s,1.0);
        }
      }

       time_until  = fn_length(fn_subVec3(target,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));
      time_until  = fn_length(fn_subVec3(tpos,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));
      time_until  = fn_length(fn_subVec3(tpos,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));
      time_until  = fn_length(fn_subVec3(tpos,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));
      data[i].gaze = fn_normalizeVec3(fn_subVec3(tpos,entities[i].aabb.position));

      break;
      case ROCKETING:

       time_until  = fn_length(fn_subVec3(target,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));
      time_until  = fn_length(fn_subVec3(tpos,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));
      time_until  = fn_length(fn_subVec3(tpos,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));
      time_until  = fn_length(fn_subVec3(tpos,entities[i].aabb.position))/1.5;
      tpos = fn_addVec3(target,fn_multVec3s(target_velocity,time_until));

      fn_vec3 rdir = fn_normalizeVec3(fn_subVec3(tpos,entities[i].aabb.position));
      data[i].gaze = fn_normalizeVec3(fn_subVec3(tpos,entities[i].aabb.position));
      if (!data[i].fired_volley)
      {
        float eye_fire_offset = c->super ? 500*SUPER_EYE_ROCKET_TIME : 500;
        if ((th_time() - data[i].last_fire_time) > eye_fire_offset)
        {
          data[i].last_fire_time = th_time();
          data[i].shots_fired = data[i].shots_fired + 1;
        //  printf("%s\n","Rocketing" );



        th_rocketSpawn(c->levelstate->rocket,entities[i].aabb.position,fn_multVec3s(rdir,1),fn_multVec3s(rdir,1));

      //fn_normalizeVec3(fn_lerpVec3(data[i].gaze,fn_normalizeVec3(fn_subVec3(tpos,entities[i].aabb.position)),0.05));
        }

        int num_rockets_fire = c->super ? 6 : 3;
        if (data[i].shots_fired > num_rockets_fire)
        {
          data[i].fired_volley = true;
          data[i].dwell_timer = th_time() + ROCKETDWELLTIME;
        }
      }
      else if (th_time() > data[i].dwell_timer)
      {
        if (c->super)
        {
          data[i].target = target;
        }
        else
        {
          data[i].target = eyeballPickTarget(target,world);
        }
        data[i].state = TRAVELING;
        data[i].last_time_targeted = th_time();
        data[i].fired_volley = false;
        data[i].shots_fired = 0;
      }
      else
      {
        float interp_alpha = ( data[i].dwell_timer - th_time())/(ROCKETDWELLTIME);
        interp_alpha = 1.0 - interp_alpha;
        interp_alpha = fn_clamp(interp_alpha,0.0,1.0);

        interp_alpha = 1.0 - exp((-interp_alpha)/0.2);
        data[i].pupilsize = fn_lerp(1.0,2.4,1.0 - interp_alpha);
      }

      //shoot a rocket at the player, based on player's velocity

      //pick a new target and switch to traveling


      break;
      default:
      break;
    }

    fn_vec3 impulse = fn_createVec3s(0);
    if (data[i].state != DEADEYEBALL)
    {
      // for (int j =0;j < c->count ;j++)
      // {
      //   if (j != i && data[j].state != DEADEYEBALL &&  data[j].enabled)
      //   {
      //     th_AABB a;
      //
      //     a = entities[j].aabb;
      //
      //     a.hwidth = fn_addVec3(fn_multVec3s(a.hwidth,2),fn_createVec3s(1));
      //
      //     th_AABB b = entities[i].aabb;
      //     b.hwidth = fn_addVec3(fn_multVec3s(b.hwidth,2),fn_createVec3s(1));
      //     if (fn_aabbCheck(a,b))
      //     {
      //
      //       fn_vec3 sub = fn_subVec3(entities[i].aabb.position,entities[j].aabb.position);
      //       fn_vec3 diff = fn_multVec3s(fn_normalizeVec3(sub),fn_length(sub)*0.05);
      //       impulse = fn_addVec3(impulse,diff);
      //     }
      //
      //   }
      // }

      impulse = th_avoideyeballs(c,&entities[i],i,0.000015*0.25,0.00002);

      float seperation = 0.000015;
      float directional = 0.00002;

      fn_vec3 r5 = c->levelstate->centipede == NULL ? fn_createVec3s(0) : avoidagents(c->levelstate->centipede,c->levelstate->centipede->count,&entities[i],seperation*1.0,directional);


      bool spawn_delay_force = th_time() - c->data[i].spawn_time > 1000;

      th_HorseGroup* hptr = c->levelstate->horse;
      fn_vec3 r7 = (hptr == NULL || !spawn_delay_force) ? fn_createVec3s(0) :  avoidwalkers(hptr,&entities[i],seperation*1.0,directional);

      th_TricolumnGroup* tptr = c->levelstate->tricolumn;
      fn_vec3 r8 = (tptr == NULL || !spawn_delay_force) ? fn_createVec3s(0) :  avoidtricols(tptr,&entities[i],seperation*1.0,directional);

      impulse = fn_addVec3(impulse,r5);
      impulse = fn_addVec3(impulse,r7);
      impulse = fn_addVec3(impulse,r8);
    }


    if (data[i].state == DEADEYEBALL)
    {
      entities[i].playercollideable = false;
      entities[i].aabb.hwidth = fn_createVec3s(75*sizemult);


      if (  data[i].audio_source_scream == NULL && ! data[i].gibbed)
      {
        data[i].audio_source_scream = a_playVirtualSource(38,0, entities[i].aabb.position,NULL);
        a_setVSLoop(data[i].audio_source_scream,false);
        a_setVSPos(data[i].audio_source_scream,entities[i].aabb.position);
        a_setVSVel(data[i].audio_source_scream,entities[i].velocity);
        a_setVSGain(data[i].audio_source_scream,1.0);
        a_setVSCleanup(data[i].audio_source_scream,&data[i].audio_source_scream,a_standardCleanup);

        th_Hitmarker hmarker;
        hmarker.entity_pos_ref = &entities[i].aabb.position;
        hmarker.entity_transform_ref = NULL;//&c->transforms_gems[i*7 + j];
        hmarker.alive_ref = &data[i].gibbed;
        hmarker.timer = th_time();
        hmarker.last_good_pos = fn_createVec3s(0.0);
        hmarker.is_alive = true;
        hmarker.radius = 100.0*sizemult*2.0 ;
        th_pushHitmarker(hmarker);

        data[i].health = data[i].health - 0.5;
      }
    }

    if (!fn_equalVec3(impulse,fn_createVec3s(0)) && data[i].state != DEADEYEBALL)
    {
      // th_Entity beforeimpluse = entities[i];
      // entities[i].velocity = impulse;
      //
      // entities[i].aabb.hwidth = fn_createVec3s(150*sizemult);
      // th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,0));
      // entities[i].aabb.hwidth = fn_createVec3s(75*sizemult);
      //
      // fn_vec3 position_new = entities[i].aabb.position;
      // entities[i] = beforeimpluse;
      // entities[i].aabb.position = position_new;

      entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(impulse,dt));

    }



    data[i].last_position = entities[i].aabb.position;
    if (data[i].state == DEADEYEBALL)
    {
      th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,0));
    }
    else
    {
      entities[i].aabb.hwidth = fn_createVec3s(150*sizemult);
      th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,0));
      entities[i].aabb.hwidth = fn_createVec3s(75*sizemult);
    }



    if (!entities[i].grounded  )
    {
      entities[i].velocity.y += 0.001*dt*0.25;
    }



    fn_vec3 s = fn_createVec3s(fn_max(fn_min(1.0,(th_time() - data[i].spawn_time)/EXPANSION_TIME   ),0.5  ));
    s = fn_multVec3s(s,c->super ? SUPER_EYE_SIZE*SUPER_EYE_SIZE_APPEARANCE : 1.0);
    float offset = i*0.01;

    // if (data[i].state == DEADEYEBALL ))

    if (data[i].state == ROCKETING || data[i].state == PREPARINGROCKET || data[i].state == DEADEYEBALL )
    {

    //fn_normalizeVec3(fn_lerpVec3(data[i].gaze,fn_normalizeVec3(fn_subVec3(tpos,entities[i].aabb.position)),0.05));
    }
    else
    {
        data[i].gaze = fn_normalizeVec3(fn_lerpVec3(data[i].gaze,entities[i].velocity,0.05));
    }


    fn_vec3 up = fn_createVec3(0,-1,0);
    if (fn_almostEqualf(fn_dot(fn_normalizeVec3(data[i].gaze),up),1.0,1e-5) ||  fn_almostEqualf(fn_dot(fn_normalizeVec3(data[i].gaze),up),-1.0,1e-5))
    {
      up = fn_createVec3(1,0,0);
    }

    fn_mat4 view = fn_lookat(entities[i].aabb.position,fn_addVec3(entities[i].aabb.position,data[i].gaze),up);
    //fn_printVec3(fn_normalizeVec3(entities[i].velocity));
    fn_mat4 iview = fn_inverse(view);

    fn_quat q_temp1 = fn_getRotationQuaternion(fn_createVec3(0,0,1),(fn_multVec3(data[i].gaze,fn_createVec3(1,1,-1))));

    fn_vec3 stretch_modifier = fn_createVec3(1.0,1.0,1.0);
    if (th_time() < data[i].stretch_timer)
    {
       float alpha_interp = (data[i].stretch_timer - th_time())/(STRETCH_TIME_MS);

       if (data[i].stretch_permanent == 1.0)
       {
         alpha_interp = fmod((data[i].stretch_timer - th_time())/(STRETCH_TIME_MS),1.0);
       }

       alpha_interp = 1.0 - alpha_interp;
       alpha_interp = fn_clamp(alpha_interp,0.0,1.0);

       //float mval = exp(-(3*3.141597)/(2*40.0*0.19));
       // float mag = data[i].stretch_amt*sin(alpha_interp*40.0 - asin(mval))*exp(-(alpha_interp/0.19)) + data[i].stretch_amt*mval + 1.0;
       float n_oscil = 5;
       float sterm = sin(3.14159*n_oscil*alpha_interp);
       float mag = 1.0 + sterm*sterm*exp(-((data[i].stretch_amt*alpha_interp*(1.0 - alpha_interp))));

       fn_vec3 stret = fn_multVec3s(data[i].stretch_axis,mag);
       float comp = sqrt(1.0/mag);
       if (data[i].stretch_axis.x == 0.0)
       {
         stret.x = comp;
       }
       if (data[i].stretch_axis.y == 0.0)
       {
         stret.y = comp;
       }
       if (data[i].stretch_axis.z == 0.0)
       {
         stret.z = comp;
       }

       stretch_modifier = stret;
    }

    c->transforms[i] = fn_translaterotatescaleq(entities[i].aabb.position,q_temp1,fn_multVec3(s,stretch_modifier));//fn_multMat4(fn_makescale(s),iview);

    fn_mat4 pupilpos = fn_maketranslate(fn_multVec3s(c->pupil_center,1.0));
    fn_mat4 pupilneg = fn_maketranslate(fn_multVec3s(c->pupil_center,-1.0));
    fn_mat4 pupilscale = fn_multMat4(fn_multMat4(pupilneg,fn_makescale(fn_createVec3s(fn_clamp(data[i].pupilsize,1.0,2.4)))),pupilpos);


    float tr_alpha = (fn_clamp(data[i].pupilsize,1.0,2.4) - 1.0)/(2.4 - 1.0);

    float tr = fn_lerp(0.1,0.25,tr_alpha);

    pupilscale = fn_multMat4(pupilscale,fn_maketranslate(fn_createVec3(0,0,tr)));

    c->transforms_pupil[i] = fn_multMat4(pupilscale,fn_translaterotatescaleq(entities[i].aabb.position,q_temp1,fn_multVec3s(fn_multVec3(s,stretch_modifier),25)));

    float rspeed = 0.006;
    if (data[i].state == PREPARINGROCKET  )
    {
      float alpha = fn_clamp((th_time() - data[i].time_started_rocket)/(ROCKETWARMUPTIME*warmupmult),0,1);
      rspeed = 0.006*( 1 - alpha) + alpha*0.012*4;
    }
    else if (data[i].state == ROCKETING  )
    {
      rspeed = 0.001;
    }
    else if (data[i].state == DEADEYEBALL)
    {
      rspeed = 0.0000;
    }

     if (c->data[i].jitter_time_b <= 0.0 && c->data[i].jitter_time_a <= 0.0)
     {
      data[i].rotation += rspeed*dt;
    }

  //  c->transforms[i] = fn_translaterotatescaleq(entities[i].aabb.position,fn_createVec4(0,0,0,1),s);

    fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(1,0,0),(fn_multVec3(data[i].gaze,fn_createVec3(1,-1,1))));

    if (data[i].state != ROCKETING)
    {
      q_temp = fn_createVec4(0,0,0,1);
    }

    fn_quat qa = fn_makeQuaternion(data[i].rotation + offset,fn_normalizeVec3(fn_createVec3(1,-1,0)));
    fn_quat qb = fn_makeQuaternion(data[i].rotation + offset,fn_normalizeVec3(fn_createVec3(0,1,1)));

    if (data[i].state == DEADEYEBALL)
    {

      th_Entity* entity = &data[i].ring_gib_a;
      fn_vec3 oldvelocity = entity->velocity;
      th_updateEntity(entity,world,dt,th_getPhysicsMemory(world,0));
      if (!entity->grounded)
      {
        entity->velocity.y += 0.001*dt;
      }
      if (entity->collided){

        if (fn_length2(entity->velocity) > 0.05*0.05){
          a_VirtualSource* s = a_playVirtualSource(55,-4,entity->aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entity->aabb.position);
          a_setVSVel(s,entity->velocity);
          a_setVSGain(s,fn_remap(fn_length(entity->velocity),0,0.5,0.5,0.7));
          a_setVSPitch(s,fn_remap(fn_length(entity->velocity),0,0.5,0.7,1.0));
        }

        float dot = fn_dot(oldvelocity,entity->collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
        fn_vec3 u = fn_multVec3s(entity->collision_normal , dot);
        fn_vec3 w = fn_subVec3(oldvelocity , u);
        entity->velocity = fn_multVec3s(fn_subVec3(w , u),0.75);//reduce bounce height 0.99,0.75,0.99
        for (int k = 0 ; k < 3;k++)
        {
          if (fn_almostEqualf(entity->velocity.v[k],0,0.05))
          {
            entity->velocity.v[k] = 0;
          }
        }
        if (fn_almostEqualf(entity->velocity.v[1],0,0.05) && !fn_isIdentity(c->data[i].old_ringa))
        {
          entity->alive = false;
        }

      }

      if (!entity->alive)
      {
        c->transforms_ring_a[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_ringa,0.003*dt),c->data[i].old_ringa);
      }
      else
      {
        c->transforms_ring_a[i] = fn_translaterotatescaleq(entity->aabb.position,qa,s);
        c->data[i].old_ringa = c->transforms_ring_a[i];
      }



      c->frustum_data->min_x[c->frustum_offset_ring_a + i] = entity->aabb.position.x - entity->aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_ring_a + i] = entity->aabb.position.y - entity->aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_ring_a + i] = entity->aabb.position.z - entity->aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_ring_a + i] = entity->aabb.position.x + entity->aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_ring_a + i] = entity->aabb.position.y + entity->aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_ring_a + i] = entity->aabb.position.z + entity->aabb.hwidth.z;

      entity = &data[i].ring_gib_b;
      oldvelocity = entity->velocity;
      th_updateEntity(entity,world,dt,th_getPhysicsMemory(world,0));
      if (!entity->grounded)
      {
        entity->velocity.y += 0.001*dt;
      }
      if (entity->collided){

        if (fn_length2(entity->velocity) > 0.05*0.05){
          a_VirtualSource* s = a_playVirtualSource(55,-4,entity->aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entity->aabb.position);
          a_setVSVel(s,entity->velocity);
          a_setVSGain(s,fn_remap(fn_length(entity->velocity),0,0.5,0.5,0.7));
          a_setVSPitch(s,fn_remap(fn_length(entity->velocity),0,0.5,0.7,1.0));
        }

        float dot = fn_dot(oldvelocity,entity->collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
        fn_vec3 u = fn_multVec3s(entity->collision_normal , dot);
        fn_vec3 w = fn_subVec3(oldvelocity , u);
        entity->velocity = fn_multVec3s(fn_subVec3(w , u),0.75);//reduce bounce height 0.99,0.75,0.99
        for (int k = 0 ; k < 3;k++)
        {
          if (fn_almostEqualf(entity->velocity.v[k],0,0.05))
          {
            entity->velocity.v[k] = 0;
          }
        }
        if (fn_almostEqualf(entity->velocity.v[1],0,0.05) && !fn_isIdentity(c->data[i].old_ringb))
        {
          entity->alive = false;
        }

      }

      if (!entity->alive)
      {
        c->transforms_ring_b[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_ringb,0.003*dt),c->data[i].old_ringb);
      }
      else
      {
        c->transforms_ring_b[i] = fn_translaterotatescaleq(entity->aabb.position,qb,fn_multVec3s(s,c->super ? RINGB_SUPER_SCALE : 1.0));
        c->data[i].old_ringb = c->transforms_ring_b[i];
      }



      c->frustum_data->min_x[c->frustum_offset_ring_b + i] = entity->aabb.position.x - entity->aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_ring_b + i] = entity->aabb.position.y - entity->aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_ring_b + i] = entity->aabb.position.z - entity->aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_ring_b + i] = entity->aabb.position.x + entity->aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_ring_b + i] = entity->aabb.position.y + entity->aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_ring_b + i] = entity->aabb.position.z + entity->aabb.hwidth.z;


    }
    else
    {

      fn_vec3 ringa_jitter = fn_multVec3s(c->data[i].ring_jitter_a ,sin(c->data[i].jitter_time_a*0.012)*c->data[i].jitter_time_a*0.17 );
      c->data[i].jitter_time_a -= dt;
      if (c->data[i].jitter_time_a <= 0.0)
      {
        c->data[i].jitter_time_a = 0.0;
      }

      fn_vec3 ringb_jitter = fn_multVec3s(c->data[i].ring_jitter_b ,cos(c->data[i].jitter_time_b*0.012)*c->data[i].jitter_time_b*0.17 );
      c->data[i].jitter_time_b -= dt;
      if (c->data[i].jitter_time_b <= 0.0)
      {
        c->data[i].jitter_time_b = 0.0;
      }

      c->transforms_ring_a[i] = fn_translaterotatescaleq(fn_addVec3(entities[i].aabb.position,ringa_jitter),qa,s);
      c->transforms_ring_b[i] = fn_translaterotatescaleq(fn_addVec3(entities[i].aabb.position,ringb_jitter),qb,fn_multVec3s(s,c->super ? RINGB_SUPER_SCALE : 1.0));
      c->data[i].old_ringa = c->transforms_ring_a[i];
      c->data[i].old_ringb = c->transforms_ring_b[i];

      c->frustum_data->min_x[c->frustum_offset_ring_a + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_ring_a + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_ring_a + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_ring_a + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_ring_a + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_ring_a + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;

      c->frustum_data->min_x[c->frustum_offset_ring_b + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_ring_b + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_ring_b + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_ring_b + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_ring_b + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_ring_b + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;


    }

    if (data[i].gibbed)
    {
      c->transforms[i] = fn_makescale(fn_createVec3s(0));
      c->transforms_pupil[i] = fn_makescale(fn_createVec3s(0));

      th_Entity* entity = &data[i].eyeball_gib_a;
      fn_vec3 oldvelocity = entity->velocity;
      th_updateEntity(entity,world,dt,th_getPhysicsMemory(world,0));
      if (!entity->grounded)
      {
        entity->velocity.y += 0.001*dt;
      }
      if (entity->collided){
        float dot = fn_dot(oldvelocity,entity->collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
        fn_vec3 u = fn_multVec3s(entity->collision_normal , dot);
        fn_vec3 w = fn_subVec3(oldvelocity , u);
        entity->velocity = fn_multVec3s(fn_subVec3(w , u),0.75);//reduce bounce height 0.99,0.75,0.99
        for (int k = 0 ; k < 3;k++)
        {
          if (fn_almostEqualf(entity->velocity.v[k],0,0.05))
          {
            entity->velocity.v[k] = 0;
          }
        }
        if (fn_almostEqualf(entity->velocity.v[1],0,0.05) && !fn_isIdentity(c->data[i].old_giba))
        {
          entity->alive = false;
        }

      }


      fn_quat q_temp1 = fn_getRotationQuaternion(fn_createVec3(0,0,1),(fn_multVec3(data[i].gaze,fn_createVec3(1,1,-1))));

      if (!entity->alive)
      {
        c->transforms_eye_gib_a[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_giba,0.003*dt),c->data[i].old_giba);
      }
      else
      {
        c->transforms_eye_gib_a[i] = fn_translaterotatescaleq(entity->aabb.position,q_temp1,s);
        c->data[i].old_giba = c->transforms_eye_gib_a[i];
      }

      c->frustum_data->min_x[c->frustum_offset_eye_gib_a + i] = entity->aabb.position.x - entity->aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_eye_gib_a + i] = entity->aabb.position.y - entity->aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_eye_gib_a + i] = entity->aabb.position.z - entity->aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_eye_gib_a + i] = entity->aabb.position.x + entity->aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_eye_gib_a + i] = entity->aabb.position.y + entity->aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_eye_gib_a + i] = entity->aabb.position.z + entity->aabb.hwidth.z;

      entity = &data[i].eyeball_gib_b;
      oldvelocity = entity->velocity;
      th_updateEntity(entity,world,dt,th_getPhysicsMemory(world,0));
      if (!entity->grounded)
      {
        entity->velocity.y += 0.001*dt;
      }
      if (entity->collided){
        float dot = fn_dot(oldvelocity,entity->collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
        fn_vec3 u = fn_multVec3s(entity->collision_normal , dot);
        fn_vec3 w = fn_subVec3(oldvelocity , u);
        entity->velocity = fn_multVec3s(fn_subVec3(w , u),0.75);//reduce bounce height 0.99,0.75,0.99
        for (int k = 0 ; k < 3;k++)
        {
          if (fn_almostEqualf(entity->velocity.v[k],0,0.05))
          {
            entity->velocity.v[k] = 0;
          }
        }
        if (fn_almostEqualf(entity->velocity.v[1],0,0.05) && !fn_isIdentity(c->data[i].old_gibb))
        {
          entity->alive = false;
        }

      }


      if (!entity->alive)
      {
        c->transforms_eye_gib_b[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_gibb,0.003*dt),c->data[i].old_gibb);
      }
      else
      {
        c->transforms_eye_gib_b[i] = fn_translaterotatescaleq(entity->aabb.position,q_temp1,s);
        c->data[i].old_gibb = c->transforms_eye_gib_b[i];
      }


      c->frustum_data->min_x[c->frustum_offset_eye_gib_b + i] = entity->aabb.position.x - entity->aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_eye_gib_b + i] = entity->aabb.position.y - entity->aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_eye_gib_b + i] = entity->aabb.position.z - entity->aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_eye_gib_b + i] = entity->aabb.position.x + entity->aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_eye_gib_b + i] = entity->aabb.position.y + entity->aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_eye_gib_b + i] = entity->aabb.position.z + entity->aabb.hwidth.z;
    }
    else
    {
      c->frustum_data->min_x[c->frustum_offset_eye_gib_a + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_eye_gib_a + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_eye_gib_a + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_eye_gib_a + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_eye_gib_a + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_eye_gib_a + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;

      c->frustum_data->min_x[c->frustum_offset_eye_gib_b + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
      c->frustum_data->min_y[c->frustum_offset_eye_gib_b + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
      c->frustum_data->min_z[c->frustum_offset_eye_gib_b + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

      c->frustum_data->max_x[c->frustum_offset_eye_gib_b + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
      c->frustum_data->max_y[c->frustum_offset_eye_gib_b + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
      c->frustum_data->max_z[c->frustum_offset_eye_gib_b + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;
    }


    c->frustum_data->min_x[c->frustum_offset + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
    c->frustum_data->min_y[c->frustum_offset + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
    c->frustum_data->min_z[c->frustum_offset + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

    c->frustum_data->max_x[c->frustum_offset + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
    c->frustum_data->max_y[c->frustum_offset + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
    c->frustum_data->max_z[c->frustum_offset + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;

    if (!data[i].gibbed)
    {
      float super_mult = c->super ? SUPER_EYE_SIZE*SUPER_EYE_SIZE_APPEARANCE : 1.0;
      th_pushOccluderFrame(fn_createVec4Vec3(entities[i].aabb.position,200.0*super_mult));
    }

    if (data[i].audio_source_scream != NULL)
    {
      a_setVSPos(data[i].audio_source_scream,entities[i].aabb.position);
      a_setVSVel(data[i].audio_source_scream,entities[i].velocity);
    }


  }
}


static const float fadeout_decay_rate = 0.002;
static const float fadeout_init = 3.0;

static void init_entity(th_EyeballGroup* c,int i,fn_vec3 position,bool alive)
{
  c->entities[i] = TH_DEFAULT_ENTITY;
  c->entities[i].mode = TH_SLIDE_MODE;
  c->entities[i].aabb.position = position;
  c->entities[i].aabb.hwidth = c->super ? fn_createVec3s(75*SUPER_EYE_SIZE) : fn_createVec3(75,75,75);
  c->entities[i].velocity = fn_createVec3s(0);
  c->entities[i].grounded = false;
  c->entities[i].aabb.mode = SPHERE;
  c->entities[i].alive = alive;
  c->entities[i].collided = false;
  c->entities[i].impact = false;
  c->entities[i].delete_me = false;
  c->entities[i].impact_count = 0;
  c->transforms[i] = fn_makescale(fn_createVec3s(0));
  c->transforms_ring_a[i] = fn_makescale(fn_createVec3s(0));
  c->transforms_ring_b[i] = fn_makescale(fn_createVec3s(0));
  c->transforms_eye_gib_a[i] = fn_makescale(fn_createVec3s(0));
  c->transforms_eye_gib_b[i] = fn_makescale(fn_createVec3s(0));
  c->transforms_pupil[i] = fn_makescale(fn_createVec3s(0));
  c->data[i].state = BIRTH;
  c->data[i].gaze = fn_createVec3(1,0,0);
  c->data[i].target = fn_createVec3(0,0,0);
  c->data[i].last_time = 0;
  c->data[i].last_position = position;
  c->data[i].last_time_wallrode = 0;
  c->data[i].last_time_targeted = 0;
  c->data[i].fired_volley = false;
  c->data[i].shots_fired = 0;
  c->data[i].last_fire_time = 0;
  c->data[i].time_started_rocket = 0;
  c->data[i].rotation = 0.0;
  c->data[i].health = c->super ? 50.0 : 100.0;
  c->data[i].audio_source_scream = NULL;
  c->data[i].spawn_time = th_time();
  c->data[i].enabled = alive; // DIABLED BY DEFAULT

  c->data[i].ring_gib_a = TH_DEFAULT_ENTITY;
  c->data[i].ring_gib_a.mode = TH_SLIDE_MODE;
  c->data[i].ring_gib_a.aabb.position = position;
  c->data[i].ring_gib_a.aabb.hwidth = c->super ? fn_createVec3s(150*SUPER_EYE_SIZE) : fn_createVec3(150,150,150);
  c->data[i].ring_gib_a.velocity = fn_createVec3s(0);
  c->data[i].ring_gib_a.grounded = false;
  c->data[i].ring_gib_a.aabb.mode = SPHERE;
  c->data[i].ring_gib_a.alive = alive;
  c->data[i].ring_gib_a.collided = false;
  c->data[i].ring_gib_a.impact = false;
  c->data[i].ring_gib_a.delete_me = false;
  c->data[i].ring_gib_a.impact_count = 0;

  c->data[i].ring_gib_b = TH_DEFAULT_ENTITY;
  c->data[i].ring_gib_b.mode = TH_SLIDE_MODE;
  c->data[i].ring_gib_b.aabb.position = position;
  c->data[i].ring_gib_b.aabb.hwidth = c->super ? fn_createVec3s(150*SUPER_EYE_SIZE) : fn_createVec3(150,150,150);
  c->data[i].ring_gib_b.velocity = fn_createVec3s(0);
  c->data[i].ring_gib_b.grounded = false;
  c->data[i].ring_gib_b.aabb.mode = SPHERE;
  c->data[i].ring_gib_b.alive = alive;
  c->data[i].ring_gib_b.collided = false;
  c->data[i].ring_gib_b.impact = false;
  c->data[i].ring_gib_b.delete_me = false;
  c->data[i].ring_gib_b.impact_count = 0;


  c->data[i].gibbed = false;
  c->data[i].eyeball_gib_a = TH_DEFAULT_ENTITY;
  c->data[i].eyeball_gib_a.mode = TH_SLIDE_MODE;
  c->data[i].eyeball_gib_a.aabb.position = position;
  c->data[i].eyeball_gib_a.aabb.hwidth = c->super ? fn_createVec3s(75*SUPER_EYE_SIZE) : fn_createVec3(75,75,75);
  c->data[i].eyeball_gib_a.velocity = fn_createVec3s(0);
  c->data[i].eyeball_gib_a.grounded = false;
  c->data[i].eyeball_gib_a.aabb.mode = SPHERE;
  c->data[i].eyeball_gib_a.alive = alive;
  c->data[i].eyeball_gib_a.collided = false;
  c->data[i].eyeball_gib_a.impact = false;
  c->data[i].eyeball_gib_a.delete_me = false;
  c->data[i].eyeball_gib_a.impact_count = 0;

  c->data[i].eyeball_gib_b = TH_DEFAULT_ENTITY;
  c->data[i].eyeball_gib_b.mode = TH_SLIDE_MODE;
  c->data[i].eyeball_gib_b.aabb.position = position;
  c->data[i].eyeball_gib_b.aabb.hwidth = c->super ? fn_createVec3s(75*SUPER_EYE_SIZE) : fn_createVec3(75,75,75);
  c->data[i].eyeball_gib_b.velocity = fn_createVec3s(0);
  c->data[i].eyeball_gib_b.grounded = false;
  c->data[i].eyeball_gib_b.aabb.mode = SPHERE;
  c->data[i].eyeball_gib_b.alive = alive;
  c->data[i].eyeball_gib_b.collided = false;
  c->data[i].eyeball_gib_b.impact = false;
  c->data[i].eyeball_gib_b.delete_me = false;
  c->data[i].eyeball_gib_b.impact_count = 0;


  c->data[i].fadeout_ringa =fadeout_init;
  c->data[i].old_ringa = fn_identityMat4();

  c->data[i].fadeout_ringb = fadeout_init;
  c->data[i].old_ringb = fn_identityMat4();

  c->data[i].fadeout_giba = fadeout_init;
  c->data[i].old_giba = fn_identityMat4();

  c->data[i].fadeout_gibb = fadeout_init;
  c->data[i].old_gibb = fn_identityMat4();


  c->data[i].jitter_time_a = 0.0;
  c->data[i].jitter_time_b = 0.0;

  c->data[i].ring_jitter_a = fn_createVec3(1,0,0);
  c->data[i].ring_jitter_b = fn_createVec3(1,0,0);

  c->data[i].dwell_timer = 0;
  c->data[i].pupilsize = 1.0;

  c->data[i].stretch_amt = 1.0;
  c->data[i].stretch_axis = fn_createVec3(1,0,0);
  c->data[i].stretch_timer = 0;
  c->data[i].stretch_permanent = 0.0;
}

void th_eyeballInitialize(th_Allocator* alloc,th_EyeballGroup* c,int count,fn_vec3* positions,th_LevelState* levelstate,bool super)
{
  c->super = super;
  c->levelstate = levelstate;
  c->data = th_alloc(alloc,sizeof(th_EyeballData)*count);
  c->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  c->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->transforms_ring_a = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->transforms_ring_b = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->transforms_eye_gib_a = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->transforms_eye_gib_b = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->transforms_pupil = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->count = count;

  c->free_index_stack = th_alloc(alloc,sizeof(int)*count);
  c->free_index_stack_count = 0;
  c->pupil_center = fn_createVec3(0,0,0);

  for (int i = 0; i < count; i++) {
    c->free_index_stack[i] = i;
    c->free_index_stack_count++;
    init_entity(c,i,positions[i],false);

  }
}

int th_eyeballSpawn(th_EyeballGroup* c,fn_vec3 position)
{
  //stack pop
  if (c->free_index_stack_count > 0)
  {
    th_markEnemyBirth(1);
    c->free_index_stack_count--;
    int idx = c->free_index_stack[c->free_index_stack_count];
    init_entity(c,idx,position,true);
    c->data[idx].state = EYE_SPAWNING;

    // c->data[idx].enabled = true;
    // c->data[idx].alive = true;
    return idx;
  }
  return -1;

}
