#include "th_shotgun.h"
#include "../fn_engine/th_time.h"
#include "../fn_engine/th_decal.h"
#include "../fn_engine/th_particle.h"
#include "../fn_engine/th_audio.h"

#include "../fn_engine/th_threads.h"

#include "../fn_engine/th_level.h"


void th_shotgunInitialize(th_Allocator* alloc,th_ShotgunObject* shotgun,int count,th_LevelState* levelstate)
{
  shotgun->levelstate = levelstate;
  shotgun->firing = false;
  shotgun->entity_count = count;
  shotgun->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  shotgun->bounce = th_alloc(alloc,sizeof(int)*count);
  shotgun->scale = th_alloc(alloc,sizeof(float)*count);
  shotgun->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  for (int i = 0 ; i < count;i++)
  {
    shotgun->bounce[i] = 0;
    shotgun->scale[i] = 1.0;
    shotgun->entities[i] = TH_DEFAULT_ENTITY;
    shotgun->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
    shotgun->entities[i].aabb.hwidth = fn_createVec3(19,19,19);
    shotgun->entities[i].velocity = fn_createVec3s(0);
    shotgun->entities[i].grounded = false;
    shotgun->entities[i].collided = false;
    shotgun->entities[i].aabb.mode = SPHERE;
    shotgun->entities[i].type = TH_SHOTGUN_SHELL;
    shotgun->transforms[i] = fn_makescale(fn_createVec3s(0));
  }

  shotgun->light_current = 0;
  shotgun->time_fired = 0;
  shotgun->num_used = 0;

  shotgun->newVelocity = th_alloc(alloc,sizeof(fn_vec3)*256);





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

static float genRandRange(float min,float max)
{
	float scale = th_random() / (float) RAND_MAX; /* [0, 1.0] */
	return min + scale * ( max - min );      /* [min, max] */
}



typedef struct
{
  th_ShotgunObject* shotgun;
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

  th_ImpactBuffered** impact_list_2d;
  int* impact_counts_2d;

}th_bulletThreadData;

static void thread_bullet(void* data)
{
  th_bulletThreadData* mdata = (th_bulletThreadData*)data;

  int* bounce =  mdata->shotgun->bounce;
  th_Entity* entities = mdata->shotgun->entities;
  fn_mat4* transforms = mdata->shotgun->transforms;
  int offset = mdata->offset;
  int start = mdata->start;
  int thread_id = mdata->thread_id;

  float dt = mdata->dt;

  th_Particle** particles_list2d = mdata->particles_list2d;
  int* counts2d = mdata->counts2d;

  th_DecalOrientation** decals_list2d = mdata->decals_list2d;
  int* counts2d2 = mdata->counts2d2;

  th_ImpactBuffered** impact_list_2d = mdata->impact_list_2d;
  int* impact_counts_2d = mdata->impact_counts_2d;

    th_World* world = mdata->world;

  th_ShotgunObject* shotgun = mdata->shotgun;

  th_Entity* player = mdata->player;

    for (int i = mdata->start + offset ; i < mdata->start + mdata->range + offset;i++)
  {
    if (entities[i].collided)
    {
      transforms[i] = fn_maketranslate(fn_createVec3s(10000000));
      continue;
    }


    fn_vec3 oldvelocity = entities[i].velocity;
    th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,thread_id));
    bool initial_hit = entities[i].collided;
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

      if (impact_counts_2d[thread_id] < 1024)
      {
        th_ImpactBuffered buffered = {(void*)col_e,{(void*)(&entities[i]),pos}};
        impact_list_2d[thread_id][impact_counts_2d[thread_id]] = buffered;
        impact_counts_2d[thread_id] = impact_counts_2d[thread_id] + 1;
      }


      // col_e->impact = true;
      // th_Impact impact = {(void*)(&entities[i]),pos};
      // col_e->impacts[col_e->impact_count] = impact;
      // col_e->impact_count++;
      // if(col_e->impact_count >= MAX_IMPACTS)
      // {
      //   printf("%s\n","MAX IMPACT" );
      //   col_e->impact_count = 0;
      // }

    //  col_e->alive = false;
      shotgun->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);



      continue;
    }



      float len = fn_length(fn_subVec3(player->aabb.position,entities[i].aabb.position));
      if (entities[i].collided)
      {
        // #pragma omp critical
        {
          a_VirtualSource* s = a_playVirtualSource(10 + th_random() % 3,-1,entities[i].aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entities[i].aabb.position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.3);
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
        // p.overbright = 1.1;
        // p.texture_handle = th_getParticleTexture(TH_SPARKS);
        // p.makedecal = false;
        // p.hasphysics = false;
        // p.alphascale = 1.0;
        // p.has_gravity = true;
        // // p.haslight = true;
        // // p.lightcolor = fn_createVec3(60,45,0);
        // // p.lightq = shotgun->levelstate->general_light_query;
        //
        //
        //
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

        th_spawnSparksFewLight(entities[i].collision_normal,entities[i].collision_position,thread_id,mdata->shotgun->levelstate->general_light_query,100);

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


        testdecal.tangent = th_getDecalTangentVector(testdecal.normal);
        testdecal.material_handle = fn_createVec2(th_getParticleTexture(TH_BULLETHOLE).x,0);//14

        if (counts2d2[thread_id]< 50)
        {
          decals_list2d[thread_id][counts2d2[thread_id]] = testdecal;
          counts2d2[thread_id] = counts2d2[thread_id] + 1;
        }





        if (len < 150)
        {
          fn_vec3 av = fn_multVec3s(fn_normalizeVec3(oldvelocity),(-1.f/fn_length(oldvelocity))*0.5f*0.25);
          player->velocity = fn_addVec3(av,player->velocity);
        }

        if (bounce[i] == 0)
        {


          shotgun->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
        }
        else
        {
          entities[i].collided = false;
          bounce[i] = bounce[i] - 1;

          float dot = fn_dot(oldvelocity,entities[i].collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
          fn_vec3 u = fn_multVec3s(entities[i].collision_normal , dot);
          fn_vec3 w = fn_subVec3(oldvelocity , u);
          entities[i].velocity = fn_multVec3s(fn_subVec3(w , u),1.0);//reduce bounce height 0.99,0.75,0.99
        }

      }
      else if (len > 150)
      {


        fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,-1,0),fn_normalizeVec3(fn_multVec3(shotgun->entities[i].velocity,fn_createVec3(1,-1,1))));
        transforms[i] = fn_translaterotatescaleq(entities[i].aabb.position,q_temp,fn_createVec3s(shotgun->scale[i]));

      }
      else
      {


        transforms[i] = fn_maketranslaterotate(fn_createVec3s(10000000),th_time()*0.1,fn_createVec3(1,0,0));
      }





  }
}


void th_shotgunUpdate(th_ShotgunObject* shotgun,float dt,fn_RawInput* input,fn_vec3 direction,fn_vec3 right,fn_vec3 up)
{
  th_World* world = shotgun->levelstate->world;
  fn_vec3 pos = shotgun->levelstate->player_e.aabb.position;
  th_Entity* player = &shotgun->levelstate->player_e;
  bool canfire = shotgun->levelstate->weapon->chosen_weapon == TH_SHOTGUN;

  shotgun->firing = false;
  int count = shotgun->entity_count;
  th_Entity* entities = shotgun->entities;
  fn_mat4* transforms = shotgun->transforms;

  float firedelay = 500;
  if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 2)
  {
    firedelay = 300;
  }
  if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
  {
    firedelay = 400;
  }

  if (input->left && (th_time() - shotgun->time_fired) > firedelay && canfire)
  {


    shotgun->firing = true;

    fn_vec3 offset = fn_multVec3s(up,20);

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
    //   shotgun->entities[shotgun->light_current].aabb.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),0));
    // }
      fn_vec3 target = fn_addVec3(shotgun->entities[shotgun->light_current].aabb.position,fn_normalizeVec3(direction));

    a_VirtualSource* s = a_playVirtualSource(14,-1,pos,NULL);
    a_setVSLoop(s,false);
    a_setVSGain(s,0.5);
    //a_setVSPos(s,fn_addVec3(fn_addVec3(pos,offset),fn_multVec3s(fn_normalizeVec3(direction),20)));
    a_setVSPos(s,fn_multVec3s(fn_createVec3(0,0,0),20));
    a_setVSRelativeToListener(s,true);
    a_setVSVel(s,fn_createVec3s(0));

    fn_vec3 origin = shotgun->entities[shotgun->light_current].aabb.position;
    fn_vec3 ddir = fn_normalizeVec3(fn_subVec3(target,origin));

    float r1 =  (float)th_random()/(float)(RAND_MAX/2.0);
    r1 = (r1 - 1)*0.01;
    float r2 =  (float)th_random()/(float)(RAND_MAX/2.0);
    r2 = (r2 - 1)*0.01;
    ddir = fn_addVec3(ddir,fn_multVec3s(right,r1));
    ddir = fn_addVec3(ddir,fn_multVec3s(up,r2));

    fn_vec3 l = right;
    l =fn_multVec3s(l,-0.5);

    fn_vec3 r = right;
    r =fn_multVec3s(r,0.5);
    fn_vec3 shellpos = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),100));
    fn_vec3 shelldir = direction;

    //start
    int amountFired = 12;
    if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 2)
    {
      amountFired = 30;
    }

    if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
    {
      amountFired = 63;
    }

    fn_vec3* newVelocity = shotgun->newVelocity;

    if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
    {
      int num_groups = 9;
      float ox[9] = {0.0,0.4,-0.4,0.2,-0.2,0.0,0.0,0.2,-0.2 };
      float oy[9] = {0.0,0.0,0.0,0.2,0.2,0.4,-0.4,-0.2,-0.2 };
      float spread = 0.03;
      float choke = 0.43;
      for (int i =0 ;i < num_groups;i++)
      {

        for (int j =0 ;j < 7;j++)
        {
          fn_vec3 add = fn_addVec3(fn_multVec3s(up,oy[i]*choke + genRandRange(-spread,spread)),fn_multVec3s(right,ox[i]*choke + genRandRange(-spread,spread)));
          newVelocity[i*7 + j] = fn_addVec3(direction,add);
        }

      }
    }
    else
    {
      for (int i =0 ;i < amountFired;i++)
      {
        float spread = 0.1;
        if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 2)
        {
          spread=  0.27;
        }
        fn_vec3 add = fn_addVec3(fn_multVec3s(up,genRandRange(-spread,spread)),fn_multVec3s(right,genRandRange(-spread,spread)));
        newVelocity[i] = fn_addVec3(direction,add);
      }
    }


    for (int i =0;i < amountFired;i++)
    {
      if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
      {
        shotgun->bounce[shotgun->light_current] = 3;
        shotgun->scale[shotgun->light_current] = 3.0;
      }
      else
      {
        shotgun->bounce[shotgun->light_current] = 0;
        shotgun->scale[shotgun->light_current] = 2.0;
      }
      float velmult = 1.0;
      if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
      {
        velmult = 1.35;
      }

      shotgun->entities[shotgun->light_current].velocity = fn_multVec3s(fn_normalizeVec3(newVelocity[i]),3*velmult);
      shotgun->entities[shotgun->light_current].aabb.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),20));
      shotgun->entities[shotgun->light_current].collided = false;
      shotgun->light_current++;
      if (shotgun->num_used < (uint)shotgun->entity_count)
      {
        shotgun->num_used++;
      }

      if (shotgun->light_current == (uint)shotgun->entity_count)
      {
        shotgun->light_current = 0;
      }
    }


    //end

    float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
    float z =  (float)th_random()/(float)(RAND_MAX/2.0);
    z -= 1;
    float x = cos(theta);
    float y = sin(theta);
    fn_vec3 n = fn_normalizeVec3(fn_createVec3(x,y,z));


    shelldir =fn_addVec3(shelldir,n);
    r= fn_addVec3(r,fn_multVec3s(n,0.1));
    r = fn_addVec3(r,fn_multVec3s(fn_normalizeVec3(direction),0.1));

    l= fn_addVec3(l,fn_multVec3s(n,0.1));
    l = fn_addVec3(l,fn_multVec3s(fn_normalizeVec3(direction),0.1));

    if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
    {
      //Weapon model will spawn the brass
      //th_brassSpawnScaled(shotgun->levelstate->shotbrass,shellpos,fn_addVec3(r,fn_createVec3(0,-0.5,0)),shelldir,3.5);
    }
    else
    {
      th_brassSpawn(shotgun->levelstate->shotbrass,shellpos,fn_addVec3(r,fn_createVec3(0,-0.5,0)),shelldir);
      th_brassSpawn(shotgun->levelstate->shotbrass,shellpos,fn_addVec3(l,fn_createVec3(0,-0.5,0)),shelldir);
    }


    // th_brassSpawn(shotgun->levelstate->brass,shellpos,fn_createVec3(0,-0.5,0),shelldir);

    th_PointLight light;

    light.pos = fn_createVec4Vec3(fn_addVec3(fn_addVec3(pos,fn_multVec3s(up,20)),fn_multVec3s(fn_normalizeVec3(direction),100)) ,0);

    if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
    {
          light.color = fn_createVec4(8000*12,3000*12,0,0);
    }
    else
    {
          light.color = fn_createVec4(8000*6,3000*6,0,0);
    }

    light.lightmat = fn_identityMat4();
    light.shadowindex = fn_createVec4(0,0,0,0);
    light.pos2 = fn_createVec4(0,0,0,0);
    shotgun->lights[1] = light;


    shotgun->time_fired = th_time();
    // shotgun->levelstate->weapon->kickback = 60;
    if (shotgun->levelstate->player->level_weapon[TH_SHOTGUN]  == 1)
    {
      shotgun->levelstate->weapon->kickback_velocity = 11*0.1;
      shotgun->levelstate->weapon->kickback_acceleration = -0.75*0.01;
    }
    else
    {
      light.pos = fn_createVec4Vec3(fn_addVec3(fn_addVec3(pos,fn_multVec3s(up,20)),fn_multVec3s(fn_normalizeVec3(direction),200)) ,0);
      shotgun->levelstate->weapon->kickback_velocity = 11*0.1*1.5;
      shotgun->levelstate->weapon->kickback_acceleration = -0.75*0.01*2.0;
    }


  }

  if (!input->left || (th_time() - shotgun->time_fired) > 75 )
  {
    th_PointLight light;
    light.pos = fn_createVec4Vec3(fn_createVec3s(1000000),0);
    light.color = fn_createVec4(8000*4,4000*4,0,0);
    light.lightmat = fn_identityMat4();
    light.shadowindex = fn_createVec4(0,0,0,0);
    light.pos2 = fn_createVec4(0,0,0,0);
    shotgun->lights[1] = light;
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

  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_bulletThreadData,shotgun->num_used)
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].offset = 0;
  data[th_thread_id].shotgun = shotgun;
  data[th_thread_id].thread_id = th_thread_id;
  data[th_thread_id].world = world;
  data[th_thread_id].dt = dt;
  data[th_thread_id].particles_list2d = particles_list2d;
  data[th_thread_id].counts2d = counts2d;
  data[th_thread_id].decals_list2d = decals_list2d;
  data[th_thread_id].counts2d2 = counts2d2;
  data[th_thread_id].impact_counts_2d = impact_counts_2d;
  data[th_thread_id].impact_list_2d = impact_list_2d;
  data[th_thread_id].player = player;
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
