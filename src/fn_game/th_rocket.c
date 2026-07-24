#include "th_rocket.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_time.h"

#include "th_player.h"
#include "../fn_engine/th_level.h"

static void rockethit(th_Entity* entity,th_RocketObject* rocket,float dt,th_World* world,th_Entity* player,bool hit)
{
  int count = rocket->entity_count;
  th_Entity* entities = rocket->entities;
  fn_mat4* transforms = rocket->transforms;
  int i = entity->index;

  entity->alive = false;
  if (fn_length2(fn_subVec3(entity->aabb.position,player->aabb.position)) < 200*200)
  {
    fn_vec3 n;
    bool can_hit = false;
    float t;
    th_trace(world,entity->aabb.position,player->aabb.position,&n,&can_hit,th_getPhysicsMemory(world,0),&t);

    if (!can_hit)
    {
      fn_vec3 d = fn_subVec3(entity->aabb.position,player->aabb.position);
      fn_vec3 av = fn_multVec3s(fn_normalizeVec3(d),-1.75f);
      player->velocity = fn_addVec3(av,player->velocity);

      th_setGameGlow(fn_createVec3(1,0,0),0.4);

      th_PlayerObject* po = rocket->levelstate->player;
      th_decrementPlayerHealth(po,15);
      th_decrementPlayerGem(po,15);

      rocket->levelstate->player->screenshake_f = 0.13*0.4;
      rocket->levelstate->player->screenshake_t = 0;
      rocket->levelstate->player->screenshake_amplitude = 0.1;
    }

  }

  int forcecount = 0;
  th_Entity** eptr = th_getEntityPointers(world,0);
  th_getEntitiesInRadius(TH_BOID,entity->aabb.position,500,0,dt,&forcecount,eptr);

  for (int k = 0; k < forcecount; k++) {
    th_Entity* e = eptr[k];
    //fn_addVec3(e->velocity,
    e->velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(e->aabb.position,entity->aabb.position)),20);//);

  }

  entity->velocity.x = 0;
  entity->velocity.y = 0;
  entity->velocity.z = 0;
  transforms[i] = fn_makescale(fn_createVec3s(0));

  if (rocket->sources[i] != NULL)
  {
    a_stopVS(rocket->sources[i]);
    rocket->sources[i] = NULL;

  }

  {
    a_VirtualSource* s = a_playVirtualSource(32,-1,entity->aabb.position,NULL );
    a_setVSLoop(s,false);
    a_setVSPos(s,entity->aabb.position);
    a_setVSVel(s,fn_createVec3s(0));
    a_setVSGain(s,0.8);
  }

  th_spawnExplosion(entity->aabb.position,-1);

  if (!hit)
  {
    th_DecalOrientation testdecal;
    testdecal.normal = fn_multVec3(entity->collision_normal,fn_createVec3(-1,-1,-1));
    testdecal.pos = fn_addVec3(entity->collision_position,fn_multVec3s(testdecal.normal,2));
    testdecal.scale = fn_createVec3(75*4,75*4,50*4);

    testdecal.tangent = th_getDecalTangentVector(testdecal.normal);
    testdecal.material_handle = fn_createVec2(th_getParticleTexture(TH_BULLETHOLE).x,0);//14
    th_addDecal(testdecal);
  }


  th_killLight(rocket->lightq,rocket->lights[i]);

  th_LightProperties lnew = th_getDefaultLight();
  lnew.life = 700;
  lnew.fadeout = true;
  float sc = 100;
  lnew.base_color = fn_createVec3(1000*sc,400*sc,0);
  th_makeLight(rocket->lightq,lnew,fn_createVec3(1000*sc,400*sc,0),entity->aabb.position);
}

int th_rocketDamageCallback(void* ep,void* pep,float dt,void* world)
{
  th_Entity* e = (th_Entity*)ep;
  th_Entity* player = (th_Entity*)pep;

  rockethit(e,e->damage_callback_data,dt,world,player,true);

  return 20;

}

void th_rocketInit(th_Allocator* alloc,th_RocketObject* rocket,int count,th_LightQuery* lightq,th_LevelState* levelstate)
{
  rocket->levelstate = levelstate;
  rocket->lightq = lightq;
  rocket->num_used = 0;
  rocket->current_count = 0;
  rocket->entity_count = count;
  rocket->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  rocket->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  rocket->orientations = th_alloc(alloc,sizeof(fn_vec3)*count);
  rocket->sources = th_alloc(alloc,sizeof(a_VirtualSource*)*count);
  rocket->lights = th_alloc(alloc,sizeof(th_LightIdTuple)*count);
  rocket->timings = th_alloc(alloc,sizeof(th_timer_t)*count);

  for (int i = 0 ; i < count;i++)
  {
    rocket->entities[i] = TH_DEFAULT_ENTITY;
    rocket->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
    rocket->entities[i].aabb.hwidth = fn_createVec3(50,50,50);
    rocket->entities[i].velocity = fn_createVec3s(0);
    rocket->entities[i].grounded = false;
    rocket->entities[i].alive = false;
    rocket->entities[i].collided = false;
    rocket->entities[i].aabb.mode = SPHERE;
    rocket->entities[i].impact = false;
    rocket->entities[i].impact_count = 0;
    rocket->entities[i].damage_callback = th_rocketDamageCallback;
    rocket->entities[i].damage_callback_data = (void*)rocket;
    rocket->entities[i].index = i;
    rocket->entities[i].type = TH_ROCKET_ENTITY;
    rocket->transforms[i] = fn_makescale(fn_createVec3s(0));
    rocket->orientations[i] = fn_createVec3(1,0,0);
    rocket->sources[i] = NULL;
    rocket->lights[i] = th_makeLightIDTuple(0,0);
    rocket->timings[i] = 0;
  }
}

void th_rocketUpdate(th_RocketObject* rocket,float dt)
{
  th_World* world = rocket->levelstate->world;
  th_Entity* player = &rocket->levelstate->player_e;
  int count = rocket->entity_count;
  th_Entity* entities = rocket->entities;
  fn_mat4* transforms = rocket->transforms;


  for (int i = 0; i <rocket->num_used;i++)
  {
    int thread_id = 0;
    fn_vec3 oldvelocity = entities[i].velocity;
    if (entities[i].collided || !entities[i].alive)
    {
      continue;
    }
    bool hammer_hit = false;
    if (entities[i].impact)
    {

      entities[i].impact = false;

      for (int j = 0; j < entities[i].impact_count; j++) {
        th_Entity* projectile = (th_Entity*)entities[i].impacts[j].entity;
        if (projectile->type == TH_HAMMER_PROJECTILE)
        {
          hammer_hit = true;
        }
        // else if (projectile->type == TH_MACHINEGUN_BULLET)
        // {
        //
        // }
        // else if (projectile->type == TH_SHOTGUN_SHELL)
        // {
        //
        // }
      }

      entities[i].impact_count = 0;




    }

    if (!hammer_hit)
    {
      th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,0));
    }


    a_setVSPos(rocket->sources[i],entities[i].aabb.position);
    fn_vec3 lpos = fn_subVec3(entities[i].aabb.position,fn_multVec3s(rocket->orientations[i],150));
    if (th_time() > rocket->timings[i] + 32)
    {
      th_spawnSmokeRocket(lpos,-1);
      rocket->timings[i] = th_time();
    }


    th_setLight(rocket->lightq,rocket->lights[i],fn_createVec3(1000*100,250*100,0),lpos);


    th_Entity state = entities[i];

    fn_vec3 n,pos;
    float time = 0;
    bool hit = false;
    th_Entity* col_e = th_collideWithEntitiesExclusionary(TH_ENEMY ,TH_EYEBALL ,&entities[i],1,&n,&hit,thread_id,dt,&pos,&time);
    if (!hit)
    {
      entities[i] = state;
    }
    else
    {

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

    }

    if (entities[i].collided || hammer_hit)
    {
      rockethit(&entities[i],rocket,dt,world,player,hit);
    }
    else
    {
      fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,-1,0),(fn_multVec3(rocket->orientations[i],fn_createVec3(1,-1,1))));
    transforms[i] = fn_translaterotatescaleq(entities[i].aabb.position,q_temp,fn_createVec3s(1));
    }






  }
}

void th_rocketSpawn(th_RocketObject* rocket,fn_vec3 position,fn_vec3 velocity,fn_vec3 orientation)
{

  rocket->entities[rocket->current_count].aabb.position = position;

  rocket->entities[rocket->current_count].velocity = velocity;
  rocket->entities[rocket->current_count].collided = false;
  rocket->entities[rocket->current_count].grounded = false;
  rocket->entities[rocket->current_count].alive = true;

  rocket->orientations[rocket->current_count] = orientation;

  rocket->lights[rocket->current_count] = th_getLight(rocket->lightq,th_getDefaultLight(),fn_createVec3(1000,250,0),position);
  rocket->timings[rocket->current_count] = th_time();

  if (rocket->sources[rocket->current_count] == NULL)
  {
    {
      a_VirtualSource* s = a_playVirtualSource(31,-1,position,NULL );
      a_setVSLoop(s,false);
      a_setVSPos(s,position);
      a_setVSVel(s,fn_createVec3s(0));
      a_setVSGain(s,0.7);
    }

    {
      a_VirtualSource* s = a_playVirtualSource(30,-1,position,NULL );
      a_setVSLoop(s,true);
      a_setVSPos(s,position);
      a_setVSVel(s,velocity);
      a_setVSGain(s,0.7);

      rocket->sources[rocket->current_count] = s;
    }
  }

  rocket->current_count++;

  if (rocket->num_used < rocket->entity_count)
  {
    rocket->num_used++;
  }

  if (rocket->current_count == rocket->entity_count)
  {
    rocket->current_count = 0;
  }
}
