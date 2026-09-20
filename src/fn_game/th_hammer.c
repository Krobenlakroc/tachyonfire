#include "th_hammer.h"
#include "../fn_engine/th_time.h"
#include "../fn_engine/th_decal.h"
#include "../fn_engine/th_particle.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_threads.h"
#include "th_builtins.h"
#include "th_splineutils.h"

#include "../fn_engine/th_level.h"

#define TH_GEM_SUCK_HAMMER 300

#define TH_SWING_ANIM_CNT 6
static fn_Transform swing_anim_transforms[TH_SWING_ANIM_CNT];
static float swing_anim_alphas[TH_SWING_ANIM_CNT];

static fn_mat4 r_camera(fn_vec3 pos,fn_vec2 angles)
{

  angles.y = fn_clamp(angles.y,-fn_radians(90),fn_radians(90));
  fn_mat4 modelView = fn_identityMat4();//glm::lookAt(-getLocation(),getLook(),glm::vec3(0,-1,0));
  //modelView = glm::lookAt(getLocation(),getLook(),glm::vec3(0,1,0));
  //  modelView = fn_rotate(modelView, -glm::radians(camRoll), glm::vec3(0.0f, 0.0f, 1.0f));
  modelView = fn_rotate(modelView, angles.y, fn_createVec3(1.0f, 0.0f, 0.0f));
  modelView = fn_rotate(modelView, angles.x, fn_createVec3(0.0f, 1.0f, 0.0f));
  //  modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
  modelView = fn_translate(modelView,fn_multVec3(pos,fn_createVec3(1,1,1)));//*glm::vec3(-1,1,-1));
  modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
  return modelView;
}

void th_hammerInitialize(th_Allocator* alloc,th_HammerObject* object,fn_mat4* transforms,fn_mat4* sledge_transform,th_LevelState* levelstate)
{
  object->impact_timer = 0;
  object->impact_pos = fn_createVec3s(0);
  object->entity_count = 2;

  object->entities_impacts = th_alloc(alloc,sizeof(th_Entity)*3);

  object->entities = th_alloc(alloc,sizeof(th_Entity)*2);
  object->levelstate = levelstate;
  object->transforms = transforms;
  object->sledge_transform = sledge_transform;
  for (int i = 0; i < 2;i++)
  {
    object->time_fired[i] = 0;

    object->is_held[i] = true;
    object->has_gravity[i] = false;
    object->flysound[i] = NULL;
    object->last_hit_time[i] = 0.0;
    object->num_hits[i] = 0;
    object->old_up[i] =fn_createVec3(0,-1,0);
    object->rot_angles_flight[i] = 0.0;
    object->caught_up_animation[i] = false;
    object->animation_interpose[i] = false;
    object->timer_equip_animation[i] = 0.0;
    object->equip_frame_0[i] = fn_identityMat4();
  }

  for (int i = 0 ; i < 2;i++)
  {
    object->entities[i] = TH_DEFAULT_ENTITY;
    object->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
    object->entities[i].aabb.hwidth = fn_createVec3(40,40,40);
    object->entities[i].velocity = fn_createVec3s(0);
    object->entities[i].grounded = false;
    object->entities[i].collided = false;
    object->entities[i].aabb.mode = SPHERE;
    object->entities[i].type = TH_HAMMER_PROJECTILE;
  }

  for (int i = 0 ; i < 3;i++)
  {
    object->entities_impacts[i] = TH_DEFAULT_ENTITY;
    object->entities_impacts[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
    object->entities_impacts[i].aabb.hwidth = fn_createVec3(40,40,40);
    object->entities_impacts[i].velocity = fn_createVec3s(0);
    object->entities_impacts[i].grounded = false;
    object->entities_impacts[i].collided = false;
    object->entities_impacts[i].aabb.mode = SPHERE;
    object->entities_impacts[i].type = TH_HAMMER_PROJECTILE;
  }

  object->is_held_sledge = true;
  object->time_fired_sledge = -5000;
  object->sledge_forward = fn_createVec3(0,-1,0);
  object->sledge_up = fn_createVec3(1,0,0);
  object->sledge_position = fn_createVec3(0,0,0);
  object->sledge_position_collider = fn_createVec3(0,0,0);
  object->sledge_fly = false;
  object->sledge_impact_timer = -5000;
  object->sledge_impact = false;
  object->sledge_entity = TH_DEFAULT_ENTITY;
  object->sledge_entity.aabb.position = fn_createVec3(10000000,10000000,10000000);
  object->sledge_entity.aabb.hwidth = fn_createVec3(10,10,10);
  object->sledge_entity.velocity = fn_createVec3s(0);
  object->sledge_entity.grounded = false;
  object->sledge_entity.collided = false;
  object->sledge_entity.aabb.mode = SPHERE;
  object->sledge_entity.type = TH_HAMMER_PROJECTILE;

  for (int i = 0 ; i < 6;i++)
  {
    object->lightning_tips[i] = fn_createVec3(0,0,0);

    for (int k = 0;k < (HAMMER_LIGHT_SEGMENTS - 2)*2;k++)
    {
      object->randstate_lightning_beam[i][k] = 0;
    }

  }
  object->lightning_timer = 0;
  object->num_lightning_tips = 0;

  object->equip_duration = 150.0;
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




void th_hammerUpdate(th_HammerObject* object,float dt,fn_RawInput* input,fn_vec3 direction,fn_vec3 right,fn_vec3 up,fn_vec3 target_pos,fn_vec2 target_angles)
{
  th_World* world = object->levelstate->world;
  fn_vec3 pos = object->levelstate->player_e.aabb.position;
  th_Entity* player = &object->levelstate->player_e;
  bool canfire = object->levelstate->weapon->chosen_weapon == TH_HAMMER;

  int count = object->entity_count;
  th_Entity* entities = object->entities;
  fn_mat4* transforms = object->transforms;

  bool fire_time = (th_time() - object->time_fired[0]) > 700;
  if (object->levelstate->player->level_weapon[TH_HAMMER]  == 2)
  {
    fire_time = (th_time() - object->time_fired[0]) > 500 && (th_time() - object->time_fired[1]) > 500;
  }

  fire_time = fire_time && (th_time() - object->sledge_impact_timer > 1000);

  bool fire_a = object->is_held[0] && fire_time;

  bool fire_b = object->is_held[1] && fire_time && (object->levelstate->player->level_weapon[TH_HAMMER]  == 2);

  if (object->levelstate->player->level_weapon[TH_HAMMER]  == 2 && fire_a && fire_b && (th_time() - object->time_fired[1]) > (th_time() - object->time_fired[0]) )
  {
    fire_a = false;
  }



  if (input->left && canfire)
  {
    if (object->levelstate->player->level_weapon[TH_HAMMER]  == 3)
    {
      if ((th_time() - object->time_fired_sledge) > 5000)
      {
        object->time_fired_sledge = th_time();
        th_setGameplayTimeScale(fn_createVec3(0.33,0.00000,0.0000002));
        object->is_held_sledge = false;
        object->sledge_forward = fn_normalizeVec3(fn_createVec3(direction.x,1,direction.z));
        object->sledge_up = fn_cross(object->sledge_forward,fn_normalizeVec3(fn_createVec3(-direction.x,0,-direction.z)));
        object->sledge_fly = false;
        object->sledge_impact = false;
        //spawn hammer projectile box in front of player at t = delta
        //check if can hit ground, activate ground smash
      }

    }
    else
    {
      if (fire_a)
      {
        object->old_up[0] = fn_multVec3s(right,-1.0);
        object->rot_angles_flight[0] = 0.0;
        object->time_fired[0] = th_time();
        object->num_hits[0] = 0;
        object->caught_up_animation[0] = false;
        //fire
        object->is_held[0] = false;

        object->entities[0].aabb.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),40));

        fn_vec3 target = fn_addVec3(object->entities[0].aabb.position,fn_normalizeVec3(direction));

        object->entities[0].velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(target,object->entities[0].aabb.position)),3);

        {
          a_VirtualSource* s = a_playVirtualSource(42,0,object->entities[0].aabb.position,NULL );
          a_setVSLoop(s,true);
          a_setVSPos(s,object->entities[0].aabb.position);
          a_setVSVel(s,object->entities[0].velocity);
          a_setVSGain(s,0.4);

          object->flysound[0] = s;
        }
      }
      else if (fire_b)
      {
        object->old_up[1] = fn_multVec3s(right,1.0);
        object->rot_angles_flight[1] = 0.0;
        object->time_fired[1] = th_time();
        object->num_hits[1] = 0;
        //fire
        object->is_held[1] = false;
        object->caught_up_animation[1] = false;

        object->entities[1].aabb.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(direction),40));

        fn_vec3 target = fn_addVec3(object->entities[1].aabb.position,fn_normalizeVec3(direction));

        object->entities[1].velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(target,object->entities[1].aabb.position)),3);

        {
          a_VirtualSource* s = a_playVirtualSource(42,0,object->entities[1].aabb.position,NULL );
          a_setVSLoop(s,true);
          a_setVSPos(s,object->entities[1].aabb.position);
          a_setVSVel(s,object->entities[1].velocity);
          a_setVSGain(s,0.4);

          object->flysound[1] = s;
        }
      }
    }



  }

  int totaldmgcount = 0;
  bool sledge_impacted = false;

  if (!object->is_held_sledge)
  {
    if (th_time() - object->time_fired_sledge < 320)
    {

      float delt_angle = th_time() - object->time_fired_sledge < 120 ? 0.0 : 0.7;

      fn_vec3 flatdir = fn_normalizeVec3(fn_createVec3(-direction.x,0,-direction.z));
      fn_vec3 flatdir_forward = fn_normalizeVec3(fn_createVec3(-direction.x,-0.4,-direction.z));
      object->sledge_up = fn_cross(fn_createVec3(0,1,0),flatdir);
      fn_quat sledge_quat = th_update_orient(&object->sledge_forward,flatdir_forward,delt_angle,dt,&object->sledge_up);

      fn_vec3 radial_offset = fn_multVec3s(object->sledge_forward,-150);
      object->sledge_position = fn_addVec3(player->aabb.position,fn_addVec3(radial_offset,fn_multVec3s(right,-30)));
      object->sledge_transform[0] = fn_translaterotatescaleq(object->sledge_position,sledge_quat,fn_createVec3s(2));
      object->sledge_position_collider = player->aabb.position;

      fn_Transform start = fn_decomposeMat4(object->levelstate->weapon->weapon_transform_sledge_ref);

      fn_Transform end = fn_decomposeMat4(object->sledge_transform[0]);

      float alpha = th_time() - object->time_fired_sledge;
      alpha = fn_clamp(alpha/120.0,0.0,1.0);

      fn_Transform interp =  fn_lerpTransforms(start,end,alpha);




      if (th_time() - object->time_fired_sledge < 120)
      {
        object->sledge_transform[0] = fn_transformToMat(interp);
      }
      // else
      // {
      //   object->sledge_transform[0] = fn_transformToMat(interp);
      // }




      object->sledge_fly = !player->grounded;
      //make weapon switch not possible


    }
    else if (!object->sledge_impact )
    {
      bool impacted = false;
      fn_vec3 impact_normal = fn_createVec3(0,-1,0);
      if (!object->sledge_fly)
      {
        //play sound
        //AOE kill
        //particle effect
        //slowmo hit
        //dcrease level to level1
        th_setGameplayTimeScale(fn_createVec3(0.1,0.00000,0.0000002));
        object->sledge_impact_timer = th_time();
        object->sledge_impact = true;
        impacted = true;
      }
      else
      {
        fn_vec3 old_pos = object->sledge_position_collider;
        fn_vec3 new_pos = fn_addVec3(old_pos,fn_multVec3s(fn_createVec3(0,1,0),dt*3));

        fn_vec3 new_pos_display = fn_addVec3(object->sledge_position,fn_multVec3s(fn_createVec3(0,1,0),dt*3));
        object->sledge_position = new_pos_display;

        bool is_hit = false;
        fn_vec3 hpos =  th_traceVolume(world,old_pos,new_pos,20,&impact_normal,&is_hit,th_getPhysicsMemory(world,0));
        object->sledge_position_collider = hpos;
        fn_quat sledge_quat = th_update_orient(&object->sledge_forward,object->sledge_forward,0.0,dt,&object->sledge_up);
        object->sledge_transform[0] = fn_translaterotatescaleq(object->sledge_position,sledge_quat,fn_createVec3s(2));
        if (is_hit)
        {
          //play sound
          //AOE kill
          //particle effect
          //slowmo hit
          //dcrease level to level1
          th_setGameplayTimeScale(fn_createVec3(0.1,0.00000,0.0000002));
          object->sledge_impact_timer = th_time();
          object->sledge_impact = true;
          impacted = true;
        }
      }

      if (impacted)
      {
        sledge_impacted = true;

        {
          a_VirtualSource* s = a_playVirtualSource(43,-1,object->sledge_position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,object->sledge_position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.6);
        }

        {
          a_VirtualSource* s = a_playVirtualSource(15 + th_random() % 3,-1,object->sledge_position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,object->sledge_position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.6);
        }
        th_Particle p = th_defaultParticle();
        float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
        // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
        p.theta = 0;
        p.alive = true;
        p.life = 5000;
        p.start_time = th_time();
        p.stretch = 2;
        p.scale = 2.0;
        p.use_premultiplied = 1.0;
        p.overbright = 10;
        p.texture_handle = th_getParticleTexture(TH_SPARKS);
        p.makedecal = false;
        p.hasphysics = false;
        p.alphascale = 1.0;
        p.has_gravity = true;
        // p.haslight = true;
        // p.lightcolor = fn_createVec3(60,45,0);
        // p.lightq = shotgun->levelstate->general_light_query;



        for (int k = 0 ; k < 50;k++)
        {
          p.velocity = fn_multVec3(impact_normal,fn_createVec3s(0.7));
          p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.3));
          p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(1.5));
          p.position = object->sledge_position;
          th_addParticle(p);


        }


        th_spawnImpactRing(object->sledge_position,-1);

        object->num_lightning_tips = 0;
        int k_idx = 0;

        fn_vec3 offsets_dirs[6];
        offsets_dirs[0] = fn_createVec3(450,0,0);
        offsets_dirs[1] = fn_createVec3(-450,0,0);
        offsets_dirs[2] = fn_createVec3(0,450,0);
        offsets_dirs[3] = fn_createVec3(0,-450,0);
        offsets_dirs[4] = fn_createVec3(0,0,-450);
        offsets_dirs[5] = fn_createVec3(0,0,450);

        for (int dirs = 0; dirs < 6;dirs++)
        {
          int dmgcount = 0;
          th_Entity** eptr = th_getEntityPointers(world,0);

          fn_vec3 sl_pos = fn_addVec3(object->sledge_position,offsets_dirs[dirs]);

          th_getEntitiesInRadius(TH_ENEMY,sl_pos,900,0,dt,&dmgcount,eptr);

          object->sledge_entity.aabb.position = object->sledge_position;
          object->sledge_entity.velocity = impact_normal;


          totaldmgcount += dmgcount;

          for (int k = 0; k < dmgcount; k++) {

            if (k_idx % 10 == 0 && object->num_lightning_tips < 6)
            {
              object->lightning_tips[object->num_lightning_tips] = eptr[k]->aabb.position;
              object->num_lightning_tips++;
            }
            int hits_to_give = 3;
            // if (fn_distance2(eptr[k]->aabb.position,object->sledge_position) < 700*700)
            // {
            //   hits_to_give = 4;
            // }
            // else if (fn_distance2(eptr[k]->aabb.position,object->sledge_position) < 1000*1000)
            // {
            //   hits_to_give = 3;
            // }
            // else if (fn_distance2(eptr[k]->aabb.position,object->sledge_position) < 1500*1500)
            // {
            //   hits_to_give = 2;
            // }

            for (int l = 0 ; l < hits_to_give;l++)
            {
              th_Entity* e = eptr[k];
              //fn_addVec3(e->velocity,
              th_incrementViolence();
              e->impact = true;
              object->entities_impacts[2] = object->sledge_entity;
              th_Impact impact = {(void*)(&object->entities_impacts[2]),e->aabb.position};
              e->impacts[e->impact_count] = impact;
              e->impact_count++;
              if(e->impact_count >= MAX_IMPACTS)
              {
                printf("%s\n","MAX IMPACT" );
                e->impact_count = 0;
              }
            }

            k_idx = k_idx + 1;
          }
        }

      }
    }

    if (totaldmgcount >= 6 && sledge_impacted)
    {


      int start_gem = object->levelstate->player->gem_count[TH_HAMMER];
      if (start_gem < 60)
      {

        object->levelstate->player->level_weapon[TH_HAMMER] = 1;
        object->levelstate->player->gem_count[TH_HAMMER] = 0;
      }
      else
      {
        object->levelstate->player->gem_count[TH_HAMMER] = object->levelstate->player->gem_count[TH_HAMMER] - 60;
      }





    }


    if (th_time() - object->sledge_impact_timer > 1000 && object->sledge_impact )
    {
      object->is_held_sledge = true;
      object->sledge_fly = false;
      object->sledge_impact = false;
      object->time_fired_sledge = th_time() - 5000.0;




    }
    else if (object->sledge_impact)
    {
      if (th_time() > object->lightning_timer + 20)
      {
        for (int l = 0 ; l < object->num_lightning_tips; l++ )
        {
          for (int k = 0; k < (HAMMER_LIGHT_SEGMENTS - 2)*2;k++)
          {
            object->randstate_lightning_beam[l][k] = th_random();
          }
        }


        object->lightning_timer = th_time();
      }
      //printf("Spawning %i Lightnings\n",object->num_lightning_tips);
      for (int k = 0 ; k < object->num_lightning_tips; k++ )
      {
        th_spawnLighting(object->sledge_position,object->lightning_tips[k],-1,object->levelstate->general_light_query,HAMMER_LIGHT_SEGMENTS,object->randstate_lightning_beam[k],9.0,175);
      }
    }
  }


  for (int i = 0 ; i < 2;i++)
  {

    object->animation_interpose[i] = false;
    if (!object->is_held[i])
    {
      int thread_id = 0;
      float len = fn_length(fn_subVec3(player->aabb.position,entities[i].aabb.position));

      if ((len < 150 && entities[i].grounded) || (len < 150 && object->has_gravity[i] ))
      {
        {
          a_VirtualSource* s = a_playVirtualSource(44,-1,player->aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,player->aabb.position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.5);
        }
        a_stopVS(object->flysound[i]);
        object->is_held[i] = true;

        // if (!(input->left && canfire))
        // {
          object->animation_interpose[i] = true;
          object->timer_equip_animation[i] = th_time();
          object->equip_frame_0[i] = transforms[i];
        // }
        object->equip_duration = 150.0;
        if (th_time() - object->time_fired[i] < 15.0)
        {
          swing_anim_transforms[0] = object->levelstate->weapon->hammer_transforms[i];


          swing_anim_transforms[1] = swing_anim_transforms[0];
          swing_anim_transforms[1].translate = fn_subVec3(swing_anim_transforms[1].translate,fn_createVec3(0,-10,-50));
          swing_anim_transforms[1].rotate = fn_multquat(swing_anim_transforms[1].rotate,fn_makeQuaternion(-fn_radians(70.0),fn_createVec3(1,0,0)));


          swing_anim_transforms[2] = swing_anim_transforms[1];
          swing_anim_transforms[2].translate = fn_subVec3(swing_anim_transforms[2].translate,fn_createVec3(0,0,-5));
          swing_anim_transforms[2].rotate = fn_multquat(swing_anim_transforms[2].rotate,fn_makeQuaternion(-fn_radians(5.0),fn_createVec3(1,0,0)));


          swing_anim_transforms[3] = swing_anim_transforms[2];
          swing_anim_transforms[3].translate = fn_subVec3(swing_anim_transforms[3].translate,fn_createVec3(0,-51,45));
          swing_anim_transforms[3].rotate = fn_multquat(swing_anim_transforms[3].rotate,fn_makeQuaternion(fn_radians(85.0),fn_createVec3(1,0,0)));


          swing_anim_transforms[4] = swing_anim_transforms[3];
          swing_anim_transforms[4].translate = fn_subVec3(swing_anim_transforms[4].translate,fn_createVec3(0,46,70));
          swing_anim_transforms[4].rotate = fn_multquat(swing_anim_transforms[4].rotate,fn_makeQuaternion(fn_radians(75.0),fn_createVec3(1,0,0)));


          fn_mat4 cam_a = fn_inverse(r_camera(target_pos,target_angles));

          fn_mat4 tr_temp = cam_a;//fn_translaterotatescaleq(fn_createVec3s(0.0),fn_createVec4(0,0,0,1),fn_createVec3s(2));
          tr_temp = fn_multMat4(fn_transformToMat(swing_anim_transforms[4]),tr_temp);

          tr_temp = fn_scale(tr_temp,fn_createVec3(-1,-1,-1));

          object->equip_frame_0[i] = tr_temp;
          object->equip_duration = 275.0;
        }

        if (!canfire)//!(object->levelstate->player->level_weapon[TH_HAMMER]  == 3)
        {
          transforms[i] = fn_makescale(fn_createVec3s(0));
        }
        object->has_gravity[i] = false;
        object->entities[i].grounded = false;
        object->entities[i].collided = false;
        continue;
      }
      else if (object->has_gravity[i] && (th_time() - object->last_hit_time[i]) > 100)
      {
        // entities[i].velocity = fn_addVec3(entities[i].velocity,fn_multVec3s(fn_normalizeVec3(fn_subVec3(player->aabb.position,entities[i].aabb.position)),0.2));
        fn_vec3 trgt = player->aabb.position;
        trgt = fn_addVec3(trgt,fn_multVec3s(direction,50));
        if (i == 0)
        {
          trgt = fn_addVec3(trgt,fn_multVec3s(right,-50));
        }
        else
        {
          trgt = fn_addVec3(trgt,fn_multVec3s(right,50));
        }

        entities[i].velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(trgt,entities[i].aabb.position)),2.1);
        if (fn_length(entities[i].velocity) > 2.1)
        {
          entities[i].velocity = fn_multVec3s(fn_normalizeVec3(entities[i].velocity),2.1);
        }




      }

      a_setVSPos(object->flysound[i],entities[i].aabb.position);
      a_setVSVel(object->flysound[i],entities[i].velocity);



      //
      // if (entities[i].grounded)
      // {
      //
      //
      //
      //
      //   fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,0,1),fn_normalizeVec3(fn_multVec3(object->entities[i].velocity,fn_createVec3(-1,-1,1))));
      //   transforms[0] = fn_translaterotatescaleq(entities[0].aabb.position,q_temp,fn_createVec3s(2));
      //   return;
      // }



      //

      fn_vec3 oldvelocity = entities[i].velocity;
      fn_vec3 oldposition = entities[i].aabb.position;

      float time_cutoff = 7000;
      if ((object->levelstate->player->level_weapon[TH_HAMMER]  == 2))
      {
        time_cutoff = 750;
      }
      if ( th_time() - object->time_fired[i] > time_cutoff || object->num_hits[i] >= 3)
      {
        object->has_gravity[i] = true;
        entities[i].aabb.position = fn_addVec3(entities[i].aabb.position,fn_multVec3s(entities[i].velocity,dt));
        entities[i].grounded = false;
        entities[i].collided = false;
      }
      else
      {



        entities[i].aabb.hwidth = fn_createVec3(10,10,10);

        th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,thread_id));

        entities[i].aabb.hwidth = fn_createVec3(40,40,40);





      }

      // if (!entities[i].grounded && object->has_gravity[i])
      // {
      //   //entities[i].velocity.y += 0.001*dt;
      // }

      th_Entity state = entities[i];
      entities[i].velocity = oldvelocity;
      entities[i].aabb.position = oldposition;

      fn_vec3 n,pos;
      float time = 0;
      bool hit = false;
      th_Entity* col_e = NULL;


      col_e = th_collideWithEntities(TH_ENEMY | TH_ENEMY_ROCKET,&entities[i],1,&n,&hit,thread_id,dt,&pos,&time);




      if (object->num_hits[i] >= 7)
      {
          hit = false;
      }

      if (!hit)
      {
        entities[i] = state;
      }
      else
      {

        // {
        //   a_VirtualSource* s = a_playFile(0 );
        //   a_setVSLoop(s,false);
        //   a_setVSPos(s,entities[i].aabb.position);
        //   a_setVSVel(s,fn_createVec3s(0));
        //   a_setVSGain(s,120.0);
        // }
        if (col_e->type == TH_ROCKET_ENTITY)
        {
          object->time_fired[i] = 0;//send hammer straight back, without collision
          object->last_hit_time[i] = 0;
        }
        th_incrementViolence();


        entities[i].collided = true;
        entities[i].collision_normal = n;
        entities[i].collision_position = pos;
        col_e->impact = true;
        object->entities_impacts[i] = object->entities[i];
        th_Impact impact = {(void*)(&object->entities_impacts[i]),pos};
        col_e->impacts[col_e->impact_count] = impact;
        col_e->impact_count++;
        if(col_e->impact_count >= MAX_IMPACTS)
        {
          col_e->impact_count = 0;
        }


        // th_Particle p;
        // float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
        // // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
        // p.theta = theta;
        // p.alive = true;
        // p.life = 10000;
        // p.start_time = th_time();
        // p.stretch = 4;
        // p.scale = 15;
        // p.overbright = 1;
        // p.texture_handle = 30.0;
        // p.makedecal = true;
        // th_DecalOrientation blood_decal;
        // blood_decal.scale = fn_createVec3(75,75,50);
        // blood_decal.material_handle = fn_createVec2(0,30);//14
        // p.decal = blood_decal;
        //
        // for (int k = 0 ; k < 7;k++)
        // {
        //   p.velocity = fn_multVec3(fn_createVec3(0,-1,0),fn_createVec3s(0.7));
        //   p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.4));
        //   p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(0.7));
        //   p.position = pos;
        //   th_addParticle(p);
        //
        // }


      }




      if (entities[i].collided )
      {
        object->num_hits[i] += 1;
        object->last_hit_time[i] = th_time();
        {
          a_VirtualSource* s = a_playVirtualSource(43,-1,entities[i].collision_position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entities[i].collision_position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.4);
        }

        {
          a_VirtualSource* s = a_playVirtualSource(15 + th_random() % 3,-1,entities[i].collision_position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,entities[i].collision_position);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.4);
        }


        int forcecount = 0;
        th_Entity** eptr = th_getEntityPointers(world,0);
        th_getEntitiesInRadius(TH_BOID,entities[i].collision_position,300,0,dt,&forcecount,eptr);

        for (int k = 0; k < forcecount; k++) {
          th_Entity* e = eptr[k];
          //fn_addVec3(e->velocity,
          e->velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(e->aabb.position,entities[i].aabb.position)),20);//);

        }

        if (object->levelstate->player->level_weapon[TH_HAMMER]  >= 2)
        {
          int dmgcount = 0;
          th_getEntitiesInRadius(TH_ENEMY,entities[i].collision_position,150,0,dt,&dmgcount,eptr);

          for (int k = 0; k < dmgcount; k++) {
            th_Entity* e = eptr[k];
            //fn_addVec3(e->velocity,
            th_incrementViolence();
            e->impact = true;
            object->entities_impacts[i] = entities[i];
            th_Impact impact = {(void*)(&object->entities_impacts[i]),pos};
            e->impacts[e->impact_count] = impact;
            e->impact_count++;
            if(e->impact_count >= MAX_IMPACTS)
            {
              printf("%s\n","MAX IMPACT" );
              e->impact_count = 0;
            }

          }
        }

        // #pragma omp critical
        // {
        //   a_VirtualSource* s = a_playFile(10 + th_random() % 3 );
        //   a_setVSLoop(s,false);
        //   a_setVSPos(s,entities[i].aabb.position);
        //   a_setVSVel(s,fn_createVec3s(0));
        //   a_setVSGain(s,60.0*1.3);
        // }

        // th_Particle p = th_defaultParticle();
        // float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
        // // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
        // p.theta = 0;
        // p.alive = true;
        // p.life = 500;
        // p.start_time = th_time();
        // p.stretch = 2;
        // p.scale = 1;
        // p.overbright = 1.1;
        // p.texture_handle = th_getParticleTexture(TH_SPARKS);
        // p.makedecal = false;
        // p.hasphysics = false;
        // p.alphascale = 1.0;
        // p.has_gravity = true;

        if (!object->has_gravity[i] && !hit)
        {
          // for (int k = 0 ; k < 50;k++)
          // {
          //   p.velocity = fn_multVec3(entities[i].collision_normal,fn_createVec3s(0.3));
          //   p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.7));
          //   p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(1.5));
          //   p.position = entities[i].collision_position;
          //   th_addParticle(p);
          //
          //
          // }

          for (int k = 0 ; k < 7;k++)
          {
            th_spawnSparksFewLight(entities[i].collision_normal,entities[i].collision_position,-1,object->levelstate->general_light_query,3);
          }
        }

        th_spawnImpactRing(fn_addVec3(entities[i].collision_position,fn_multVec3s(entities[i].collision_normal,60.0)),-1);


        object->impact_timer = th_time() + TH_GEM_SUCK_HAMMER;
        object->impact_pos = entities[i].collision_position;


        float dot = fn_dot(oldvelocity,entities[i].collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
        fn_vec3 u = fn_multVec3s(entities[i].collision_normal , dot);
        fn_vec3 w = fn_subVec3(oldvelocity , u);
        entities[i].velocity = fn_multVec3s(fn_subVec3(w , u),1);//reduce bounce height 0.99,0.75,0.99
        object->has_gravity[i] = true;

        if (fn_length(entities[i].velocity) > 1)
        {
          entities[i].velocity = fn_multVec3s(fn_normalizeVec3(entities[i].velocity),1);
        }
        // if (entities[i].grounded)
        // {
        //   entities[i].velocity = oldvelocity;
        // }






      }

      // fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,0,1),fn_normalizeVec3(fn_multVec3(object->entities[i].velocity,fn_createVec3(-1,-1,1))));
      // fn_mat4 end_transform = fn_translaterotatescaleq(entities[i].aabb.position,q_temp,fn_createVec3s(2));
      float flip = 1.0;
      if (!(object->has_gravity[i] && (th_time() - object->last_hit_time[i]) > 100))
      {
        flip = -1.0;
      }

      fn_mat4 cam_rot =  th_6dofCamera(NULL,NULL,&object->old_up[i],fn_multVec3(object->entities[i].velocity,fn_createVec3s(flip)),fn_createVec3(0,0,0));
      fn_mat4 end_transform = fn_translaterotatescalem(entities[i].aabb.position,fn_multMat4(fn_inverse(cam_rot),fn_makerotate(object->rot_angles_flight[i],object->old_up[i])),fn_createVec3s(2));

      const float swing_duration = 300.0;


      float rads_sec = fn_radians(85.0) + fn_radians(75.0);
      rads_sec = rads_sec / (swing_duration/1000.0);

      if (th_time() - object->time_fired[i] > swing_duration*0.75 || flip == 1.0)
      {
        float flip_akimbo = i == 1 ? -1.0 : 1.0;
        object->rot_angles_flight[i] = object->rot_angles_flight[i] + flip_akimbo*flip*0.001*dt*rads_sec;//13.96;
      }

      if ( th_time() - object->time_fired[i] < swing_duration && flip == -1.0)
      {
        // fn_Transform weapo = object->levelstate->weapon->hammer_transforms[0];
        // fn_Transform weapo2 = weapo;
        // weapo2.translate = fn_addVec3(weapo2.translate,fn_createVec3(0,0,-100));

        swing_anim_transforms[0] = object->levelstate->weapon->hammer_transforms[i];
        swing_anim_alphas[0] = 0.0;

        swing_anim_transforms[1] = swing_anim_transforms[0];
        swing_anim_transforms[1].translate = fn_subVec3(swing_anim_transforms[1].translate,fn_createVec3(0,-10,-50));
        swing_anim_transforms[1].rotate = fn_multquat(swing_anim_transforms[1].rotate,fn_makeQuaternion(-fn_radians(70.0),fn_createVec3(1,0,0)));
        swing_anim_alphas[1] = 0.15;

        swing_anim_transforms[2] = swing_anim_transforms[1];
        swing_anim_transforms[2].translate = fn_subVec3(swing_anim_transforms[2].translate,fn_createVec3(0,0,-5));
        swing_anim_transforms[2].rotate = fn_multquat(swing_anim_transforms[2].rotate,fn_makeQuaternion(-fn_radians(5.0),fn_createVec3(1,0,0)));
        swing_anim_alphas[2] = 0.35;

        swing_anim_transforms[3] = swing_anim_transforms[2];
        swing_anim_transforms[3].translate = fn_subVec3(swing_anim_transforms[3].translate,fn_createVec3(0,-51,45));
        swing_anim_transforms[3].rotate = fn_multquat(swing_anim_transforms[3].rotate,fn_makeQuaternion(fn_radians(85.0),fn_createVec3(1,0,0)));
        swing_anim_alphas[3] = 0.6;

        swing_anim_transforms[4] = swing_anim_transforms[3];
        swing_anim_transforms[4].translate = fn_subVec3(swing_anim_transforms[4].translate,fn_createVec3(0,46,70));
        swing_anim_transforms[4].rotate = fn_multquat(swing_anim_transforms[4].rotate,fn_makeQuaternion(fn_radians(75.0),fn_createVec3(1,0,0)));
        swing_anim_alphas[4] = 0.75;



        fn_mat4 scale_neg = fn_scale(fn_identityMat4(), fn_createVec3(-1,-1,-1));
        fn_mat4 cam = r_camera(target_pos, target_angles);
        fn_mat4 end_in_camspace = fn_multMat4(scale_neg, fn_multMat4(end_transform, cam));

        swing_anim_transforms[5] = fn_decomposeMat4(end_in_camspace);
        swing_anim_alphas[5] = 1.0;

        float alpha = (th_time() - object->time_fired[i])/swing_duration;

        alpha = fn_clamp(alpha,0.0,1.0);
        // fn_Transform weapo_interp = fn_lerpTransforms(weapo,weapo2,alpha);

        fn_Transform weapo_interp = fn_interpTransforms(swing_anim_transforms,swing_anim_alphas,TH_SWING_ANIM_CNT,alpha);
        fn_mat4 weapo_mat = fn_transformToMat(weapo_interp);


        fn_mat4 cam_a = fn_inverse(r_camera(target_pos,target_angles));

        transforms[i] = cam_a;//fn_translaterotatescaleq(fn_createVec3s(0.0),fn_createVec4(0,0,0,1),fn_createVec3s(2));
        transforms[i] = fn_multMat4(weapo_mat,transforms[i]);

        transforms[i] = fn_scale(transforms[i],fn_createVec3(-1,-1,-1));
      }
      else
      {
        transforms[i] = end_transform;

      }


    }
    else
    {


      if ( th_time() - object->timer_equip_animation[i] < object->equip_duration && object->timer_equip_animation[i] != 0 )
      {
        object->animation_interpose[i] = true;
        // fn_Transform weapo = object->levelstate->weapon->hammer_transforms[0];
        // fn_Transform weapo2 = weapo;
        // weapo2.translate = fn_addVec3(weapo2.translate,fn_createVec3(0,0,-100));

        fn_mat4 scale_neg = fn_scale(fn_identityMat4(), fn_createVec3(-1,-1,-1));
        fn_mat4 cam = r_camera(target_pos, target_angles);
        fn_mat4 begin_in_camspace = fn_multMat4(scale_neg, fn_multMat4(object->equip_frame_0[i], cam));



        swing_anim_transforms[1] = object->levelstate->weapon->hammer_transforms[i];
        swing_anim_alphas[1] = 1.0;
        swing_anim_transforms[0] = fn_decomposeMat4(begin_in_camspace);
        swing_anim_alphas[0] = 0.0;






        float alpha = (th_time() - object->timer_equip_animation[i])/object->equip_duration;

        alpha = fn_clamp(alpha,0.0,1.0);
        // fn_Transform weapo_interp = fn_lerpTransforms(weapo,weapo2,alpha);

        fn_Transform weapo_interp = fn_interpTransforms(swing_anim_transforms,swing_anim_alphas,2,alpha);
        fn_mat4 weapo_mat = fn_transformToMat(weapo_interp);


        fn_mat4 cam_a = fn_inverse(r_camera(target_pos,target_angles));
        if (canfire)
        {
          transforms[i] = cam_a;//fn_translaterotatescaleq(fn_createVec3s(0.0),fn_createVec4(0,0,0,1),fn_createVec3s(2));
          transforms[i] = fn_multMat4(weapo_mat,transforms[i]);

          transforms[i] = fn_scale(transforms[i],fn_createVec3(-1,-1,-1));
        }
        else
        {
          transforms[i] = fn_makescale(fn_createVec3s(0));
        }

      }
    }
  }

  if (th_time() < object->impact_timer)
  {
    for (uint32_t l = 0 ; l < object->levelstate->gems->num_used;l++)
    {

      object->levelstate->gems->entities[l].velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(object->impact_pos,object->levelstate->gems->entities[l].aabb.position)),1);
      if (fn_length(object->levelstate->gems->entities[l].velocity) > 1)
      {
        object->levelstate->gems->entities[l].velocity = fn_multVec3s(fn_normalizeVec3(object->levelstate->gems->entities[l].velocity),1);
      }

    }
  }

}
