#include "th_gems.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_time.h"
#include "th_builtins.h"
#include "../fn_engine/th_level.h"


// static uint num_used = 0;
// static int current_count = 0;
void th_gemInit(th_Allocator* alloc,th_GemObject* gem,int count,th_LevelState* levelstate)
{
  gem->num_used = 0;
  gem->current_count = 0;
  gem->levelstate = levelstate;
  gem->entity_count = count;
  gem->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  gem->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  gem->orientations = th_alloc(alloc,sizeof(fn_vec3)*count);
  gem->lifes = th_alloc(alloc,sizeof(float)*count);
  gem->sources = th_alloc(alloc,sizeof(a_VirtualSource*)*count);
  gem->blast_delay = th_alloc(alloc,sizeof(float)*count);
  for (int i = 0 ; i < count;i++)
  {
    gem->sources[i] = NULL;
    gem->entities[i] = TH_DEFAULT_ENTITY;
    gem->entities[i].robust_collisions = false;
    gem->entities[i].mode = TH_SLIDE_MODE;
    gem->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
    gem->entities[i].aabb.hwidth = fn_createVec3(30,30,30);
    gem->entities[i].velocity = fn_createVec3s(0);
    gem->entities[i].grounded = false;
    gem->entities[i].collided = false;
    gem->entities[i].aabb.mode = SPHERE;
    gem->transforms[i] = fn_makescale(fn_createVec3(0,0,0));
    gem->lifes[i] = -1.0;
    gem->blast_delay[i] = 0.0;
  //  gem->orientations[i] = fn_createVec3(1,0,0);
  }
}


static fn_vec3 th_avoidgems(th_GemObject* gem,th_Entity* e,uint32_t i,float weight2)
{


  uint j;
  fn_vec3 pc = fn_createVec3s(0.f);
  // fn_vec3 pc2 = fn_createVec3s(0.f);
  int colCount = 0;
  for (j = 0; j <gem->num_used;j++)
  {

    if (j == i || gem->lifes[j] < 0)
    {
      continue;
    }

    float mag2 = fn_length2(fn_subVec3(e->aabb.position,gem->entities[j].aabb.position));

    // fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,gem->entities[j].aabb.position),weight2);

    //    float vel_mag = fn_length(e->velocity);

    if (mag2 < 45*45)
    {
      // pc = fn_addVec3(delta,pc);
      //
      // pc2 = fn_addVec3(pc2,gem->entities[j].velocity);
      colCount++;


      float mag = sqrtf(mag2);

      //  if (mag < (weight))
      //  {
      //(1.f/mag)*1.5f
      //fn_min(FN_UNIT*0.015,mag)
      fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,gem->entities[j].aabb.position),weight2*fmin(1.0,45.0/mag));
      //  float interp = fn_clamp(1.f/(mag*mag),0,1);

      float vel_mag = fn_length(e->velocity);

      // fn_vec3 old_dir = fn_normalizeVec3(fn_addVec3(e->velocity,pc));
      // fn_vec3 new_dir = fn_normalizeVec3(fn_subVec3(fn_addVec3(e->velocity,pc),delta));
      if (mag < 90*1.5)
      {
        pc = fn_addVec3(delta,pc);//fn_multVec3s(fn_lerpVec3(old_dir,new_dir,interp),vel_mag);
      }
    }



  }


  // if (colCount > 0)
  //   pc2 = fn_multVec3s(pc2,1.f/colCount);


  return pc;//fn_addVec3(pc,fn_multVec3s(fn_subVec3(pc2,e->velocity),weight3));
}



void th_gemUpdate(th_GemObject* gem,float dt)
{

  bool firingShotgun = gem->levelstate->shotgun->firing;
  bool firingMachineGun = gem->levelstate->plasma->firing;

  fn_vec3 target = gem->levelstate->player_e.aabb.position;
  th_World* world = gem->levelstate->world;
  int count = gem->entity_count;
  th_Entity* entities = gem->entities;
  fn_mat4* transforms = gem->transforms;

  bool got_a_gem = false;
  for (uint i = 0; i <gem->num_used;i++)
  {
    if (gem->lifes[i] < 0)
    {
      if (gem->sources[i] != NULL)
      {
        a_stopVS(gem->sources[i]);
        gem->sources[i] = NULL;
      }

      gem->frustum_data->skip_culling_flag[gem->frustum_offset + i] = true;
      continue;
    }

     gem->blast_delay[i] =  gem->blast_delay[i] - dt;
     if ( gem->blast_delay[i] < 0.0)
     {
        gem->blast_delay[i]  = 0.0;
     }

    fn_vec3 oldvelocity = entities[i].velocity;
    bool oldground = entities[i].grounded;
    // if (entities[i].grounded)
    // {
    //   continue;
    // }
    a_setVSPos(gem->sources[i],entities[i].aabb.position);
    a_setVSVel(gem->sources[i],entities[i].velocity);


    entities[i].velocity = fn_addVec3(entities[i].velocity,th_avoidgems(gem,&entities[i],i,0.005));
    th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,0));
    if (!entities[i].grounded)
    {
      entities[i].velocity.y += 0.001*dt;
    }
    if (entities[i].collided){
      float dot = fn_dot(oldvelocity,entities[i].collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
      fn_vec3 u = fn_multVec3s(entities[i].collision_normal , dot);
      fn_vec3 w = fn_subVec3(oldvelocity , u);
      entities[i].velocity = fn_multVec3s(fn_subVec3(w , u),0.75);//reduce bounce height 0.99,0.75,0.99
      for (int k = 0 ; k < 3;k++)
      {
        if (fn_almostEqualf(entities[i].velocity.v[k],0,0.01))
        {
          entities[i].velocity.v[k] = 0;
        }
      }

    }

    bool notlevel3 = gem->levelstate->player->level_weapon[gem->levelstate->weapon->chosen_weapon] == 3 && gem->levelstate->player->gem_count[gem->levelstate->weapon->chosen_weapon] >= 100;
    notlevel3 = !notlevel3;
    if (fn_distance2(target,entities[i].aabb.position) > 75*75)
    {
      if (fn_distance2(target,entities[i].aabb.position) < 1200*1200)
      {
        fn_vec3 to_target = fn_normalizeVec3(fn_subVec3(target,entities[i].aabb.position));

        if (gem->levelstate->weapon->chosen_weapon == TH_MACHINEGUN && firingMachineGun)
        {

          entities[i].velocity = fn_multVec3s(to_target,-1.6);

          gem->blast_delay[i] = 60.0;

        }
        else if (gem->levelstate->weapon->chosen_weapon == TH_SHOTGUN && firingShotgun)
        {
          entities[i].velocity = fn_multVec3s(to_target,-6);
          gem->blast_delay[i] = 60.0;

        }
        else if (notlevel3)
        {
          entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(to_target,0.2));

        }

        if (fn_dot(to_target,fn_normalizeVec3(entities[i].velocity)) > 0.0 && gem->blast_delay[i] == 0.0)
        {
          if (fn_length(entities[i].velocity) > 1.4)
          {
            entities[i].velocity = fn_multVec3s(fn_normalizeVec3(entities[i].velocity),1.4);
          }
        }
        // else
        // {
        //   if (fn_length(entities[i].velocity) > 10)
        //   {
        //     entities[i].velocity = fn_multVec3s(fn_normalizeVec3(entities[i].velocity),10);
        //   }
        // }

      }
      else
      {
        if (fn_length(entities[i].velocity) > 1)
        {
          entities[i].velocity = fn_multVec3s(fn_normalizeVec3(entities[i].velocity),1);
        }
      }
    }
    else if (gem->lifes[i] > 0 && notlevel3 && !(gem->levelstate->weapon->chosen_weapon == TH_SHOTGUN && firingShotgun) && !(gem->levelstate->weapon->chosen_weapon == TH_MACHINEGUN && firingMachineGun))
    {
      a_stopVS(gem->sources[i]);
      gem->sources[i] = NULL;
      gem->lifes[i] = -1;
      gem->frustum_data->skip_culling_flag[gem->frustum_offset + i] = true;
      // gem->levelstate->player->gem_count[gem->levelstate->weapon->chosen_weapon] = gem->levelstate->player->gem_count[gem->levelstate->weapon->chosen_weapon] + 1;
      th_incrementPlayerGem(gem->levelstate->player);

      got_a_gem = true;

      // th_setGameGlow(fn_createVec3(0,1,0),0.2);
      //printf("%s\n","GOT GEM" );
    }
    // if (!oldground && entities[i].grounded && oldvelocity.y > 0.1)
    // {
    //
    //   {
    //     a_VirtualSource* s = a_playFile(5 );
    //     a_setVSLoop(s,false);
    //     a_setVSPos(s,entities[i].aabb.position);
    //     a_setVSVel(s,fn_createVec3s(0));
    //   }
    //
    // }


    // fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,-1,0),gem->orientations[i]);
    transforms[i] = fn_translaterotatescale(entities[i].aabb.position,gem->lifes[i],fn_createVec3(0,1,0),fn_createVec3s(10));
    if (gem->lifes[i] < 25.0)
    {
      if (fmod(gem->lifes[i],2) < 1)
      {
        transforms[i] = fn_translaterotatescale(entities[i].aabb.position,gem->lifes[i],fn_createVec3(0,1,0),fn_createVec3s(0));
      }
    }

    //fn_printMat4(transforms[i]);

    gem->frustum_data->min_x[gem->frustum_offset + i] = entities[i].aabb.position.x - entities[i].aabb.hwidth.x;
    gem->frustum_data->min_y[gem->frustum_offset + i] = entities[i].aabb.position.y - entities[i].aabb.hwidth.y;
    gem->frustum_data->min_z[gem->frustum_offset + i] = entities[i].aabb.position.z - entities[i].aabb.hwidth.z;

    gem->frustum_data->max_x[gem->frustum_offset + i] = entities[i].aabb.position.x + entities[i].aabb.hwidth.x;
    gem->frustum_data->max_y[gem->frustum_offset + i] = entities[i].aabb.position.y + entities[i].aabb.hwidth.y;
    gem->frustum_data->max_z[gem->frustum_offset + i] = entities[i].aabb.position.z + entities[i].aabb.hwidth.z;

    gem->lifes[i] -= dt*0.001*10;

  }

  if (got_a_gem)
  {
    th_LightQuery* lq = gem->levelstate->general_light_query;

    th_LightProperties lnew = th_getDefaultLight();
    lnew.life = 350;
    lnew.fadeout = true;
    float sc = 250;
    fn_vec3 lcolor = fn_createVec3(700*sc,1000*sc,700*sc);
    lnew.base_color = lcolor;
    th_makeLight(lq,lnew,lcolor,gem->levelstate->player_e.aabb.position);
  }
}

int th_gemSpawn(th_GemObject* gem,fn_vec3 position,fn_vec3 velocity)
{
  velocity = fn_createVec3s(0.0);

  gem->entities[gem->current_count].aabb.position = position;

  gem->entities[gem->current_count].velocity = velocity;
  gem->entities[gem->current_count].grounded = false;
  gem->lifes[gem->current_count] = 200.0;
  gem->frustum_data->skip_culling_flag[gem->frustum_offset + gem->current_count] = false;
//  gem->orientations[gem->current_count] = orientation;

  {
    a_VirtualSource* s = a_playVirtualSource(56,0,position,NULL );
    a_setVSLoop(s,true);
    a_setVSPos(s,position);
    a_setVSVel(s,velocity);
    a_setVSGain(s,1.0);
    a_setVSRolloff(s,0.45);

    gem->sources[gem->current_count] = s;
  }

  int ret = gem->current_count;

  gem->current_count++;

  if (gem->num_used < (uint)gem->entity_count)
  {
    gem->num_used++;
  }

  if (gem->current_count == gem->entity_count)
  {
    gem->current_count = 0;
  }

  return ret;
}
