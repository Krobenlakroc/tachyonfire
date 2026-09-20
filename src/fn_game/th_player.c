#include "th_player.h"
#include <stdio.h>
#include "th_builtins.h"

#include "../fn_engine/th_level.h"
#include "../fn_engine/th_system.h"
#include "../fn_engine/th_globals.h"
#include "../fn_engine/th_ui.h"

#define TH_CROUCH_MS 50

void th_playerInitialize(th_PlayerObject* obj,char* healthbar,
char* health_number,
char* level_bar,
char* level_number,fn_vec3* levelcolorptr,char* gem_cap_number,th_LevelState* levelstate)
{
  obj->gem_cap_number = gem_cap_number;
  obj->levelstate = levelstate;
  obj->healthbar = healthbar;
  obj->health_number = health_number;
  obj->level_bar = level_bar;
  obj->level_number = level_number;
  obj->hp = 100;
  obj->levelcolorptr = levelcolorptr;

  for (int i = 0 ; i < TH_NUM_WEAPONS;i++)
  {
    obj->level_weapon[i] = 1;
    obj->gem_count[i] = 0;
    obj->weapon_name[i] = "";
  }

  obj->weapon_name[TH_MACHINEGUN] = "Machinegun";
  obj->weapon_name[TH_SHOTGUN] = "Shotgun";
  obj->weapon_name[TH_HAMMER] = "Hammer";

  //obj->level_weapon[TH_MACHINEGUN] = 3;
  //obj->level_weapon[TH_SHOTGUN] = 2;
   // obj->level_weapon[TH_HAMMER] = 3;
   // obj->gem_count[TH_HAMMER] = 100;

  obj->screenshake_f = 0;
  obj->screenshake_t = 0;
  obj->screenshake_amplitude = 0;
  obj->fov_delta = 1.0;
  obj->fov_delta_target = 1.0;
  obj->request_uncrouch = false;
  obj->is_dead = false;
  obj->slide_source = NULL;

  obj->crouch_time = 0.0;
  obj->uncrouch_time = 0.0;
  obj->physics_y = 0.0;
  obj->set_physics_y = false;
  obj->stepoffset = 0;

  obj->overheal_timer = 0;
  obj->noclip = false;

  obj->level_pct_size = NULL;
  obj->health_pct_size = NULL;
  obj->health_pct_color = NULL;
}

void th_playerUpdate(th_PlayerObject* obj,float dt,th_Character* cmap,fn_vec2 screenSize)
{
  if (obj->hp <= 0 )
  {
    obj->is_dead = true;
  }
  int selected_weapon = obj->levelstate->weapon->chosen_weapon;
  sprintf(obj->health_number,"%i",obj->hp);//\'/,

  fn_vec2 dims_health = th_stringDims(obj->health_number,1.0,cmap);

  *obj->health_pos = fn_createVec2(0.18*screenSize.x - dims_health.x*0.5,0.05*screenSize.y - dims_health.y*0.5);
  *obj->health_pos = fn_multVec2(*obj->health_pos,fn_createVec2(1.0/screenSize.x,1.0/screenSize.y));

  //sprintf(obj->healthbar,"|    |");

  int gemcap_weapon = 20;
  if (obj->level_weapon[selected_weapon] > 2)
  {
    gemcap_weapon = 100;
  }
  else if (obj->level_weapon[selected_weapon] > 1)
  {
    gemcap_weapon = 34;
  }

  sprintf(obj->gem_cap_number,"/ %i",gemcap_weapon);
  // for (int i = 1;i <= obj->hp;i++)
  // {
  //   obj->healthbar[i] = ',';
  // }

  // sprintf(obj->level_number,"LV %i %s",obj->level_weapon[selected_weapon],obj->weapon_name[selected_weapon]);
  // const char* roman_numeral = "I";
  // if (obj->level_weapon[selected_weapon] == 2)
  // {
  //   roman_numeral = "II";
  // }
  // else if (obj->level_weapon[selected_weapon] == 3)
  // {
  //   roman_numeral = "III";
  // }

  //,obj->weapon_name[selected_weapon]
  if (obj->level_weapon[selected_weapon] > 2)
  {
    sprintf(obj->level_number,"MAX");
  }
  else
  {
    sprintf(obj->level_number,"LV %i",obj->level_weapon[selected_weapon]);
  }

  //\'/,
  float weapon_pct = ((float)obj->gem_count[selected_weapon] / 100.0)*(float)gemcap_weapon;

  sprintf(obj->level_bar,"%i",(int)weapon_pct);
  *obj->levelcolorptr = fn_lerpVec3(fn_createVec3(1,0,0),fn_createVec3(1,1,1),obj->gem_count[selected_weapon]/100.0);




  if (obj->hp <= 100)
  {
    *obj->health_pct_color = fn_lerpVec3(fn_createVec3(1,0,0),fn_createVec3(1,1,1),fn_clamp((obj->hp - 15)/70.0,0.0,1.0));
  }
  else
  {
    *obj->health_pct_color = fn_lerpVec3(fn_createVec3(1,1,1),fn_createVec3(0,1,1),fn_clamp(((float)obj->hp - 100.0)/25.0,0.0,1.0));
  }



  //lvl pct base 0.65
  //health pct base 1
  float lvl_pct_base = 1.0;
  const float health_pct_base = 1.0;

  if ((int)weapon_pct == 100)
  {
    lvl_pct_base = 0.5;
  }

  fn_vec2 dims_levelbar = th_stringDims(obj->level_bar,lvl_pct_base,cmap);

  float dims_levelbar_y = th_stringDims(obj->level_bar,1,cmap).y;

  *obj->level_pos = fn_createVec2(0.80666*screenSize.x - dims_levelbar.x,0.05*screenSize.y - dims_levelbar_y*0.5);
  *obj->level_pos = fn_multVec2(*obj->level_pos,fn_createVec2(1.0/screenSize.x,1.0/screenSize.y));


  if (*obj->level_pct_size > lvl_pct_base)
  {
    *obj->level_pct_size = *obj->level_pct_size - 0.001*dt;

    if (*obj->level_pct_size < lvl_pct_base)
    {
      *obj->level_pct_size = lvl_pct_base;
    }
  }
  else if (*obj->level_pct_size < lvl_pct_base)
  {
    *obj->level_pct_size = *obj->level_pct_size + 0.001*dt;

    if (*obj->level_pct_size > lvl_pct_base)
    {
      *obj->level_pct_size = lvl_pct_base;
    }
  }

  if (*obj->health_pct_size > health_pct_base)
  {
    *obj->health_pct_size = *obj->health_pct_size - 0.001*dt;

    if (*obj->health_pct_size < health_pct_base)
    {
      *obj->health_pct_size = health_pct_base;
    }
  }
  else if (*obj->health_pct_size < health_pct_base)
  {
    *obj->health_pct_size = *obj->health_pct_size + 0.001*dt;

    if (*obj->health_pct_size > health_pct_base)
    {
      *obj->health_pct_size = health_pct_base;
    }
  }



  obj->screenshake_t += dt*obj->screenshake_f;
  obj->screenshake_amplitude -= dt*0.00005;
  obj->screenshake_f -= dt*0.00005;
  if (obj->screenshake_amplitude < 0)
  {
    obj->screenshake_amplitude = 0;
  }
  if (obj->screenshake_f < 0)
  {
    obj->screenshake_f = 0;
  }

  if (obj->hp > 100)
  {
    obj->overheal_timer = obj->overheal_timer + dt;
    if (obj->overheal_timer > 1000)
    {
      obj->overheal_timer = 0;
      obj->hp = obj->hp - 1;
    }
  }
  else
  {
    obj->overheal_timer = 0;
  }
}

void th_incrementPlayerGem(th_PlayerObject* obj)
{
  int selected_weapon = obj->levelstate->weapon->chosen_weapon;

  if (selected_weapon == TH_NOWEAPON)
  {
    return;
  }

  if (obj->gem_count[selected_weapon] == 100)
  {
    return;
  }

  if (obj->level_weapon[selected_weapon] > 2)
  {
    obj->gem_count[selected_weapon] = obj->gem_count[selected_weapon] + 1;
  }
  else if (obj->level_weapon[selected_weapon] > 1)
  {
    obj->gem_count[selected_weapon] = obj->gem_count[selected_weapon] + 3;
  }
  else
  {
    obj->gem_count[selected_weapon] = obj->gem_count[selected_weapon] + 5;
  }

  *obj->level_pct_size = 0.87;


    a_VirtualSource* s = a_playVirtualSource(50 + th_random() % 3,-1,obj->levelstate->player_e.aabb.position,NULL );
    a_setVSLoop(s,false);
    a_setVSPos(s,obj->levelstate->player_e.aabb.position);
    a_setVSVel(s,fn_createVec3s(0));
    a_setVSGain(s,0.65);


  if (obj->level_weapon[selected_weapon] == 3)
  {
    if (obj->gem_count[selected_weapon] >= 100)
    {
        obj->gem_count[selected_weapon] = 100;
    }
    return;
  }


  if (obj->gem_count[selected_weapon] >= 100)
  {
    int levelsound = obj->level_weapon[selected_weapon] == 2 ? sound_level_up2 : sound_level_up1;
    {
      a_VirtualSource* s = a_playVirtualSource(levelsound,-1,a_getPos(),NULL );
      a_setVSLoop(s,false);
      a_setVSPos(s,a_getPos());
      a_setVSVel(s,fn_createVec3s(0));
      a_setVSGain(s,1.0);

      if (obj->level_weapon[selected_weapon] == 2)
      {
        a_duckVS(s,5100.0,0.333);
      }
      else
      {
        a_duckVS(s,2000.0,0.333);
      }
    }
    th_setGameplayTimeScale(fn_createVec3(0.45,0.0000002,0.00000005));
    th_setGameGlow(fn_createVec3(0,1,0),0.5);
    obj->gem_count[selected_weapon] = obj->gem_count[selected_weapon] - 100;
    obj->level_weapon[selected_weapon] = obj->level_weapon[selected_weapon] + 1;

    th_LightQuery* lq = obj->levelstate->general_light_query;

    th_LightProperties lnew = th_getDefaultLight();
    lnew.life = 700;
    lnew.fadeout = true;
    float sc = 400;

    if (obj->level_weapon[selected_weapon] == 3)
    {
      sc = 1000;
      th_setGameplayTimeScale(fn_createVec3(0.2,0.00000025,0.000000055));
    }
    lnew.base_color = fn_createVec3(0,1000*sc,0);
    th_makeLight(lq,lnew,fn_createVec3(0,1000*sc,0),obj->levelstate->player_e.aabb.position);


    if (selected_weapon == TH_MACHINEGUN)
    {
      obj->levelstate->weapon->machinegun_temp_interp = 0.0;
    }
    else if (selected_weapon == TH_SHOTGUN)
    {
      obj->levelstate->weapon->shotgun_temp_interp = 0.0;
    }


    obj->levelstate->weapon->weapon_levelup_timers[selected_weapon] = th_time();
  }
}

void th_decrementPlayerGem(th_PlayerObject* obj,int points)
{
  if (obj->is_dead)
  {
    return;
  }

  int selected_weapon = obj->levelstate->weapon->chosen_weapon;
  if (obj->level_weapon[selected_weapon] > 1)
  {
    obj->gem_count[selected_weapon] = obj->gem_count[selected_weapon] - points;
  }
  else
  {
    obj->gem_count[selected_weapon] = obj->gem_count[selected_weapon] - points;
  }

   *obj->level_pct_size = 0.5;

  if (obj->level_weapon[selected_weapon] == 1 && obj->gem_count[selected_weapon] < 0)
  {
    obj->gem_count[selected_weapon] = 0;
  }
  else if (obj->gem_count[selected_weapon] < 0 && obj->level_weapon[selected_weapon] > 1 )
  {
    // {
    //   a_VirtualSource* s = a_playVirtualSource(23,-1,a_getPos(),NULL );
    //   a_setVSLoop(s,false);
    //   a_setVSPos(s,a_getPos());
    //   a_setVSVel(s,fn_createVec3s(0));
    //   a_setVSGain(s,200);
    // }
    // th_setGameplayTimeScale(fn_createVec3(0.45,0.0000002,0.00000005));
    th_setGameGlow(fn_createVec3(1,1,0),0.5);
    obj->gem_count[selected_weapon] = 100 + obj->gem_count[selected_weapon];
    obj->level_weapon[selected_weapon] = obj->level_weapon[selected_weapon] - 1;



    obj->levelstate->weapon->weapon_levelup_timers[selected_weapon] = th_time();

    if (selected_weapon == TH_MACHINEGUN)
    {
      obj->levelstate->weapon->machinegun_temp_interp = 0.0;
    }
    else if (selected_weapon == TH_SHOTGUN)
    {
      obj->levelstate->weapon->shotgun_temp_interp = 0.0;
    }

  }
}

void th_decrementPlayerHealth(th_PlayerObject* obj,int points)
{
  if (obj->is_dead)
  {
    return;
  }

  obj->hp = obj->hp - points;
  *obj->health_pct_size = 0.95;

  if (points > 0){
    a_VirtualSource* s = a_playVirtualSource(46 + th_random() % 3,-1,obj->levelstate->player_e.aabb.position,NULL );
    a_setVSLoop(s,false);
    a_setVSPos(s,fn_createVec3s(0));
    a_setVSRelativeToListener(s,true);
    a_setVSVel(s,fn_createVec3s(0));
    a_setVSGain(s,1.0);
  }
}

void th_incrementPlayerHealth(th_PlayerObject* obj,int points)
{
  if (obj->is_dead)
  {
    return;
  }

  if (obj->hp >= 200)
  {
    return;
  }

  obj->hp = obj->hp + points;

  *obj->health_pct_size = 1.05;

  if (obj->hp >= 200)
  {
    obj->hp = 200;
  }

  obj->overheal_timer = 0;

  if (points > 0){
    a_VirtualSource* s = a_playVirtualSource(sound_healthpickup,-1,obj->levelstate->player_e.aabb.position,NULL );
    a_setVSLoop(s,false);
    a_setVSPos(s,fn_createVec3s(0));
    a_setVSRelativeToListener(s,true);
    a_setVSVel(s,fn_createVec3s(0));
    a_setVSGain(s,1.0);
  }
}

void th_playerPhysicsUpdate(th_LevelState* ls,fn_RawInput* input,float dt,a_AudioSystem* audiosystem,fn_vec3 normal,fn_vec3 right,fn_vec3 look)
{
  bool flying = ls->player->noclip;
  bool tracing = false;

  th_Entity* player = &ls->player_e;
  bool old_collided = player->collided;

  float movespeeed = 1;//(0.7*dt);
  if (flying)
  {
    movespeeed = 0.7*dt;
  }

  fn_vec3 vel = fn_createVec3s(0);

  if (!ls->player->is_dead)
  {
    if (input->currentKeyStates[input->binding_forward])
    {
      if (flying)
      {
        vel = fn_addVec3(vel,fn_multVec3s(look,movespeeed));
      }
      else
      {
        vel = fn_addVec3(vel,fn_multVec3s(normal,movespeeed));
      }

    }
    if (input->currentKeyStates[input->binding_right])
    {
      vel = fn_addVec3(vel,fn_multVec3s(right,movespeeed));
    }
    if (input->currentKeyStates[input->binding_left])
    {
      vel = fn_addVec3(vel,fn_multVec3s(right,-movespeeed));
    }
    if (input->currentKeyStates[input->binding_back])
    {
      vel = fn_addVec3(vel,fn_multVec3s(normal,-movespeeed));
    }
  }


  // if (flying)
  // {
  //   if (input->currentKeyStates[input->binding_forward])
  //   {
  //     vel = fn_addVec3(vel,fn_multVec3s(look,-movespeeed));
  //   }
  //   if (input->currentKeyStates[input->binding_back])
  //   {
  //     vel = fn_addVec3(vel,fn_multVec3s(look,movespeeed));
  //   }
  // }

  // bool debug_key = false;
  // if (input->currentKeyStates[SDL_SCANCODE_H] && !input->currentKeyStatesPrev[SDL_SCANCODE_H])
  // {
  //   debug_key = true;
  // }


  bool jump_cancel = input->currentKeyStates[input->binding_crouch] && player->grounded;
  //
  if (tracing)
  {
    player->aabb.hwidth = fn_createVec3((3.04*10*6.4*0.6*2.0*1.2),(3.04*10*6.4*0.6*2.0*1.2),(3.04*10*6.4*0.6*2.0*1.2));
    player->radius = fn_createVec3((3.04*10*6.4*0.6*2.0*1.2),(3.04*10*6.4*0.6*2.0*1.2),(3.04*10*6.4*0.6*2.0*1.2));
  }
  else
  {
    player->aabb.hwidth = fn_createVec3(20,40,20);
    player->radius = fn_createVec3(20,40,20);
  }

  player->mode = TH_SLIDE_MODE;
  player->aabb.mode = SPHERE ;//| EDGE_MODE;
  if ((input->currentKeyStates[input->binding_crouch] &&!input->currentKeyStatesPrev[input->binding_crouch]) && !ls->player->is_dead && th_time() > ls->player->crouch_time && th_time() > ls->player->uncrouch_time)
  {
  //  player->grounded = false;
    // player->aabb.hwidth.y = 15;
    // player->radius.y = 15;

    // player->aabb.hwidth.y = 40;
    // player->radius.y = 40;
    // player->aabb.mode = SPHERE;
    //if ()
    //ls->player->request_uncrouch = false;

    ls->player->crouch_time = th_time() + TH_CROUCH_MS;

  }

  if ((!input->currentKeyStates[input->binding_crouch] && input->currentKeyStatesPrev[input->binding_crouch]) && !ls->player->is_dead && th_time() > ls->player->crouch_time && th_time() > ls->player->uncrouch_time)
  {
  ls->player->request_uncrouch = true;
  ls->player->uncrouch_time = th_time() + TH_CROUCH_MS;
  ls->player->fov_delta_target = 1.0;
  // player->aabb.position.y = player->aabb.position.y - (27);
  //   ls->player->fov_delta_target = 1.0;


  // player->grounded = false;
    //player->velocity.y -=
  }

  // if (ls->player->request_uncrouch && !ls->player->is_dead)
  // {
  //   //check to see if you can actually make the move
  //   // bool trace_successful = true;
  //
  //   // th_Entity tracer = TH_DEFAULT_ENTITY;
  //   // tracer.aabb.position = player->aabb.position;
  //   // tracer.aabb.hwidth = fn_multVec3(player->radius,fn_createVec3(1.0,0.99,1.0));
  //   // tracer.radius = fn_multVec3(player->radius,fn_createVec3(1.0,0.99,1.0));
  //   // tracer.velocity = fn_createVec3(0,-27.0*2.0,0);
  //   // tracer.grounded = false;
  //   // tracer.collided = false;
  //   // tracer.aabb.mode = SPHERE;
  //   // tracer.mode = TH_IMPACT_MODE;
  //   // tracer.robust_collisions = false;
  //   //
  //   // th_updateEntity(&tracer,ls->world,1.0,th_getPhysicsMemory(ls->world,0));
  //   //
  //   // if (!tracer.collided)
  //   // {
  //   //   printf("Uncrouched\n");
  //   //   ls->player->request_uncrouch = false;
  //   //   player->aabb.hwidth = fn_createVec3(20,40,20);
  //   //   player->radius = fn_createVec3(20,40,20);
  //   //
  //   //   player->aabb.position.y = player->aabb.position.y - (27);
  //   //   ls->player->fov_delta_target = 1.0;
  //   // }
  //
  //   ls->player->request_uncrouch = false;
  //
  // }

  // printf("%f\n", player->aabb.position.y);
  fn_vec3 player_original_size = player->aabb.hwidth;

  player->type = TH_PLAYER_ENTITY;
  // player->velocity = vel;//fn_addVec3(player->velocity,vel);

  //  player->velocity = vel;
  th_PlayerDefs pdefs_default;
  pdefs_default.friction = 0.02f;
  // pdefs_default.gravity = 0.001;
  pdefs_default.gravity = 0.001;

  if (input->currentKeyStates[input->binding_crouch])
  {
    pdefs_default.gravity = 0.0025;
  }

  pdefs_default.jumpSpeed = -0.45;
  pdefs_default.runAcceleration = 0.02;
  pdefs_default.runDeacceleration = 0.01;
  pdefs_default.moveSpeed = 0.75;
  pdefs_default.sideStrafeSpeed = 1;
  pdefs_default.sideStrafeAcceleration = 0.13;
  pdefs_default.airDecceleration =0.0018;
  pdefs_default.airAcceleration = 0.0018;

  if (input->currentKeyStates[input->binding_jump] && player->can_jump && !jump_cancel && !ls->player->is_dead )
  {
    {
      a_VirtualSource* s = a_playVirtualSource(29,0,a_getPos(),NULL );
      a_setVSLoop(s,false);
      a_setVSPos(s,a_getPos());
      a_setVSVel(s,fn_createVec3s(0));
      a_setVSGain(s,0.4);
    }
  }
  float old_y_velocity = player->velocity.y;
  bool oldground = player->grounded;

  if (ls->player->set_physics_y)
  {
    player->aabb.position.y = ls->player->physics_y;
  }
  else
  {
    ls->player->set_physics_y = true;
    ls->player->physics_y = player->aabb.position.y;
  }


  // if (input->currentKeyStates[SDL_SCANCODE_Q] && !input->currentKeyStatesPrev[SDL_SCANCODE_Q])
  // {
  //   fn_printVec3(player->aabb.position);
  // }
  bool slide_start = false;
  if (flying)
  {
    player->aabb.position = fn_addVec3(player->aabb.position,vel);
  }
  else
  {
   slide_start = th_updatePlayerSlide(player,ls->world,fn_normalizeVec3(vel),!input->currentKeyStates[input->binding_crouch] && input->currentKeyStates[input->binding_jump] && !jump_cancel && !ls->player->is_dead ,dt,pdefs_default,&ls->world->phys_mem[0],false,look,normal,input->currentKeyStates[input->binding_crouch]);
  }

  if (!player->grounded)
  {
    th_incrementAirtime(dt);
  }
  th_incrementCompletionTime(dt);

  ls->player->physics_y = player->aabb.position.y;

  if (th_time() <  ls->player->crouch_time || input->currentKeyStates[input->binding_crouch])
  {
    float interp_alpha = fn_clamp(1.0 - ((ls->player->crouch_time - th_time())/(TH_CROUCH_MS)),0.0,1.0);
    player->aabb.position.y = fn_lerp(ls->player->physics_y,ls->player->physics_y + 25,interp_alpha);
  }
  else if (th_time() < ls->player->uncrouch_time || !input->currentKeyStates[input->binding_crouch])
  {
    float interp_alpha = fn_clamp(1.0 - ((ls->player->uncrouch_time - th_time())/(TH_CROUCH_MS)),0.0,1.0);
    player->aabb.position.y = fn_lerp(ls->player->physics_y + 25,ls->player->physics_y,interp_alpha);
  }

  // player->velocity = fn_createVec3s(1);


//  printf("%i\n",player->grounded );
  if (slide_start )//
  {
    {
      ls->player->fov_delta_target = 1.1;
      a_VirtualSource* s = a_playVirtualSource(45,0,fn_createVec3(0,0,0),NULL );
      a_setVSRelativeToListener(s,true);
      a_setVSLoop(s,false);
      a_setVSPos(s,fn_createVec3(0,0,0));
      a_setVSVel(s,fn_createVec3s(0));
      a_setVSGain(s,0.7);
      a_setVSPitch(s,th_randomFloat(0.7,1.0));


    }
    {
      a_VirtualSource* s = a_playVirtualSource(sound_landinghard,0,fn_createVec3(0,0,0),NULL );
      a_setVSRelativeToListener(s,true);
      a_setVSLoop(s,false);
      a_setVSPos(s,fn_createVec3(0,0,0));
      a_setVSVel(s,fn_createVec3s(0));
      a_setVSGain(s,0.7);
    }
  }

  static th_timer_t landtime = 0.0;
  //
  // if (player->grounded && !oldground)
  // {
  //     th_printlnDevConsole("%i %i %i %i %i %i \n",th_frame(),old_y_velocity > 0.01 , player->velocity.y <= 0 , th_time() > landtime + 100 , player->grounded && !oldground , !input->currentKeyStates[input->binding_crouch]);
  // }
  //(old_y_velocity > 0.01 && player->velocity.y <= 0 && th_time() > landtime + 100 && player->grounded && !oldground) && !input->currentKeyStates[input->binding_crouch]
  if ((old_y_velocity > 0.2 && th_time() > landtime + 100 && player->grounded && !oldground)  )//&& !input->currentKeyStates[input->binding_crouch]
  {

    landtime = th_time();
    {
      a_VirtualSource* s = a_playVirtualSource(28,-1,fn_createVec3s(0),NULL );
      a_setVSRelativeToListener(s,true);
      a_setVSLoop(s,false);
      a_setVSPos(s,fn_createVec3(0,40,0));
      a_setVSVel(s,fn_createVec3s(0));
      a_setVSGain(s,0.8);
    }
  }

  if (input->currentKeyStates[input->binding_crouch] && fn_length2(player->velocity) > 0.21*0.21 && player->collided)
  {
    if (ls->player->slide_source != NULL )
    {
     // a_playVS(ls->player->slide_source);
      a_setVSLoop(ls->player->slide_source,true);
      //a_setVSPitch(ls->player->slide_source,th_randomFloat(0.7,1.0));


      fn_vec3 dir_relsnd = fn_subVec3(player->collision_position,player->aabb.position) ;

      float right_component = fn_dot(dir_relsnd,right);
      float look_component = -fn_dot(dir_relsnd,look);
      float up_component = fn_dot(dir_relsnd,fn_cross(right,look));

      a_setVSPos(ls->player->slide_source,fn_createVec3(right_component,up_component,look_component));
    }
    else if (ls->player->slide_source == NULL && !old_collided)
    {
      ls->player->slide_source = a_playVirtualSource(sound_slideloop,0,fn_createVec3(0,0,0),NULL );
      a_setVSRelativeToListener(ls->player->slide_source,true);
      a_setVSLoop(ls->player->slide_source,true);
      a_setVSPos(ls->player->slide_source,fn_createVec3(0,0,0));
      a_setVSVel(ls->player->slide_source,fn_createVec3s(0));
      a_setVSGain(ls->player->slide_source,0.2);
      a_setVSCleanup(ls->player->slide_source,&ls->player->slide_source,a_standardCleanup);
      a_setVSPitch(ls->player->slide_source,th_randomFloat(0.7,1.0));

      fn_vec3 dir_relsnd = fn_subVec3(player->collision_position,player->aabb.position) ;

      float right_component = -fn_dot(dir_relsnd,right);
      float look_component = -fn_dot(dir_relsnd,look);
      float up_component = fn_dot(dir_relsnd,fn_cross(right,look));

      a_setVSPos(ls->player->slide_source,fn_createVec3(right_component,up_component,look_component));
    }


  }
  else
  {
    if (ls->player->slide_source != NULL)
    {
      a_setVSLoop(ls->player->slide_source,false);
      //a_stopVS(ls->player->slide_source);
      //ls->player->slide_source = NULL;
      //printf("Stopped %i\n",th_frame());
    }
  }

  static th_timer_t steptime = 0.0;

  if (player->grounded && (input->currentKeyStates[input->binding_forward] || input->currentKeyStates[input->binding_back] || input->currentKeyStates[input->binding_left]  || input->currentKeyStates[input->binding_right]) && th_time() > steptime + 500)
  {
    steptime = th_time();
    {
      ls->player->stepoffset = (ls->player->stepoffset + (1 + (th_frame() % 2))) % 4;
      a_VirtualSource* s = a_playVirtualSource(24 + ls->player->stepoffset,0,a_getPos(),NULL );
      a_setVSLoop(s,false);
      a_setVSPos(s, fn_createVec3(0,40,0));
      a_setVSVel(s,fn_createVec3s(0));
      a_setVSGain(s,0.3);
      a_setVSRelativeToListener(s,true);
    }
  }



  //player->aabb.position = fn_addVec3(player->aabb.position,fn_normalizeVec3(vel));


  fn_vec3 result_normal;
  bool is_hit_enemy = false;
  fn_vec3 result_pos;
  float result_time;
  player->aabb.hwidth = fn_createVec3(20,20,20);
  player->aabb.mode = SPHERE;
  th_Entity* hit = th_collideWithEntities(TH_ENEMY | TH_ENEMY_ROCKET | TH_CAN_KILL_PLAYER,player,1,&result_normal,&is_hit_enemy,0,dt,&result_pos,&result_time);
  player->aabb.hwidth = player_original_size;
  player->aabb.mode = CAPSULE;
  if(is_hit_enemy && !ls->player->is_dead)
  {
    if (hit->damage_callback != NULL)
    {
      int dmg = hit->damage_callback((void*)hit,(void*)player,dt,(void*)ls->world );
      th_decrementPlayerHealth(ls->player,dmg);
      if (dmg > 0)
      {
        th_decrementPlayerGem(ls->player,4);

      }

      if (hit->type == TH_ROCKET_ENTITY)
      {
        ls->player->screenshake_f = 0.13*0.4;
        ls->player->screenshake_t = 0;
        ls->player->screenshake_amplitude = 0.1;
      }
      else
      {
        ls->player->screenshake_f = 0.13*0.1;
        ls->player->screenshake_t = 0;
        ls->player->screenshake_amplitude = 0.01;
      }

      th_setGameGlow(fn_createVec3(1,0,0),0.43);
    }
    //
    //
  }
  float osign = fn_sign(ls->player->fov_delta_target - ls->player->fov_delta);
  ls->player->fov_delta = ls->player->fov_delta + osign*dt*0.001;
  if (osign == 1.0 &&  ls->player->fov_delta > ls->player->fov_delta_target)
  {
    ls->player->fov_delta = ls->player->fov_delta_target;
  }
  if (osign == -1.0 &&  ls->player->fov_delta < ls->player->fov_delta_target)
  {
    ls->player->fov_delta = ls->player->fov_delta_target;
  }


}
