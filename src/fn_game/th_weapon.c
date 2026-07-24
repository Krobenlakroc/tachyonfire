#include "th_weapon.h"
#include "../fn_engine/th_time.h"
#include <stdlib.h>
#include "../fn_engine/th_level.h"

#include "../fn_math/fn_common.h"
//#define NOVIEWMODEL

void th_weaponInitialize(th_Allocator* alloc,th_Weapon* object,th_LevelState* levelstate)
{
  object->levelstate = levelstate;
  object->weapon_transform_count = 1;
  object->weapon_transforms = th_alloc(alloc,sizeof(fn_mat4)*1);
  object->weapon_transforms[0] = fn_translaterotatescale(fn_createVec3(200,-180,0),fn_radians(0),fn_createVec3(1,0,0),fn_createVec3s(10));

  object->weapon_transforms_bolt = th_alloc(alloc,sizeof(fn_mat4)*1);
  object->weapon_transforms_bolt[0] = fn_identityMat4();

  object->sten_kickback = 0.0;
  object->sten_kickback_velocity = 0.0; //assuem constant acceleration forward

  object->weapon_transform_count_hammer = 2;
  object->weapon_transforms_hammer = th_alloc(alloc,sizeof(fn_mat4)*2);
  object->weapon_transforms_hammer[0] = fn_translaterotatescale(fn_createVec3(200,-180,0),fn_radians(0),fn_createVec3(1,0,0),fn_createVec3s(10));
  object->weapon_transforms_hammer[1] = fn_makescale(fn_createVec3s(0));

  object->weapon_transform_count_shotgun = 1;
  object->weapon_transforms_shotgun = th_alloc(alloc,sizeof(fn_mat4)*1);
  object->weapon_transforms_shotgun[0] = fn_translaterotatescale(fn_createVec3(200,-180,0),fn_radians(0),fn_createVec3(1,0,0),fn_createVec3s(10));

  object->angle = 0;
  object->angle_velocity = 0;
  object->currentAngles = fn_createVec2(0,0);
  object->currentPos = fn_createVec3(0,0,0);
  object->kickback = 0;
  object->kickback_velocity = 0;

  object->angle_akimbo = 0;
  object->angle_velocity_akimbo = 0;

  object->currentPos_akimbo = fn_createVec3(0,0,0);
  object->currentAngles_akimbo = fn_createVec2(0,0);

  object->kickback_akimbo = 0;
  object->kickback_velocity_akimbo = 0;
  object->kickback_acceleration_akimbo = 0;


  object->chosen_weapon = TH_MACHINEGUN;

  object->weapon_transform_count_level2 = 2;
  object->weapon_transforms_level2 = th_alloc(alloc,sizeof(fn_mat4)*2);
  object->weapon_transforms_level2[0] = fn_makescale(fn_createVec3(0,0,0));
  object->weapon_transforms_level2[1] = fn_makescale(fn_createVec3(0,0,0));


  object->weapon_transform_count_shotgun_level2 = 1;
  object->weapon_transforms_shotgun_level2 = th_alloc(alloc,sizeof(fn_mat4)*1);
  object->weapon_transforms_shotgun_level2[0] = fn_makescale(fn_createVec3(0,0,0));

  object->weapon_switch_time = 0.0;
  object->weapon_switch_request = TH_NOWEAPON;


  object->weapon_transform_count_flak_cannon_front = 1;
  object->weapon_transforms_flak_cannon_front = th_alloc(alloc,sizeof(fn_mat4)*1);
  object->weapon_transforms_flak_cannon_front[0] = fn_makescale(fn_createVec3s(0));

  object->weapon_transform_count_flak_cannon_back = 1;
  object->weapon_transforms_flak_cannon_back = th_alloc(alloc,sizeof(fn_mat4)*1);
  object->weapon_transforms_flak_cannon_back[0] = fn_makescale(fn_createVec3s(0));

  object->weapon_transform_count_sledge = 1;
  object->weapon_transforms_sledge = th_alloc(alloc,sizeof(fn_mat4)*1);
  object->weapon_transforms_sledge[0] = fn_makescale(fn_createVec3s(0));

  object->lowering_interp = 0.0;

  object->shell_id = -1;

  object->spawned_shell_id = -1;
  object->spawned_shell_time = 0.0;
  object->spawn_shell_pos = fn_createVec3(0,0,0);
  object->shell_respawn_scale = 0.0;

  object->hammer_transforms[0] = fn_identityTransform();
  object->hammer_transforms[1] = fn_identityTransform();

  object->sten_axis_a = fn_createVec3(0,0,0);
  object->sten_axis_b = fn_createVec3(1,0,0);

  object->chaingun_axis_a = fn_createVec3(0,0,0);
  object->chaingun_axis_b = fn_createVec3(1,0,0);

  object->spas12_axis_a = fn_createVec3(0,0,0);
  object->spas12_axis_b = fn_createVec3(1,0,0);

  object->shotgun_temp_interp = 0;
  object->machinegun_temp_interp = 0;

  object->weapon_transform_sledge_ref = fn_identityMat4();


  for (int i = 0 ; i < TH_NUM_WEAPONS; i++)
  {
    object->weapon_levelup_timers[i] = -1000.0;
  }



}

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



float px_external = 47.000000;
float py_external =   -39.000000;
float pz_external=  -59.000000;

float xtheta =  -63.000000;
float ytheta =  -81.000000;

void th_weaponCheckLowering(th_Weapon* object,float dt,fn_vec3 target_pos,fn_vec3 direction)
{
  float trace_radius = 40;

  fn_vec3 end_trace = fn_addVec3(target_pos,fn_multVec3s(fn_normalizeVec3(direction),80));
  end_trace = fn_addVec3(end_trace,fn_createVec3(0,10,0));

  fn_vec3 begin_trace = fn_addVec3(target_pos,fn_multVec3s(fn_normalizeVec3(direction),trace_radius));

  bool hit_wall = false;
  fn_vec3 result_trace =  th_traceVolume(object->levelstate->world,begin_trace,end_trace,trace_radius,NULL,&hit_wall,th_getPhysicsMemory(object->levelstate->world,0));

  th_Entity tracer = TH_DEFAULT_ENTITY;
  tracer.aabb.position = target_pos;
  tracer.aabb.hwidth = fn_createVec3(trace_radius,trace_radius,trace_radius);
  tracer.radius = fn_createVec3(trace_radius,trace_radius,trace_radius);
  tracer.velocity = fn_createVec3s(0.0);
  tracer.grounded = false;
  tracer.collided = false;
  tracer.aabb.mode = SPHERE;
  tracer.mode = TH_IMPACT_MODE;


  bool alt_hit = th_checkCollisionWorld(object->levelstate->world,&tracer,th_getPhysicsMemory(object->levelstate->world,0));

  hit_wall = hit_wall || alt_hit;

  if (hit_wall)
  {
    object->lowering_interp += 0.007*dt;
    if (object->lowering_interp > 1.0)
    {
      object->lowering_interp = 1.0;
    }
  }
  else
  {
    object->lowering_interp -= 0.007*dt;
    if (object->lowering_interp < 0.0)
    {
      object->lowering_interp = 0.0;
    }
  }
}

static float bolt_func(float time,float T)
{
  const float a = 0.4;
  float k = fmod(time,T)/T;
  return (1 - cos(2*3.14159*powf(k,a)))/2.0;
}

void th_weaponUpdate(th_Weapon* object, fn_RawInput* input,float dt,fn_vec3 target_pos,fn_vec2 target_angles,fn_vec3 direction,fn_vec3 up)
{



  //object->lowering_interp = 0.0;


  float mov_curve = sinf(object->lowering_interp*3.14159 - (3.14159*0.5))*0.5 + 0.5;

  float scl = (1.0 - object->lowering_interp)*0.8 + 0.2;


  bool is_dead = object->levelstate->player->is_dead;
  bool hammerheld = object->levelstate->hammer->is_held[0];
  bool hammerheld_2 = object->levelstate->hammer->is_held[1];

  bool hammerheld_sledge = object->levelstate->hammer->is_held_sledge;


  object->weapon_transforms_shotgun[0] = fn_makescale(fn_createVec3(0,0,0));
  object->weapon_transforms[0] = fn_makescale(fn_createVec3(0,0,0));
  object->weapon_transforms_level2[0] = fn_makescale(fn_createVec3(0,0,0));
  object->weapon_transforms_level2[1] = fn_makescale(fn_createVec3(0,0,0));
  object->weapon_transforms_shotgun_level2[0] = fn_makescale(fn_createVec3(0,0,0));
  object->weapon_transforms_flak_cannon_front[0] = fn_makescale(fn_createVec3s(0));
  object->weapon_transforms_flak_cannon_back[0] = fn_makescale(fn_createVec3s(0));
  object->weapon_transforms_bolt[0] = fn_makescale(fn_createVec3s(0));


  if (hammerheld && ! object->levelstate->hammer->animation_interpose[0])
  {
    object->weapon_transforms_hammer[0] = fn_makescale(fn_createVec3(0,0,0));
  }

  if (hammerheld_2 && ! object->levelstate->hammer->animation_interpose[1])
  {
    object->weapon_transforms_hammer[1] = fn_makescale(fn_createVec3(0,0,0));
  }

  if (hammerheld_sledge)
  {
    object->weapon_transforms_sledge[0] = fn_makescale(fn_createVec3s(0));
  }

  #ifdef NOVIEWMODEL
  return;
  #endif

  SDL_Scancode bindings[3] = {input->binding_weapon1,input->binding_weapon3,input->binding_weapon2};


  if (input->currentKeyStates[bindings[0]] && !input->currentKeyStatesPrev[bindings[0]] && object->weapon_switch_request == TH_NOWEAPON)
  {
    object->weapon_switch_request = TH_MACHINEGUN;
    object->weapon_switch_time = th_time();
  }
  else if (input->currentKeyStates[bindings[1]] && !input->currentKeyStatesPrev[bindings[1]] && object->weapon_switch_request == TH_NOWEAPON)
  {
    object->weapon_switch_request = TH_HAMMER;
    object->weapon_switch_time = th_time();
  }
  else if (input->currentKeyStates[bindings[2]] && !input->currentKeyStatesPrev[bindings[2]] && object->weapon_switch_request == TH_NOWEAPON)
  {
    object->weapon_switch_request = TH_SHOTGUN;
    object->weapon_switch_time = th_time();
  }

  if (object->weapon_switch_request != TH_NOWEAPON && th_time() > object->weapon_switch_time + 75 && !(object->levelstate->hammer->sledge_impact))
  {
    object->chosen_weapon = object->weapon_switch_request;
    object->weapon_switch_request = TH_NOWEAPON;
  }

  // if (input->currentKeyStates[SDL_SCANCODE_T])
  // {
  //   pz_external -= 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_G])
  // {
  //   pz_external += 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_F])
  // {
  //   px_external -= 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_H])
  // {
  //   px_external += 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_V])
  // {
  //   py_external -= 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_B])
  // {
  //   py_external += 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_J])
  // {
  //   xtheta -= 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_K])
  // {
  //   xtheta += 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_U])
  // {
  //   ytheta -= 1;
  // }
  //
  // if (input->currentKeyStates[SDL_SCANCODE_I])
  // {
  //   ytheta += 1;
  // }

  //printf("%f %f %f %f %f\n", px_external,py_external,pz_external,xtheta,ytheta);

  if (is_dead)
  {
    object->chosen_weapon = TH_NOWEAPON;
  }

  bool set_shell_transform = false;

  if (object->chosen_weapon == TH_SHOTGUN && object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
  {
    //spawn and/or set scale/orient/physics position
    if (object->shell_id == -1)
    {
      object->shell_id = th_brassSpawnScaled(object->levelstate->shotbrass, target_pos,fn_createVec3s(0),fn_createVec3(0,1,0),0.0);
    }
    set_shell_transform = true;
  }


  if (object->levelstate->player->level_weapon[TH_SHOTGUN]  != 3 && object->shell_id != -1)
  {
    //despawn shell
    object->shell_id = -1;
  }


  th_PointLight light;
  light.pos = fn_createVec4Vec3(fn_createVec3s(1000000),0);
  light.color = fn_createVec4(0,0,0,0);
  light.lightmat = fn_identityMat4();
  light.shadowindex = fn_createVec4(0,0,0,0);
  light.pos2 = fn_createVec4(0,0,0,0);
  object->levelstate->general_light_query->pointlights_physical[2] = light;

  if (!(input->left && object->chosen_weapon == TH_MACHINEGUN))
  {
    object->machinegun_temp_interp =  object->machinegun_temp_interp - (1.0/30000.0)*dt;
    if (object->machinegun_temp_interp < 0.0)
    {
      object->machinegun_temp_interp = 0.0;
    }
  }
  else
  {
    float rate = 1.0/15000.0;
    if (object->levelstate->player->level_weapon[TH_MACHINEGUN]  >= 2)
    {
      rate = 1.0/7000.0;
    }
    object->machinegun_temp_interp =  object->machinegun_temp_interp + rate*dt;

    if (object->machinegun_temp_interp > 1.0)
    {
      object->machinegun_temp_interp = 1.0;
    }
  }

  if (!(input->left && object->chosen_weapon == TH_SHOTGUN))
  {
    object->shotgun_temp_interp =  object->shotgun_temp_interp - (1.0/30000.0)*dt;
    if (object->shotgun_temp_interp < 0.0)
    {
      object->shotgun_temp_interp = 0.0;
    }
  }
  else
  {
    float rate = 1.0/15000.0;
    if (object->levelstate->player->level_weapon[TH_SHOTGUN]  >= 2)
    {
      rate = 1.0/7000.0;
    }
    object->shotgun_temp_interp =  object->shotgun_temp_interp + rate*dt;

    if (object->shotgun_temp_interp > 1.0)
    {
      object->shotgun_temp_interp = 1.0;
    }
  }

  if (object->chosen_weapon == TH_MACHINEGUN)
  {
    object->levelstate->barrel_color_interp = object->machinegun_temp_interp;
  }
  else if (object->chosen_weapon == TH_SHOTGUN)
  {
    object->levelstate->barrel_color_interp = object->shotgun_temp_interp;
  }

  float levelup_interp = 1.0 - fn_clamp((th_time() - object->weapon_levelup_timers[object->chosen_weapon])/250.0,0.0,1.0);


  if (object->chosen_weapon == TH_MACHINEGUN)
  {
    float y_extens = 0;
    float z_extens = 0;

    float px = 0;
    float py =  -60 - levelup_interp*150;
    float pz= -55 ;

    float p_pitch = 0;
    float scale = 1.0;

    float pz_bolt = 0;

    const float sten_kickback_kick = 0.00085*75.0;
    object->sten_kickback += object->sten_kickback_velocity*dt;
    object->sten_kickback_velocity += -0.00085*dt;//const accel forward

    object->sten_kickback = fn_max(object->sten_kickback ,0);//- 0.0035
    object->sten_kickback = fn_min(object->sten_kickback, 37.0);

    object->sten_kickback_velocity = fn_min(object->sten_kickback_velocity,1.0);

    //printf("%f %f\n",object->sten_kickback,object->sten_kickback_velocity);

    if (object->sten_kickback == 0.0)
    {
      object->sten_kickback_velocity = 0;
    }

    if (object->levelstate->player->level_weapon[TH_MACHINEGUN]  == 1)
    {
      px = 25;
      py =  -70 - levelup_interp*150;
      pz= -120;//-55;
      pz = pz + object->sten_kickback;

      p_pitch = 19;


      //(sin(th_time()*(3.14159/75.0)) + 1)*0.5
      pz_bolt = pz + bolt_func(fn_clamp(th_time() - object->levelstate->plasma->time_fired,0.0,75.0),75.0)*25;

      // if (scl == 1.0)
      // {
        //scale = 1.4;
     // }
      float pscl = 1.0/1.4;
      px*= pscl;

      py*= pscl;

      pz*= pscl;

      pz_bolt *= pscl;
    }
    scale = scale*scl;


    px*= scl;
    py*= scl*(1.0 + mov_curve*0.25);
    pz*= scl*(1.0 - mov_curve*0.1);

    pz_bolt*= scl*(1.0 - mov_curve*0.1);

    float aspect = py/pz;
    fn_mat4 inst;
    fn_mat4 inst_flash;

    fn_mat4 inst_bolt = fn_identityMat4();

    fn_mat4 inst_shellspawn = fn_identityMat4();

    if (object->levelstate->player->level_weapon[TH_MACHINEGUN]  == 1)
    {
      inst_bolt = fn_translaterotatescale(fn_createVec3(px,py,pz_bolt ),p_pitch,fn_createVec3(1,0,0),fn_createVec3s(5*scale));

      inst = fn_translaterotatescale(fn_createVec3(px,py,pz ),p_pitch,fn_createVec3(1,0,0),fn_createVec3s(5*scale));

      inst_flash = fn_translaterotatescale(fn_createVec3(27*scl,(-55 - levelup_interp*150 )*scl*(1.0 + mov_curve*0.2),-110*scl*(1.0 - mov_curve*0.1) ),p_pitch,fn_createVec3(1,0,0),fn_createVec3s(1));

      inst_shellspawn = fn_translaterotatescale(fn_createVec3(27,-35*(1.0 + mov_curve*0.25),-60*(1.0 - mov_curve*0.1) ),p_pitch,fn_createVec3(1,0,0),fn_createVec3s(1));





    }
    else
    {
      inst = fn_translaterotatescale(fn_createVec3(px,py,pz ),fn_radians(object->angle),fn_createVec3(0,0,1),fn_createVec3s(5*scale));

      inst_flash = fn_translaterotatescale(fn_createVec3(0,(-45 - levelup_interp*150 )*scl*(1.0 + mov_curve*0.25),-110*scl*(1.0 - mov_curve*0.1) ),0.0,fn_createVec3(0,0,1),fn_createVec3s(1));
    }


    //-45
    //float yflash = fn_lerp(-45,-65,mov_curve);


    if (input->left)
    {
      object->angle_velocity = fn_min(object->angle_velocity + 0.1,0.52);
    }
    else
    {
      object->angle_velocity = fn_max(object->angle_velocity - 0.0035,0);
    }
    object->angle += dt*object->angle_velocity;
    //  fn_printVec3(fn_createVec3(px,py,pz));
    float lerpfactor = fn_clamp(fn_distance(target_pos,object->currentPos)/30.0,0.01,1);

    float lerpfactorangles = fn_clamp(fn_distanceVec2(target_angles,object->currentAngles)/(3.141590),0.2,1);

    object->currentAngles = fn_lerpVec2(object->currentAngles,target_angles,(lerpfactorangles));
    object->currentPos = fn_lerpVec3(object->currentPos,target_pos,(lerpfactor));

    float offset_max = 20;//20*scl;
    object->currentPos = fn_clampVec3(object->currentPos,fn_addVec3(target_pos,fn_createVec3s(-offset_max)),fn_addVec3(target_pos,fn_createVec3s(offset_max)));


    float weapon_dist = fn_distance(object->currentPos,target_pos);

    fn_vec3 dir_weapon = object->currentPos;
    if (weapon_dist > 0.01)
    {
      dir_weapon = fn_normalizeVec3(fn_subVec3(object->currentPos,target_pos));
      dir_weapon = fn_addVec3(target_pos,fn_multVec3s(dir_weapon,weapon_dist*scl));
    }

    //
    inst = fn_multMat4(inst,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst = fn_scale(inst,fn_createVec3(-1,-1,-1));

    inst_flash = fn_multMat4(inst_flash,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst_flash = fn_scale(inst_flash,fn_createVec3(-1,-1,-1));

    inst_bolt = fn_multMat4(inst_bolt,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst_bolt = fn_scale(inst_bolt,fn_createVec3(-1,-1,-1));

    inst_shellspawn = fn_multMat4(inst_shellspawn,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst_shellspawn = fn_scale(inst_shellspawn,fn_createVec3(-1,-1,-1));

    if (object->levelstate->player->level_weapon[TH_MACHINEGUN]  == 1)
    {

      object->levelstate->blackbody_barrel_pointa = fn_transformVec3(object->sten_axis_a,inst);
      object->levelstate->blackbody_barrel_pointb = fn_transformVec3(object->sten_axis_b,inst);

      object->weapon_transforms[0] = inst;
      object->weapon_transforms_bolt[0] = inst_bolt;

      object->levelstate->barrel_shape_min = 0.34;
      object->levelstate->barrel_shape_max = 0.41;
      object->levelstate->barrel_maxtemp = 3000.0;

      if (object->levelstate->plasma->firing)
      {



        object->sten_kickback_velocity += sten_kickback_kick;
        // th_brassSpawn(object->levelstate->plasma->levelstate->brass,shellpos,fn_addVec3(r,fn_createVec3(0,-0.5,0)),shelldir);

        fn_vec3 right_temp = fn_normalizeVec3(fn_cross(direction,up));
        fn_vec3 vel = fn_createVec3(0,0,0);
        vel = fn_addVec3(vel,fn_multVec3s(right_temp,-0.7));

        vel = fn_addVec3(vel,fn_multVec3s(direction,0.7));

        vel = fn_addVec3(vel,object->levelstate->player_e.velocity);

        //fn_vec3 shpos = fn_addVec3(target_pos,fn_multVec3s(direction,120));

        fn_vec3 shpos = fn_transformVec3(fn_createVec3s(0.0),inst_shellspawn);

        int shell_id = th_brassSpawn(object->levelstate->brass,shpos,fn_addVec3(vel,fn_createVec3(0,-0.9,0)),fn_createVec3(0,-1,0));
        object->levelstate->brass->angular_vel[shell_id] = 0.012;
      }





      fn_vec3 blackbodypos = object->levelstate->blackbody_barrel_pointb;//fn_transformVec3(fn_createVec3(0.0,0.0,2.0),inst);

      fn_vec3 blackbodycolor = th_computeBlackBody(object->machinegun_temp_interp,3000.0);

      blackbodycolor = fn_multVec3s(blackbodycolor,5000.0*scl*scl);

      th_PointLight light;
      light.pos = fn_createVec4Vec3(blackbodypos,0);
      light.color = fn_createVec4(blackbodycolor.x,blackbodycolor.y,blackbodycolor.z,0);
      light.lightmat = fn_identityMat4();
      light.shadowindex = fn_createVec4(0,0,0,0);
      light.pos2 = fn_createVec4(0,0,0,0);
      object->levelstate->general_light_query->pointlights_physical[2] = light;



      if ((th_time() - object->levelstate->plasma->time_flash_scalechange) < 37.5 && object->levelstate->plasma->time_flash_scalechange > 1.0)
      {
        fn_vec3 flashpos = fn_createVec3(inst_flash.m[12],inst_flash.m[13],inst_flash.m[14]);

        th_Particle p = object->levelstate->plasma->flash_particle;
        p.start_time = th_time();
        p.position = fn_addVec3(flashpos,fn_multVec3s(fn_normalizeVec3(direction),-pz*2.0));
        p.position = fn_addVec3(p.position,fn_multVec3s(fn_normalizeVec3(up),-py));
        p.alphascale = 0.25;
        p.alphascale_base = 0.25;
        p.scale = p.scale * scl;
        th_addParticle(p);

        //object->levelstate->plasma->lights[0].pos = fn_createVec4Vec3(fn_addVec3(flashpos,fn_multVec3s(fn_normalizeVec3(direction),100)) ,0);
      }
    }
    else if (object->levelstate->player->level_weapon[TH_MACHINEGUN]  == 2)
    {

      object->levelstate->blackbody_barrel_pointa = fn_transformVec3(object->chaingun_axis_a,inst);
      object->levelstate->blackbody_barrel_pointb = fn_transformVec3(object->chaingun_axis_b,inst);

      object->levelstate->barrel_shape_min = 0.1;
      object->levelstate->barrel_shape_max = 0.34;
      object->levelstate->barrel_maxtemp = 1000.0;

      object->weapon_transforms_level2[0] = inst;


      fn_vec3 blackbodypos = object->levelstate->blackbody_barrel_pointb;//fn_transformVec3(fn_createVec3(0.0,0.0,15.0),inst);

      fn_vec3 blackbodycolor = th_computeBlackBody(object->machinegun_temp_interp,1000.0);

      blackbodycolor = fn_multVec3s(blackbodycolor,5000.0*scl*scl);

      th_PointLight light;
      light.pos = fn_createVec4Vec3(blackbodypos,0);
      light.color = fn_createVec4(blackbodycolor.x,blackbodycolor.y,blackbodycolor.z,0);
      light.lightmat = fn_identityMat4();
      light.shadowindex = fn_createVec4(0,0,0,0);
      light.pos2 = fn_createVec4(0,0,0,0);
      object->levelstate->general_light_query->pointlights_physical[2] = light;

      if ((th_time() - object->levelstate->plasma->time_flash_scalechange) < 37.5 && object->levelstate->plasma->time_flash_scalechange > 1.0)
      {
        fn_vec3 flashpos = fn_createVec3(inst_flash.m[12],inst_flash.m[13],inst_flash.m[14]);

        th_Particle p = object->levelstate->plasma->flash_particle;
        p.start_time = th_time();
        p.position = fn_addVec3(flashpos,fn_multVec3s(fn_normalizeVec3(direction),-pz*2.0));
        p.position = fn_addVec3(p.position,fn_multVec3s(fn_normalizeVec3(up),-py));
        p.alphascale = 0.25;
        p.alphascale_base = 0.25;
        p.scale = p.scale * scl*1.5;
        th_addParticle(p);

        //object->levelstate->plasma->lights[0].pos = fn_createVec4Vec3(fn_addVec3(flashpos,fn_multVec3s(fn_normalizeVec3(direction),100)) ,0);
      }
    }
    else if (object->levelstate->player->level_weapon[TH_MACHINEGUN]  == 3)
    {
      float px = -75;
      float py =  -60 - levelup_interp*150;
      float pz= -55;

      px*= scl;
      py*= scl*(1.0 + mov_curve*0.25);
      pz*= scl*(1.0 - mov_curve*0.1);

      fn_mat4 inst_a = fn_translaterotatescale(fn_createVec3(px,py,pz ),fn_radians(object->angle),fn_createVec3(0,0,1),fn_createVec3s(5*scl));
      fn_mat4 inst_b = fn_translaterotatescale(fn_createVec3(-px,py,pz ),fn_radians(object->angle),fn_createVec3(0,0,1),fn_createVec3s(5*scl));

      fn_mat4 inst_flash_a = fn_translaterotatescale(fn_createVec3(-150*scl,(-45 - levelup_interp*150)*scl*(1.0 + mov_curve*0.25),-110*scl*(1.0 - mov_curve*0.1) ),0.0,fn_createVec3(0,0,1),fn_createVec3s(1));
      inst_flash_a = fn_multMat4(inst_flash_a,fn_inverse(r_camera(dir_weapon,target_angles)));
      inst_flash_a = fn_scale(inst_flash_a,fn_createVec3(-1,-1,-1));
      fn_vec3 flashpos_a = fn_createVec3(inst_flash_a.m[12],inst_flash_a.m[13],inst_flash_a.m[14]);

      fn_mat4 inst_flash_b = fn_translaterotatescale(fn_createVec3(150*scl,(-45 - levelup_interp*150)*scl*(1.0 + mov_curve*0.25),-110*scl*(1.0 - mov_curve*0.1) ),0.0,fn_createVec3(0,0,1),fn_createVec3s(1));
      inst_flash_b = fn_multMat4(inst_flash_b,fn_inverse(r_camera(dir_weapon,target_angles)));
      inst_flash_b = fn_scale(inst_flash_b,fn_createVec3(-1,-1,-1));
      fn_vec3 flashpos_b = fn_createVec3(inst_flash_b.m[12],inst_flash_b.m[13],inst_flash_b.m[14]);


      float offset_max = 20;
      object->currentPos = fn_clampVec3(object->currentPos,fn_addVec3(target_pos,fn_createVec3s(-offset_max)),fn_addVec3(target_pos,fn_createVec3s(offset_max)));

      fn_vec3 dir_weapon = object->currentPos;
      if (weapon_dist > 0.01)
      {
        dir_weapon = fn_normalizeVec3(fn_subVec3(object->currentPos,target_pos));
        dir_weapon = fn_addVec3(target_pos,fn_multVec3s(dir_weapon,weapon_dist*scl));
      }

      inst_a = fn_multMat4(inst_a,fn_inverse(r_camera(dir_weapon,target_angles)));
      inst_a = fn_scale(inst_a,fn_createVec3(-1,-1,-1));

      inst_b = fn_multMat4(inst_b,fn_inverse(r_camera(dir_weapon,target_angles)));
      inst_b = fn_scale(inst_b,fn_createVec3(-1,-1,-1));

      object->weapon_transforms_level2[0] = inst_a;
      object->weapon_transforms_level2[1] = inst_b;


      object->levelstate->blackbody_barrel_pointa = fn_transformVec3(object->chaingun_axis_a,inst_a);
      object->levelstate->blackbody_barrel_pointb = fn_transformVec3(object->chaingun_axis_b,inst_a);

      object->levelstate->barrel_shape_min = 0.1;
      object->levelstate->barrel_shape_max = 0.34;
      object->levelstate->barrel_maxtemp = 1000.0;



      fn_vec3 blackbodypos = object->levelstate->blackbody_barrel_pointb;//fn_transformVec3(fn_createVec3(0.0,0.0,15.0),inst);

      fn_vec3 blackbodycolor = th_computeBlackBody(object->machinegun_temp_interp,1000.0);

      blackbodycolor = fn_multVec3s(blackbodycolor,5000.0*scl*scl);

      th_PointLight light;
      light.pos = fn_createVec4Vec3(blackbodypos,0);
      light.color = fn_createVec4(blackbodycolor.x,blackbodycolor.y,blackbodycolor.z,0);
      light.lightmat = fn_identityMat4();
      light.shadowindex = fn_createVec4(0,0,0,0);
      light.pos2 = fn_createVec4(0,0,0,0);
      object->levelstate->general_light_query->pointlights_physical[2] = light;

      if ((th_time() - object->levelstate->plasma->time_flash_scalechange) < 37.5 && object->levelstate->plasma->time_flash_scalechange > 1.0)
      {


        th_Particle p = object->levelstate->plasma->flash_particle;
        p.start_time = th_time();
        p.position = fn_addVec3(flashpos_a,fn_multVec3s(fn_normalizeVec3(direction),-pz*2.0));
        p.position = fn_addVec3(p.position,fn_multVec3s(fn_normalizeVec3(up),-py));
        p.alphascale = 0.25;
        p.alphascale_base = 0.25;
        p.scale = p.scale * scl*1.25;
        th_addParticle(p);

        p = object->levelstate->plasma->flash_particle;
        p.start_time = th_time();
        p.position = fn_addVec3(flashpos_b,fn_multVec3s(fn_normalizeVec3(direction),-pz*2.0));
        p.position = fn_addVec3(p.position,fn_multVec3s(fn_normalizeVec3(up),-py));
        p.alphascale = 0.25;
        p.alphascale_base = 0.25;
        p.scale = p.scale * scl*1.25;
        p.theta = object->levelstate->plasma->flash_2_angle;
        th_addParticle(p);

        //object->levelstate->plasma->lights[0].pos = fn_createVec4Vec3(fn_addVec3(target_pos,fn_multVec3s(fn_normalizeVec3(direction),100)) ,0);
      }
    }

  }
  else if (object->chosen_weapon == TH_HAMMER)
  {

    if (object->levelstate->player->level_weapon[TH_HAMMER]  == 3)
    {

        float px = 75;
        float py = py_external - levelup_interp*150;
        float pz= pz_external;

        px*= scl;
        py*= scl*(1.0 + mov_curve*0.25);
        pz*= scl*(1.0 - mov_curve*0.1);


        fn_mat4 tr = fn_translaterotatescale(fn_createVec3(0,0,0),fn_radians(xtheta),fn_createVec3(1,0,0),fn_createVec3s(1));
        fn_mat4 tr2 = fn_translaterotatescale(fn_createVec3(0,0,0),fn_radians(ytheta),fn_createVec3(0,0,1),fn_createVec3s(1));
        tr = fn_multMat4(tr2,tr);

        fn_mat4 inst = fn_translaterotatescale(fn_createVec3(px,py,pz ),fn_radians(0),fn_createVec3(0,0,1),fn_createVec3s(2*scl));
        inst = fn_multMat4(tr,inst);


        //  fn_printVec3(fn_createVec3(px,py,pz));
        float lerpfactor = fn_clamp(fn_distance(target_pos,object->currentPos)/30.0,0.01,0.75);

        float lerpfactorangles = fn_clamp(fn_distanceVec2(target_angles,object->currentAngles)/(3.141590),0.2,1);

        object->currentAngles = fn_lerpVec2(object->currentAngles,target_angles,(lerpfactorangles));
        object->currentPos = fn_lerpVec3(object->currentPos,target_pos,(lerpfactor));

        float offset_max = 20;
        object->currentPos = fn_clampVec3(object->currentPos,fn_addVec3(target_pos,fn_createVec3s(-offset_max)),fn_addVec3(target_pos,fn_createVec3s(offset_max)));

        float weapon_dist = fn_distance(object->currentPos,target_pos);

        fn_vec3 dir_weapon = object->currentPos;
        if (weapon_dist > 0.01)
        {
          dir_weapon = fn_normalizeVec3(fn_subVec3(object->currentPos,target_pos));
          dir_weapon = fn_addVec3(target_pos,fn_multVec3s(dir_weapon,weapon_dist*scl));
        }

        inst = fn_multMat4(inst,fn_inverse(r_camera(dir_weapon,target_angles)));
        inst = fn_scale(inst,fn_createVec3(-1,-1,-1));

      object->weapon_transform_sledge_ref = inst;
      if (hammerheld_sledge)
      {
        object->weapon_transforms_sledge[0] = inst;
      }

    }
    else
    {
      if (hammerheld)
      {
        float px = 75;
        float py =  py_external - levelup_interp*150;
        float pz= pz_external;

        float px_old = px;
        float py_old = py;
        float pz_old = pz;

        px*= scl;
        py*= scl*(1.0 + mov_curve*0.25);
        pz*= scl*(1.0 - mov_curve*0.1);


        fn_mat4 tr = fn_translaterotatescale(fn_createVec3(0,0,0),fn_radians(xtheta),fn_createVec3(1,0,0),fn_createVec3s(1));
        fn_mat4 tr2 = fn_translaterotatescale(fn_createVec3(0,0,0),fn_radians(ytheta),fn_createVec3(0,0,1),fn_createVec3s(1));
        tr = fn_multMat4(tr2,tr);

        fn_mat4 inst = fn_translaterotatescale(fn_createVec3(px,py,pz ),fn_radians(0),fn_createVec3(0,0,1),fn_createVec3s(2*scl));
        inst = fn_multMat4(tr,inst);


        //  fn_printVec3(fn_createVec3(px,py,pz));
        float lerpfactor = fn_clamp(fn_distance(target_pos,object->currentPos)/30.0,0.01,0.75);

        float lerpfactorangles = fn_clamp(fn_distanceVec2(target_angles,object->currentAngles)/(3.141590),0.2,1);

        object->currentAngles = fn_lerpVec2(object->currentAngles,target_angles,(lerpfactorangles));
        object->currentPos = fn_lerpVec3(object->currentPos,target_pos,(lerpfactor));

        float offset_max = 20;
        object->currentPos = fn_clampVec3(object->currentPos,fn_addVec3(target_pos,fn_createVec3s(-offset_max)),fn_addVec3(target_pos,fn_createVec3s(offset_max)));

        float weapon_dist = fn_distance(object->currentPos,target_pos);

        fn_vec3 dir_weapon = object->currentPos;
        if (weapon_dist > 0.01)
        {
          dir_weapon = fn_normalizeVec3(fn_subVec3(object->currentPos,target_pos));
          dir_weapon = fn_addVec3(target_pos,fn_multVec3s(dir_weapon,weapon_dist*scl));
        }

        object->hammer_transforms[0].translate = fn_createVec3(px_old,py_old,pz_old );
        object->hammer_transforms[0].scale = fn_createVec3(2.0,2.0,2.0);
        object->hammer_transforms[0].rotate = fn_mat4toquat(tr);

        inst = fn_multMat4(inst,fn_inverse(r_camera(dir_weapon,target_angles)));
        inst = fn_scale(inst,fn_createVec3(-1,-1,-1));

        if (!object->levelstate->hammer->animation_interpose[0] && (th_time() - object->levelstate->hammer->sledge_impact_timer > 1000))
        {
           object->weapon_transforms_hammer[0] = inst;
        }




      }



      if (object->levelstate->player->level_weapon[TH_HAMMER]  == 2)
      {
        if (hammerheld_2)
        {
          float px = -75;
          float py =  py_external - levelup_interp*150;
          float pz= pz_external;

          px*= scl;
          py*= scl*(1.0 + mov_curve*0.25);
          pz*= scl*(1.0 - mov_curve*0.1);


          fn_mat4 tr = fn_translaterotatescale(fn_createVec3(0,0,0),fn_radians(xtheta),fn_createVec3(1,0,0),fn_createVec3s(1));
          fn_mat4 tr2 = fn_translaterotatescale(fn_createVec3(0,0,0),fn_radians(-ytheta),fn_createVec3(0,0,1),fn_createVec3s(1));
          tr = fn_multMat4(tr2,tr);

          fn_mat4 inst = fn_translaterotatescale(fn_createVec3(px,py,pz ),fn_radians(0),fn_createVec3(0,0,1),fn_createVec3s(2*scl));
          inst = fn_multMat4(tr,inst);


          //  fn_printVec3(fn_createVec3(px,py,pz));
          float lerpfactor = fn_clamp(fn_distance(target_pos,object->currentPos_akimbo)/30.0,0.01,0.75);

          float lerpfactorangles = fn_clamp(fn_distanceVec2(target_angles,object->currentAngles_akimbo)/(3.141590),0.2,1);

          object->currentAngles_akimbo = fn_lerpVec2(object->currentAngles_akimbo,target_angles,(lerpfactorangles));
          object->currentPos_akimbo = fn_lerpVec3(object->currentPos_akimbo,target_pos,(lerpfactor));

          float offset_max = 20;
          object->currentPos = fn_clampVec3(object->currentPos_akimbo,fn_addVec3(target_pos,fn_createVec3s(-offset_max)),fn_addVec3(target_pos,fn_createVec3s(offset_max)));


          float weapon_dist = fn_distance(object->currentPos_akimbo,target_pos);

          fn_vec3 dir_weapon = object->currentPos_akimbo;
          if (weapon_dist > 0.01)
          {
            dir_weapon = fn_normalizeVec3(fn_subVec3(object->currentPos_akimbo,target_pos));
            dir_weapon = fn_addVec3(target_pos,fn_multVec3s(dir_weapon,weapon_dist*scl));
          }

          object->hammer_transforms[1] = fn_decomposeMat4(inst);

          inst = fn_multMat4(inst,fn_inverse(r_camera(dir_weapon,target_angles)));
          inst = fn_scale(inst,fn_createVec3(-1,-1,-1));

          if (!object->levelstate->hammer->animation_interpose[1])
          {
            object->weapon_transforms_hammer[1] = inst;
          }

        }


      }
    }

  }
  else if (object->chosen_weapon == TH_SHOTGUN)
  {
    float px = 0;
    float py =  -60 - levelup_interp*150;
    float pz= -20;//-55;
    float p_pitch = 0;
    float scale = 1.0;

    if (object->levelstate->player->level_weapon[TH_SHOTGUN]  == 2 || object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
    {
      px = 25;
      py =  -75 - levelup_interp*150;
      pz= -100;//-55;
      p_pitch = 19;
    }

    if (object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
    {
      px = 35;
      py =  -75 - levelup_interp*150;
      pz= -60;//-55;
      p_pitch = 12;

      if (scl == 1.0)
      {
        scale = 1.25;
      }

    }

    float px_shell = px;
    float py_shell = py - 15;
    float pz_shell = pz - 85;

    px*= scl;
    py*= scl*(1.0 + mov_curve*0.25);
    pz*= scl*(1.0 - mov_curve*0.1);

    px_shell*= scl;
    py_shell*= scl*(1.0 + mov_curve*0.29);
    pz_shell*= scl*(1.0 - mov_curve*0.12);

    scale = scale*scl;



    //
    fn_vec3 kick = fn_createVec3(0,0,object->kickback);
    fn_mat4 r_rel = fn_maketranslaterotate(fn_createVec3(0,0,0),fn_radians(p_pitch),fn_createVec3(1,0,0));
    kick = fn_transformVec3(kick,r_rel);

    fn_mat4 inst = fn_translaterotatescale(fn_addVec3(fn_createVec3(px,py,pz ),kick ),fn_radians(p_pitch - object->kickback*0.01) ,fn_createVec3(1,0,0),fn_createVec3s(5*scale));
    fn_mat4 inst_shell = fn_translaterotatescale(fn_addVec3(fn_createVec3(px_shell,py_shell,pz_shell ),kick ),fn_radians(p_pitch - object->kickback*0.01) ,fn_createVec3(1,0,0),fn_createVec3s(3.5*scl));

    inst_shell = fn_multMat4(fn_makerotate(fn_radians(-90.0),fn_createVec3(1,0,0)),inst_shell);

    fn_mat4 inst_nokick = fn_translaterotatescale(fn_addVec3(fn_createVec3(px,py,pz ),fn_multVec3s(kick,0.05) ),fn_radians(p_pitch - object->kickback*0.01) ,fn_createVec3(1,0,0),fn_createVec3s(5*scale));

    // if (input->left)
    // {
    //   object->angle_velocity = fn_min(object->angle_velocity + 0.1,0.52);
    // }
    // else
    // {

    // }

    float old_kickback_velocity = object->kickback_velocity;

    object->kickback += dt*object->kickback_velocity;
    object->kickback_velocity += dt*object->kickback_acceleration;

    //object->kickback_velocity = fn_max(object->kickback_velocity - 0.0035,0);
    float old_kickback = object->kickback;
    object->kickback = fn_max(object->kickback - 0.0035,0);
    if (object->kickback == 0.0)
    {
      object->kickback_velocity = 0;
      object->kickback_acceleration = 0;


    }


    //printf("%f \n",object->kickback_velocity);
    fn_vec3 shpos = fn_addVec3(target_pos,fn_multVec3s(direction,120));

    if (object->kickback_velocity < 0 && old_kickback_velocity > 0 && object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3 )
    {
      //spawn the new shell

      fn_vec3 right_temp = fn_normalizeVec3(fn_cross(direction,up));
      fn_vec3 vel = fn_createVec3(0,0,0);
      vel = fn_addVec3(vel,fn_multVec3s(right_temp,0.5));

      object->spawned_shell_id = th_brassSpawnScaled(object->levelstate->shotbrass,shpos,vel,fn_createVec3(0,-1,0),3.5);
      object->levelstate->shotbrass->angular_vel[object->spawned_shell_id] = 0.012;

      object->spawned_shell_time = th_time() + 40;

      object->spawn_shell_pos = shpos;
/*
      if (object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3 )
      {
        object->levelstate->shotbrass->entities[object->spawned_shell_id].velocity = fn_addVec3(object->levelstate->shotbrass->entities[object->spawned_shell_id].velocity,object->levelstate->player_e.velocity);
      }*/
    }


    //  fn_printVec3(fn_createVec3(px,py,pz));
    float lerpfactor = fn_clamp(fn_distance(target_pos,object->currentPos)/30.0,0.01,1);

    float lerpfactorangles = fn_clamp(fn_distanceVec2(target_angles,object->currentAngles)/(3.141590),0.2,1);

    object->currentAngles = fn_lerpVec2(object->currentAngles,target_angles,(lerpfactorangles));
    object->currentPos = fn_lerpVec3(object->currentPos,target_pos,(lerpfactor));

    float offset_max = 20;
    object->currentPos = fn_clampVec3(object->currentPos,fn_addVec3(target_pos,fn_createVec3s(-offset_max)),fn_addVec3(target_pos,fn_createVec3s(offset_max)));

    float weapon_dist = fn_distance(object->currentPos,target_pos);

    fn_vec3 dir_weapon = object->currentPos;
    if (weapon_dist > 0.01)
    {
      dir_weapon = fn_normalizeVec3(fn_subVec3(object->currentPos,target_pos));
      dir_weapon = fn_addVec3(target_pos,fn_multVec3s(dir_weapon,weapon_dist*scl));
    }

    inst = fn_multMat4(inst,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst = fn_scale(inst,fn_createVec3(-1,-1,-1));

    inst_nokick = fn_multMat4(inst_nokick,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst_nokick = fn_scale(inst_nokick,fn_createVec3(-1,-1,-1));

    inst_shell = fn_multMat4(inst_shell,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst_shell = fn_scale(inst_shell,fn_createVec3(-1,-1,-1));

    float flashx = 0.0;
    float flashy = -50.0 - levelup_interp*150;
    float flashz = -135.0;
    float y_curve = 0.25;
    if (object->levelstate->player->level_weapon[TH_SHOTGUN]  == 1)
    {
      object->weapon_transforms_shotgun[0] = inst;
    }
    else if (object->levelstate->player->level_weapon[TH_SHOTGUN]  == 2)
    {
      object->levelstate->blackbody_barrel_pointa = fn_transformVec3(object->spas12_axis_a,inst);
      object->levelstate->blackbody_barrel_pointb = fn_transformVec3(object->spas12_axis_b,inst);

      object->levelstate->barrel_shape_min = 0.05;
      object->levelstate->barrel_shape_max = 0.2;
      object->levelstate->barrel_maxtemp = 1000.0;

      fn_vec3 blackbodypos = object->levelstate->blackbody_barrel_pointb;//fn_transformVec3(fn_createVec3(0.0,0.0,15.0),inst);

      fn_vec3 blackbodycolor = th_computeBlackBody(object->shotgun_temp_interp,1000.0);

      blackbodycolor = fn_multVec3s(blackbodycolor,5000.0*scl*scl);

      th_PointLight light;
      light.pos = fn_createVec4Vec3(blackbodypos,0);
      light.color = fn_createVec4(blackbodycolor.x,blackbodycolor.y,blackbodycolor.z,0);
      light.lightmat = fn_identityMat4();
      light.shadowindex = fn_createVec4(0,0,0,0);
      light.pos2 = fn_createVec4(0,0,0,0);
      object->levelstate->general_light_query->pointlights_physical[2] = light;


      object->weapon_transforms_shotgun_level2[0] = inst;
      flashx = 27.0;
      flashy = -120.0 - levelup_interp*150;
      flashz = -250.0;
      y_curve = 0.13;
    }
    else if (object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3)
    {
      object->weapon_transforms_flak_cannon_front[0] = inst;
      object->weapon_transforms_flak_cannon_back[0] = inst_nokick;


      flashx = 27.0;
      flashy = -120.0 - levelup_interp*150;
      flashz = -250.0;
      y_curve = 0.13;

      if (th_time() > object->spawned_shell_time && object->spawned_shell_id != -1 && object->kickback_velocity < 0 && object->kickback > 0)
      {
        if (set_shell_transform)
        {
          object->levelstate->shotbrass->transforms[object->shell_id] = fn_makescale(fn_createVec3(0,0,0));
        }

        fn_vec3 old_sh_pos = object->levelstate->shotbrass->entities[object->spawned_shell_id].aabb.position;

        fn_vec3 delta_sh = fn_subVec3(shpos,object->spawn_shell_pos);
        object->levelstate->shotbrass->entities[object->spawned_shell_id].aabb.position = fn_addVec3(old_sh_pos,delta_sh);



        object->spawn_shell_pos = shpos;

         th_brassUpdateTransform(object->levelstate->shotbrass,object->spawned_shell_id);

        fn_Transform brass1 = fn_decomposeMat4(object->levelstate->shotbrass->transforms[object->spawned_shell_id]);

        fn_Transform brass2 = fn_decomposeMat4(inst_shell);


        float alpha = th_time() - object->spawned_shell_time;


        alpha = alpha/65.0;

        //printf("%f \n",alpha);

        alpha = fn_clamp(alpha,0.0,1.0);

        fn_Transform brass_interp = fn_lerpTransforms(brass2,brass1,alpha);


        object->levelstate->shotbrass->transforms[object->spawned_shell_id] = fn_transformToMat(brass_interp);

        object->shell_respawn_scale = 0.0;

        fn_vec3 delta_vel = fn_createVec3(0,-0.004,0);
        object->levelstate->shotbrass->entities[object->spawned_shell_id].velocity = fn_addVec3(object->levelstate->shotbrass->entities[object->spawned_shell_id].velocity,fn_multVec3s(delta_vel,dt));
        //if (object->kickback)
        //object->levelstate->shotbrass->transforms[object->spawned_shell_id] = inst_shell;
      }
      else if (object->spawned_shell_id != -1 && object->kickback_velocity < 0 && object->kickback > 0)
      {
        if (set_shell_transform)
        {
          object->levelstate->shotbrass->transforms[object->shell_id] = inst_shell;
          object->levelstate->shotbrass->transforms[object->spawned_shell_id] = fn_makescale(fn_createVec3(0,0,0));
        }
      }
      else
      {
        if (set_shell_transform)
        {
          object->levelstate->shotbrass->transforms[object->shell_id] = fn_multMat4(fn_makescale(fn_createVec3s(object->shell_respawn_scale)),inst_shell);

          if (object->shell_respawn_scale < 1.0)
          {
            object->shell_respawn_scale = object->shell_respawn_scale + (1.0/125.0)*dt;
            if (object->shell_respawn_scale > 1.0)
            {
              object->shell_respawn_scale = 1.0;
            }
          }
        }
      }

    }

    fn_vec3 flash_coords = fn_createVec3(flashx*scl,flashy*scl*(1.0 + mov_curve*y_curve),flashz*scl*(1.0 - mov_curve*0.1) );
    if (object->levelstate->player->level_weapon[TH_SHOTGUN]  != 3)
    {
      flash_coords = fn_addVec3(flash_coords,kick);
    }


    fn_mat4 inst_flash = fn_translaterotatescale(flash_coords,0.0,fn_createVec3(0,0,1),fn_createVec3s(1));
    inst_flash = fn_multMat4(inst_flash,fn_inverse(r_camera(dir_weapon,target_angles)));
    inst_flash = fn_scale(inst_flash,fn_createVec3(-1,-1,-1));
    if ((th_time() - object->levelstate->shotgun->time_fired) < 75)//
    {
      fn_vec3 flashpos = fn_createVec3(inst_flash.m[12],inst_flash.m[13],inst_flash.m[14]);


      th_Particle p = th_defaultParticle() ;

      float the_factor = (float)th_random()/(float)(RAND_MAX/(1.0));

      float theta2 = 3.14159*(the_factor > 0.5 ? 1.0 : 0.0);//0.7853982 + (float)th_random()/(float)(RAND_MAX/(0.5*3.14159));
      // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
      p.theta = theta2;
      p.alive = true;
      p.life = 0.1;
      p.start_time = th_time();
      p.stretch = 0;

      p.scale = object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3 ? 20.0 : 10;
      p.scale = p.scale *2.0 *scl;

      p.scale_end = p.scale;
      p.overbright = 1.01;
      p.texture_handle = object->levelstate->player->level_weapon[TH_SHOTGUN]  == 3 ? th_getParticleTexture(TH_MACHINEGUN_FLASH) : th_getParticleTexture(TH_SHOTGUN_FLASH);
      p.makedecal = false;
      p.alphascale = fn_lerp(0.7,0.1,fn_clamp((th_time() - object->levelstate->shotgun->time_fired)/75.0,0.0,1.0));
      p.alphascale_base = p.alphascale ;
      p.hasphysics = false;
      p.has_gravity = false;

      p.velocity = fn_createVec3s(0);
      p.position = flashpos;

      bool succ = th_addParticle(p);
/*
      printf("%i %f %f %f \n",succ,flashpos.x,flashpos.y,flashpos.z);*/

      //object->levelstate->plasma->lights[0].pos = fn_createVec4Vec3(fn_addVec3(flashpos,fn_multVec3s(fn_normalizeVec3(direction),100)) ,0);
    }
  }

  // }

}
