#include "th_boids.h"

#include "../fn_engine/th_threads.h"
#include "th_builtins.h"

#define FN_UNIT 6.4

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include "../fn_engine/th_level.h"
#include "../fn_engine/th_globals.h"
#include "../fn_engine/th_occlusion.h"
#include "../fn_engine/th_hitmarker.h"

static const float EXPANSION_TIME = 3000.0;
const float TH_BOID_SIZE = 1.25;

fn_vec3 fn_boidRule1(th_Entity** cols,int colCount,th_Entity* e,float weight)
{
  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  for (j = 0; j <colCount;j++)
  {
    if (cols[j] != e)
    {
      pc = fn_addVec3(pc,cols[j]->aabb.position);
    }
  }

  pc = fn_multVec3s(pc,1.f/colCount);

  return fn_multVec3s(fn_subVec3(pc,e->aabb.position),weight);
}



fn_vec3 fn_boidRule2(th_Entity** cols,int colCount,th_Entity* e,float weight)
{
  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  for (j = 0; j <colCount;j++)
  {
    if (cols[j] != e)
    {
      float mag = fn_length(fn_subVec3(e->aabb.position,cols[j]->aabb.position));

      //  if (mag < (weight))
      //  {
      //(1.f/mag)*1.5f
      //fn_min(FN_UNIT*0.015,mag)
      fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,cols[j]->aabb.position),weight*fmin(1.0,45.0/mag));
    //  float interp = fn_clamp(1.f/(mag*mag),0,1);

      float vel_mag = fn_length(e->velocity);

      // fn_vec3 old_dir = fn_normalizeVec3(fn_addVec3(e->velocity,pc));
      // fn_vec3 new_dir = fn_normalizeVec3(fn_subVec3(fn_addVec3(e->velocity,pc),delta));
      if (mag < 90*1.5)
      {
        pc = fn_addVec3(delta,pc);//fn_multVec3s(fn_lerpVec3(old_dir,new_dir,interp),vel_mag);
      }
      //fn_cerpVec3(pc,fn_subVec3(pc,delta),interp);


    //  cols[j]->velocity = fn_lerpVec3(cols[j]->velocity,fn_addVec3(cols[j]->velocity,delta),interp);
      //cols[j]->velocity = fn_addVec3(cols[j]->velocity,fn_multVec3s(delta,1));

      // }

    }
  }

  return pc;
}

fn_vec3 fn_boidRule3(th_Entity** cols,int colCount,th_Entity* e,float weight)
{
  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  for (j = 0; j <colCount;j++)
  {
    if (cols[j] != e)
    {
      pc = fn_addVec3(pc,cols[j]->velocity);
    }
  }

  pc = fn_multVec3s(pc,1.f/colCount);

  return fn_multVec3s(fn_subVec3(pc,e->velocity),weight);
}

fn_vec3 fn_boidGotoPlace(th_Entity* e,fn_vec3 goal,float speed,float weight)
{
  fn_vec3 target = fn_multVec3s(fn_subVec3(goal,e->aabb.position),weight);

  if (fn_length(target) > speed)
  {
    return fn_multVec3s(fn_multVec3s(target,1.f/fn_length(target)),speed);
  }
  return target;
}

fn_vec3 fn_boidLimitVelocity(th_Entity* e ,float limit)
{
  if (fn_length(e->velocity) > limit)
  {
    return fn_multVec3s(fn_multVec3s(e->velocity,1.f/fn_length(e->velocity)),limit);
  }
  return e->velocity;
}

fn_vec3 fn_boidAvoidAgents(th_CentipedeGroup* c,int count,th_Entity* e,float weight2,float weight3)
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

fn_vec3 fn_boidAvoidEyeballs(th_EyeballGroup* eyes,th_Entity* e,float weight2,float weight3,float rad_mult2)
{

  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  fn_vec3 pc2 = fn_createVec3s(0.f);
  int colCount = 0;
  for (j = 0; j <eyes->count;j++)
  {

    if (!eyes->entities[j].alive)
    {
      continue;
    }

      float mag2 = fn_length2(fn_subVec3(e->aabb.position,eyes->entities[j].aabb.position));

      fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,eyes->entities[j].aabb.position),weight2);

  //    float vel_mag = fn_length(e->velocity);

      if (mag2 < 350*350*rad_mult2)
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

fn_vec3 fn_boidAvoidWalkers(th_HorseGroup* horse,th_Entity* e,float weight2,float weight3)
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

fn_vec3 fn_boidAvoidTricols(th_TricolumnGroup* tricol,th_Entity* e,float weight2,float weight3)
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



typedef struct
{
  int start;
  int range;
  int thread_id;
  th_BoidGroup* boids;
  float dt;
  fn_vec3 target;
  th_World* world;
  float neightborhood_rad;
  float speed;
  float gotoweight;
  float attraction;
  float seperation;
  float directional;
  float speedlimit;
  th_Entity* entities;
  int boidscount;
  fn_Grid* grid;
  fn_mat4* transforms;
  fn_vec3* new_velocities;
  th_FrustumCullData* frustum_data;
  int frustum_offset;

  fn_mat4* transforms_sheild;
  int frustum_offset_sheild;
}th_boidsThreadData;

void thread_boidscompute( void* id)
{
  th_boidsThreadData data = *((th_boidsThreadData*)id);

  th_BoidGroup* boids = data.boids;
  float dt = data.dt;
  fn_vec3 target = data.target;
  th_World* world = data.world;
  float neightborhood_rad = data.neightborhood_rad;
  float speed = data.speed;
  float gotoweight = data.gotoweight;
  float attraction = data.attraction;
  float seperation = data.seperation;
  float directional = data.directional;
  float speedlimit = data.speedlimit;
  th_Entity* entities = data.entities;
  int boidscount = data.boidscount;
  fn_Grid* grid = data.grid;
  fn_mat4* transforms = data.transforms;
  fn_vec3* new_velocities = data.new_velocities;
  int thread_id = data.thread_id;


  for (int i = data.start; i <data.start + data.range ;i++)
  {
    if ( !entities[i].alive)
    {
      if (boids->audio_sources[i] != NULL)
      {
          a_VirtualSource* s = boids->audio_sources[i];
          a_stopVS(s);
          boids->audio_sources[i] = NULL;
      }
      new_velocities[i] = entities[i].velocity;
      continue;
    }

    th_Entity*** cols = &boids->boidgroup_memory[thread_id].cols;
    int* allocated = &boids->boidgroup_memory[thread_id].allocated;



    int colCountPossible = 0;
    th_AABB neighborhood = entities[i].aabb;
    neighborhood.hwidth  = fn_createVec3s(neightborhood_rad);//2


    th_Entity** colsPossible = fn_getCollidableAABB(grid,neighborhood,&colCountPossible,&grid->grid_mem[thread_id]);

    int colCount = 0;
    if (colCountPossible > *allocated)
    {
      boids->boidgroup_memory[thread_id].cols = th_reallocCleanup(boids->alloc,boids->boidgroup_memory[thread_id].cols,sizeof(th_Entity*)*colCountPossible);
      *allocated = colCountPossible;
    }



    for (int j = 0; j <colCountPossible;j++)
    {
      if (colsPossible[j] != &entities[i])
      {
        if (fn_aabbCheck(colsPossible[j]->aabb,neighborhood) && colsPossible[j]->alive)
        {
          boids->boidgroup_memory[thread_id].cols[colCount] = colsPossible[j];
          colCount++;
        }
      }
    }





    fn_vec3 r1 = fn_createVec3s(0.f);
    fn_vec3 r2 = fn_createVec3s(0.f);//fn_boidRule2(cols,colCount,&game->skulls[i]);
    fn_vec3 r3 = fn_createVec3s(0.f);//fn_boidRule3(cols,colCount,&game->skulls[i]);



    fn_vec3 r4 = fn_boidGotoPlace(&entities[i],target,speed,gotoweight);//fn_boidGotoPlace(&game->skulls[i],game->player.aabb.position,0.15,1.f/100.f);//1/60
    //fn_vec3 r5 = fn_boidGotoPlace(&game->skulls[i],fn_createVec3(100,-FN_UNIT*5,0),0.05,-1.f/100.f);
    fn_vec3 r5 = boids->levelstate->centipede == NULL ? fn_createVec3s(0) : fn_boidAvoidAgents(boids->levelstate->centipede,boids->levelstate->centipede->count,&entities[i],seperation*0.25,directional);



    th_EyeballGroup* eyeptr = boids->levelstate->eyeball;
    fn_vec3 r6 = eyeptr == NULL ? fn_createVec3s(0) :  fn_boidAvoidEyeballs(eyeptr,&entities[i],seperation*0.25,directional,1.0);

    th_HorseGroup* hptr = boids->levelstate->horse;
    fn_vec3 r7 = hptr == NULL ? fn_createVec3s(0) :  fn_boidAvoidWalkers(hptr,&entities[i],seperation*0.25,directional);

    th_TricolumnGroup* tptr = boids->levelstate->tricolumn;
    fn_vec3 r8 = tptr == NULL ? fn_createVec3s(0) :  fn_boidAvoidTricols(tptr,&entities[i],seperation*0.25,directional);

    eyeptr = boids->levelstate->eyeball_super;
    fn_vec3 r9 = eyeptr == NULL ? fn_createVec3s(0) :  fn_boidAvoidEyeballs(eyeptr,&entities[i],seperation*0.25,directional,1.5*1.5);

    fn_vec3 r10 = boids->levelstate->centipede_fast == NULL ? fn_createVec3s(0) : fn_boidAvoidAgents(boids->levelstate->centipede_fast,boids->levelstate->centipede_fast->count,&entities[i],seperation*0.25,directional);

    if (colCount != 0)
    {

      r1 = fn_boidRule1(*cols,colCount,&entities[i],attraction);//300
      r2 = fn_boidRule2(*cols,colCount,&entities[i],seperation);//1.5
      r3 = fn_boidRule3(*cols,colCount,&entities[i],directional);//100


    }



    if (fn_length2(fn_subVec3(entities[i].aabb.position,target)) < pow(6.4*8,2) )
    {

      // r5 = fn_boidGotoPlace(&game->skulls[i],game->player.aabb.position,0.3,1.f/100.f);
      r3 = fn_createVec3s(0.f);
      r2 = fn_createVec3s(0.f);
      r1 = fn_createVec3s(0.f);
    }




    fn_vec3 add = fn_addVec3(fn_addVec3(fn_addVec3(fn_addVec3(r1,r2),fn_addVec3(r3,r4)),r5),r6);
    add = fn_addVec3(add,r7);
    add = fn_addVec3(add,r8);
    add = fn_addVec3(add,r9);
    add = fn_addVec3(add,r10);

    if (colCount != 0)
    {

      for (int k = 0; k < colCount; k++) {
        if ((*cols)[k] != &entities[i] && fn_distance2((*cols)[k]->aabb.position,entities[i].aabb.position) < 0.3*0.3)
        {
          fn_vec3 direct_repel = fn_normalizeVec3(fn_subVec3(entities[i].aabb.position,(*cols)[k]->aabb.position));
           add = fn_addVec3(entities[i].velocity,fn_multVec3s(direct_repel,0.5));
        }

      }

    }

    fn_vec3 new_velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(add,dt));

    new_velocities[i] = new_velocity;

  }

}

void thread_boidsphysics( void* id)
{
  th_boidsThreadData data = *((th_boidsThreadData*)id);

  th_BoidGroup* boids = data.boids;
  float dt = data.dt;
  fn_vec3 target = data.target;
  th_World* world = data.world;
  float neightborhood_rad = data.neightborhood_rad;
  float speed = data.speed;
  float gotoweight = data.gotoweight;
  float attraction = data.attraction;
  float seperation = data.seperation;
  float directional = data.directional;
  float speedlimit = data.speedlimit;
  th_Entity* entities = data.entities;
  int boidscount = data.boidscount;
  fn_Grid* grid = data.grid;
  fn_mat4* transforms = data.transforms;
  fn_mat4* transforms_sheild = data.transforms_sheild;
  fn_vec3* new_velocities = data.new_velocities;
  int thread_id = data.thread_id;

  for (int i = data.start; i < data.start + data.range;i++)
  {
    if (!entities[i].alive)
    {
      if (boids->audio_sources[i] != NULL)
      {
          a_VirtualSource* s = boids->audio_sources[i];
          a_stopVS(s);
          boids->audio_sources[i] = NULL;
      }
      continue;
    }

    if (entities[i].impact)
    {
      bool can_damage = false;
      if (data.boids->has_sheild[i])
      {
        for (int k = 0 ; k < entities[i].impact_count;k++ )
        {
          th_Entity* projectile = (th_Entity*)entities[i].impacts[k].entity;
          fn_vec3 vel = fn_multVec3s(fn_normalizeVec3(projectile->velocity),-1.0);
          fn_vec3 sheild_dir = boids->sheild_direction[i];
          if ( fn_dot(sheild_dir,vel) > 0.25881898618*2.0 && !(projectile->type == TH_HAMMER_PROJECTILE) )
          {
            //hit shield
            th_spawnSparks(fn_multVec3s(vel,1),entities[i].impacts[k].pos,data.thread_id,boids->levelstate->general_light_query);
            th_spawnSparks(fn_multVec3s(vel,1),entities[i].impacts[k].pos,data.thread_id,boids->levelstate->general_light_query);


            {
              a_VirtualSource* s = a_playVirtualSource(15 + th_random() % 3,-1,entities[i].impacts[k].pos,NULL );
              a_setVSLoop(s,false);
              a_setVSPos(s,entities[i].impacts[k].pos);
              a_setVSVel(s,fn_createVec3s(0));
              a_setVSGain(s,0.7);
            }

            new_velocities[i] = fn_addVec3(new_velocities[i],fn_multVec3s(fn_normalizeVec3(projectile->velocity),0.2));//impact momentum impulse

          }
          else
          {
            // do damage
            can_damage = true;
          }


        }
        entities[i].impact_count = 0;
        entities[i].impact = false;

      }
      else
      {
        can_damage = true;
      }

      if (can_damage)
      {
        th_Hitmarker hmarker;
        hmarker.entity_pos_ref = &entities[i].aabb.position;
        hmarker.entity_transform_ref = NULL;//&c->transforms_gems[i*7 + j];
        hmarker.alive_ref = NULL;
        hmarker.timer = th_time();
        hmarker.last_good_pos = entities[i].aabb.position;
        hmarker.is_alive = true;
        hmarker.radius = 45.0 ;
        th_pushHitmarker(hmarker);


        if (boids->audio_sources[i] != NULL)
        {
          a_VirtualSource* s = boids->audio_sources[i];
          a_stopVS(s);
          boids->audio_sources[i] = NULL;
        }

        {
          a_VirtualSource* s = a_playVirtualSource(sound_boid_break1 + th_random() % 3,-1,entities[i].aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entities[i].aabb.position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSPitch(s,fn_remap(th_randomFloat(0.0,1.0),0.0,1.0,0.8,1.3));
          a_setVSGain(s,0.6);
        }

        fn_mat4 view = fn_lookat(fn_createVec3s(0),entities[i].velocity,fn_createVec3(0,-1,0));
        fn_mat4 original_orient = fn_inverse(view);
        // fn_mat4 view = fn_lookat(entities[i].aabb.position,fn_addVec3(entities[i].aabb.position,entities[i].velocity),fn_createVec3(0,-1,0));
        // fn_mat4 original_orient = fn_multMat4(fn_makescale(fn_createVec3s(4.45)),fn_inverse(view));

        th_gibSpawn(boids->levelstate->boid_gibs,entities[i].aabb.position,original_orient);
        th_spawnBloodNoSound(entities[i].aabb.position,thread_id);
        transforms[i] = fn_makescale(fn_createVec3s(0));
        transforms_sheild[i] = fn_makescale(fn_createVec3s(0));
        data.frustum_data->skip_culling_flag[data.frustum_offset_sheild + i] = true;
        entities[i].aabb.position = fn_createVec3s(1000000);
        entities[i].impact = false;
        entities[i].delete_me = true;


        continue;
      }

    }
    entities[i].velocity =  new_velocities[i];//fn_cerpVec3(entities[i].velocity,new_velocity,0.1*dt);

    float spd_scale = fn_min(1.0,(th_time() - boids->spawn_times[i])/EXPANSION_TIME   );
    entities[i].velocity = fn_boidLimitVelocity(&entities[i],speedlimit*spd_scale);

    if ((th_time() - boids->spawn_times[i])/EXPANSION_TIME  < 0.3)
    {
      entities[i].velocity = boids->prime_velocity[i];
    }
    else if ((th_time() - boids->spawn_times[i])/EXPANSION_TIME  < 1)
    {
      fn_vec3 accel = fn_normalizeVec3(fn_subVec3(boids->prime_target[i],entities[i].aabb.position));
      entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(accel,dt*0.001));//boids->prime_velocity[i];
    }


  //   entities[i].velocity = fn_multVec3s(entities[i].velocity,(1.0/16.66));
    //*(1.0/16.66)
    th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,thread_id));

    // entities[i].velocity = fn_multVec3s(entities[i].velocity,(16.66));

    if (  boids->audio_sources[i] == NULL)
    {
      boids->audio_sources[i] = a_playVirtualSource(20,-5,entities[i].aabb.position,NULL);
      a_setVSLoop(boids->audio_sources[i],true);
      a_setVSGain(boids->audio_sources[i],0.17);
      a_setVSOffset(boids->audio_sources[i],(float)th_random()/(float)(RAND_MAX/(14.0)));
    }
    a_setVSPos(boids->audio_sources[i],entities[i].aabb.position);
    a_setVSVel(boids->audio_sources[i],entities[i].velocity);
    // if (boids->audio_sources[i]->physical_source == NULL)
    // {
    //   printf("Journal: ");
    //   for (size_t p = 0; p < boids->audio_sources[i]->journal_count; p++) {
    //     printf("%i ", boids->audio_sources[i]->journal[p]);
    //   }
    //   printf("\n");
    // }

    float s = fn_max(fn_min(1.0,(th_time() - boids->spawn_times[i])/EXPANSION_TIME   ),0.25);

    fn_mat4 view = fn_lookat(entities[i].aabb.position,fn_addVec3(entities[i].aabb.position,entities[i].velocity),fn_createVec3(0,-1,0));
    transforms[i] = fn_multMat4(fn_makescale(fn_createVec3s(6.4*3*TH_BOID_SIZE*s)),fn_inverse(view));

    data.frustum_data->min_x[data.frustum_offset + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
    data.frustum_data->min_y[data.frustum_offset + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
    data.frustum_data->min_z[data.frustum_offset + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

    data.frustum_data->max_x[data.frustum_offset + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
    data.frustum_data->max_y[data.frustum_offset + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
    data.frustum_data->max_z[data.frustum_offset + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;

    if (data.boids->has_sheild[i])
    {
      fn_quat orient = th_update_orient(&boids->sheild_direction[i],fn_normalizeVec3(entities[i].velocity),0.08,dt,&boids->old_up_sheild[i]);

      transforms_sheild[i] = fn_translaterotatescaleq(entities[i].aabb.position,orient,fn_createVec3s(6.4*2.2*s*TH_BOID_SIZE));
      data.frustum_data->min_x[data.frustum_offset_sheild + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
      data.frustum_data->min_y[data.frustum_offset_sheild + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
      data.frustum_data->min_z[data.frustum_offset_sheild + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

      data.frustum_data->max_x[data.frustum_offset_sheild + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
      data.frustum_data->max_y[data.frustum_offset_sheild + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
      data.frustum_data->max_z[data.frustum_offset_sheild + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;
      data.frustum_data->skip_culling_flag[data.frustum_offset_sheild + i] = false;
    }
    else
    {
      data.frustum_data->skip_culling_flag[data.frustum_offset_sheild + i] = true;
    }

  }
}


void th_boidsUpdate(th_BoidGroup* boids,float dt,th_BoidProperties* props)
{
  fn_vec3 target = boids->levelstate->player_e.aabb.position;
  th_World* world = boids->levelstate->world;
  float neightborhood_rad = 100;//props->neightborhood_rad;
  float speed = 0.0075;//props->speed*(1.0/12.66);
  float gotoweight = 0.0000007;//props->gotoweight;
  float attraction = 0.000009;//props->attraction;
  float seperation = 0.000015;//props->seperation*0.75;
  float directional = 0.00002;//props->directional;
  float speedlimit = 0.6;//props->speedlimit*(1.0/12.0);//*(1.0/16.66);

  th_Entity* entities =boids->entities;
  int boidscount = boids->boidscount;
  fn_Grid* grid = &boids->grid;
  fn_mat4* transforms = boids->transforms;
  fn_vec3* new_velocities = malloc(sizeof(fn_vec3)*boidscount);

  boids->num_audio_playing = 0;

  // for (int i = 0; i <boidscount;i++)
  // {
  //   entities[i].aabb.hwidth = fn_createVec3s(35);
  // }

  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_boidsThreadData,boidscount)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].boids = boids;
  data[th_thread_id].dt = dt;
  data[th_thread_id].target = target;
  data[th_thread_id].world = world;
  data[th_thread_id].neightborhood_rad = neightborhood_rad;
  data[th_thread_id].speed = speed;
  data[th_thread_id].gotoweight = gotoweight;
  data[th_thread_id].attraction = attraction;
  data[th_thread_id].seperation = seperation;
  data[th_thread_id].directional = directional;
  data[th_thread_id].speedlimit = speedlimit;
  data[th_thread_id].entities = entities;
  data[th_thread_id].boidscount = boidscount;
  data[th_thread_id].grid = grid;
  data[th_thread_id].transforms = transforms;
  data[th_thread_id].new_velocities = new_velocities;
  data[th_thread_id].thread_id = th_thread_id;

  TH_SCHEDULING_FUNC

  th_setThread(thread_boidscompute,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING


  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_boidsThreadData,boidscount)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].boids = boids;
  data[th_thread_id].dt = dt;
  data[th_thread_id].target = target;
  data[th_thread_id].world = world;
  data[th_thread_id].neightborhood_rad = neightborhood_rad;
  data[th_thread_id].speed = speed;
  data[th_thread_id].gotoweight = gotoweight;
  data[th_thread_id].attraction = attraction;
  data[th_thread_id].seperation = seperation;
  data[th_thread_id].directional = directional;
  data[th_thread_id].speedlimit = speedlimit;
  data[th_thread_id].entities = entities;
  data[th_thread_id].boidscount = boidscount;
  data[th_thread_id].grid = grid;
  data[th_thread_id].transforms = transforms;
  data[th_thread_id].new_velocities = new_velocities;
  data[th_thread_id].thread_id = th_thread_id;
  data[th_thread_id].frustum_data = boids->frustum_data;
  data[th_thread_id].frustum_offset = boids->frustum_offset;

  data[th_thread_id].transforms_sheild = boids->transforms_sheild;
  data[th_thread_id].frustum_offset_sheild = boids->frustum_offset_sheild;
  TH_SCHEDULING_FUNC
  th_setThread(thread_boidsphysics,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING



  for (int i = 0; i <boidscount;i++)
  {
    if (!entities[i].delete_me && entities[i].alive)
    {//*(1.0/16.66)
      fn_updateEntityGrid(grid,i,dt);
    }
    else if (entities[i].alive && entities[i].delete_me)
    {
      fn_removeEntityGrid(grid,i);
      entities[i].delete_me = false;
      entities[i].alive = false;
      transforms[i] = fn_makescale(fn_createVec3s(0));
      entities[i].aabb.position = fn_createVec3s(1000000);

      //stack push
      th_markEnemyDeath(1);
      boids->free_index_stack[boids->free_index_stack_count] = i;
      boids->free_index_stack_count++;

    }

    if (entities[i].alive)
    {
      th_pushOccluderFrame(fn_createVec4Vec3(entities[i].aabb.position,70.0));
    }


  //  entities[i].aabb.hwidth = fn_createVec3s(80);
  }

  free(new_velocities);
}

int th_boidsDamageCallback(void* ep,void* pep,float dt,void* world)
{
  th_Entity* e = (th_Entity*)ep;
  th_Entity* player = (th_Entity*)pep;

  float len = fn_length( fn_subVec3(player->aabb.position,e->aabb.position));
  fn_vec3 comp = fn_multVec3s(fn_normalizeVec3(fn_subVec3(e->aabb.position,player->aabb.position)),len + 2);
  comp = fn_addVec3(comp,fn_multVec3s(fn_normalizeVec3(player->velocity),fn_length(player->velocity)*2.f));
  e->velocity = fn_addVec3(e->velocity,comp);

  if (th_time() > e->time_of_damage + 1000)
  {
    e->time_of_damage = th_time();
    return 5;
  }
  else
  {
    return 0;
  }

}


static void init_entity(th_BoidGroup* boids, int i,fn_vec3 pos,bool alive,bool has_sheild)
{
  boids->enabled_array[i] = alive;
  boids->entities[i] = TH_DEFAULT_ENTITY;
  boids->entities[i].mode = TH_SLIDE_MODE;
  boids->entities[i].aabb.position = pos;
  boids->entities[i].aabb.hwidth = boids->hwidth;
  boids->entities[i].velocity = fn_createVec3s(0);
  boids->entities[i].grounded = false;
  boids->entities[i].aabb.mode = BOX;
  boids->entities[i].alive = alive;
  boids->entities[i].impact = false;
  boids->entities[i].delete_me = false;
  boids->entities[i].impact_count = 0;
  boids->transforms[i] = fn_makescale(fn_createVec3s(0));
  boids->entities[i].damage_callback = th_boidsDamageCallback;
  boids->spawn_times[i] = th_time();
  boids->prime_velocity[i] = fn_createVec3(0,0,0);
  boids->prime_target[i] = pos;


  boids->has_sheild[i] = has_sheild;
  boids->sheild_direction[i] = fn_createVec3(1,0,0);
  boids->old_up_sheild[i] = fn_createVec3(1,0,0);
  boids->transforms_sheild[i] = fn_makescale(fn_createVec3s(0));
}

void th_boidsInitialize(th_Allocator* alloc,th_BoidGroup* boids,fn_vec3 cellsize,int celldim,int bcount,fn_vec3 hwidth,th_LevelState* levelstate)
{
  boids->alloc = alloc;
  boids->num_audio_playing = 0;
  boids->hwidth = hwidth;
  boids->levelstate = levelstate;
  boids->audio_sources = th_alloc(alloc,sizeof(a_VirtualSource*)*bcount);
  for (int j = 0; j < bcount; j++) {
    boids->audio_sources[j] = NULL;
  }
  boids->cellsize = cellsize;
  boids->celldim = celldim;
  boids->boidscount = bcount;
  boids->entities = th_alloc(alloc,sizeof(th_Entity)*bcount);
  boids->transforms = th_alloc(alloc,sizeof(fn_mat4)*bcount);
  boids->spawn_times = th_alloc(alloc,sizeof(th_timer_t)*bcount);

  boids->enabled_array = th_alloc(alloc,sizeof(bool)*bcount);

  boids->prime_velocity = th_alloc(alloc,sizeof(fn_vec3)*bcount);
  boids->prime_target = th_alloc(alloc,sizeof(fn_vec3)*bcount);

  boids->has_sheild = th_alloc(alloc,sizeof(bool)*bcount);
  boids->sheild_direction = th_alloc(alloc,sizeof(fn_vec3)*bcount);
  boids->old_up_sheild = th_alloc(alloc,sizeof(fn_vec3)*bcount);
  boids->transforms_sheild = th_alloc(alloc,sizeof(fn_mat4)*bcount);

  boids->free_index_stack = th_alloc(alloc,sizeof(int)*bcount);//stack data struct for holding indexes of dead boids
  boids->free_index_stack_count = 0;

  for (int i = 0 ; i < bcount;i++)
  {

    boids->free_index_stack[i] = i;
    boids->free_index_stack_count++;
    init_entity(boids,i,fn_createVec3s(10000000),false,false);

  }

  boids->boidgroup_memory = th_alloc(alloc,sizeof(th_BoidGroupMemory)*th_getNumThreads());
  for (int i = 0 ; i < th_getNumThreads();i++)
  {
    boids->boidgroup_memory[i].cols = th_reallocCleanup(alloc,NULL,1);
    boids->boidgroup_memory[i].allocated = 0;
  }
}

int th_boidsSpawn(th_BoidGroup* boids,fn_vec3 pos)
{
  //stack pop
  if (boids->free_index_stack_count > 0)
  {
    th_markEnemyBirth(1);
    boids->free_index_stack_count--;
    int idx = boids->free_index_stack[boids->free_index_stack_count];
    init_entity(boids,idx,pos,true,false);
    return idx;
  }
  return -1;

}

int th_boidsSpawnShield(th_BoidGroup* boids,fn_vec3 pos)
{
  //stack pop
  if (boids->free_index_stack_count > 0)
  {
    th_markEnemyBirth(1);
    boids->free_index_stack_count--;
    int idx = boids->free_index_stack[boids->free_index_stack_count];
    init_entity(boids,idx,pos,true,true);
    return idx;
  }
  return -1;

}
