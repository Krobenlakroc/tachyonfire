#include "th_physics.h"
#include "../fn_math/fn_grid.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>
#include "../fn_game/th_builtins.h"
#include "../fn_game/th_debugger.h"
#include "th_system.h"

//#define PLAYER_DEBUG

void th_initWorld(th_World* w)
{
  w->volumes = NULL;
  w->volumecount = 0;
}


static int* pvols = NULL;
void th_getPotentialVolumes(th_Entity* e,th_CollidableVolume* outvolumes,int* outvolumecount,th_World* w,th_CollisionMemory* memory,float dt)
{
  // if (pvols == NULL)
  // {
  //     pvols = malloc(sizeof(int)*w->volumecount);
  // }
  #if 0
  int indiceCount = 0;
  th_AABB broadphase = fn_getSweptBroadphaseBox(e->aabb,fn_multVec3s(e->velocity,dt));

  // broadphase.hwidth = fn_addVec3(broadphase.hwidth,fn_createVec3(0,40,0));
  fn_getAABBSTouchv(broadphase,&w->octree,&indiceCount,memory->pvols,memory->indices);
  // for (int i = 0 ; i < w->volumecount;i++)
  // {
  //   if (fn_aabbCheck(w->aabbs[i],broadphase))
  //   {
  //     memory->pvols[indiceCount] = i;
  //     indiceCount++;
  //   }
  // }

  for (int i = 0 ; i < indiceCount;i++)
  {
    outvolumes[i] = w->volumes[memory->pvols[i]];
  }
  *outvolumecount = indiceCount;
  #endif


  #if 1
 memcpy(outvolumes,w->volumes,sizeof(th_CollidableVolume)*w->volumecount);
 *outvolumecount = w->volumecount;
 #endif
}

void th_getPotentialVolumesBPhase(fn_vec3 radius,fn_vec3 position,fn_vec3 delta,th_CollidableVolume* outvolumes,int* outvolumecount,th_World* w,th_CollisionMemory* memory)
{
  //
  int indiceCount = 0;
  th_AABB box;
  box.position = position;
  box.hwidth = radius;
  box.mode = BOX;
  th_AABB broadphase = fn_getSweptBroadphaseBox(box,delta);
  broadphase.hwidth = fn_addVec3(broadphase.hwidth,fn_createVec3s(10));

  fn_getAABBSTouchv(broadphase,&w->octree,&indiceCount,memory->pvols,memory->indices);

  for (int i = 0 ; i < indiceCount;i++)
  {
    outvolumes[i] = w->volumes[memory->pvols[i]];
  }
  *outvolumecount = indiceCount;
  // memcpy(outvolumes,w->volumes,sizeof(th_CollidableVolume)*w->volumecount);
  // *outvolumecount = w->volumecount;
}





#define	STOP_EPSILON	0.0001



#define STEPSIZE 10





void entity_check_collision(th_Entity *entity,th_CollisionMemory* memory,th_World* w)
{

  int volumecount;
  fn_vec3 pos_r3 = fn_multVec3(entity->packet.e_base_point,entity->radius);
  fn_vec3 vel_r3 = fn_multVec3(entity->packet.e_velocity,entity->radius);
  th_getPotentialVolumesBPhase(entity->radius,pos_r3,vel_r3,memory->volumes,&volumecount,w,memory);

  for (int i = 0; i < volumecount; i++) {
    th_collideTriangle(&entity->packet,&memory->volumes[i]);
  }
}

void entity_check_grounded(th_Entity *entity)
{
  if (!entity->packet.found_collision)
    return;

  fn_vec3 axis = fn_createVec3(0,-1,0);

  fn_vec3 a, b, c;
  a = fn_multVec3(entity->packet.a, entity->radius);
  b = fn_multVec3(entity->packet.b, entity->radius);
  c = fn_multVec3(entity->packet.c, entity->radius);
  th_Plane plane = th_makePlaneTriangle(a, b, c);
  float f = fn_dot(plane.normal, axis);

  if (f >= 0.99)
    entity->grounded = 1;
}

void collide_with_world(th_Entity *entity, fn_vec3* e_position, fn_vec3* e_velocity,th_CollisionMemory* memory,th_World* w)
{
  th_Plane first_plane;
  fn_vec3 dest;
  dest = fn_addVec3(*e_position, *e_velocity);


  // check for collision
  fn_vec3 temp;

  for (int i=0; i<3; i++) {

    // setup coll packet
    temp = fn_normalizeVec3(*e_velocity);
    entity->packet.e_norm_velocity = temp;
    entity->packet.e_velocity = fn_addVec3(*e_velocity,fn_multVec3s(temp,0.001));
    entity->packet.e_base_point = *e_position;
    entity->packet.e_radius = entity->radius;
    entity->packet.found_collision = 0;
    entity->packet.nearest_distance = FLT_MAX;
    entity->packet.t = 0.0f;

    // check for collision
    entity_check_collision(entity,memory,w);

    entity_check_grounded(entity);



    // no collision move along
    static int lframe = 0;
    if (entity->packet.found_collision == 0  ) {


      *e_position = fn_addVec3(*e_position, *e_velocity);
      return;
    }




    float very_close_distance = 0.0005;// 0.0005;
    // point touching tri
    fn_vec3 touch_point = *e_position;
    // if (entity->packet.nearest_distance > very_close_distance)
    // {
      temp = fn_multVec3s( *e_velocity, fn_clamp(entity->packet.t - 0.03125f,0.0,1.0));
      touch_point = fn_addVec3( *e_position, temp);

      float dist = fn_length(*e_velocity) * fn_clamp(entity->packet.t,0.0,1.0);
      float short_dist = fmax(dist - (very_close_distance ), 0.0f);

      temp = fn_normalizeVec3( *e_velocity);
      temp = fn_multVec3s( temp, short_dist);

      //
       fn_vec3 old_p = *e_position;
    //  *e_position = fn_addVec3( *e_position, temp);
    float scale_factor = 0.03125f;
    //0.03125f*
    fn_vec3 choice_a = fn_addVec3(old_p,fn_multVec3s(*e_velocity,fn_clamp(entity->packet.t - scale_factor,0,1)));

    fn_vec3 choice_b = fn_addVec3( old_p, temp);

    // if (fn_length2(fn_subVec3(choice_b,old_p)) < fn_length2(fn_subVec3(choice_a,old_p)))
    // {
    //   *e_position = choice_b;
    // }
    // else
    // {
      *e_position = choice_a;

      entity->collided = true;//entity->packet.found_collision;
      if(entity->packet.found_collision)
      {
        entity->collision_normal = th_makePlaneTriangle(fn_multVec3(entity->packet.a,entity->radius), fn_multVec3(entity->packet.b,entity->radius), fn_multVec3(entity->packet.c,entity->radius)).normal;
        entity->collision_position = fn_multVec3(choice_a,entity->radius);
      }

      if (entity->mode == TH_IMPACT_MODE)
      {


        return;
      }
  //  }

   // }
   // else
   // {

   if (entity->type == TH_PLAYER_ENTITY)
   {
    //  th_printlnDevConsole("%.9g",entity->packet.t );
    // th_printlnDevConsole("%f %f %f",entity->packet.plane.normal.x,entity->packet.plane.normal.y,entity->packet.plane.normal.z );
    // th_getDefaultDebugger()->points_default[i] = fn_multVec3(entity->packet.intersect_point,entity->packet.e_radius);
   }


   // }
   if ( i == 2)
   {

   }

    //
    // if (entity->packet.nearest_distance <= 0)
    // {
    //   // temp = fn_multVec3s( *e_velocity, (entity->packet.t - 0.03125f));
    //   // *e_position = fn_addVec3( *e_position, temp);
    //   th_printlnDevConsole("STUCK %f %i",fn_length(*e_velocity),i);
    // }




    // calculate sliding plane
    fn_vec3 slide_plane_origin, slide_plane_normal;

    // use intersect point as origin
    slide_plane_origin = entity->packet.intersect_point;

    // normal = touch_point - intersect_point
    // dont use normal from packet.plane.normal!
    slide_plane_normal = fn_subVec3(touch_point, entity->packet.intersect_point);
    slide_plane_normal = fn_normalizeVec3(slide_plane_normal);



    // double alt_t = (1.00 + FLT_EPSILON + fn_dot(slide_plane_origin,slide_plane_normal) - fn_dot(old_p,slide_plane_normal))/(fn_dot(*e_velocity,slide_plane_normal));
     // if (fn_dot(*e_velocity,slide_plane_normal) == 0.0)
     // {
     //   alt_t = 0.0;
     // }
  //   *e_position = old_p;//fn_addVec3(old_p,fn_multVec3s(*e_velocity,fn_clamp(alt_t - 1.0,0,1)));
    // if (signed_dist_to_plane < 1.0)
    // {
    //
    //   *e_position = fn_addVec3(*e_position,fn_multVec3s(slide_plane_normal,1.0005 - signed_dist_to_plane));
     // double signed_dist_to_plane =  fn_dot(*e_position,slide_plane_normal) - fn_dot(entity->packet.intersect_point,slide_plane_normal);
     // if (signed_dist_to_plane < 1.0)
     // {
     //     th_printlnDevConsole("%f %i",signed_dist_to_plane,i );
     // }

    // }
    // if (entity->packet.nearest_distance <= 0)
    // {
    //   // temp = fn_multVec3s( *e_velocity, (entity->packet.t - 0.03125f));
    //   // *e_position = fn_addVec3( *e_position, temp);
    //   th_printlnDevConsole("STUCK %f %f %f",slide_plane_normal.x,slide_plane_normal.y,slide_plane_normal.z);
    // }
    if (isnan(slide_plane_normal.x) || isnan(slide_plane_normal.y) || isnan(slide_plane_normal.z))
    {
       th_printlnDevConsole("ERROR" );
    }


  fn_vec3 axis = fn_createVec3(0,-1,0);
  float f = fn_dot(slide_plane_normal, axis);

    if (f >= 0.4)
      entity->grounded = 1;

    if (f >= 0.95)
      entity->can_jump = 1;

    bool docrease = false;
    if ( i ==  1)
    {
      float angle = fn_dot(slide_plane_normal,first_plane.normal);
      // printf("OLD NORMAL ANGLE%f\n",angle );

      if (angle <= 0 ) //> 90.0001
      {

        // printf("%f\n",angle );
        docrease = true;
      }
    }

    if (i == 2)
    {
      return;
    }



    if (i == 0 || !docrease) {
       float long_radius = 1.0 + very_close_distance;
      //
       first_plane = th_makePlane(slide_plane_origin, slide_plane_normal);


      float dist_to_plane  = th_signedDistanceToPlane(dest, &first_plane) - long_radius;


      temp  = fn_multVec3s(first_plane.normal, dist_to_plane);
      dest = fn_subVec3(dest,temp);

       *e_velocity = fn_subVec3(dest,*e_position);
       //ClipVelocity (e_velocity, &entity->packet.plane.normal, e_velocity, 1.0f);
      //  dest = fn_addVec3( *e_position, *e_velocity);
    } else if (i >= 1 && docrease) {
      th_Plane second_plane = th_makePlane(slide_plane_origin, slide_plane_normal);

      fn_vec3 crease;
      crease = fn_normalizeVec3(fn_cross( second_plane.normal,first_plane.normal));

      temp = fn_subVec3( dest, *e_position);
      float dis = fn_dot(temp, crease);
    //  crease = fn_normalizeVec3(crease);

      *e_velocity = fn_multVec3s(crease, dis);
      dest = fn_addVec3( *e_position, *e_velocity);

      first_plane = second_plane;
      // fn_vec3 dir = fn_normalizeVec3(fn_cross(planes[0].normal,planes[1].normal));//potential problem
      // // if (fn_almostequalVec3(planes[0].normal,planes[1].normal,0.001f))
      // // {
      // //   printf("%s\n","PROBLEM" );
      // // }
      // float d = fn_dot (dir, e->velocity);
      // e->velocity = fn_multVec3s(dir,d);
    }
  }

  //*e_position =  dest;
}

void collide_and_slide(th_Entity *entity,th_CollisionMemory* memory,th_World* w)
{
  entity->packet.r3_position = entity->aabb.position;//, sizeof(fn_vec3));
  entity->packet.r3_velocity = entity->velocity;//, sizeof(fn_vec3));
  entity->packet.e_radius =    entity->radius;//,   sizeof(fn_vec3));
  entity->packet.robust = entity->robust_collisions;
  // y velocity in a seperate pass
  fn_vec3 gravity = fn_createVec3(0,0,0);
  if (entity->mode == TH_SLIDE_MODE && !entity->slide_no_gravity_step)
  {
    gravity.y = entity->packet.r3_velocity.y;
    entity->packet.r3_velocity.y = 0.0f;
  }


  // lets get e-spacey?
  fn_vec3 e_position, e_velocity, final_position;
  e_position = fn_divVec3(entity->packet.r3_position, entity->packet.e_radius);
  e_velocity = fn_divVec3(entity->packet.r3_velocity, entity->packet.e_radius);

  // do velocity iteration
  collide_with_world(entity, &e_position, &e_velocity,memory,w);

  // if ( entity->type == TH_PLAYER_ENTITY && entity->packet.found_collision)
  // {
  //   th_printlnDevConsole("%f %f %f\n",entity->aabb.position.x,entity->aabb.position.y,entity->aabb.position.z);
  // }

  // do gravity iteration
  if (entity->mode == TH_SLIDE_MODE && !entity->slide_no_gravity_step)
  {
    entity->packet.r3_velocity = gravity;
    e_velocity = fn_divVec3( gravity, entity->packet.e_radius);
    collide_with_world(entity, &e_position, &e_velocity,memory,w);
  }

  // finally set entity position & velocity
  entity->aabb.position = fn_multVec3(e_position, entity->packet.e_radius);
}


//
// static th_CollidableVolume* cvols = NULL;
void th_updateEntity(th_Entity* e,th_World* w,float timeleft,th_CollisionMemory* memory)
{
  int volumecount = 0;
  e->radius = e->aabb.hwidth;

  e->grounded = 0;
  e->can_jump = 0;
  e->collided = false;
  //}


  // e->grounded = false;
  timeleft = timeleft / 1.0;
  //  th_printlnDevConsole("%f",timeleft);
  if (timeleft < 0.0001)
  {
    return;
  }



  e->velocity = fn_multVec3s(e->velocity, timeleft);
  for (int i=0; i<1; i++)
    collide_and_slide(e, memory,w);
  e->velocity = fn_subVec3( e->aabb.position, e->packet.r3_position);
  e->velocity = fn_multVec3s( e->velocity, 1.0 / timeleft);





}




void Accelerate(th_Entity* entity,fn_vec3 wishdir, float wishspeed, float accel,float t)
{
  float addspeed;
  float accelspeed;
  float currentspeed;

  currentspeed = fn_dot(entity->velocity, wishdir);
  addspeed = wishspeed - currentspeed;
  if(addspeed <= 0)
  return;
  accelspeed = accel  * wishspeed * t; //delta time dt mult
  if(accelspeed > addspeed)
  accelspeed = addspeed;

  entity->velocity.x += accelspeed * wishdir.x;
  entity->velocity.z += accelspeed * wishdir.z;
}



// float friction = 0.02f;
// const float gravity = 0.001;
// float jumpSpeed = -0.4;
// float runAcceleration = 0.02;
// float runDeacceleration = 0.01;
// float moveSpeed = 0.75;
// float sideStrafeSpeed = 1;
// float sideStrafeAcceleration = 0.13;
// float airDecceleration =0.0005;
// float airAcceleration = 0.0005;

void applyFriction(th_Entity* entity,float t,float t2,th_PlayerDefs pdefs)
{


  fn_vec3 vec = entity->velocity; // Equivalent to: VectorCopy();

  float speed;
  float newspeed;
  float control;
  float drop;

  vec.y = 0.0f;
  speed = fn_length(vec);
  drop = 0.0f;

  /* Only if the player is on the ground then apply friction */
  if(entity->grounded)
  {
      control = speed < pdefs.runDeacceleration ? pdefs.runDeacceleration : speed;
      drop = control * pdefs.friction * t*t2  ;//mult by dt
  }

  newspeed = speed - drop;
  //playerFriction = newspeed;
  if(newspeed < 0)
      newspeed = 0;
  if(speed > 0)
      newspeed /= speed;

  entity->velocity.x *= newspeed;
  // playerVelocity.y *= newspeed;
  entity->velocity.z *= newspeed;
}

void applyFrictionOmni(th_Entity* entity,float t,float t2,th_PlayerDefs pdefs)
{


  fn_vec3 vec = entity->velocity; // Equivalent to: VectorCopy();

  float speed;
  float newspeed;
  float control;
  float drop;

  //vec.y = 0.0f;
  speed = fn_length(vec);
  drop = 0.0f;


  control = speed < pdefs.runDeacceleration ? pdefs.runDeacceleration : speed;
  drop = control * pdefs.friction * t*t2  ;//mult by dt


  newspeed = speed - drop;
  //playerFriction = newspeed;
  if(newspeed < 0)
    newspeed = 0;
  if(speed > 0)
    newspeed /= speed;

  entity->velocity.x *= newspeed;
  entity->velocity.y *= newspeed;
  entity->velocity.z *= newspeed;
}


void GroundMove(th_Entity* entity,fn_vec3 direction,bool wishJump,float t,th_PlayerDefs pdefs)
{

  fn_vec3 wishdir;

  wishdir = direction;
  wishdir = fn_normalizeVec3(wishdir);


  float wishspeed = fn_length(wishdir);
  wishspeed *= pdefs.moveSpeed;//*fn_max(0.0, 1- fn_dot(entity->ground_normal,fn_normalizeVec3(wishdir)));
//  printf("%f\n",( 1- fn_dot(entity->collision_normal,fn_normalizeVec3(wishdir))) );

  Accelerate(entity,wishdir, wishspeed, pdefs.runAcceleration,t);

  //entity->velocity.y = 0;
  // printf("%f\n",fn_max(0.0, 1- fn_dot(entity->ground_normal,fn_normalizeVec3(wishdir))) );
  if(wishJump)
  {
    fn_vec3 grnd = entity->collision_normal;

     entity->velocity.y = pdefs.jumpSpeed;//*fn_max(0.0, 1- fn_dot(entity->ground_normal,fn_normalizeVec3(wishdir)));

     entity->velocity.x += pdefs.jumpSpeed*-fn_clamp(grnd.x,-0.1,0.1);
     entity->velocity.z += pdefs.jumpSpeed*-fn_clamp(grnd.z,-0.1,0.1);
  }
}

void AirMove(th_Entity* entity,fn_vec3 direction,bool fwd,bool side,float t,th_PlayerDefs pdefs)
{
  fn_vec3 wishdir = direction;

  float accel;

  float scale = 1.f;//CmdScale();

  float wishspeed = fn_length(wishdir);
  wishspeed *= pdefs.moveSpeed;

  wishdir = fn_normalizeVec3(wishdir);

  wishspeed *= scale;



  if (fn_dot(entity->velocity, wishdir) < 0)
  accel = pdefs.airDecceleration;
  else
  accel = pdefs.airAcceleration;

  if(!fwd && side)
  {
    if(wishspeed > pdefs.sideStrafeSpeed)
    wishspeed = pdefs.sideStrafeSpeed;
    accel = pdefs.sideStrafeAcceleration;
  }

  Accelerate(entity,wishdir, wishspeed, accel,t);


}


void th_updatePlayer(th_Entity* e,th_World* w,fn_vec3 direction,bool jumping,float t,th_PlayerDefs pdefs,th_CollisionMemory* memory,bool flying,fn_vec3 look)
{
  // printf("%s\n","Playerupdate" );
  bool flymode = flying;

  if (flymode)
  {
    if (direction.x != 0.0 || direction.z !=0){

        direction.y = look.y;
          e->velocity = fn_multVec3s(fn_normalizeVec3(direction),1);
    }
    else {
      e->velocity = fn_createVec3s(0);
    }

  }
  else {
    if ((e->grounded && !fn_equalVec3(direction,fn_createVec3(0,0,0)) )|| (jumping && e->grounded))
    {
      GroundMove(e,direction,jumping && e->grounded,t,pdefs);
    }
    else
    {
      AirMove(e,direction,true,false,t,pdefs);
    }
    if (e->grounded)
    {

      if (!jumping)
      {
        applyFriction(e,1.0,t,pdefs);
      }
      else
      {
        applyFriction(e,0.1f,t,pdefs);
      }

    }

    e->velocity.y += pdefs.gravity*t;

  }

  // fn_printVec3(direction);
  // fn_printVec3(e->velocity);
  th_updateEntity(e,w,t,memory);
  // fn_printVec3(e->velocity);


}

static th_timer_t slowmo_time = 0.0;
static th_timer_t control_time = 0.0;

th_timer_t th_get_slide_timer()
{
  return slowmo_time;
}

bool th_updatePlayerSlide(th_Entity* e,th_World* w,fn_vec3 direction,bool jumping,float t,th_PlayerDefs pdefs,th_CollisionMemory* memory,bool flying,fn_vec3 look,fn_vec3 normal,bool slide)
{
  float old_y = e->velocity.y;
  bool flymode = flying;

  if (flymode)
  {
    if (direction.x != 0.0 || direction.z !=0){

      direction.y = look.y;
      e->velocity = fn_multVec3s(fn_normalizeVec3(direction),1);
    }
    else {
      e->velocity = fn_createVec3s(0);
    }

  }
  else {
    if (e->grounded && slide  && !fn_equalVec3(direction,fn_createVec3(0,0,0)) && th_time() < control_time + 1000)
    {
      float control = (th_time() - control_time)/1000.0;
      control = 1.0 - control;

      float accel = fn_lerp(0.009,0.02,control);


      fn_vec3 wishdir;

      wishdir = direction;
      wishdir = fn_normalizeVec3(wishdir);


      float wishspeed = 1.0;
      wishspeed *= fn_length(fn_multVec3(e->velocity,fn_createVec3(1,0,1)));//*fn_max(0.0, 1- fn_dot(entity->ground_normal,fn_normalizeVec3(wishdir)));
    //  printf("%f\n",( 1- fn_dot(entity->collision_normal,fn_normalizeVec3(wishdir))) );
      float old_vel = fn_length(e->velocity);
      Accelerate(e,wishdir, wishspeed, accel,t);
      e->velocity = fn_multVec3s(fn_normalizeVec3(e->velocity),old_vel);
    }
    if (((e->grounded && !fn_equalVec3(direction,fn_createVec3(0,0,0)) )|| (jumping && e->can_jump)) && !(e->grounded && slide))
    {
      GroundMove(e,direction,jumping && e->can_jump,t,pdefs);
    }
    else //if (!(e->grounded && slide))
    {
      AirMove(e,direction,true,false,t,pdefs);
    }
    if (e->grounded)
    {
      if ( slide)
      {
        applyFriction(e,0.05f,t,pdefs);
      }
      else if (!(jumping && e->can_jump) )
      {
        applyFriction(e,1.0,t,pdefs);
      }
      else
      {
        applyFriction(e,0.1f,t,pdefs);
      }

    }

    e->velocity.y += pdefs.gravity*t;

  }

  // fn_printVec3(direction);
  // fn_printVec3(e->velocity);
  bool slide_timer_override = old_y > 0.3 && !e->grounded && fn_length(e->velocity) > 0.7;
  bool canslide = true;

  bool canslide_small = false;//old_y > 0.1 && !e->grounded && fn_length(e->velocity) > 0.2;
  // printf("%f\n",fn_length(e->velocity) );
  float mag = fn_clamp(fabs(e->velocity.y)*10,0,1.2);


  fn_vec3 evel_original = e->velocity;
  fn_vec3 epos_original = e->aabb.position;
  fn_vec3 gravity = fn_createVec3(0,e->velocity.y,0);

  // if (slide)
  // {
   //e->slide_no_gravity_step = true;
  // }
  // else
  // {
  //   e->slide_no_gravity_step = false;
  // }

  float vel_len = fn_length(e->velocity);

  //printf("%f\n",vel_len);
  th_updateEntity(e,w,t,memory);

  if (slide && e->collided)
  {
    float new_len = fn_length(e->velocity);

    if (new_len > 0.01)
    {

      // fn_vec3 sdir = e->velocity;
      //
      // fn_vec3 normal_slide_plane = e->collision_normal;
      // float allignment = fn_dot(normal_slide_plane,sdir);
      //
      //
      // sdir = fn_subVec3(sdir , fn_multVec3s(normal_slide_plane,allignment));

      if (fn_length2(e->velocity) > 0.01*0.01)
      {
        //sdir = fn_normalizeVec3(sdir);

         //e->velocity = sdir;//fn_multVec3s(fn_normalizeVec3(sdir),new_len);//fn_multVec3s(fn_normalizeVec3(e->velocity),vel_len);

         if (e->grounded)
         {
           e->velocity = fn_multVec3s(fn_normalizeVec3(e->velocity),vel_len);
         }
         // else
         // {
         //  applyFrictionOmni(e,0.02f,t,pdefs);
         // }


      }


    }
  }







  bool slide_started = false;
  if (e->grounded && slide )
  {
    fn_vec3 sdir = normal;
    if (!fn_equalVec3(direction,fn_createVec3(0,0,0)))
    {
      sdir = direction;
    }

    // fn_vec3 normal_slide_plane = e->collision_normal;
    // float allignment = fn_dot(normal_slide_plane,sdir);
    //
    //
    // sdir = fn_subVec3(sdir , fn_multVec3s(normal_slide_plane,allignment));
    //
    // if (fn_length2(sdir) > 0.01*0.01)
    // {
    //   sdir = fn_normalizeVec3(sdir);
    // }

    if (canslide && (th_time() > slowmo_time + 1500 ) )
    {
      slide_started = true;
      slowmo_time = th_time();
      control_time = th_time();
      // printf("%s\n","Slide" );
      th_setGameplayTimeScale(fn_createVec3(0.3,0.00000,0.0000002));//2.55

      fn_vec3 hvelocity = fn_multVec3(e->velocity,fn_createVec3(1.0,0.0,1.0));
      float hvdotsdir = fn_dot(fn_normalizeVec3(hvelocity),fn_normalizeVec3(sdir));

      float vlen_boost = fn_dot(hvelocity,hvelocity)*hvdotsdir*hvdotsdir > 2.75*2.75 ? fn_length(hvelocity)*hvdotsdir : 2.75;

      fn_vec3 slide_amount = fn_multVec3s(fn_normalizeVec3(sdir),vlen_boost);//2.75
      //3.0
      e->velocity.x = 0;
      e->velocity.z = 0;
      e->velocity = fn_addVec3(e->velocity,slide_amount);
    }
    else if ((slide_timer_override) )
    {
      //slide_started = true;
      control_time = th_time();
      th_setGameplayTimeScale(fn_createVec3(0.3,0.00000,0.0000002));
    }
    else if (canslide_small)
    {
      slide_started = true;
      // slowmo_time = th_time();
      fn_vec3 slide_amount = fn_multVec3s(fn_normalizeVec3(sdir),0.5);
      e->velocity = fn_addVec3(e->velocity,slide_amount);
    }

  }

  return slide_started;
}

fn_vec3 th_traceVolume(th_World* w,fn_vec3 start,fn_vec3 end,float radius,fn_vec3* normal,bool* is_hit,th_CollisionMemory* memory)
{
  th_Entity tracer = TH_DEFAULT_ENTITY;
  tracer.aabb.position = start;
  tracer.aabb.hwidth = fn_createVec3(radius,radius,radius);
  tracer.radius = fn_createVec3(radius,radius,radius);
  tracer.velocity = fn_subVec3(end,start);
  tracer.grounded = false;
  tracer.collided = false;
  tracer.aabb.mode = SPHERE;
  tracer.mode = TH_IMPACT_MODE;

  th_updateEntity(&tracer,w,1.0,memory);

  if (is_hit != NULL)
  {
    *is_hit = tracer.collided;
  }

  if (normal != NULL)
  {
    *normal = tracer.collision_normal;
  }

  if (is_hit != NULL)
  {
    *is_hit = tracer.collided;
  }

  if (tracer.collided)
  {
    return tracer.collision_position;
  }

  return end;

}

bool th_checkCollisionWorld(th_World* w,th_Entity* e,th_CollisionMemory* memory)
{
  int volumecount;
  fn_vec3 pos_r3 = e->aabb.position;
  fn_vec3 vel_r3 = fn_createVec3s(0);
  th_getPotentialVolumesBPhase(e->aabb.hwidth,pos_r3,vel_r3,memory->volumes,&volumecount,w,memory);


  for (int i = 0; i < volumecount; i++) {

    if (th_box_tri_intersect(e->aabb.position, e->aabb.hwidth, memory->volumes[i].points[0], memory->volumes[i].points[1], memory->volumes[i].points[2]))
    {
        return true;
    }

  }

  return false;
}


fn_vec3 th_trace(th_World* w,fn_vec3 start,fn_vec3 end,fn_vec3* normal,bool* is_hit,th_CollisionMemory* memory,float* out_t)
{
  int volumecount;
  fn_vec3 pos_r3 = start;
  fn_vec3 vel_r3 = fn_subVec3(end,start);


  th_getPotentialVolumesBPhase(fn_createVec3(0.1,0.1,0.1),pos_r3,vel_r3,memory->volumes,&volumecount,w,memory);


   float dist = FLT_MAX;
   int index = -1;
   fn_vec3 intersect;
  for (int i = 0; i < volumecount; i++) {
    // fn_vec3 n = th_makePlaneTriangle(memory->volumes[i].points[0], memory->volumes[i].points[1], memory->volumes[i].points[2]).normal;
    // if (fn_dot(fn_normalizeVec3(fn_subVec3(end,start)),n ) < 0)
    // {
    //   continue;
    // }


    if (th_ray_in_tri(start, fn_subVec3(end,start), memory->volumes[i].points[0], memory->volumes[i].points[1], memory->volumes[i].points[2], &intersect))
    {
      float len = fn_length2(fn_subVec3(intersect,start));
      if (len < dist)
      {
        dist = len;
        index = i;
      }
    }

  }

  if (dist < FLT_MAX && index != -1)
  {
    if (is_hit != NULL)
    {
      *is_hit = true;
    }

    if (normal != NULL)
    {
      *normal = th_makePlaneTriangle(memory->volumes[index].points[0], memory->volumes[index].points[1], memory->volumes[index].points[2]).normal;
    }

    if (out_t != NULL)
    {
        *out_t = sqrtf(fn_length2(fn_subVec3(intersect,start))/fn_length2(fn_subVec3(end,start)));
    }


    return intersect;
  }
  else
  {
    if (is_hit != NULL)
    {
      *is_hit = false;
    }
    if (out_t != NULL)
    {
      *out_t = 1.0;
    }
    if (normal != NULL)
    {
      *normal = fn_createVec3(0,0,0);
    }
    return end;
  }
}




void th_allocatePhysicsMemory(th_Allocator* alloc,th_World* w,int threads)
{
  w->phys_mem = th_alloc(alloc,sizeof(th_CollisionMemory)*threads);

  for (int i = 0 ; i < threads;i++)
  {
    w->phys_mem[i].indices = th_alloc(alloc,sizeof(char)*w->octree.aabbCount);
    w->phys_mem[i].pvols = th_alloc(alloc,sizeof(int)*w->volumecount);
    w->phys_mem[i].volumes = th_alloc(alloc,sizeof(th_CollidableVolume)*w->volumecount);
  }
}

th_CollisionMemory* th_getPhysicsMemory(th_World* w,int thread)
{
  return &w->phys_mem[thread];
}

void th_allocateEntityPointers(th_Allocator* alloc,th_World* w,int threads)
{
  w->eptrs = th_alloc(alloc,sizeof(th_Entity**)*threads);

  for (int i = 0; i < threads; i++) {
    w->eptrs[i] = th_alloc(alloc,sizeof(th_Entity*)*TH_MAX_ENTITY_PTRS);
  }
}

th_Entity** th_getEntityPointers(th_World* w,int thread)
{
  return w->eptrs[thread];
}
