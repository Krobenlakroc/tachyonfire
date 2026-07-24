#include "th_plasma.h"
#include "../fn_engine/th_time.h"
#include "../fn_engine/th_decal.h"
#include "../fn_engine/th_particle.h"
#include "../fn_engine/th_audio.h"

#include "../fn_engine/th_threads.h"
#include "../fn_engine/th_level.h"
#include "th_weapon.h"

void th_plasmaInitialize(th_Allocator* alloc,th_PlasmaObject* plasma,int count,th_LevelState* levelstate)
{
  plasma->levelstate = levelstate;
  plasma->num_used = 0;
  plasma->firing = false;
  plasma->entity_count = count;
  plasma->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  plasma->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  plasma->scale = th_alloc(alloc,sizeof(float)*count);
  plasma->explosive = th_alloc(alloc,sizeof(int)*count);
  plasma->fakevelocity = th_alloc(alloc,sizeof(fn_vec3)*count);
  for (int i = 0 ; i < count;i++)
  {
    plasma->fakevelocity[i] = fn_createVec3(1,0,0);
    plasma->entities[i] = TH_DEFAULT_ENTITY;
    plasma->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
    plasma->entities[i].aabb.hwidth = fn_createVec3(19,19,19);
    plasma->entities[i].velocity = fn_createVec3s(0);
    plasma->entities[i].grounded = false;
    plasma->entities[i].collided = false;
    plasma->entities[i].aabb.mode = SPHERE;
    plasma->entities[i].type = TH_MACHINEGUN_BULLET;
    plasma->transforms[i] = fn_makescale(fn_createVec3s(0));
    plasma->scale[i] = 1.0;
    plasma->explosive[i] = 0;
  }

  plasma->num_used = 0;
  plasma->light_current = 0;
  plasma->time_fired = 0;
  plasma->expl_entity = TH_DEFAULT_ENTITY;
  plasma->expl_entity.aabb.position = fn_createVec3(10000000,10000000,10000000);
  plasma->expl_entity.aabb.hwidth = fn_createVec3(20,20,20);
  plasma->expl_entity.velocity = fn_createVec3s(0);
  plasma->expl_entity.grounded = false;
  plasma->expl_entity.collided = false;
  plasma->expl_entity.aabb.mode = SPHERE;
  plasma->expl_entity.type = TH_HAMMER_PROJECTILE;

  plasma->flash_particle = th_defaultParticle();
  plasma->flash_particle.scale = 24;
  plasma->flash_2_angle = 0.0;
  plasma->time_flash_scalechange = 0.0;
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
#define FIRE_TIME 75
#define HALF_FIRE_TIME 37.5

typedef struct
{
  th_PlasmaObject* plasma;
  float dt;
  int offset;
  int start;
  int range;
  int thread_id;
  th_World* world;

  th_Particle** particles_list2d;
  int* counts2d;

  th_DecalOrientation** decals_list2d;
  int* counts2d2;

  th_Entity* player;
  int level3;

  th_ImpactBuffered** impact_list_2d;
  int* impact_counts_2d;

}th_bulletThreadData;

static void thread_bullet(void* data)
{

  th_bulletThreadData* mdata = (th_bulletThreadData*)data;

  th_Entity* entities = mdata->plasma->entities;
  fn_mat4* transforms = mdata->plasma->transforms;
  int offset = mdata->offset;
  int start = mdata->start;
  int thread_id = mdata->thread_id;

  float dt = mdata->dt;

  th_Particle** particles_list2d = mdata->particles_list2d;
  int* counts2d = mdata->counts2d;

  th_DecalOrientation** decals_list2d = mdata->decals_list2d;
  int* counts2d2 = mdata->counts2d2;

    th_World* world = mdata->world;

  th_PlasmaObject* plasma = mdata->plasma;

  th_Entity* player = mdata->player;

  th_ImpactBuffered** impact_list_2d = mdata->impact_list_2d;
  int* impact_counts_2d = mdata->impact_counts_2d;

    for (int i = mdata->start + offset ; i < mdata->start + mdata->range + offset;i++)
  {
    if (entities[i].collided)
    {
      transforms[i] = fn_maketranslate(fn_createVec3s(10000000));
      continue;
    }


    fn_vec3 oldvelocity = entities[i].velocity;
    th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,thread_id));
    th_Entity state = entities[i];
    entities[i].velocity = oldvelocity;



    fn_vec3 n,pos;
    float time = 0;
    bool hit = false;
    entities[i].aabb.hwidth = fn_createVec3(32,32,32);
    th_Entity* col_e = th_collideWithEntities(TH_ENEMY,&entities[i],1,&n,&hit,thread_id,dt,&pos,&time);
    entities[i].aabb.hwidth = fn_createVec3(20,20,20);
    if (!hit)
    {
      entities[i] = state;
    }
    else
    {
      th_incrementViolence();
      entities[i].collided = true;
      // col_e->impact = true;
      // th_Impact impact = {(void*)(&entities[i]),pos};
      // col_e->impacts[col_e->impact_count] = impact;
      // col_e->impact_count++;
      // if(col_e->impact_count >= MAX_IMPACTS)
      // {
      //   printf("%s\n","MAX IMPACT" );
      //   col_e->impact_count = 0;
      // }
      if (impact_counts_2d[thread_id] < 1024)
      {
        th_ImpactBuffered buffered = {(void*)col_e,{(void*)(&entities[i]),pos}};
        impact_list_2d[thread_id][impact_counts_2d[thread_id]] = buffered;
        impact_counts_2d[thread_id] = impact_counts_2d[thread_id] + 1;
      }

      if (plasma->explosive[i])
      {
        th_enactExplosion(pos,plasma->levelstate->player,dt,world,player,thread_id,plasma->levelstate->general_light_query,&plasma->expl_entity,impact_list_2d,impact_counts_2d);
      }

    //  col_e->alive = false;
      plasma->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);



      continue;
    }



      float len = fn_length(fn_subVec3(player->aabb.position,entities[i].aabb.position));
      if (entities[i].collided)
      {

        if (plasma->explosive[i])
        {
          fn_vec3 expl_pos = fn_addVec3(entities[i].collision_position,fn_multVec3s(entities[i].collision_normal,20));
          th_enactExplosion(expl_pos,plasma->levelstate->player,dt,world,player,thread_id,plasma->levelstate->general_light_query,&plasma->expl_entity,impact_list_2d,impact_counts_2d);
        }
        else
        {
          th_spawnSparks(entities[i].collision_normal,entities[i].collision_position,thread_id,plasma->levelstate->general_light_query);
        }
       // #pragma omp critical
        {
          a_VirtualSource* s = a_playVirtualSource(10 + th_random() % 3,-1,entities[i].aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entities[i].aabb.position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.5);
        }

        // th_Particle p = th_defaultParticle();
        // float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
        // // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
        // p.theta = 0;
        // p.alive = true;
        // p.life = 5000;
        // p.start_time = th_time();
        // p.stretch = 2;
        // p.scale = 1;
        // p.overbright = 10;
        // p.texture_handle = th_getParticleTexture(TH_SPARKS);
        // p.makedecal = false;
        // p.hasphysics = false;
        // p.alphascale = 1.0;
        // p.has_gravity = true;
        // p.haslight = true;
        // p.lightcolor = fn_createVec3(60,45,0);
        // p.lightq = plasma->levelstate->general_light_query;



        // for (int k = 0 ; k < 7;k++)
        // {
        //   p.velocity = fn_multVec3(entities[i].collision_normal,fn_createVec3s(0.7));
        //   p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.3));
        //   p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(1.5));
        //   p.position = entities[i].collision_position;
        //   // th_addParticle(p);
        //   if (counts2d[thread_id]< 50)
        //   {
        //     particles_list2d[thread_id][counts2d[thread_id]] = p;
        //     counts2d[thread_id] = counts2d[thread_id] + 1;
        //   }
        //
        // }

        th_DecalOrientation testdecal;
        testdecal.normal = fn_multVec3(entities[i].collision_normal,fn_createVec3(-1,-1,-1));
        testdecal.pos = fn_addVec3(entities[i].collision_position,fn_multVec3s(testdecal.normal,2));
        testdecal.scale = fn_createVec3(20,20,50);

        // fn_quat q;
        // if (fn_equalVec3(testdecal.normal,fn_createVec3(-1.000000 ,-0.000000, -0.000000)))
        // {
        //   q = fn_getRotationQuaternion(fn_createVec3(-1,0,0),testdecal.normal);
        // }
        // else
        // {
        //   q = fn_getRotationQuaternion(fn_createVec3(1,0,0),testdecal.normal);
        // }
        //
        //
        // testdecal.tangent = fn_rotatePointQuat(fn_createVec3(0,-1,0),q);
        // fn_vec3 up_decal = fn_createVec3(0,-1,0);
        // if (fn_almostequalVec3(fn_createVec3(0,-1,0),testdecal.normal,0.001) || fn_almostequalVec3(fn_createVec3(0,1,0),testdecal.normal,0.001))
        // {
        //   up_decal = fn_createVec3(1,0,0);
        // }
        testdecal.tangent = th_getDecalTangentVector(testdecal.normal);
        testdecal.material_handle = fn_createVec2(th_getParticleTexture(TH_BULLETHOLE).x,0);//14

        if (counts2d2[thread_id]< 50)
        {
        decals_list2d[thread_id][counts2d2[thread_id]] = testdecal;
        counts2d2[thread_id] = counts2d2[thread_id] + 1;
      }





        if (len < 150 && !mdata->level3)
        {
        fn_vec3 av = fn_multVec3s(fn_normalizeVec3(oldvelocity),(-1.f/3.0)*0.5f);
        player->velocity = fn_addVec3(av,player->velocity);
        }

          plasma->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
      }
      else if (len > 250)
      {


        fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,-1,0),fn_normalizeVec3(fn_multVec3(plasma->entities[i].velocity,fn_createVec3(1,-1,1))));
        transforms[i] = fn_translaterotatescaleq(entities[i].aabb.position,q_temp,fn_createVec3s(plasma->scale[i]));

        if (len > 400)
        {
          fn_vec3 trail_begin = fn_addVec3(entities[i].aabb.position,fn_multVec3s(plasma->fakevelocity[i],-55));

          th_spawnTracer(entities[i].aabb.position,trail_begin,thread_id);
        }

      }
      else
      {


        transforms[i] = fn_maketranslaterotate(fn_createVec3s(10000000),th_time()*0.1,fn_createVec3(1,0,0));
      }





  }
}


void th_plasmaUpdate(th_PlasmaObject* plasma,float dt,fn_RawInput* input,fn_vec3 direction,fn_vec3 right,fn_vec3 up)
{
  const int EXPL_CHANCE = 10;
  th_World* world = plasma->levelstate->world;
  fn_vec3 pos = plasma->levelstate->player_e.aabb.position;
  th_Entity* player = &plasma->levelstate->player_e;
  th_PlayerObject* playerobj = plasma->levelstate->player;
  bool canfire = plasma->levelstate->weapon->chosen_weapon == TH_MACHINEGUN;

  plasma->firing = false;
  int count = plasma->entity_count;
  th_Entity* entities = plasma->entities;
  fn_mat4* transforms = plasma->transforms;

  float firetimemult = 1.0;
  if (playerobj->level_weapon[TH_MACHINEGUN]  == 2)
  {
    firetimemult = 0.65;
  }

  if (playerobj->level_weapon[TH_MACHINEGUN]  == 3)
  {
    firetimemult = 0.5;
  }

  if (input->left && (th_time() - plasma->time_fired) > FIRE_TIME*firetimemult && canfire)
  {
    int reps = 1;
    if (playerobj->level_weapon[TH_MACHINEGUN]  == 3)
    {
      reps = 2;
    }
    for (int k = 0 ; k < reps;k++)
    {
      plasma->firing = true;

      fn_vec3 offset = fn_multVec3s(up,20);
      plasma->entities[plasma->light_current].aabb.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),20));
      fn_vec3 fake_origin =  fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(up),35));

      if (playerobj->level_weapon[TH_MACHINEGUN]  == 3)
      {
        if (k == 0)
        {
          fake_origin =  fn_addVec3(fake_origin,fn_multVec3s(fn_normalizeVec3(right),10));
        }
        else
        {
          fake_origin =  fn_addVec3(fake_origin,fn_multVec3s(fn_normalizeVec3(right),-10));
        }
      }

      //plasma->entities[plasma->light_current].aabb.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(up),35));

      plasma->entities[plasma->light_current].collided = false;
      plasma->scale[plasma->light_current] = 1.0;
      plasma->explosive[plasma->light_current] = 0;
      if (playerobj->level_weapon[TH_MACHINEGUN]  == 3)
      {
        plasma->scale[plasma->light_current] = 1.7;
        plasma->explosive[plasma->light_current] = plasma->light_current % EXPL_CHANCE == 0;
      }
      float w_time;
      bool w_hit;
      //fn_vec3 target = th_trace(world,pos,fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),1000)),NULL,&w_hit,th_getPhysicsMemory(world,0),&w_time);//fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),1000));

      // fn_vec3 e_norm,e_pos;
      // bool e_hit;
      // float e_time;
      // th_EntityEdictFlags flags = 0;
      // if (!w_hit)
      // {
      //   flags = TH_USE_RAY;
      // }
      // //th_traceWithEntitites(flags  ,pos,fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),w_time*1000)),&e_norm,&e_hit,0,1.0,&e_pos,&e_time);
      // if ((e_hit && e_time < w_time) || (!w_hit && e_hit))
      // {
      //   target = e_pos;
      // }

      // if (fn_distance(target,pos)< 500)
      // {
      //   plasma->entities[plasma->light_current].aabb.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),0));
      // }
      fn_vec3 target = fn_addVec3(plasma->entities[plasma->light_current].aabb.position,fn_normalizeVec3(direction));

      fn_vec3 target_fake = fn_addVec3(plasma->entities[plasma->light_current].aabb.position,fn_multVec3s(fn_normalizeVec3(direction),500));

      if (k == 0)
      {
        a_VirtualSource* s = a_playVirtualSource(1 + th_frame()%4,-1,fn_createVec3(0,0,0),NULL);
        // printf("%i\n",s->vid );
        a_setVSLoop(s,false);
        // a_setVSPos(s,fn_addVec3(fn_addVec3(pos,offset),fn_multVec3s(fn_normalizeVec3(direction),20)));
        //a_setVSPos(s,fn_multVec3s(fn_normalizeVec3(direction),20));
        a_setVSPos(s,fn_multVec3s(fn_createVec3(0,0,0),20));
        a_setVSRelativeToListener(s,true);
        a_setVSVel(s,fn_createVec3s(0));
        a_setVSGain(s,0.5);
      }


      fn_vec3 origin = plasma->entities[plasma->light_current].aabb.position;
      fn_vec3 ddir = fn_normalizeVec3(fn_subVec3(target,origin));

      float spread = 0.01;
      if (playerobj->level_weapon[TH_MACHINEGUN]  == 2)
      {
        spread = 0.05;
      }
      if (playerobj->level_weapon[TH_MACHINEGUN]  == 3)
      {
        spread = 0.15;
      }

      float r1 =  (float)th_random()/(float)(RAND_MAX/2.0);
      r1 = (r1 - 1)*spread;
      float r2 =  (float)th_random()/(float)(RAND_MAX/2.0);
      r2 = (r2 - 1)*spread;
      ddir = fn_addVec3(ddir,fn_multVec3s(right,r1));
      ddir = fn_addVec3(ddir,fn_multVec3s(up,r2));
      plasma->entities[plasma->light_current].velocity = fn_multVec3s(fn_normalizeVec3(ddir),6.7);

      fn_vec3 ddir_fake = fn_normalizeVec3(fn_subVec3(target_fake,fake_origin));
      ddir_fake = fn_addVec3(ddir_fake,fn_multVec3s(right,r1));
      ddir_fake = fn_addVec3(ddir_fake,fn_multVec3s(up,r2));

      plasma->fakevelocity[plasma->light_current] = fn_multVec3s(fn_normalizeVec3(ddir_fake),6.7);

      fn_vec3 r = right;
      r =fn_multVec3s(r,0.5);

      if (playerobj->level_weapon[TH_MACHINEGUN]  == 3 && k == 1)
      {
         r =fn_multVec3s(r,-1);
      }
      fn_vec3 shellpos = fn_addVec3(plasma->entities[plasma->light_current].aabb.position,fn_multVec3s(fn_normalizeVec3(direction),100));
      if (playerobj->level_weapon[TH_MACHINEGUN]  == 3 )
      {
        if (k == 0)
        {
          shellpos = fn_addVec3(shellpos,fn_multVec3s(fn_normalizeVec3(right),65));
          shellpos = fn_addVec3(shellpos,fn_multVec3s(fn_normalizeVec3(up),30));
        }
        else
        {
          shellpos = fn_addVec3(shellpos,fn_multVec3s(fn_normalizeVec3(right),-65));
          shellpos = fn_addVec3(shellpos,fn_multVec3s(fn_normalizeVec3(up),30));
        }
      }

      fn_vec3 shelldir = direction;

      float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
      float z =  (float)th_random()/(float)(RAND_MAX/2.0);
      z -= 1;
      float x = cos(theta);
      float y = sin(theta);
      fn_vec3 n = fn_normalizeVec3(fn_createVec3(x,y,z));


      shelldir =fn_addVec3(shelldir,n);
      r= fn_addVec3(r,fn_multVec3s(n,0.1));

      if (playerobj->level_weapon[TH_MACHINEGUN]  != 1)
      {
        th_brassSpawn(plasma->levelstate->brass,shellpos,fn_addVec3(r,fn_createVec3(0,-0.5,0)),shelldir);
      }

      th_PointLight light;
      light.pos = fn_createVec4Vec3(fn_addVec3(plasma->entities[plasma->light_current].aabb.position,fn_multVec3s(fn_normalizeVec3(direction),100)) ,0);
      light.color = fn_createVec4(8000*2,3000*2,0,0);
      light.lightmat = fn_identityMat4();
      light.shadowindex = fn_createVec4(0,0,0,0);
      light.pos2 = fn_createVec4(0,0,0,0);
      plasma->lights[0] = light;


      th_Particle p = plasma->flash_particle ;
      float theta2 = 0.0;//0.7853982 + (float)th_random()/(float)(RAND_MAX/(0.5*3.14159));
      // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
      p.theta = theta2;
      p.alive = true;
      p.life = 0.1;
      p.start_time = th_time();
      p.stretch = 0;

        p.scale = 10 + (float)th_random()/(float)(RAND_MAX/(7.0));
        p.scale = p.scale *2.0;

      if ((th_time() - plasma->time_flash_scalechange) > FIRE_TIME)
      {
        plasma->time_flash_scalechange = th_time();
      }

      p.scale_end = p.scale;
      p.overbright = 1.01;
      p.texture_handle = th_getParticleTexture(TH_MACHINEGUN_FLASH);
      p.makedecal = false;
      p.alphascale = 0.7;
      p.hasphysics = false;
      p.has_gravity = false;
      p.use_premultiplied = 0.0;

      p.velocity = fn_createVec3s(0);
      p.position = fn_addVec3(plasma->entities[plasma->light_current].aabb.position,fn_multVec3s(fn_normalizeVec3(direction),110));
      p.position = fn_addVec3(p.position,fn_multVec3s(fn_normalizeVec3(up),50));
      //th_addParticle(p);
      plasma->flash_particle = p;
      plasma->flash_2_angle = 0.0;// 0.7853982 + (float)th_random()/(float)(RAND_MAX/(0.5*3.14159));



      plasma->light_current++;
      plasma->time_fired = th_time();
      if (plasma->num_used < (uint)plasma->entity_count)
      {
        plasma->num_used++;
      }

      if (plasma->light_current == (uint)plasma->entity_count)
      {
        plasma->light_current = 0;
      }
    }

  }


  if (playerobj->level_weapon[TH_MACHINEGUN]  == 1 )
  {
    if (!input->left || (th_time() - plasma->time_fired) > HALF_FIRE_TIME*firetimemult )
    {
      th_PointLight light;
      light.pos = fn_createVec4Vec3(fn_createVec3s(1000000),0);
      light.color = fn_createVec4(8000*2,3000*2,0,0);
      light.lightmat = fn_identityMat4();
      light.shadowindex = fn_createVec4(0,0,0,0);
      light.pos2 = fn_createVec4(0,0,0,0);
      plasma->lights[0] = light;
    }
  }
  else
  {
    if (!input->left || !canfire )
    {
      th_PointLight light;
      light.pos = fn_createVec4Vec3(fn_createVec3s(1000000),0);
      light.color = fn_createVec4(8000*2,3000*2,0,0);
      light.lightmat = fn_identityMat4();
      light.shadowindex = fn_createVec4(0,0,0,0);
      light.pos2 = fn_createVec4(0,0,0,0);
      plasma->lights[0] = light;
    }

  }




  static th_Particle** particles_list2d = NULL;
  static int* counts2d = NULL;
  static th_DecalOrientation** decals_list2d = NULL;
  static int* counts2d2 = NULL;
  static th_ImpactBuffered** impact_list_2d = NULL;
  static int* impact_counts_2d = NULL;

 if (counts2d2 == NULL)
 {
   particles_list2d = malloc(sizeof(th_Particle*)*th_getNumThreads());//[th_getNumThreads()][50];
   for (int i = 0; i < th_getNumThreads(); i++) {
     particles_list2d[i] = malloc(sizeof(th_Particle)*50);
   }
   counts2d = malloc(sizeof(int)*th_getNumThreads());

   decals_list2d = malloc(sizeof(th_DecalOrientation*)*th_getNumThreads());//[th_getNumThreads()][50];
   for (int i = 0; i < th_getNumThreads(); i++) {
     decals_list2d[i] = malloc(sizeof(th_DecalOrientation)*50);
   }
   counts2d2 = malloc(sizeof(int)*th_getNumThreads());

   impact_list_2d = malloc(sizeof(th_ImpactBuffered*)*th_getNumThreads());
   for (int i = 0; i < th_getNumThreads(); i++) {
     impact_list_2d[i] = malloc(sizeof(th_ImpactBuffered)*1024);
   }
   impact_counts_2d = malloc(sizeof(int)*th_getNumThreads());
 }


  memset(counts2d,0,sizeof(int)*th_getNumThreads());




  memset(counts2d2,0,sizeof(int)*th_getNumThreads());

  memset(impact_counts_2d,0,sizeof(int)*th_getNumThreads());



  //TODO Multithread

  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_bulletThreadData,plasma->num_used)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].offset = 0;
  data[th_thread_id].plasma = plasma;
  data[th_thread_id].thread_id = th_thread_id;
  data[th_thread_id].world = world;
  data[th_thread_id].dt = dt;
  data[th_thread_id].particles_list2d = particles_list2d;
  data[th_thread_id].counts2d = counts2d;
  data[th_thread_id].decals_list2d = decals_list2d;
  data[th_thread_id].counts2d2 = counts2d2;
  data[th_thread_id].player = player;
  data[th_thread_id].level3 = playerobj->level_weapon[TH_MACHINEGUN]  == 3;
  data[th_thread_id].impact_counts_2d = impact_counts_2d;
  data[th_thread_id].impact_list_2d = impact_list_2d;
  TH_SCHEDULING_FUNC
  th_setThread(thread_bullet,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING



    for (int i = 0 ; i < th_getNumThreads();i++)
    {
      for (int j = 0 ; j < counts2d2[i];j++)
      {
        th_addDecal(decals_list2d[i][j]);
      }
    }

  for (int i = 0 ; i < th_getNumThreads();i++)
  {
    for (int j = 0 ; j < counts2d[i];j++)
    {
      th_addParticle(particles_list2d[i][j]);
    }
  }

  for (int i = 0 ; i < th_getNumThreads();i++)
  {
    for (int j = 0 ; j < impact_counts_2d[i];j++)
    {
      th_Entity* col_e = (th_Entity*)impact_list_2d[i][j].col_e;
      col_e->impact = true;
      th_Impact impact = impact_list_2d[i][j].impact;//{(void*)(&entities[i]),pos};
      col_e->impacts[col_e->impact_count] = impact;
      col_e->impact_count++;
      if(col_e->impact_count >= MAX_IMPACTS)
      {
        printf("%s\n","MAX IMPACT" );
        col_e->impact_count = 0;
      }
    }
  }
}
