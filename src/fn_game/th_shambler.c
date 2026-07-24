#include "th_shambler.h"
#include <stdio.h>
#include "../fn_engine/th_time.h"
#include "th_builtins.h"

#include "../fn_engine/th_level.h"
#include "../fn_engine/th_globals.h"
#include "../fn_engine/th_hitmarker.h"
static const float shambler_scale_factor = 3.5/1.5;
static const float WALK_TIMESCALE = 3.0;//1.56;
static const float WALK_TIMESCALE_FLYING = 0.8;
static const float INJURED_SCALETIME = 0.8;
#define LIGHTNING_RESET_TIME 3000

static void initShambler(th_ShamblerGroup* shamblers, int i)
{
  th_ShamblerData* data = (th_ShamblerData*) shamblers->data;
  shamblers->entities[i] = TH_DEFAULT_ENTITY;
  shamblers->entities[i].mode = TH_SLIDE_MODE;
  shamblers->entities[i].aabb.position = fn_createVec3(0,0,0);

  shamblers->entities[i].aabb.hwidth = fn_createVec3(30*shambler_scale_factor,35*2*shambler_scale_factor,30*shambler_scale_factor);
  shamblers->entities[i].velocity = fn_createVec3s(0);
  shamblers->entities[i].grounded = false;
  shamblers->entities[i].aabb.mode = CAPSULE;
  shamblers->entities[i].alive = false;
  shamblers->entities[i].impact = false;
  shamblers->entities[i].impact_count = 0;
  shamblers->transforms[i] = fn_identityMat4();
  data[i].dir_current = fn_createVec3(-1,0,0);
  data[i].state = RUNNING;
  data[i].reset_anims = true;
  data[i].health = 204.0;
  data[i].mele_attack_timer = -1.0;
  data[i].lightning_hit_timer = -1.0;
  data[i].lightning_timer = th_time() + LIGHTNING_RESET_TIME*2.0 + (float)th_random()/(float)(RAND_MAX/(2000));
  data[i].lightning_target = fn_createVec3s(0);
  data[i].audio_source_growl = NULL;
  data[i].audio_source_growlstep = NULL;
  data[i].audio_source_lightning_charge = NULL;
  data[i].audio_source_lightning_strike = NULL;
  data[i].audio_source_death = NULL;
  data[i].old_mat = fn_identityMat4();
  data[i].fadeout = 1.0;
  data[i].rag_to_death_timer = 0;
  data[i].spawn_time = th_time();

  if (data[i].model_id != -1)
  {
    th_Model* model = &shamblers->levelstate->animated_models[data[i].model_id];
    th_resetModelRagdoll(model,i);
  }


  for (int k = 0; k < (TH_LIGHTNING_HAND_SEGMENTS - 2)*2;k++)
  {
    data[i].randstate_lightning_hand[k] = th_random();
  }

  for (int k = 0; k < (TH_LIGHTNING_BEAM_SEGMENTS - 2)*2;k++)
  {
    data[i].randstate_lightning_beam[k] = th_random();
  }
  data[i].lightning_beam_timer = 0.0;
  data[i].lightning_hand_timer = 0.0;

  data[i].hand_flip_flop = 0;

}

int th_shamblersSpawn(th_ShamblerGroup* shamblers,fn_vec3 pos)
{
  if (shamblers->id_stack.stack_count > 0)
  {
    th_markEnemyBirth(1);
    int x = th_stackPop(&shamblers->id_stack);
    initShambler(shamblers,x);
    shamblers->entities[x].alive = true;
    shamblers->entities[x].aabb.position = pos;
    return x;
  }
  return -1;

}

static float* spawntimes = NULL;
static int order_by_spawntime(const void* a,const void* b)
{

  int idx_a = *(const int *)a;
  int idx_b = *(const int *)b;

  float d1 = spawntimes[idx_a];
  float d2 = spawntimes[idx_b];
  if (d1 < d2)
  {
    return -1;
  }
  else
  {
    return 1;
  }
}

void th_shamblerInitialize(th_Allocator* alloc,th_ShamblerGroup* shamblers,int count,th_LevelState* levelstate,fn_vec3* spawn_locations,
                           float* spawn_times,
                           int num_spawns)
{
  th_stackInit(alloc,&shamblers->id_stack,count);

  int* indices_spawns = malloc(sizeof(int)*num_spawns);
  for (int i = 0 ; i < num_spawns;i++)
  {
    indices_spawns[i] = i;
  }

  shamblers->spawn_locations = spawn_locations;
  shamblers->spawn_times = spawn_times;
  shamblers->num_spawns = num_spawns;
  shamblers->spawn_offset = 0;

  spawntimes = shamblers->spawn_times;
  if (num_spawns > 1)
  {
    qsort(indices_spawns,num_spawns,sizeof(int),order_by_spawntime);

    fn_vec3* tmp_locations = malloc(sizeof(fn_vec3)*num_spawns);
    float* tmp_spawntimes = malloc(sizeof(float)*num_spawns);
    for (int i = 0 ; i < num_spawns;i++)
    {
      tmp_locations[i] = shamblers->spawn_locations[i];
      tmp_spawntimes[i] = shamblers->spawn_times[i];
    }

    for (int i = 0 ; i < num_spawns;i++)
    {
      shamblers->spawn_locations[i] = tmp_locations[indices_spawns[i]];
      shamblers->spawn_times[i] = tmp_spawntimes[indices_spawns[i]];

      printf("SPAWN SHAMBLER TIME %f\n",shamblers->spawn_times[i]);
    }

    free(tmp_locations);
    free(tmp_spawntimes);
  }



  free(indices_spawns);

  shamblers->levelstate = levelstate;
  shamblers->entity_count = count;
  shamblers->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  shamblers->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  th_ShamblerData* data = th_alloc(alloc,sizeof(th_ShamblerData)*count);
  shamblers->data = (void*)data;
  for (int i = 0 ; i < count;i++)
  {
    data[i].model_id = -1;
    initShambler(shamblers,i);
    data[i].state = DEAD;

    th_stackPush(&shamblers->id_stack,i);
  }

}

static void playSoundTerminated(a_VirtualSource** s,int i,th_Entity* entities,int sound_index)
{
  if ( *s == NULL)
  {
    *s = a_playVirtualSource(sound_index,1, entities[i].aabb.position,NULL);
    a_setVSLoop(*s,false);
    a_setVSPos(*s,entities[i].aabb.position);
    a_setVSVel(*s,entities[i].velocity);
    a_setVSGain(*s,0.6);
    a_setVSCleanup(*s,s,a_standardCleanup);
  }
  else
  {
    a_setVSLoop(*s,false);
    a_setVSPos(*s,entities[i].aabb.position);
    a_setVSVel(*s,entities[i].velocity);
    a_setVSGain(*s,0.6);
    a_setVSCleanup(*s,s,a_standardCleanup);
    a_setVSOffset(*s,0.0);
    a_playVS(*s);
  }
}

static void playSoundIfNotPlaying(a_VirtualSource** s,fn_vec3 position,int sound_index)
{
  if ( *s == NULL)
  {
    *s = a_playVirtualSource(sound_index,0, position,NULL);
    a_setVSLoop(*s,false);
    a_setVSPos(*s,position);
    a_setVSVel(*s,fn_createVec3s(0));
    a_setVSGain(*s,0.7);
    a_setVSCleanup(*s,s,a_standardCleanup);

  //  printf("playing %i\n",th_frame());
  }

}

static void updateSound(a_VirtualSource** s,int i,th_Entity* entities)
{
  if (*s != NULL)
  {
    a_setVSPos(*s,entities[i].aabb.position);
    a_setVSVel(*s,entities[i].velocity);
  }
}



// static fn_vec3 dir_current = {-1,0,0};
// static th_Shamblerstate state = RUNNING;
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

static fn_vec3 th_avoidshamblers(th_ShamblerGroup* sham,th_Entity* e,int i,float weight2,float weight3)
{

  th_ShamblerData* data = sham->data;

  int j;
  fn_vec3 pc = fn_createVec3s(0.f);
  fn_vec3 pc2 = fn_createVec3s(0.f);
  int colCount = 0;
  for (j = 0; j <sham->entity_count;j++)
  {

    if (j == i || !sham->entities[j].alive)
    {
      continue;
    }

    float mag2 = fn_length2(fn_subVec3(e->aabb.position,sham->entities[j].aabb.position));

    fn_vec3 delta = fn_multVec3s(fn_subVec3(e->aabb.position,sham->entities[j].aabb.position),weight2);

    //    float vel_mag = fn_length(e->velocity);

    if (mag2 < 350*350)
    {
      pc = fn_addVec3(delta,pc);

      pc2 = fn_addVec3(pc2,sham->entities[j].velocity);
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

//attack03 is lightning

void th_shamblersUpdate(th_ShamblerGroup* shamblers,float dt)
{
  if (shamblers->num_spawns > shamblers->spawn_offset)
  {
    if (th_time() - shamblers->levelstate->level_start_time > shamblers->spawn_times[shamblers->spawn_offset])
    {
      //do spawn
      th_markEnemyDeath(1); //avoid double counting
      th_shamblersSpawn(shamblers,shamblers->spawn_locations[shamblers->spawn_offset]);
      shamblers->spawn_offset++;
    }
  }

  fn_vec3 target = shamblers->levelstate->player_e.aabb.position;
  th_World* world = shamblers->levelstate->world;

  th_Entity* entities =shamblers->entities;
  int count = shamblers->entity_count;
  fn_Grid* grid = &shamblers->grid;
  fn_mat4* transforms = shamblers->transforms;

  fn_vec3 target_primal = target;

  th_ShamblerData* data = (th_ShamblerData*)shamblers->data;
  for (int i = 0; i <count;i++)
  {

    if (data[i].state == RAGDOLLED)
    {

      th_Model* model = &shamblers->levelstate->animated_models[data[i].model_id];
      if(!model->instances[i].ragdoll)
      {
        //fn_vec3 look =fn_normalizeVec3(fn_multVec3(data[i].dir_current,fn_createVec3(1,0,1)));
        // model->instances[i].ragdoll_velocity_center = fn_addVec3(entities[i].velocity,fn_multVec3s(look,-0.45));
        // model->instances[i].ragdoll_velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(look,-0.25));
        //model->instances[i].ragdoll_velocity = entities[i].velocity;
        model->instances[i].ragdoll_mat = transforms[i];
        model->instances[i].ragdoll = true;
        transforms[i] = fn_identityMat4();
        th_updateModelInstance(model,0.0,world,i);

      }
      else
      {
        transforms[i] = fn_identityMat4();
      }

      //updateSound(&data[i].audio_source_death,i,entities);

      if (data[i].audio_source_death != NULL)
      {
        fn_vec3 ragpos = model->instances[i].ragdoll_data.bodies[0].e.aabb.position;
        fn_vec3 ragvel = model->instances[i].ragdoll_data.bodies[0].e.velocity;

        //fn_printVec3(ragpos);

        a_setVSPos(data[i].audio_source_death,ragpos);
        a_setVSVel(data[i].audio_source_death,ragvel);
      }

      if (data[i].rag_to_death_timer + 3500 > th_time())
      {
        fn_mat4 pointa_mat = fn_multMat4(th_getSkeletonElement(model,"mixamorig:LeftHand",i),transforms[i]);

        fn_mat4 pointb_mat = fn_multMat4(th_getSkeletonElement(model,"mixamorig:RightHand",i),transforms[i]);

        fn_vec3 pointa = fn_transformVec3(fn_createVec3(0,0,0),pointa_mat);
        fn_vec3 pointb = fn_transformVec3(fn_createVec3(0,0,0),pointb_mat);

        if (th_time() > data[i].lightning_hand_timer + TH_LIGHTNING_TR_TIME)
        {
          for (int k = 0; k < (TH_LIGHTNING_HAND_SEGMENTS - 2)*2;k++)
          {
            data[i].randstate_lightning_hand[k] = th_random();
          }
          data[i].lightning_hand_timer = th_time();
        }


        th_spawnLighting(pointa,pointb,-1,shamblers->levelstate->general_light_query,4,data[i].randstate_lightning_hand,5.0,35);
      }

      if (data[i].rag_to_death_timer + 15000 < th_time())
      {
        th_resetModelRagdoll(model,i);
        th_stackPush(&shamblers->id_stack,i);
        data[i].state = DEAD;
        transforms[i] = fn_makescale(fn_createVec3s(0));
      }

      continue;
    }

    if (data[i].state == DEAD)
    {
      transforms[i] = fn_makescale(fn_createVec3s(0));
      continue;
    }

    th_Model* model = &shamblers->levelstate->animated_models[data[i].model_id];
    th_pushModelOccluders(model,i,30.0,transforms[i]);

    if (data[i].reset_anims)
    {
      th_setAnim(model,"walk",i);
      data[i].reset_anims = false;
    }

    fn_vec3 epos = entities[i].aabb.position;//fn_createVec3(0,-30,300);

    fn_vec3 dir = fn_normalizeVec3(fn_multVec3(fn_subVec3(target_primal,epos),fn_createVec3(1,0,1)));
    fn_vec3 dir_primal = dir;
    target = fn_addVec3(target_primal,fn_multVec3s(dir,-60));
    dir = fn_normalizeVec3(fn_multVec3(fn_subVec3(target,epos),fn_createVec3(1,0,1)));


    bool close = false;
    fn_vec3 diff = fn_subVec3(entities[i].aabb.position,target_primal);
    if (fn_dotVec2(fn_createVec2(diff.x,diff.z),fn_createVec2(diff.x,diff.z)) <= 65*65 )
    {
      close = true;
    }

    bool almost_close = false;
    if (fn_dotVec2(fn_createVec2(diff.x,diff.z),fn_createVec2(diff.x,diff.z)) <= 500*500 )
    {
      almost_close = true;
    }

    bool player_is_higher = false;
    if (diff.y  > 150)
    {
      player_is_higher = true;
    }


    th_PlayerDefs pdefs_default;
    pdefs_default.friction = 0.02f;
    pdefs_default.gravity = 0.001;
    pdefs_default.jumpSpeed = -0.4;
    pdefs_default.runAcceleration = 0.02*0.9;
    pdefs_default.runDeacceleration = 0.01;
    pdefs_default.moveSpeed = 0.75;
    pdefs_default.sideStrafeSpeed = 1;
    pdefs_default.sideStrafeAcceleration = 0.13;
    pdefs_default.airDecceleration =0.0005;
    pdefs_default.airAcceleration = 0.0005*0.5;
    // entities[i].aabb.position = fn_addVec3(target,fn_createVec3(100,50,0));
    if (entities[i].impact)
    {

      entities[i].impact = false;
      if (data[i].state != PAIN)
      {
        bool no_state_transfer = model->instances[i].current_anim->name != NULL && strcmp(model->instances[i].current_anim->name,"magic") == 0 && (model->instances[i].current_frame > model->instances[i].current_anim->first + 45);//20
        if (!no_state_transfer)
        {
          data[i].lightning_timer = th_time() + LIGHTNING_RESET_TIME*0.25; // RETALIATION
          data[i].state = PAIN;
          // int rand = th_frame() % 5;
          // th_setAnimIDBlend(model,7 + rand ,0.1,i);
          const char* anim_names[] = {"injured", "block", "reaction", "stagger"};

          int idx_anim = th_frame() % 4;

          th_setAnimBlend(model,anim_names[idx_anim],0.1,i);

          th_setTimeScale(model,INJURED_SCALETIME,i);
        }

      }
      float oldhealth= data[i].health;

      fn_vec3 velocity_delta = fn_createVec3s(0);

      const float knockback_scale = 0.65;

      int num_particles = 0;
      for (int j = 0 ; j < entities[i].impact_count;j++)
      {
        th_Entity* projectile = (th_Entity*)entities[i].impacts[j].entity;
        fn_vec3 direction = fn_normalizeVec3(fn_subVec3(entities[i].impacts[j].pos,epos));
        if (projectile->type == TH_HAMMER_PROJECTILE)
        {
          data[i].health = data[i].health - 100.0;
          th_setTimeScale(model,0.9,i);
           entities[i].velocity = fn_addVec3( entities[i].velocity,fn_multVec3s(fn_normalizeVec3(projectile->velocity),0.6*knockback_scale));
           velocity_delta = fn_addVec3( velocity_delta,fn_multVec3s(fn_normalizeVec3(projectile->velocity),0.6));
        }
        else if (projectile->type == TH_MACHINEGUN_BULLET)
        {
          data[i].health = data[i].health - 4.0;
          th_setTimeScale(model,1.2,i);
          entities[i].velocity = fn_addVec3( entities[i].velocity,fn_multVec3s(fn_normalizeVec3(projectile->velocity),0.12*knockback_scale));
          velocity_delta = fn_addVec3( velocity_delta,fn_multVec3s(fn_normalizeVec3(projectile->velocity),0.12));
        }
        else if (projectile->type == TH_SHOTGUN_SHELL)
        {
          data[i].health = data[i].health - 5.0;
          th_setTimeScale(model,1.1,i);
          entities[i].velocity = fn_addVec3( entities[i].velocity,fn_multVec3s(fn_normalizeVec3(projectile->velocity),0.12*knockback_scale));
          velocity_delta = fn_addVec3( velocity_delta,fn_multVec3s(fn_normalizeVec3(projectile->velocity),0.12));
        }

        th_Hitmarker hmarker;
        hmarker.entity_pos_ref = NULL;//&entities[i].aabb.position;
        hmarker.entity_transform_ref = NULL;//&c->transforms_gems[i*7 + j];
        hmarker.alive_ref = &entities[i].alive;
        hmarker.timer = th_time();
        hmarker.last_good_pos = entities[i].impacts[j].pos;
        hmarker.is_alive = true;
        hmarker.radius = 75.0 ;
        th_pushHitmarker(hmarker);

        if (num_particles < 3)
        {
          th_spawnBlood(entities[i].impacts[j].pos,0);


          th_spawnBloodSpurt(direction,entities[i].impacts[j].pos,0);
          num_particles++;
        }

      }
      entities[i].impact_count = 0;
      //printf("%f\n",data[i].health );
      if (data[i].health <= 0 && oldhealth > 0 )
      {
        if (data[i].audio_source_growlstep != NULL)
        {
          a_stopVS(data[i].audio_source_growlstep);
        }
        //th_stackPush(&shamblers->id_stack,i);

        entities[i].alive = false;
        for (int k = 0 ; k < 2;k++)
        {
          fn_vec3 randir = sampleRandomSphere();
          randir.y = -fabs(randir.y)*3;
          randir = fn_multVec3s(fn_normalizeVec3(randir),0.75);
          th_gemSpawn(shamblers->levelstate->gems,entities[i].aabb.position,randir);
        }

        th_markEnemyDeath(1);
        data[i].state = RAGDOLLED;
        data[i].rag_to_death_timer = th_time() ;
        // fn_vec3 look =fn_normalizeVec3(fn_multVec3(data[i].dir_current,fn_createVec3(1,0,1)));
        // model->instances[i].ragdoll_velocity_center = fn_addVec3(model->instances[i].ragdoll_velocity_center,fn_multVec3s(look,-0.45));
        // model->instances[i].ragdoll_velocity = fn_addVec3(model->instances[i].ragdoll_velocity,fn_multVec3s(look,-0.25));
        velocity_delta = fn_multVec3s(velocity_delta,0.65);
        velocity_delta = fn_addVec3(velocity_delta,entities[i].velocity);
        model->instances[i].ragdoll_velocity_center = fn_multVec3s(velocity_delta,0.333);
        model->instances[i].ragdoll_velocity = velocity_delta;

        th_playSoundIfNotPlaying(&data[i].audio_source_death,entities[i].aabb.position,sound_shambler_death,1.0);

        a_setVSPitch(data[i].audio_source_death,th_randomFloat(0.85,1.15));
        a_setVSRolloff(data[i].audio_source_death,0.45);

        //transforms[i] = fn_makescale(fn_createVec3s(0));
        continue;


      }

    }


    fn_vec3 impulse = fn_createVec3s(0);

    float seperation = 0.000015;
    float directional = 0.00002;

    impulse = th_avoidshamblers(shamblers,&entities[i],i,seperation*0.6,directional);
    // fn_vec3 r5 = shamblers->levelstate->centipede == NULL ? fn_createVec3s(0) : avoidagents(shamblers->levelstate->centipede,shamblers->levelstate->centipede->count,&entities[i],seperation*1.0,directional);
    //
    // bool spawn_delay_force = th_time() - data[i].spawn_time > 1000;
    //
    // th_HorseGroup* hptr = shamblers->levelstate->horse;
    // fn_vec3 r7 = (hptr == NULL || !spawn_delay_force) ? fn_createVec3s(0) :  avoidwalkers(hptr,&entities[i],seperation*1.0,directional);
    //
    // th_TricolumnGroup* tptr = shamblers->levelstate->tricolumn;
    // fn_vec3 r8 = (tptr == NULL || !spawn_delay_force)  ? fn_createVec3s(0) :  avoidtricols(tptr,&entities[i],seperation*1.0,directional);
    //
    // impulse = fn_addVec3(impulse,r5);
    // impulse = fn_addVec3(impulse,r7);
    // impulse = fn_addVec3(impulse,r8);


    switch (data[i].state) {
      case RUNNING:
      if (close)
      {
        data[i].lightning_timer = th_time() + LIGHTNING_RESET_TIME;
        data[i].state = ATTACK;
        playSoundTerminated(&data[i].audio_source_growl,i,entities,sound_shambler_growl);
        th_setTimeScale(model,1.2,i);
        // if (player_is_higher)
        // {
          th_setAnimBlend(model,"attack",0.3,i);
        // }
        // else
        // {
        //   //1 through 5
        //   int id = 1 + sin(th_frame())*2 + 2;
        //   id = id == 4 ? 3 : id;
        //   th_setAnimIDBlend(model,id ,0.3,i);
        // }
      }
      if (almost_close && player_is_higher)
      {
        playSoundTerminated(&data[i].audio_source_growl,i,entities,sound_shambler_growl);
        data[i].state = ATTACK;
        th_setTimeScale(model,1.2,i);
        th_setAnimBlend(model,"attack",0.3,i);
        data[i].lightning_timer = th_time() + LIGHTNING_RESET_TIME;
      }
      if (!close && !(almost_close && player_is_higher) && th_time() > data[i].lightning_timer)
      {
        data[i].lightning_hit_timer = -1.0;
        data[i].state = LIGHTNING;
        data[i].audio_source_lightning_strike = NULL;
        th_setTimeScale(model,2.5,i);
        th_setAnimBlend(model,"magic",0.2,i);
        data[i].lightning_target = target;
        playSoundTerminated(&data[i].audio_source_lightning_charge,i,entities,sound_shambler_lightning_strike);
        //playSoundTerminated(&data[i].audio_source_lightning_charge,i,entities,audio_source_lightning_charge);
      }

      th_setTimeScale(model,WALK_TIMESCALE,i);
      data[i].dir_current = fn_cerpVec3(data[i].dir_current,dir,0.01*dt);
      entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(impulse,dt));

      bool prevgrounded = entities[i].grounded;
      th_updatePlayer(&entities[i],world,data[i].dir_current,false,dt,pdefs_default,th_getPhysicsMemory(world,0),false,fn_createVec3s(0));

      if (entities[i].grounded && data[i].audio_source_growlstep == NULL && data[i].state == RUNNING)
      {
        playSoundTerminated(&data[i].audio_source_growlstep,i,entities,sound_shambler_growlstep);
      }

      bool iswalking = model->instances[i].current_anim != NULL && strcmp(model->anims[model->instances[i].current_anim->id].name, "walk") == 0;

      if (iswalking && (!prevgrounded && !entities[i].grounded ) )
      {
        th_setTimeScale(model,WALK_TIMESCALE_FLYING,i);
      }
      else if (iswalking)
      {
        th_setTimeScale(model,WALK_TIMESCALE,i);
      }

      if (data[i].state != RUNNING && data[i].audio_source_growlstep != NULL)
      {
        a_stopVS(data[i].audio_source_growlstep);
      }

      break;
      case ATTACK:
      th_setTimeScale(model,1,i);
      if (model->instances[i].anim_finished && !close)
      {
        th_setAnimBlend(model,"walk",0.3,i);
        th_setTimeScale(model,WALK_TIMESCALE,i);
        data[i].state = RUNNING;
        data[i].lightning_timer = th_time() + LIGHTNING_RESET_TIME;
      }
       else if ( model->instances[i].anim_finished)
      {
        // if (player_is_higher)
        // {
          th_setAnimBlend(model,"attack",0.3,i);
        // }
        // else
        // {
        //   int id = 1 + sin(th_frame())*2 + 2;
        //   id = id == 4 ? 3 : id;
        //   th_setAnimIDBlend(model,id ,0.3,i);
        // }
        data[i].lightning_timer = th_time() + LIGHTNING_RESET_TIME;

      }


      if (close)
      {
        data[i].dir_current = fn_cerpVec3(data[i].dir_current,dir_primal,0.01*dt);
      }

      if ( close && fn_distance2(epos,target) < 250*250 && (data[i].mele_attack_timer == -1.0 || th_time() > data[i].mele_attack_timer) )
      {

        th_setGameGlow(fn_createVec3(1,0,0),0.25);

        th_PlayerObject* po = shamblers->levelstate->player;
        th_decrementPlayerHealth(po,10);
        th_decrementPlayerGem(po,10);
        data[i].mele_attack_timer = th_time() + 333;
      }


      pdefs_default.jumpSpeed = -sqrtf(2*pdefs_default.gravity*(fabs(diff.y) + 200));//-0.4;
      entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(impulse,dt));
      fn_vec3 movdir = data[i].dir_current;
      if (!player_is_higher)
      {
        movdir = fn_createVec3(0,0,0);
      }
      th_updatePlayer(&entities[i],world,movdir,player_is_higher,dt,pdefs_default,th_getPhysicsMemory(world,0),false,fn_createVec3s(0));
      break;
      case LIGHTNING:
      th_setTimeScale(model,0.75,i);

      if (model->instances[i].anim_finished)
      {
        th_setAnimBlend(model,"walk",0.3,i);
        th_setTimeScale(model,WALK_TIMESCALE,i);
        data[i].lightning_timer = th_time() + LIGHTNING_RESET_TIME;
        data[i].state = RUNNING;
      }

      fn_vec3 dir_lightning = fn_normalizeVec3(fn_multVec3(fn_subVec3(data[i].lightning_target,epos),fn_createVec3(1,0,1)));
      fn_vec3 dir_lightning_true = fn_normalizeVec3(fn_multVec3(fn_subVec3(data[i].lightning_target,epos),fn_createVec3(1,1,1)));
      //frame 56
      if (model->instances[i].current_frame >= model->instances[i].current_anim->first + 56)
      {
        fn_vec3 n_temp,p_temp;
        float t_temp;
        bool traced_entity;
        fn_vec3 entity_trace_start = fn_addVec3(epos,fn_multVec3s(fn_normalizeVec3(fn_subVec3(target,epos)),170));
        th_Entity* traced_e = th_traceWithEntitites(TH_ENEMY | TH_USE_RAY,entity_trace_start,target,&n_temp,&traced_entity,0,dt,&p_temp,&t_temp,10);

        fn_vec3 target_dist = fn_addVec3(entities[i].aabb.position,fn_multVec3s(dir_lightning_true,2000));
        fn_vec3 light_offset = fn_multVec3s(dir_lightning_true,150);
        light_offset = fn_addVec3(light_offset,fn_createVec3(0,-150,0));

        fn_vec3 trgt = th_traceVolume(world,epos,target_dist,0.1,NULL,NULL,th_getPhysicsMemory(world,0));

        if (traced_entity && fn_distance2(epos,trgt) > fn_distance2(epos,p_temp))
        {
          trgt = p_temp;
        }

        if (th_time() > data[i].lightning_beam_timer + TH_LIGHTNING_TR_TIME)
        {
          for (int k = 0; k < (TH_LIGHTNING_BEAM_SEGMENTS - 2)*2;k++)
          {
            data[i].randstate_lightning_beam[k] = th_random();
          }
          data[i].lightning_beam_timer = th_time();
          data[i].hand_flip_flop  = (data[i].hand_flip_flop + 1) % 2;
        }
        fn_mat4 pointa_mat = fn_multMat4(th_getSkeletonElement(model,"mixamorig:LeftHand",i),transforms[i]);

        fn_mat4 pointb_mat = fn_multMat4(th_getSkeletonElement(model,"mixamorig:RightHand",i),transforms[i]);

        fn_vec3 pointa = fn_transformVec3(fn_createVec3(0,0,0),pointa_mat);
        fn_vec3 pointb = fn_transformVec3(fn_createVec3(0,0,0),pointb_mat);

        fn_vec3 origin_point = data[i].hand_flip_flop == 0 ? pointa : pointb;

        th_spawnLighting(origin_point,trgt,-1,shamblers->levelstate->general_light_query,20,data[i].randstate_lightning_beam,9.0,175);
        playSoundIfNotPlaying(&data[i].audio_source_lightning_strike,trgt,sound_shambler_lightning_charge);

        bool hit_obstacle = false;
        float t = 0.0;
        fn_vec3 toplayer = th_traceVolume(world,epos,target,0.1,NULL,&hit_obstacle,th_getPhysicsMemory(world,0));



        if ( !traced_entity && fn_distance2(epos,trgt) >= fn_distance2(epos,target)  && (!hit_obstacle || t >= 1.0) && fn_dot(fn_normalizeVec3(fn_subVec3(toplayer,epos)),fn_normalizeVec3(fn_subVec3(trgt,epos))) > 0.997  )
        {
          if (data[i].lightning_hit_timer == -1.0 || th_time() > data[i].lightning_hit_timer )
          {
            th_Entity* player = &shamblers->levelstate->player_e;
            // fn_vec3 d = fn_subVec3(entity->aabb.position,player->aabb.position);
            fn_vec3 av = fn_multVec3s(dir_lightning_true,0.1f);
            player->velocity = fn_addVec3(av,player->velocity);

            th_setGameGlow(fn_createVec3(1,0,0),0.25);

            th_PlayerObject* po = shamblers->levelstate->player;
            th_decrementPlayerHealth(po,2);
            th_decrementPlayerGem(po,2);
            data[i].lightning_hit_timer = th_time() + 50;
          }

        }
      }
      else if (model->instances[i].current_frame <= model->instances[i].current_anim->first + 45)//50
      {

        data[i].lightning_target = target;
      }

      if (model->instances[i].current_frame < model->instances[i].current_anim->first + 56)
      {
        //finger lightning
        fn_mat4 pointa_mat = fn_multMat4(th_getSkeletonElement(model,"mixamorig:LeftHand",i),transforms[i]);

        fn_mat4 pointb_mat = fn_multMat4(th_getSkeletonElement(model,"mixamorig:RightHand",i),transforms[i]);

        fn_vec3 pointa = fn_transformVec3(fn_createVec3(0,0,0),pointa_mat);
        fn_vec3 pointb = fn_transformVec3(fn_createVec3(0,0,0),pointb_mat);

        if (th_time() > data[i].lightning_hand_timer + TH_LIGHTNING_TR_TIME)
        {
          for (int k = 0; k < (TH_LIGHTNING_HAND_SEGMENTS - 2)*2;k++)
          {
            data[i].randstate_lightning_hand[k] = th_random();
          }
          data[i].lightning_hand_timer = th_time();
        }


        th_spawnLighting(pointa,pointb,-1,shamblers->levelstate->general_light_query,4,data[i].randstate_lightning_hand,5.0,35);
      }



      //  else if ( model->instances[i].anim_finished)
      // {
      //   if (player_is_higher)
      //   {
      //     th_setAnim(model,"attack",i);
      //   }
      //   else
      //   {
      //     th_setAnimID(model,1 + sin(th_frame())*2 + 2 ,i);
      //   }
      //
      // }


      if (!close)
      {


        data[i].dir_current = fn_cerpVec3(data[i].dir_current,dir_lightning,0.01*dt);
        //data[i].dir_current = fn_cerpVec3(data[i].dir_current,dir_primal,0.01*dt);
      }

      pdefs_default.jumpSpeed = -sqrtf(2*pdefs_default.gravity*(fabs(diff.y) + 200));//-0.4;
      entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(impulse,dt));
      fn_vec3 movdir2 = data[i].dir_current;
      movdir2 = fn_createVec3(0,0,0);

      th_updatePlayer(&entities[i],world,movdir2,player_is_higher,dt,pdefs_default,th_getPhysicsMemory(world,0),false,fn_createVec3s(0));

      break;

      case PAIN:
      if (model->instances[i].anim_finished)
      {
        th_setAnimBlend(model,"walk",0.3,i);
        th_setTimeScale(model,WALK_TIMESCALE,i);
        data[i].state = RUNNING;
      }



      th_updatePlayer(&entities[i],world,fn_createVec3(0,0,0),false,dt,pdefs_default,th_getPhysicsMemory(world,0),false,fn_createVec3s(0));

      break;
      default:
      break;
    }

    fn_updateEntityGrid(grid,i,dt);
    updateSound(&data[i].audio_source_lightning_charge,i,entities);
    updateSound(&data[i].audio_source_growl,i,entities);
    updateSound(&data[i].audio_source_growlstep,i,entities);
    // updateSound(&data[i].audio_source_death,i,entities);


    fn_quat rot_quat = fn_getRotationQuaternion(fn_createVec3(0,0,-1),fn_createVec3(0,-1,0));
     target = fn_normalizeVec3(fn_multVec3(data[i].dir_current,fn_createVec3(1,0,1)));//;

    // fn_quat rot_quat2 =  fn_getRotationQuaternion(fn_createVec3(0,0,-1),target);//fn_makeQuaternion(fn_radians(th_frame()*0.1),fn_createVec3(0,1,0));
    // rot_quat2.w = rot_quat2.w*-1;
     //target = fn_createVec3(0,0,1);

     float l_angle = atan2(fn_dot(fn_createVec3(0,-1,0),fn_cross(fn_createVec3(0,0,-1),target)),fn_dot (fn_createVec3(0,0,-1),target));
     fn_quat rot_quat2 = fn_makeQuaternion(l_angle,fn_createVec3(0,-1,0));
     rot_quat2.w = rot_quat2.w*-1;

    rot_quat = fn_multquat(rot_quat,rot_quat2);
    fn_vec3 offset = model->instances[i].pose[0].translate;
    // fn_vec3 lookdir = fn_multVec3(offset,fn_createVec3(0,0,1));
    // lookdir = fn_rotatePointQuat(lookdir,rot_quat2);
    epos = fn_addVec3(epos,fn_createVec3(0,55*1.25*shambler_scale_factor,0));
    fn_mat4 mat = fn_identityMat4();//fn_makescale(fn_createVec3s(1.5));

    data[i].fadeout = data[i].fadeout - 0.002*dt;
    if (data[i].fadeout < 0 )
    {
      data[i].fadeout = 0;
    }
    mat= fn_multMat4(mat,fn_translaterotatescaleq(epos,rot_quat,fn_createVec3s(1.0 - fn_clamp(data[i].fadeout,0.0,1.0))));



    transforms[i] = mat;
    //th_printlnDevConsole("%f %f %f",entities[i].aabb.position.x,entities[i].aabb.position.y,entities[i].aabb.position.z);

  }
}
