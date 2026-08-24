#include "th_horse.h"
#include "../fn_engine/th_time.h"
#include "../fn_engine/th_system.h"

#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_engine/th_level.h"
#include "../fn_engine/th_globals.h"
#include "th_debugger.h"

#include "th_splineutils.h"
#include "th_builtins.h"
#include "th_eyeball.h"
#include "../fn_engine/th_occlusion.h"
#include "../fn_engine/th_splinegen.h"
#include "../fn_engine/th_renderer.h"

#include "../fn_engine/th_hitmarker.h"

static const float STUN_TIME_HORSE = 750;
static const float HORSE_STUN_MAG = 0.65;
static const float HORSE_STUN_ALPHA = 0.8;

static const bool ENABLE_SPAWNING = true;
static const float CHILD_SPAWN_INTERVAL = 15000;
static const float INITIAL_SPAWN_INTERVAL = 2000;
static const float fadeout_decay_rate = 0.002;
static const float fadeout_init = 3.0;


//yeah this is pretty bad.....
//flip flip between the two arays, ternary picks the pointer, then deref so we can use as lvalue
#define LEG_ASSIGNMENT(legidx) (*((((legidx) & 1) == 0) ? &(c->transforms_legs_upper[(legidx)>>1]) : &(c->transforms_legs_lower[(legidx)>>1])))


static void positionHatch(th_HorseGroup* c,int i)
{
  fn_mat4 tr_back = fn_maketranslate(fn_multVec3s(c->door_hinge,-1.0));
  //(sinf(th_time()*0.001*4) + 1.0)
  fn_mat4 rot_trap = fn_makerotate(fn_radians(75.0)*c->hatch_interp[i],fn_createVec3(-1,0,0));

  fn_mat4 tr_forward = fn_maketranslate(fn_multVec3s(c->door_hinge,1.0));

  fn_mat4 trapdoor = fn_multMat4(fn_multMat4(tr_back,rot_trap),tr_forward);

  trapdoor = fn_multMat4(trapdoor,c->transforms[i]);

  c->hatch_transforms[i] = trapdoor;//c->transforms_cols[i*3 + j];

}

//https://github.com/TheComet/ik
void solveJointPosition(fn_vec3 to_target_in,fn_vec3* nodes,float distance_to_parent)
{

  // struct ik_node_t* node_tip;
  // struct ik_node_t* node_mid;
  // struct ik_node_t* node_base;

  fn_vec3 node_tip = nodes[2];
  fn_vec3 node_mid = nodes[1];
  fn_vec3 node_base = nodes[0];


  float a, b, c, aa, bb, cc;

  // assert(chain_length(chain) > 2);
  // node_tip  = chain_get_node(chain, 0);
  // node_mid  = chain_get_node(chain, 1);
  // node_base = chain_get_node(chain, 2);

  // assert(node_tip->effector != NULL);
  // to_target = node_tip->effector->target_position;
  // ik_vec3_static_sub_vec3(to_target.f, node_base->position.f);
  fn_vec3 to_target = fn_subVec3(to_target_in,node_base);

  /*
  * Form a triangle from the two segment lengths so we can calculate the
  * angles. Here's some visual help.
  *
  *   target *--.__  a
  *           \     --.___ (unknown position, needs solving)
  *            \      _-
  *           c \   _-
  *              \-    b
  *            base
  *
  */
  a = distance_to_parent;//node_tip->dist_to_parent;
  b = distance_to_parent;//node_mid->dist_to_parent;
  aa = a*a;
  bb = b*b;
  cc = fn_length2(to_target);
  c = sqrt(cc);

  /* check if in reach */
  if (c < a + b)
  {
    /* Cosine law to get base angle (alpha) */
    fn_quat alpha_rotation;
    float alpha = acos((bb + cc - aa) / (2.0 * distance_to_parent * sqrt(cc)));
    float cos_a = cos(alpha * 0.5);
    float sin_a = sin(alpha * 0.5);

    /* Cross product of both segment vectors defines axis of rotation */
    alpha_rotation.xyz = node_tip;
    alpha_rotation.xyz = fn_subVec3(alpha_rotation.xyz, node_mid);  /* top segment */
    node_mid = fn_subVec3(node_mid, node_base);  /* bottom segment */
    alpha_rotation.xyz = fn_cross(alpha_rotation.xyz, node_mid);

    /*
    * Set up quaternion describing the rotation of alpha. Need to
    * normalise vec3 component of quaternion so rotation is correct.
    */
    alpha_rotation.xyz = fn_normalizeVec3(alpha_rotation.xyz);
    alpha_rotation.xyz = fn_multVec3s(alpha_rotation.xyz, sin_a);
    alpha_rotation.w = cos_a;

    /* Rotate side c and scale to length of side b to get the unknown position */
    node_mid = to_target;
    node_mid = fn_normalizeVec3(node_mid);
    node_mid = fn_multVec3s(node_mid, distance_to_parent);
    // ik_vec3_static_rotate(node_mid->position.f, alpha_rotation.f);
    node_mid = fn_rotatePointQuat(node_mid,alpha_rotation);
    node_mid = fn_addVec3(node_mid, node_base);

    node_tip = to_target_in;
  }
  else
  {
    /* Just point both segments at target */
    to_target = fn_normalizeVec3(to_target);
    node_mid = to_target;
    node_tip = to_target;
    node_mid = fn_multVec3s(node_mid, distance_to_parent);
    node_tip = fn_multVec3s(node_tip, distance_to_parent);
    node_mid = fn_addVec3(node_mid, node_base);
    node_tip = fn_addVec3(node_tip, node_mid);
  }

  nodes[2] = node_tip;
  nodes[1] = node_mid;
  nodes[0] = node_base;

}

void findBestFitPlane(fn_vec3* points, int numPoints,fn_vec3* outNormal,fn_vec3* outPosition) {
    float sumX = 0.0, sumY = 0.0, sumZ = 0.0;
    float sumXY = 0.0, sumXZ = 0.0, sumYZ = 0.0;
    float sumXX = 0.0, sumYY = 0.0, sumZZ = 0.0;
    fn_vec3 avg = fn_createVec3(0,0,0);
    for (int i = 0; i < numPoints; i++) {
        avg = fn_addVec3(avg,points[i]);
        sumX += points[i].x;
        sumY += points[i].y;
        sumZ += points[i].z;
        sumXY += points[i].x * points[i].y;
        sumXZ += points[i].x * points[i].z;
        sumYZ += points[i].y * points[i].z;
        sumXX += points[i].x * points[i].x;
        sumYY += points[i].y * points[i].y;
        sumZZ += points[i].z * points[i].z;
    }
    avg = fn_multVec3s(avg,1.0/((float)numPoints));

    float n = (float)numPoints;
    float detX = sumYY * sumZZ - sumYZ * sumYZ;
    float detY = sumXX * sumZZ - sumXZ * sumXZ;
    float detZ = sumXX * sumYY - sumXY * sumXY;

    float det = sumX * sumX * detX - 2.0 * sumX * sumY * sumXZ + sumY * sumY * detY
        - 2.0 * sumX * sumZ * sumXY - 2.0 * sumY * sumZ * sumYZ + sumZ * sumZ * detZ;

    float a = (sumX * (sumYY * sumZZ - sumYZ * sumYZ) - sumY * (sumXY * sumZZ - sumXZ * sumYZ)
        + sumZ * (sumXY * sumYZ - sumXZ * sumYY)) / det;
    float b = ((sumYY * sumZZ - sumYZ * sumYZ) * sumY - (sumXY * sumZZ - sumXZ * sumYZ) * sumX
        + (sumXY * sumYZ - sumXZ * sumYY) * sumZ) / det;
    float c = ((sumXY * sumZZ - sumXZ * sumYZ) * sumY - (sumXX * sumZZ - sumXZ * sumXZ) * sumZ
        + (sumXX * sumYZ - sumXY * sumXZ) * sumX) / det;
    float d = ((sumX * sumX * (sumYY * sumZZ - sumYZ * sumYZ) - sumY * (sumXY * sumZZ - sumXZ * sumYZ) * sumX
        + sumZ * (sumXY * sumYZ - sumXZ * sumYY) * sumX) / det) - sumX / n;

    float length = sqrt(a * a + b * b + c * c);
    fn_vec3 normal = fn_createVec3(a / length, b / length, c / length);

    // Plane plane;
    *outNormal = normal;
    // outPosition->x = -d * a / length;
    // outPosition->y = -d * b / length;
    // outPosition->z = -d * c / length;
    *outPosition = avg;

    // return plane;
}





static const bool end_updates = false;

fn_vec3* th_horsePlanMotion(fn_vec3 start,fn_vec3 end,th_World* world,float radius,int* point_count,bool skip_first)
{
  th_Entity tracer = TH_DEFAULT_ENTITY;
  tracer.aabb.position = start;
  tracer.aabb.hwidth = fn_createVec3(radius,radius,radius);
  tracer.radius = fn_createVec3(radius,radius,radius);
  tracer.velocity = fn_createVec3s(0);
  tracer.grounded = false;
  tracer.collided = false;
  tracer.aabb.mode = SPHERE;
  tracer.mode = TH_SLIDE_MODE;
  #define MAX_ITERATIONS 100
  bool converged = false;
  float dt = 20.0;
  int iterations = 0;
  fn_vec3* points = malloc(sizeof(fn_vec3)*MAX_ITERATIONS);

  tracer.velocity = fn_subVec3(end,tracer.aabb.position);
  tracer.velocity = fn_normalizeVec3(tracer.velocity);
  tracer.velocity  = fn_multVec3s(tracer.velocity,5.0);


  *point_count = 0;
  while (!converged && iterations < MAX_ITERATIONS) {

    fn_vec3 velocity = fn_subVec3(end,tracer.aabb.position);
    velocity = fn_normalizeVec3(velocity);
    velocity  = fn_multVec3s(velocity,5.0);
    tracer.velocity = fn_addVec3(tracer.velocity,velocity);
    tracer.velocity = fn_normalizeVec3(tracer.velocity);
    tracer.velocity  = fn_multVec3s(tracer.velocity,5.0);
    tracer.grounded  = false;
    tracer.collided = false;

    th_updateEntity(&tracer,world,dt,th_getPhysicsMemory(world,0));

    bool tooclose = false;
    for (int i = 0; i < *point_count; i++) {
      if(fn_distance(tracer.aabb.position,points[i]) < radius*0.5)
      {
        tooclose = true;
      }
    }


    if (!tooclose  )
    {
      points[*point_count] = tracer.aabb.position;
      *point_count = *point_count + 1;
    }


    iterations++;

    if (iterations > MAX_ITERATIONS)
    {
      converged = true;
      break;
    }

    if (fn_distance2(tracer.aabb.position,end) < radius*radius*4)
    {
      converged = true;
      break;
    }
  }



  //take every fourth point
  int decimated_num = 0;
  fn_vec3* points_dec = malloc(sizeof(fn_vec3)*MAX_ITERATIONS);
  int idx = 0;
  for (int i = 0; i < *point_count; i++) {
    if (i % 2 == 0 && !(skip_first && i == 0))
    {
      points_dec[decimated_num] = points[i];
      decimated_num++;
    }
  }

  free(points);
  *point_count = decimated_num;
  return points_dec;
}

//TODO
//Use driver normal to get movement direction
//Go up
//Step in direction of driver normal

//use Motion planning to prevent weird situations
//trace several spheres over potantial paths

//TODO presimulate many paths of motion
//use this to create directed node graph

fn_vec3 closestDirection(fn_vec3 startDir, fn_vec3 endDir, float angleLimit) {
    float dotProduct = fn_dot(startDir, endDir);
    if (acos(dotProduct) < angleLimit) {
        // If end direction is within the angle limit, return it directly
        return endDir;
    } else {
        // Otherwise, interpolate towards the end direction
        fn_vec3 axis = fn_cross(startDir,endDir);
        fn_quat q =  fn_makeQuaternion(angleLimit,axis);

        fn_vec3 closest = fn_rotatePointQuat(startDir,q);
        // printf("Limited %f %f\n",acos(fn_dot(closest,startDir)),angleLimit );
        return closest;
    }
}


static bool can_progress(th_HorseGroup* c,int idx,fn_vec3 position,fn_vec3 direction)
{
  for (int i = 0; i < c->count; i++) {
    if (i == idx || !c->data[i].spawn_finished || c->data[i].gibbed)
    {
      continue;
    }
    if (fn_distance2(position,c->entities[i].aabb.position) < 800*800)
    {
      fn_vec3 dvec = fn_normalizeVec3(fn_subVec3(c->entities[i].aabb.position,position));
      if (fn_dot(dvec,direction) > 0.5)
      {
        return false;
      }
    }
  }

  return true;
}

static bool can_spawn(th_HorseGroup* c,int idx,fn_vec3 position)
{
  for (int i = 0; i < c->count; i++) {

    if (i == idx || c->data[i].gibbed)
    {
      continue;
    }

    bool stall_conflict = !( c->data[i].spawn_stalled && !c->data[i].spawn_finished);
    if (c->data[i].spawn_stalled && !c->data[i].spawn_finished && c->data[idx].spawn_stalled && !c->data[idx].spawn_finished )
    {
      //if both stalled, and i < idx then ignore
      if (i < idx)
      {
        continue;
      }
      else if (i != idx)//if both stalled, and i > idx then stall
      {
        stall_conflict = false;
      }
    }


    // if different and the different one is stalled then dont skip
    if (stall_conflict && ( !c->data[i].spawn_finished ))
    {
      continue;
    }
    if (fn_distance2(position,c->entities[i].aabb.position) < 800*800)
    {
      return false;
    }
  }

  return true;
}

static fn_vec3 selectSpawnPosition(fn_vec3 origin,fn_vec3 spawnpos,th_HorseGroup* c,float radius,bool* success)
{
  fn_vec3 n = fn_createVec3(0,0,0);
  bool hit = false;
  fn_vec3 pos = th_traceVolume(c->levelstate->world,origin,spawnpos,radius,&n,&hit,th_getPhysicsMemory(c->levelstate->world,0));
  if (success != NULL)
  {
    *success = true;
  }


  if (hit)
  {
    fn_vec3 newpos = fn_addVec3(pos,fn_multVec3s(n,radius*1.01));
    pos = th_traceVolume(c->levelstate->world,origin,newpos,radius,&n,&hit,th_getPhysicsMemory(c->levelstate->world,0));
    if (hit)
    {
      newpos = fn_addVec3(pos,fn_multVec3s(n,radius*1.01));
      pos = th_traceVolume(c->levelstate->world,origin,newpos,radius,&n,&hit,th_getPhysicsMemory(c->levelstate->world,0));
      if (hit)
      {

        if (success != NULL)
        {
          *success = false;
        }
        //printf("Spawn obstructed\n");
        return origin;
      }
      else
      {
        return newpos;
      }
    }
    else
    {
      return newpos;
    }

  }

  return spawnpos;
}

void th_horseUpdate(th_HorseGroup* c,float dt)
{

  // th_Allocator temp_alloc;
  // th_createAllocator(&temp_alloc);
  th_Allocator* temp_alloc = &c->temp_alloc;

  float offset_v = 3.04*10*6.4;





  for (int i = 0; i < c->count; i++) {

    bool spawned = true;
    bool play_sound_spawn = false;
    if (th_time() - c->levelstate->level_start_time < c->data[i].spawn_when)
    {
      spawned = false;
    }
    else if (!c->data[i].spawn_finished)
    {
      //activate entities
      for (int j = i*6;j < i*6 + 6;j++)
      {
        c->entities_gems[j].alive = true;
      }
      play_sound_spawn = true;

      for (int j = i*c->legs_per_count;j < i*c->legs_per_count + c->legs_per_count;j++)
      {
        c->entities_legs[j].alive = true;
      }

       c->data[i].spawn_finished = true;
       c->data[i].spawn_children_timer = th_time() - CHILD_SPAWN_INTERVAL + INITIAL_SPAWN_INTERVAL;
    }


    bool unstall = false;
    if (!(th_time() - c->levelstate->level_start_time > c->data[i].spawn_when - 1000))
    {

      c->transforms[i] =fn_makescale(fn_createVec3s(0));
      positionHatch(c,i);

      for (int j = 0 ; j < 12;j++)
      {
        LEG_ASSIGNMENT(i*12 + j) = fn_makescale(fn_createVec3s(0));
      }

      for (int j = 0 ; j < 6;j++)
      {
        c->transforms_gems[i*6 + j] = fn_makescale(fn_createVec3s(0));
      }

      continue;
    }
    else if (!spawned)
    {
      bool non_conflicting = can_spawn(c,i,c->entities[i].aabb.position);
      if (!non_conflicting)
      {
        //can't spawn because you'd telefrag? just wait 5 seconds
        c->data[i].spawn_when = c->data[i].spawn_when + 5000;
        c->data[i].fadeout_spawn = 1.0;
        for (int j = i*6;j < i*6 + 6;j++)
        {
          c->entities_gems[j].alive = false;
        }
        play_sound_spawn = false;

        for (int j = i*c->legs_per_count;j < i*c->legs_per_count + c->legs_per_count;j++)
        {
          c->entities_legs[j].alive = false;
        }

        c->data[i].spawn_finished = false;
        c->data[i].spawn_stalled = true;
        c->data[i].unstall_count = 0;

        c->transforms[i] =fn_makescale(fn_createVec3s(0));
        positionHatch(c,i);

        for (int j = 0 ; j < 12;j++)
        {
          LEG_ASSIGNMENT(i*12 + j) = fn_makescale(fn_createVec3s(0));
        }

        for (int j = 0 ; j < 6;j++)
        {
          c->transforms_gems[i*6 + j] = fn_makescale(fn_createVec3s(0));
        }
        //printf("Stalled %i\n",i);
        continue;
      }
      else
      {
        //printf("Un-Stalled %i\n",i);

        if (c->data[i].setposition)
        {
          c->data[i].unstall_count = c->data[i].unstall_count + 1;
        }

        if (c->data[i].unstall_count >= 2)
        {
          unstall = true;
        }

      }
    }


    if (c->data[i].gibbed)
    {
      th_Entity* e = &c->data[i].horse_body_gib;
      if (e->alive)
      {
        fn_vec3 oldvelocity = e->velocity;
        bool oldground = e->grounded;

        th_updateEntity(e,c->levelstate->world,dt,th_getPhysicsMemory(c->levelstate->world,0));
        if (!e->grounded )
        {
          e->velocity.y += 0.001*dt*1.0;
        }
        if (e->collided){

          if (fn_length2(e->velocity) > 0.05*0.05){
            a_VirtualSource* s = a_playVirtualSource(54,-4,e->aabb.position,NULL );
            a_setVSLoop(s,false);
            a_setVSPos(s,e->aabb.position);
            a_setVSVel(s,e->velocity);
            a_setVSGain(s,fn_remap(fn_length(e->velocity),0,0.5,0.3,0.4));
            a_setVSPitch(s,fn_remap(fn_length(e->velocity),0,0.5,0.8,1.3));
          }

          float dot = fn_dot(oldvelocity,e->collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
          fn_vec3 u = fn_multVec3s(e->collision_normal , dot);
          fn_vec3 w = fn_subVec3(oldvelocity , u);
          e->velocity = fn_multVec3s(fn_subVec3(w , u),0.75);//reduce bounce height 0.99,0.75,0.99
          for (int k = 0 ; k < 3;k++)
          {
            if (fn_almostEqualf(e->velocity.v[k],0,0.05))
            {
              e->velocity.v[k] = 0;
            }
          }
          if (fn_almostEqualf(e->velocity.v[1],0,0.05))
          {
            e->alive = false;
          }

        }

        if (fn_distance(e->aabb.position,c->levelstate->player_e.aabb.position) < 500)
        {
          e->alive = false;
        }

        fn_mat4 orient_mat = th_6dofCamera(&c->data[i].old_forward,&c->data[i].old_right,&c->data[i].old_up,c->data[i].old_forward,e->aabb.position);
        fn_mat4 m_prime,m_prime_s;

        //0.4 + 0.05*sin(th_time()*0.0023)
        float theta = 3.14159 + c->data[i].angle_adjust;
        fn_mat4 r = fn_makerotate(theta,fn_createVec3(0,0,1));


        //minus lifts front, plus lifts back
        float theta2 = c->data[i].pitch_adjust;
        fn_mat4 r2 = fn_makerotate(theta2,fn_createVec3(1,0,0));

        orient_mat = fn_multMat4(orient_mat,fn_multMat4(r,r2));

        //m_prime = fn_inverse(orient_mat);
        m_prime_s = fn_multMat4(fn_makescale(fn_createVec3s(10)),fn_inverse(orient_mat));
        c->transforms[i] = m_prime_s;
        positionHatch(c,i);
        c->data[i].old_body = m_prime_s;
      }
      else {


        c->transforms[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_body,fadeout_decay_rate*dt),c->data[i].old_body);
        positionHatch(c,i);
      }




      for (size_t p = 0; p < 6*2; p++) {
        //simulate entity pair
        th_Entity old_0 = c->data[i].horse_leg_gib[p][0];
        th_Entity old_1 = c->data[i].horse_leg_gib[p][1];
        for (size_t l = 0; l < 2; l++) {
          th_Entity* e = &c->data[i].horse_leg_gib[p][l];
          if (!e->alive)
          {
            //begin fadeout
            // printf("%s\n","Faded" );

            continue;
          }

          fn_vec3 oldvelocity = e->velocity;
          bool oldground = e->grounded;

          th_updateEntity(e,c->levelstate->world,dt,th_getPhysicsMemory(c->levelstate->world,0));
          if (!e->grounded )
          {
            e->velocity.y += 0.001*dt*1.0;
          }
          if (e->collided){

            if (fn_length2(e->velocity) > 0.05*0.05){
              a_VirtualSource* s = a_playVirtualSource(54,-4,e->aabb.position,NULL );
              a_setVSLoop(s,false);
              a_setVSPos(s,e->aabb.position);
              a_setVSVel(s,e->velocity);
              a_setVSGain(s,fn_remap(fn_length(e->velocity),0,0.5,0.4,0.5));
              a_setVSPitch(s,fn_remap(fn_length(e->velocity),0,0.5,0.8,1.3));
            }

            float dot = fn_dot(oldvelocity,e->collision_normal);//velocity->x * normal.x + velocity->y * normal.y + velocity->z * normal.z ;
            fn_vec3 u = fn_multVec3s(e->collision_normal , dot);
            fn_vec3 w = fn_subVec3(oldvelocity , u);
            e->velocity = fn_multVec3s(fn_subVec3(w , u),0.75);//reduce bounce height 0.99,0.75,0.99
            int n_zero = 0;
            for (int k = 0 ; k < 3;k++)
            {
              if (fn_almostEqualf(e->velocity.v[k],0,0.05))
              {
                e->velocity.v[k] = 0;
                n_zero++;
              }
            }
            if (n_zero == 3)//fn_almostEqualf(e->velocity.v[1],0,0.05)
            {
              e->alive = false;
            }

          }

          if (fn_distance(e->aabb.position,c->levelstate->player_e.aabb.position) < 500)
          {
            e->alive = false;

            c->data[i].horse_leg_gib[p][0].alive = false;
            c->data[i].horse_leg_gib[p][1].alive = false;
          }
        }

        if (!old_0.alive && !old_1.alive)
        {
          LEG_ASSIGNMENT(i*c->legs_per_count + p ) = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_legs[p],fadeout_decay_rate*dt),c->data[i].old_legs[p ]);
        }
        else
        {
          //enforce constrint
          float dist_current = fn_distance(c->data[i].horse_leg_gib[p][0].aabb.position,c->data[i].horse_leg_gib[p][1].aabb.position);
          float error = dist_current - offset_v;
          // printf("%f\n",error );
          fn_vec3 direction = fn_subVec3(c->data[i].horse_leg_gib[p][1].aabb.position,c->data[i].horse_leg_gib[p][0].aabb.position);
          direction = fn_normalizeVec3(direction);
          fn_vec3 vel_correct_0 = fn_multVec3s(direction,error*0.5);
          fn_vec3 vel_correct_1 = fn_multVec3s(direction,-error*0.5);

          fn_vec3 canidate_0 = fn_addVec3(c->data[i].horse_leg_gib[p][0].aabb.position,vel_correct_0);
          fn_vec3 canidate_1 = fn_addVec3(c->data[i].horse_leg_gib[p][1].aabb.position,vel_correct_1);
          float radius = c->data[i].horse_leg_gib[p][0].aabb.hwidth.x;

          bool hit_0 = false;
          fn_vec3 pos_correct_0 = th_traceVolume(c->levelstate->world,c->data[i].horse_leg_gib[p][0].aabb.position,canidate_0,radius,NULL,&hit_0,th_getPhysicsMemory(c->levelstate->world,0));

          bool hit_1 = false;
          fn_vec3 pos_correct_1 = th_traceVolume(c->levelstate->world,c->data[i].horse_leg_gib[p][1].aabb.position,canidate_1,radius,NULL,&hit_0,th_getPhysicsMemory(c->levelstate->world,0));

            if (c->data[i].horse_leg_gib[p][0].alive)
              c->data[i].horse_leg_gib[p][0].velocity = fn_addVec3(c->data[i].horse_leg_gib[p][0].velocity,fn_multVec3s(vel_correct_0,1.0/32.0));
            if (c->data[i].horse_leg_gib[p][0].alive)
              c->data[i].horse_leg_gib[p][1].velocity = fn_addVec3(c->data[i].horse_leg_gib[p][1].velocity,fn_multVec3s(vel_correct_1,1.0/32.0));


          fn_vec3 n0 = c->data[i].horse_leg_gib[p][0].aabb.position;
          fn_vec3 n1 = c->data[i].horse_leg_gib[p][1].aabb.position;
          fn_vec3 offset_n = fn_multVec3s(fn_addVec3(n0,n1),0.5);


          fn_vec3 tr = fn_createVec3(1,1,1);

          fn_vec3 top_pos = fn_createVec3(0,-offset_v*0.5,0);
          fn_vec3 bottom_pos = fn_createVec3(0,offset_v*0.5,0);

          fn_vec3 old_node_0 = fn_addVec3(fn_rotatePointQuat(top_pos,c->data[i].leg_orientations[p]),c->data[i].leg_offsets[p]);
          fn_vec3 old_node_1 = fn_addVec3(fn_rotatePointQuat(bottom_pos,c->data[i].leg_orientations[p]),c->data[i].leg_offsets[p]);


          fn_vec3 bone_1_from = fn_multVec3(fn_normalizeVec3(fn_subVec3(old_node_1 , old_node_0)),tr);
          fn_vec3 bone_1_to = fn_multVec3(fn_normalizeVec3(fn_subVec3(n1 ,n0)),tr);



          fn_quat legq = fn_getRotationQuaternion(bone_1_from,bone_1_to);


          legq.w = legq.w*-1;


          c->data[i].leg_orientations[p] = fn_multquat(c->data[i].leg_orientations[p],legq);
          c->data[i].leg_offsets[p ] = offset_n;
          LEG_ASSIGNMENT(i*c->legs_per_count + p ) = fn_translaterotatescaleq(offset_n,  c->data[i].leg_orientations[p],fn_createVec3s(10));
          c->data[i].old_legs[p ] = LEG_ASSIGNMENT(i*c->legs_per_count + p);
        }




      }



      continue;
    }


  //   fn_vec3 target = fn_createVec3(714.785706, -1261.300293, -1465.893677);//fn_createVec3(-165.821213, -1224.241455, -511.315369);
    // fn_vec3 target = fn_createVec3(-587.110474, -1655.462769, -325.762939);//fn_createVec3(-165.821213, -1224.241455, -511.315369);
    //fn_vec3 target = fn_createVec3(-488.702667, -693.692932 ,1055.968140);
    // fn_vec3 target = fn_createVec3(1311.622559, 141.003845 ,-90.022049);


    // fn_vec3 target = fn_createVec3(1395.104492 ,-191.705704, 35.557713);
    // fn_vec3 target = fn_createVec3(-2044.531494, -984.150269, 310.366699);
    // fn_vec3 target2 = fn_createVec3(1377.247192, 70.546082, 72.257607);
    // fn_vec3 target3 = fn_createVec3(1368.936768, 81.773331, -495.647583);
    // fn_vec3 target4 = fn_createVec3(-1300.999878 ,-1850.294189, 1254.217407);
    //
    // fn_vec3 target_ceiling = fn_createVec3(-491.291138, -1490.759888, -561.367065);
    //
    //
    // fn_vec3 target5 = fn_createVec3(-403.436462, -2460.566650, 756.373901);
    //
    // fn_vec3 targeta = fn_createVec3(1348.958740 ,108.150970, -561.376587);
    // fn_vec3 targetb = fn_createVec3(1038.033203 ,-221.031204 ,416.687164);



    fn_vec3 legg_positions_prime[6];
    fn_vec3 legg_positions[6];
    fn_vec3 leggoffset[2];
    fn_quat legq[2];
    fn_vec3 tr1 = fn_createVec3(-1,1,-1);
    //th_horseProcessCourse(c,i);





      float factor = 1.0;
      float speed_fraction = 1.0;

      //limit turning speed
      {
        fn_vec3 forward_prime = CalculateTangent(c->data[i].path[c->data[i].cprog],c->data[i].path[c->data[i].cprog + 1],c->data[i].path[c->data[i].cprog + 2],c->data[i].path[c->data[i].cprog + 3],c->data[i].interp);
        fn_vec3 d = CalculateTangentUnNorm(c->data[i].path[c->data[i].cprog],c->data[i].path[c->data[i].cprog + 1],c->data[i].path[c->data[i].cprog + 2],c->data[i].path[c->data[i].cprog + 3],c->data[i].interp);
        float speed = dt*0.05*factor*speed_fraction;//dt*0.7;
        float interp_delta = speed / sqrtf( d.x*d.x + d.y*d.y + d.z*d.z );

        float old_i = c->data[i].interp;
        int old_cprog = c->data[i].cprog;

        float interp = c->data[i].interp;
        int cprog = c->data[i].cprog;

        interp += interp_delta;
        if (interp > 1)
        {
          cprog++;
          interp = interp - 1;
          if (cprog > c->data[i].path_count - 4)
          {
            // c->data[i].cprog = 0;
            cprog = old_cprog;
            interp = old_i;
          }
        }

        fn_vec3 forward_delta = CalculateTangent(c->data[i].path[cprog],c->data[i].path[cprog + 1],c->data[i].path[cprog + 2],c->data[i].path[cprog + 3],interp);
        float angle_per_ms = acos(fn_clamp(fn_dot(forward_prime,forward_delta),-1.0,1.0 ) )/dt;

        if (angle_per_ms*1000 > 0.06)
        {

          factor =  ((0.06/1000.0)/angle_per_ms)*1.5;

        }
      }



      bool canmove = false;
      if (c->data[i].set_up)
      {
        int ccount = 0;
        for (size_t j = 0; j < 6; j++) {

          float d = fn_distance(c->data[i].leg_history[j].nodes[0],c->data[i].leg_history[j].nodes[2]);

          if ((!c->data[i].stepping[j] && d <= offset_v*1.9) || (c->data[i].stepping[j] && !c->data[i].step_linear[j]) )
          {
            ccount++;
          }

        }
        if (ccount >= 2)
        {
          canmove = true;
        }

      }

      if (th_time() < c->data[i].stun_timer)
      {
        canmove = false;
      }

      //canmove = false;

      fn_vec3 forward_dirctn = CalculateTangent(c->data[i].path[c->data[i].cprog],c->data[i].path[c->data[i].cprog + 1],c->data[i].path[c->data[i].cprog + 2],c->data[i].path[c->data[i].cprog + 3],c->data[i].interp);
      bool non_conflicting = can_progress(c,i,c->entities[i].aabb.position,fn_normalizeVec3(forward_dirctn));

      if (canmove && spawned && non_conflicting)
      {
        fn_vec3 d = CalculateTangentUnNorm(c->data[i].path[c->data[i].cprog],c->data[i].path[c->data[i].cprog + 1],c->data[i].path[c->data[i].cprog + 2],c->data[i].path[c->data[i].cprog + 3],c->data[i].interp);
        float speed = dt*0.05*factor*speed_fraction;//dt*0.7;
        float interp_delta = speed / sqrtf( d.x*d.x + d.y*d.y + d.z*d.z );

        float old_i = c->data[i].interp;
        int old_cprog = c->data[i].cprog;
        c->data[i].interp += interp_delta;
        if (c->data[i].interp > 1)
        {
          c->data[i].cprog++;
          c->data[i].interp = c->data[i].interp - 1;
          if (c->data[i].cprog > c->data[i].path_count - 4)
          {
            // c->data[i].cprog = 0;
            c->data[i].cprog = old_cprog;
            c->data[i].interp = old_i;
          }
        }
      }

    //}

    int cstart = c->data[i].cprog;
    float interp = c->data[i].interp;


    int upindex = (cstart)*2;
    upindex = (interp >= 0.5) ? upindex + 1 : upindex;
    c->data[i].old_up = fn_NlerpVec3(c->data[i].ups[upindex],c->data[i].ups[upindex + 1],fmod(interp,0.5)*2);

    fn_vec3 pos = CalculatePosition(c->data[i].path[cstart],c->data[i].path[cstart + 1],c->data[i].path[cstart + 2],c->data[i].path[cstart + 3],interp);
    fn_vec3 forward = CalculateTangent(c->data[i].path[cstart],c->data[i].path[cstart + 1],c->data[i].path[cstart + 2],c->data[i].path[cstart + 3],interp);

    pos = fn_addVec3(pos,fn_multVec3s(c->data[i].old_up,20*sin(th_time()*0.005)));
    pos = fn_addVec3(pos,fn_multVec3s(c->data[i].old_up,c->data[i].height_adjust));

    // float angle_per_ms = acos(fn_clamp(fn_dot(forward,c->data[i].old_forward),-1.0,1.0 ) )/dt;
    // th_printlnDevConsole("%f",angle_per_ms*1000);

    c->entities[i].aabb.position = pos;
    c->data[i].setposition = true;

    //0.1 rads / second -> 0.0001 rads/ millisecond
    // float rads_sec = 0.1;
    // float limit_time = rads_sec*0.001*dt;
  //  forward = closestDirection(c->data[i].old_forward,forward,limit_time);
    fn_vec3 old_up_prime = c->data[i].old_up;
    fn_mat4 orient_mat = th_6dofCamera(&c->data[i].old_forward,&c->data[i].old_right,&c->data[i].old_up,forward,pos);

    fn_mat4 orient_mat_nopos = th_6dofCamera(NULL,NULL,&old_up_prime,forward,fn_createVec3(0,0,0));
    //fn_mat4 orient_mat_prime = orient_mat;

    fn_mat4 m_prime,m_prime_s;

    //0.4 + 0.05*sin(th_time()*0.0023)
    float theta = 3.14159 + c->data[i].angle_adjust;
    fn_mat4 r = fn_makerotate(theta,fn_createVec3(0,0,1));


    //minus lifts front, plus lifts back
    float theta2 = c->data[i].pitch_adjust;
    fn_mat4 r2 = fn_makerotate(theta2,fn_createVec3(1,0,0));

    orient_mat = fn_multMat4(orient_mat,fn_multMat4(r,r2));


    fn_quat r3 = fn_createVec4(0,0,0,1);

    //stunned reorient
    if (th_time() < c->data[i].stun_timer)
    {
      fn_quat q_target_stun_to = fn_getRotationQuaternion2(fn_multVec3s(c->data[i].old_up,1),c->data[i].stun_direction);
      //q_target_stun_to.w = q_target_stun_to.w * -1;


      float fract = (c->data[i].stun_timer - th_time())/STUN_TIME_HORSE;

      float animation_alpha = HORSE_STUN_ALPHA;
      float one_minus_animation_alpha = 1.0 - animation_alpha;
      if (fract < animation_alpha)
      {
        r3 = fn_slerpVec4(r3,q_target_stun_to,fract/animation_alpha);
      }
      else
      {
        r3 = fn_slerpVec4(q_target_stun_to,r3,(fract - animation_alpha)/(one_minus_animation_alpha));
      }

     // r3 = q_target_stun_to;

    }

    m_prime = fn_inverse(orient_mat);
    m_prime_s = fn_multMat4(fn_makescale(fn_createVec3s(10)),fn_inverse(orient_mat));

    forward = c->data[i].old_forward;
    fn_vec3 up_v = c->data[i].old_up;

    c->data[i].orientation_direction = fn_multVec4Mat4(m_prime,fn_createVec4(0,-1,0,0.0)).xyz;
    c->data[i].facing = fn_multVec4Mat4(m_prime,fn_createVec4(0,0,-1,0.0)).xyz;
    fn_vec3 side = fn_multVec4Mat4(m_prime,fn_createVec4(1,0,0,0.0)).xyz;
    int num_gems = 0;


    if (th_time() < c->data[i].stun_timer)
    {
      //orient_mat = orient_mat_prime;//th_6dofCamera(&c->data[i].old_forward,&c->data[i].old_right,&c->data[i].old_up,forward,pos);

      // orient_mat = fn_multMat4(orient_mat,fn_multMat4(r,r2));
      // m_prime = fn_multMat4(fn_rotationMat(r3),fn_inverse(orient_mat));
      // m_prime_s = fn_multMat4(fn_makescale(fn_createVec3s(10)),fn_multMat4(fn_rotationMat(r3),fn_inverse(orient_mat)));

      fn_quat local_mod = fn_mat4toquat(fn_multMat4(r,r2));

      fn_quat orientation_rot = fn_mat4toquat(fn_inverse(orient_mat_nopos));
      //fn_quat q_comp = fn_multquat(r3,fn_multquat(orientation_rot,fn_inverseq(local_mod))); //noncanidate
      //fn_quat q_comp = fn_multquat(r3,fn_multquat(fn_inverseq(local_mod),orientation_rot)); //candidate

      // fn_quat q_comp = fn_multquat(fn_inverseq(local_mod),fn_multquat(r3,orientation_rot));// candidate
      fn_quat q_comp = fn_multquat(fn_inverseq(local_mod),fn_multquat(orientation_rot,r3)); //candidate
      //
      // fn_quat q_comp = fn_multquat(orientation_rot,fn_multquat(fn_inverseq(local_mod),r3)); noncanidate
      //fn_quat q_comp = fn_multquat(orientation_rot,fn_multquat(r3,fn_inverseq(local_mod))); noncanidate

     // fn_quat q_comp = fn_multquat(fn_inverseq(local_mod),orientation_rot);
      m_prime = fn_translaterotatescaleq(pos,q_comp,fn_createVec3s(1));
      m_prime_s = fn_translaterotatescaleq(pos,q_comp,fn_createVec3s(10));
    }

    for (int j = 0;j < 6;j++)
    {
      if (c->entities_gems[i*6 + j].impact)
      {

        //TODO accoutn for velocity of segments? and delta time
        for (int k = 0 ; k < c->entities_gems[i*6 + j].impact_count;k++ )
        {
          th_Entity* projectile = (th_Entity*)c->entities_gems[i*6 + j].impacts[k].entity;
          fn_vec3 vel = fn_multVec3s(fn_normalizeVec3(projectile->velocity),-1.0);

          bool positive = j % 2 == 0;
          float signed_dir = 1.0;
          if (!positive)
          {
            signed_dir = -1.0;
          }
          fn_vec3 gemdir = fn_transformNormal(fn_createVec3(signed_dir,0,0),c->transforms[i]);


          if (projectile->type == TH_HAMMER_PROJECTILE && th_time() > c->data[i].stun_timer - STUN_TIME_HORSE*0.5)
          {
            //impacted stun state
            c->data[i].stun_timer = th_time() + STUN_TIME_HORSE;

            fn_vec3 gempos = c->entities_gems[i*6 + j].aabb.position;

            fn_vec3 C = fn_normalizeVec3(projectile->velocity);
            fn_vec3 B = fn_normalizeVec3(fn_subVec3(gempos,c->entities[i].aabb.position));
            fn_vec3 A = fn_multVec3s(fn_normalizeVec3(c->data[i].old_up),1);

            float dot_dir = fn_dot(C,A);

            fn_vec3 C1 = fn_normalizeVec3(fn_addVec3(A,fn_multVec3s(B,HORSE_STUN_MAG)));
            fn_vec3 C2 = fn_normalizeVec3(fn_addVec3(A,fn_multVec3s(B,-HORSE_STUN_MAG)));
            //printf("%f\n",acos(fn_dot(A,B))*(180.0/3.14159));
            if (dot_dir > 0.0)
            {
              //printf("Undercut \n");
              c->data[i].stun_direction = C1;
            }
            else
            {
              //printf("downercut \n");
              c->data[i].stun_direction = C2;
            }

          }
          //fn_printVec3(gemdir);
          //75 degrees
          if ( (fn_dot(gemdir,vel) > -0.15  || projectile->type == TH_HAMMER_PROJECTILE )&& c->data[i].hasgem[j] ) //&& fn_angle(gemdir,vel) < fn_radians(90)
          {
            //c->entities_gems[i*6 + j].alive = false;
            if (projectile->type == TH_MACHINEGUN_BULLET)
            {
              c->data[i].gemhealth[j] -= 4.0;
            }
            else if (projectile->type == TH_SHOTGUN_SHELL)
            {
              c->data[i].gemhealth[j] -= 5.0;
            }
            else if (projectile->type == TH_HAMMER_PROJECTILE)
            {
              c->data[i].gemhealth[j] -= 100.0;
            }

            th_Hitmarker hmarker;
            hmarker.entity_pos_ref = NULL;
            hmarker.entity_transform_ref = &c->transforms_gems[i*6 + j];
            hmarker.alive_ref = &c->data[i].hasgem[j];
            hmarker.timer = th_time();
            hmarker.last_good_pos = c->entities_gems[i*6 + j].aabb.position;
            hmarker.is_alive = true;
            hmarker.radius = 95.0;
            th_pushHitmarker(hmarker);

            if (c->data[i].hasgem[j] && c->data[i].gemhealth[j] <= 0.0)
            {
              c->data[i].hasgem[j] = false;
              th_gemSpawn(c->levelstate->gems,c->entities_gems[i*6 + j].aabb.position,fn_multVec3s(gemdir,0.5));
              th_gemSpawn(c->levelstate->gems,c->entities_gems[i*6 + j].aabb.position,fn_multVec3s(gemdir,-0.5));
              th_spawnBloodNoSound(c->entities_gems[i*6 + j].aabb.position,0);

              {
                a_VirtualSource* s = a_playVirtualSource(sound_gem_breakfree,0,c->entities_gems[i*6 + j].aabb.position,NULL );
                a_setVSLoop(s,false);
                a_setVSPos(s,c->entities_gems[i*6 + j].aabb.position);
                a_setVSVel(s,fn_createVec3s(0));
                a_setVSGain(s,1.0);
                a_setVSPitch(s,th_randomFloat(0.85,1.15));
              }
            }
            else
            {
              th_spawnBloodSpurt(fn_multVec3s(vel,1),c->entities_gems[i*6 + j].impacts[k].pos,-1);
              th_spawnBloodNoSound(c->entities_gems[i*6 + j].aabb.position,0);
              fn_vec3 b1 = fn_multVec3s(fn_createVec3(signed_dir,0,0),th_randomFloat(0,1.0));
              fn_vec3 b2 = fn_multVec3s(fn_createVec3(0,1,0),th_randomFloat(-1.0,1.0));
              fn_vec3 b3 = fn_multVec3s(fn_createVec3(0,0,1),th_randomFloat(-1.0,1.0));
              if (c->data[i].gem_jitter_t[j] <= ((1.0/0.05)*3.14159)*5*0.5)
              {
                c->data[i].gem_jitter_x[j] = fn_normalizeVec3(fn_addVec3(b1,fn_addVec3(b2,b3)));
                c->data[i].gem_jitter_t[j] = ((1.0/0.05)*3.14159)*5;//10*3.14159*9;

                {
                  a_VirtualSource* s = a_playVirtualSource(sound_gem_impact,0,c->entities_gems[i*6 + j].aabb.position,NULL );
                  a_setVSLoop(s,false);
                  a_setVSPos(s,c->entities_gems[i*6 + j].aabb.position);
                  a_setVSVel(s,fn_createVec3s(0));
                  a_setVSGain(s,0.8);
                  a_setVSPitch(s,th_randomFloat(0.85,1.15));
                }
              }


            }



          }
          else
          {
            // float neg = fn_dot(gemdir,vel) > 0 ? 1 : -1;
             th_spawnSparks(fn_multVec3s(vel,1),c->entities_gems[i*6 + j].impacts[k].pos,-1,c->levelstate->general_light_query);
             th_spawnSparks(fn_multVec3s(vel,1),c->entities_gems[i*6 + j].impacts[k].pos,-1,c->levelstate->general_light_query);
             // th_spawnSparks(fn_multVec3s(gemdir,neg),c->entities_gems[i*6 + j].impacts[k].pos,-1);
             // th_spawnSparks(fn_multVec3s(gemdir,neg),c->entities_gems[i*6 + j].impacts[k].pos,-1);

             {
               a_VirtualSource* s = a_playVirtualSource(15 + th_random() % 3,-1,c->entities_gems[i*6 + j].impacts[k].pos,NULL );
               a_setVSLoop(s,false);
               a_setVSPos(s,c->entities_gems[i*6 + j].impacts[k].pos);
               a_setVSVel(s,fn_createVec3s(0));
               a_setVSGain(s,0.55);
             }

          }


        }
        c->entities_gems[i*6 + j].impact_count = 0;
        c->entities_gems[i*6 + j].impact = false;


      }
      if (c->data[i].hasgem[j])
      {
        num_gems++;
      }
    }

    for (int j = 0;j < c->legs_per_count;j++)
    {
      if (c->entities_legs[i*c->legs_per_count + j].impact)
      {

        //TODO accoutn for velocity of segments? and delta time
        for (int k = 0 ; k < c->entities_legs[i*c->legs_per_count + j].impact_count;k++ )
        {
          th_Entity* projectile = (th_Entity*)c->entities_legs[i*c->legs_per_count + j].impacts[k].entity;
          fn_vec3 vel = fn_multVec3s(fn_normalizeVec3(projectile->velocity),-1.0);







          // float neg = fn_dot(gemdir,vel) > 0 ? 1 : -1;
          th_spawnSparks(fn_multVec3s(vel,1),c->entities_legs[i*c->legs_per_count + j].impacts[k].pos,-1,c->levelstate->general_light_query);
          th_spawnSparks(fn_multVec3s(vel,1),c->entities_legs[i*c->legs_per_count + j].impacts[k].pos,-1,c->levelstate->general_light_query);


          {
            a_VirtualSource* s = a_playVirtualSource(15 + th_random() % 3,-1,c->entities_legs[i*c->legs_per_count + j].impacts[k].pos,NULL );
            a_setVSLoop(s,false);
            a_setVSPos(s,c->entities_legs[i*c->legs_per_count + j].impacts[k].pos);
            a_setVSVel(s,fn_createVec3s(0));
            a_setVSGain(s,0.55);
          }




        }
        c->entities_legs[i*c->legs_per_count + j].impact_count = 0;
        c->entities_legs[i*c->legs_per_count + j].impact = false;


      }
    }

    if (num_gems == 0)
    {
      th_markEnemyDeath(1);
      c->levelstate->num_spawners_killed = c->levelstate->num_spawners_killed + 1;
      c->data[i].gibbed = true;
      c->data[i].horse_body_gib.aabb.position = c->entities[i].aabb.position;
      c->data[i].horse_body_gib.velocity = fn_multVec3s(up_v,0.45);
      th_setGameplayTimeScale(fn_createVec3(0.45,0.00000,0.0000002));

      for (int j = i*c->legs_per_count;j < i*c->legs_per_count + c->legs_per_count;j++)
      {
        c->entities_legs[j].alive = false;
      }

      for (size_t p = 0; p < 12; p+= 2) {
        c->entities_gems[i*6 + p/2].alive = false;
        bool positive = (p/2) % 2 == 0;
        float signed_dir = 1.0;
        if (!positive)
        {
          signed_dir = -1.0;
        }
        fn_vec3 gemdir = fn_transformNormal(fn_createVec3(signed_dir,0,0),c->transforms[i]);
        float gamma = 0.65;
        float beta = 1.0;
        c->data[i].horse_leg_gib[p][0].aabb.position = c->data[i].leg_history[p/2].nodes[0];
        c->data[i].horse_leg_gib[p][0].velocity = fn_multVec3s(gemdir,-gamma);
        c->data[i].horse_leg_gib[p][1].aabb.position = c->data[i].leg_history[p/2].nodes[1];
        c->data[i].horse_leg_gib[p][1].velocity = fn_multVec3s(gemdir,gamma);

        c->data[i].horse_leg_gib[p + 1][0].aabb.position = c->data[i].leg_history[p/2].nodes[1];
        c->data[i].horse_leg_gib[p + 1][1].aabb.position = c->data[i].leg_history[p/2].nodes[2];

        c->data[i].horse_leg_gib[p+1][0].velocity = fn_multVec3s(gemdir,-gamma);
        c->data[i].horse_leg_gib[p+1][1].velocity = fn_multVec3s(gemdir,gamma);

        c->data[i].horse_leg_gib[p][0].velocity = fn_addVec3(c->data[i].horse_leg_gib[p][0].velocity,fn_multVec3s(c->data[i].old_up,beta));
        c->data[i].horse_leg_gib[p][1].velocity = fn_addVec3(c->data[i].horse_leg_gib[p][1].velocity,fn_multVec3s(c->data[i].old_up,beta));
        c->data[i].horse_leg_gib[p + 1][0].velocity = fn_addVec3(c->data[i].horse_leg_gib[p+1][0].velocity,fn_multVec3s(c->data[i].old_up,beta));
        c->data[i].horse_leg_gib[p + 1][1].velocity = fn_addVec3(c->data[i].horse_leg_gib[p+1][1].velocity,fn_multVec3s(c->data[i].old_up,beta));

        {
          a_VirtualSource* s = a_playVirtualSource(21,-1, c->data[i].leg_history[p/2].nodes[0],NULL);
          a_setVSLoop(s,false);
          a_setVSPos(s,c->data[i].leg_history[p/2].nodes[0]);
          a_setVSVel(s,fn_createVec3s(0));
          a_setVSGain(s,0.4);
        }
      }

    }

    if (end_updates)
    {
      continue;
    }

    legg_positions_prime[0] = fn_createVec3(2*10*6.4,0,-0.25*10*6.4);//left middle
    legg_positions_prime[1] = fn_createVec3(-2*10*6.4,0,-0.25*10*6.4); // right middle
    legg_positions_prime[2] = fn_createVec3(2*10*6.4,0,-0.25*10*6.4 + 3*10*6.4); //left front
    legg_positions_prime[3] = fn_createVec3(-2*10*6.4,0,-0.25*10*6.4+ 3*10*6.4); //right front
    legg_positions_prime[4] = fn_createVec3(2*10*6.4,0,-0.25*10*6.4 - 3*10*6.4); //left back
    legg_positions_prime[5] = fn_createVec3(-2*10*6.4,0,-0.25*10*6.4 - 3*10*6.4); // right back
    for (int j = 0; j < 6; j++) {
      // legg_positions[j] = fn_rotatePointQuat(legg_positions_prime[j],c->data[i].orientation);
      legg_positions[j] = fn_multVec4Mat4(m_prime,fn_createVec4Vec3(legg_positions_prime[j],0.0)).xyz;

    }


    bool contacted[6];


    float leg_extensions[6];
    int num_contacts = 0;

    //contacts that cause a course change
    bool contact_driver[6];
    fn_vec3 normals_driver[6];

    th_pushOccluderFrame(fn_createVec4Vec3(pos,230.0));

    for (int j = 0; j < c->legs_per_count; j+= 2) {

      float zdiff = legg_positions_prime[j/2].z + 0.25*10*6.4;
      if (fabs(zdiff) > 0.01)
      {
        zdiff = -fn_sign(zdiff);
      }
      else
      {
        zdiff = 0.0;
      }

      fn_vec3 voffset = fn_createVec3(0,offset_v*0.45,0);
      //  voffset = fn_rotatePointQuat(voffset,c->data[i].orientation);
      voffset = fn_multVec4Mat4(m_prime,fn_createVec4Vec3(voffset,0.0)).xyz;

      fn_vec3 poff = fn_addVec3(c->entities[i].aabb.position,voffset);
      poff = fn_addVec3(poff,legg_positions[j/2]);

      // fn_vec3 offset_z = fn_rotatePointQuat(fn_createVec3(fn_sign(legg_positions_prime[j/2].x)*100.0,0,-zdiff*200),c->data[i].orientation);
      //fn_vec3 offset_z = fn_rotatePointQuat(fn_createVec3(fn_sign(legg_positions_prime[j/2].x)*100.0,0,-zdiff*200),c->data[i].orientation);
      // fn_vec3 offset_z = fn_multVec4Mat4(m_prime,fn_createVec4Vec3(fn_createVec3(fn_sign(legg_positions_prime[j/2].x)*100.0,0,-zdiff*200),0.0)).xyz;

      fn_vec3 normal_col;
      bool is_hit = false;
      fn_vec3 p = fn_createVec3(0,0,0);

      float dist_col = -1.0;

      #define THETA_MAX 1.31924961362
      #define THETA_STEP (1.0/6.0)

      float theta_candidates[6] = {THETA_MAX*THETA_STEP*1,THETA_MAX*THETA_STEP*2,THETA_MAX*THETA_STEP*3,THETA_MAX*THETA_STEP*4,THETA_MAX*THETA_STEP*5,THETA_MAX*THETA_STEP*6};

      #undef THETA_MAX
      #undef THETA_STEP

      for (int t_idx = 0; t_idx < 6;t_idx++)
      {
        //
        float y_off = 401.764078036*sin(theta_candidates[t_idx]);

        float x_off = 401.764078036*cos(theta_candidates[t_idx]);

        fn_vec3 offset_z = fn_multVec4Mat4(m_prime,fn_createVec4Vec3(fn_createVec3(fn_sign(legg_positions_prime[j/2].x)*x_off,0,-zdiff*200),0.0)).xyz;




        // float t = 0;
        //L = 401.764078036
        //389.12 +Y 100 +/- X

        fn_vec3 trace_to = fn_createVec3(0,y_off,0);
        // trace_to = fn_rotatePointQuat(trace_to,c->data[i].orientation);
        trace_to = fn_multVec4Mat4(m_prime,fn_createVec4Vec3(trace_to,0.0)).xyz;
        trace_to = fn_addVec3(trace_to,offset_z);
        // fn_vec3 p =  th_trace(c->levelstate->world,poff,fn_addVec3(poff,trace_to),&n,&is_hit,th_getPhysicsMemory(c->levelstate->world,0),&t);


        fn_vec3 normal_col_result;
        bool is_hit_result = false;
        fn_vec3 p_result = fn_createVec3(0,0,0);

        p_result = th_traceVolume(c->levelstate->world,poff,fn_addVec3(poff,trace_to),3.0,&normal_col_result,&is_hit_result,th_getPhysicsMemory(c->levelstate->world,0));


        float damp_sq = 2.0*2.0;
        if ((is_hit_result && fn_distance2(p_result,poff) > dist_col && fn_distance2(p_result,poff) < 3.04*10*6.4*3.04*10*6.4*damp_sq ) || (t_idx == 5 && dist_col == -1.0) )
        {
          dist_col = fn_distance2(p_result,poff);

          is_hit = is_hit_result;
          normal_col = normal_col_result;
          p = p_result;
        }
      }




      fn_vec3 gem_jitter = fn_multVec3s(c->data[i].gem_jitter_x[j/2] ,sin(c->data[i].gem_jitter_t[j/2]*0.025)*c->data[i].gem_jitter_t[j/2]*0.21 );
      c->data[i].gem_jitter_t[j/2] -= dt;
      if (c->data[i].gem_jitter_t[j/2] <= 0.0)
      {
        c->data[i].gem_jitter_t[j/2] = 0.0;
      }
      else
      {
        // fn_printVec3(  c->data[i].gem_jitter_x[j/2]);
        // printf("%f\n",  c->data[i].gem_jitter_t[j/2] );
      }

      fn_mat4 m_prime_gem = m_prime;

      fn_vec3 gem_rel = fn_addVec3(fn_createVec3(0,offset_v*0,0),fn_multVec3(legg_positions_prime[j/2], fn_createVec3(1.25,1.0,1.0)  ) );
      m_prime_gem = fn_multMat4(fn_maketranslate(gem_rel),m_prime);
      m_prime_gem = fn_multMat4(fn_maketranslate(gem_jitter),m_prime_gem);
      m_prime_gem = fn_multMat4(fn_makescale(fn_createVec3s(20.0)),m_prime_gem);

      //sin(c->data[i].gem_jitter_t[j/2]*0.01*3.14159)*




    //   c->data[i].old_forward
    // c->data[i].old_right
    // c->data[i].old_up

      fn_vec3 gem_pos = fn_multVec4Mat4(m_prime,fn_createVec4Vec3(gem_rel,0.0)).xyz;

      c->entities_gems[i*6 + j/2].aabb.position = fn_addVec3(c->entities[i].aabb.position,legg_positions[j/2]);

      if (!c->data[i].set_up || dt == 0.0)
      {
        c->entities_gems[i*6 + j/2].velocity = fn_createVec3s(0);
      }
      else
      {
       c->entities_gems[i*6 + j/2].velocity = fn_multVec3s(fn_subVec3(poff,c->data[i].leg_history[j/2].nodes[0]),1.0/dt);
       //fn_printVec3(c->entities_gems[i*6 + j/2].velocity);
      }

      if (c->data[i].hasgem[j/2])
      {
        c->transforms_gems[i*6 + j/2] = m_prime_gem;
      }
      else
      {
        c->transforms_gems[i*6 + j/2] = fn_makescale(fn_createVec3s(0));
      }

       bool step_state = false;

       fn_vec3 old_foot = c->data[i].footpositions[j/2];
       if (c->data[i].set_up)
       {
         if (c->data[i].stepping[j/2])
         {
           const float sq2 = offset_v*3.0;//0.707106781187;
           const float sq1 = 0.0;
           //
           fn_vec3 la = c->data[i].footpositions[j/2];
           fn_vec3 lb = c->data[i].target_positions[j/2];
           fn_vec3 control0 = fn_multVec3s(fn_normalizeVec3(fn_subVec3(lb,la)),-fn_distance(la,lb)*sq1);
           control0 = fn_addVec3(control0,fn_multVec3s(c->data[i].orientation_direction,-sq2));
           control0 = fn_addVec3(control0,la);

           fn_vec3 control1 = fn_multVec3s(fn_normalizeVec3(fn_subVec3(lb,la)),fn_distance(la,lb)*sq1);
           control1 = fn_addVec3(control1,fn_multVec3s(c->data[i].orientation_direction,-sq2));
           control1 = fn_addVec3(control1,lb);

           if (c->data[i].step_linear[j/2])
           {

             p = fn_lerpVec3(c->data[i].footpositions[j/2],c->data[i].target_positions[j/2],c->data[i].t[j/2]);
           }
           else
           {
             p = CalculatePosition(control0,la,lb,control1,c->data[i].t[j/2]);
           }


            c->data[i].t[j/2] += dt*0.001;


          c->data[i].t[j/2] = fn_clamp(c->data[i].t[j/2],0.0,1.0);
          if (c->data[i].t[j/2] >= 1.0)
          {
            if (!c->data[i].step_linear[j/2])
            {
              th_playSoundIfNotPlaying(&c->data[i].impact_sounds[j/2],p,sound_horse_impact1 + th_random() % 3,0.6 );

            }
            if (fn_distance2(c->levelstate->player_e.aabb.position,p) < 400*400 && c->levelstate->player->screenshake_amplitude < 0.01)
            {
              c->levelstate->player->screenshake_f = 0.13*0.3;
              c->levelstate->player->screenshake_t = 0;
              c->levelstate->player->screenshake_amplitude = 0.035;
            }



            c->data[i].footpositions[j/2] = p;
            c->data[i].stepping[j/2] = false;
          }
          step_state = true;



         }
         else
         {

           if (!is_hit)
           {
             if (!c->data[i].previously_free[j/2] || c->data[i].previously_stepping[j/2] )
             {
               c->data[i].target_positions[j/2] = p;
               c->data[i].t[j/2] = 0;
               c->data[i].stepping[j/2] = true;
               c->data[i].step_linear[j/2] = true;
               p = c->data[i].footpositions[j/2];
             }
             else if (fn_distance(p,c->data[i].footpositions[j/2]) > 150  && !is_hit)
             {
                c->data[i].target_positions[j/2] = p;
                c->data[i].t[j/2] = 0;
                c->data[i].stepping[j/2] = true;
                c->data[i].step_linear[j/2] = true;
                p = c->data[i].footpositions[j/2];
             }
             else
             {
             //   // if (j/2 == 2)
             //   //  th_printlnDevConsole("Step");
                c->data[i].footpositions[j/2] = p;
            }


           }
           else
           {
             float leg_ext = fn_distance(c->data[i].leg_history[j/2].nodes[0],c->data[i].leg_history[j/2].nodes[2]);



             int other_index = j/2;
             if (j/2 % 2 ==0)
             {
               other_index += 1;
             }
             else
             {
               other_index -= 1;
             }

             bool same_side_stepping = false;
             if (j/2 == 0){
               same_side_stepping = c->data[i].stepping[4] || c->data[i].stepping[2];
             }
             else if (j/2 == 1){
               same_side_stepping = c->data[i].stepping[5] || c->data[i].stepping[3];
             }
             else if (j/2 == 2){
               same_side_stepping = c->data[i].stepping[0] || c->data[i].stepping[4];
             }
             else if (j/2 == 3){
               same_side_stepping = c->data[i].stepping[1] || c->data[i].stepping[5];
             }
             else if (j/2 == 4){
               same_side_stepping = c->data[i].stepping[0] || c->data[i].stepping[2];
             }
             else if (j/2 == 5){
               same_side_stepping = c->data[i].stepping[1] || c->data[i].stepping[3];
             }

             if (fn_distance(p,c->data[i].footpositions[j/2]) > 150 && !c->data[i].stepping[other_index] && !same_side_stepping && is_hit)
             {
                c->data[i].target_positions[j/2] = p;
                c->data[i].t[j/2] = 0;
                c->data[i].stepping[j/2] = true;
                c->data[i].step_linear[j/2] = false;
                if (leg_ext < offset_v*1.4)
                {
                  c->data[i].step_linear[j/2] = true;
                }
                p = c->data[i].footpositions[j/2];

                if (c->data[i].mechindex ==0 ){
                  th_playSoundIfNotPlaying(&c->data[i].mech1,poff,sound_horse_mech1,0.6 );
                } else {
                  th_playSoundIfNotPlaying(&c->data[i].mech2,poff,sound_horse_mech2,0.6 );
                }
                c->data[i].mechindex = (c->data[i].mechindex + 1) % 2;

             }
             else if (fn_distance(p,c->data[i].footpositions[j/2]) > 150 && !c->data[i].stepping[other_index] && !same_side_stepping && !is_hit)
             {
                c->data[i].target_positions[j/2] = p;
                c->data[i].t[j/2] = 0;
                c->data[i].stepping[j/2] = true;
                c->data[i].step_linear[j/2] = true;
                p = c->data[i].footpositions[j/2];

                if (c->data[i].mechindex ==0 ){
                  th_playSoundIfNotPlaying(&c->data[i].mech1,poff,sound_horse_mech1,0.6 );
                } else {
                  th_playSoundIfNotPlaying(&c->data[i].mech2,poff,sound_horse_mech2,0.6 );
                }
                c->data[i].mechindex = (c->data[i].mechindex + 1) % 2;
             }
             else
             {
                p = c->data[i].footpositions[j/2];
             }
           }

         }





       }

       // if (c->data[i].stepping[j/2] && c->data[i].step_linear[j/2] && c->data[i].t[j/2] > 0.0)
       // {
       //
       // }

       c->data[i].previously_free[j/2] = !is_hit;
       c->data[i].previously_stepping[j/2] = step_state;



      fn_vec3 nodes[3];

      nodes[0] = fn_addVec3(poff,fn_createVec3(0,0,0));
      // fn_vec3 hint_point = fn_createVec3(-fn_sign(legg_positions_prime[j/2].x),offset_v,0);
      fn_vec3 hint_point =fn_multVec3s(fn_addVec3(poff,p),0.5);

      fn_vec3 hint_offset = fn_multVec4Mat4(m_prime,fn_createVec4(-fn_sign(legg_positions_prime[j/2].x),0,zdiff,0.0)).xyz;
      hint_point = fn_addVec3(hint_point,hint_offset);
      // hint_point = fn_addVec3(hint_point,fn_rotatePointQuat(fn_createVec3(-fn_sign(legg_positions_prime[j/2].x),0,zdiff),rot_quat2));
      // hint_point = fn_rotatePointQuat(hint_point,rot_quat2);
      nodes[1] = hint_point;//fn_addVec3(poff,hint_point);
      nodes[2] = p;//fn_addVec3(poff,fn_createVec3(0,offset_v*extend,0));


      solveJointPosition(nodes[2],nodes,offset_v);

      if (!c->data[i].set_up)
      {
        c->data[i].leg_history[j/2].nodes[0] = nodes[0];
        c->data[i].leg_history[j/2].nodes[1] = nodes[1];
        c->data[i].leg_history[j/2].nodes[2] = nodes[2];
      }



      leggoffset[0] = fn_multVec3s(fn_addVec3(nodes[0],nodes[1]),0.5);
      leggoffset[1] = fn_multVec3s(fn_addVec3(nodes[1],nodes[2]),0.5);
      fn_vec3 tr = fn_createVec3(1,-1,1);

      if (!c->data[i].set_up )
      {
        legq[0] = fn_getRotationQuaternion(fn_createVec3(0,1,0),fn_multVec3(fn_normalizeVec3(fn_subVec3(nodes[1],nodes[0])),tr));
        legq[1] = fn_getRotationQuaternion(fn_createVec3(0,1,0),fn_multVec3(fn_normalizeVec3(fn_subVec3(nodes[2],nodes[1])),tr));
        c->data[i].leg_orientations[j + 0] = legq[0];
        c->data[i].leg_orientations[j + 1] = legq[1];
        c->data[i].leg_offsets[j + 0] = leggoffset[0];
        c->data[i].leg_offsets[j + 1] = leggoffset[1];
      }
      else
      {
        tr = fn_createVec3(1,1,1);

        fn_vec3 top_pos = fn_createVec3(0,-offset_v*0.5,0);
        fn_vec3 bottom_pos = fn_createVec3(0,offset_v*0.5,0);

        fn_vec3 old_node_0 = fn_addVec3(fn_rotatePointQuat(top_pos,c->data[i].leg_orientations[j + 0]),c->data[i].leg_offsets[j + 0]);
        fn_vec3 old_node_1 = fn_addVec3(fn_rotatePointQuat(bottom_pos,c->data[i].leg_orientations[j + 0]),c->data[i].leg_offsets[j + 0]);
        fn_vec3 old_node_2 = fn_addVec3(fn_rotatePointQuat(bottom_pos,c->data[i].leg_orientations[j + 1]),c->data[i].leg_offsets[j + 1]);



        fn_vec3 bone_1_from = fn_multVec3(fn_normalizeVec3(fn_subVec3(old_node_1 , old_node_0)),tr);
        fn_vec3 bone_1_to = fn_multVec3(fn_normalizeVec3(fn_subVec3(nodes[1] ,nodes[0])),tr);

        fn_vec3 bone_2_from = fn_multVec3(fn_normalizeVec3(fn_subVec3(old_node_2 , old_node_1)),tr);
        fn_vec3 bone_2_to = fn_multVec3(fn_normalizeVec3(fn_subVec3(nodes[2] ,nodes[1])),tr);

        legq[0] = fn_getRotationQuaternion(bone_1_from,bone_1_to);
        legq[1] = fn_getRotationQuaternion(bone_2_from,bone_2_to);

        legq[0].w = legq[0].w*-1;
        legq[1].w = legq[1].w*-1;

        c->data[i].leg_orientations[j + 0] = fn_multquat(c->data[i].leg_orientations[j + 0],legq[0]);
        c->data[i].leg_orientations[j + 1] = fn_multquat(c->data[i].leg_orientations[j + 1],legq[1]);
        c->data[i].leg_offsets[j + 0] = leggoffset[0];
        c->data[i].leg_offsets[j + 1] = leggoffset[1];
      }


        LEG_ASSIGNMENT(i*c->legs_per_count + j + 0) = fn_translaterotatescaleq(leggoffset[0],  c->data[i].leg_orientations[j + 0],fn_createVec3s(10));
        LEG_ASSIGNMENT(i*c->legs_per_count + j + 1) = fn_translaterotatescaleq(leggoffset[1],  c->data[i].leg_orientations[j + 1],fn_createVec3s(10));

        c->entities_legs[i*c->legs_per_count + j + 0].aabb.position = leggoffset[0];
        c->entities_legs[i*c->legs_per_count + j + 1].aabb.position = leggoffset[1];

        th_pushOccluderFrame(fn_createVec4Vec3(nodes[2],170.0));
        th_pushOccluderFrame(fn_createVec4Vec3(nodes[1],170.0));

        th_pushOccluderFrame(fn_createVec4Vec3(nodes[0],230.0));




      if (!c->data[i].set_up)
      {
        c->data[i].footpositions[j/2] = nodes[2];
      }

        // contacts++;
      leg_extensions[j/2] = fn_distance(nodes[2],nodes[0]);
      //endpoints[j/2] = nodes[2];
      contacted[j/2] = false;
      if ((!c->data[i].stepping[j/2] && is_hit && leg_extensions[j/2] < offset_v*1.7) )//|| is_hit_foot
      {
        num_contacts++;
        contacted[j/2] = true;
        //end_normals[j/2] = n;
        c->data[i].footnormals[j/2] = normal_col;
        c->data[i].footcontacted[j/2] = true;
      }
      else
      {
        c->data[i].footcontacted[j/2] = false;
      }

      c->data[i].leg_history[j/2].nodes[0] = nodes[0];
      c->data[i].leg_history[j/2].nodes[1] = nodes[1];
      c->data[i].leg_history[j/2].nodes[2] = nodes[2];

    }

    float extension_score_left = 0.0;
    float extension_score_right = 0.0;



    for (int k = 0; k < 6; k++) {
      if (!contacted[k])
      {
        if (k % 2 == 0)
        {
          extension_score_right += 200.0;
        }
        else
        {
          extension_score_left += 200.0;
        }
      }
      else
      {
        if (k % 2 == 0)
        {
          extension_score_right += 100*((leg_extensions[k]/(offset_v*2.0)));
        }
        else
        {
          extension_score_left += 100*((leg_extensions[k]/(offset_v*2.0)));
        }
      }
    }
    //th_printlnDevConsole("%f %f",extension_score_right,extension_score_left);
    if (fabs(extension_score_left - extension_score_right) < 50)
    {
      if (fabs(c->data[i].angle_adjust) > 0.04)
      {
        c->data[i].angle_adjust += 0.001*dt*-fn_sign(c->data[i].angle_adjust);
      }

    }
    else if (extension_score_right > extension_score_left)
    {
      c->data[i].angle_adjust += 0.001*dt*0.1;
    }
    else {
      c->data[i].angle_adjust -= 0.001*dt*0.1;
    }

    if (!contacted[3] && !contacted[2] && contacted[5] && contacted[4])
    {
      c->data[i].pitch_adjust -= 0.001*dt*0.1;
    }
    else if (!contacted[5] && !contacted[4] && contacted[2] && contacted[3])
    {
      c->data[i].pitch_adjust += 0.001*dt*0.1;
    }
    else
    {
      if (fabs(c->data[i].pitch_adjust) > 0.04)
      {
        c->data[i].pitch_adjust += 0.001*dt*-fn_sign(c->data[i].pitch_adjust);
      }
    }


    bool all_under = true;
    float max_ext = 0;
    float min_ext = offset_v*2.0;
    bool set_step = false;
    for (int k = 0; k < 6; k++) {
      if (!c->data[i].stepping[k] && (leg_extensions[k] >= offset_v*1.8)  )
      {
        all_under = false;
      }

      if (!c->data[i].stepping[k] && (!set_step || leg_extensions[k] > max_ext))
      {
        max_ext = leg_extensions[k];
        set_step = true;
      }

      if (!c->data[i].stepping[k] && ( leg_extensions[k] < min_ext))
      {
        min_ext = leg_extensions[k];
      }
    }

    if ((all_under && max_ext < offset_v*1.65) || min_ext/offset_v < 1.5)
    {
      c->data[i].height_adjust += 0.01*dt;
    }
    else
    {
      if (c->data[i].height_adjust > 0.01*35)
      {
        c->data[i].height_adjust -= 0.01*dt;
      }
    }

    //th_printlnDevConsole("%f",c->data[i].height_adjust);

    c->data[i].angle_adjust = fn_clamp(c->data[i].angle_adjust,-3.14159*0.5,3.14159*0.5);
    c->data[i].pitch_adjust = fn_clamp(c->data[i].pitch_adjust,-3.14159*0.5,3.14159*0.5);
    c->data[i].height_adjust = fn_clamp(c->data[i].height_adjust,0,50);


    c->transforms[i] = m_prime_s;//fn_translaterotatescaleq(c->entities[i].aabb.position,c->data[i].orientation,fn_createVec3s(10));
    c->data[i].set_up = true;
    positionHatch(c,i);

    if (!spawned)
    {
      if (th_time() - c->levelstate->level_start_time > c->data[i].spawn_when - 1000)
      {
        if (!c->data[i].played_spawnsound && unstall)
        {
          a_VirtualSource* s = a_playVirtualSource(sound_spawn_creature,1,c->entities[i].aabb.position,NULL );
          a_setVSLoop(s,false);
          a_setVSPos(s,c->entities[i].aabb.position);
          a_setVSVel(s,fn_createVec3s(0.0));
          a_setVSGain(s,1.0);

          a_duckVS(s,3500.0,0.40);
          a_setVSRolloff(s,0.3);

          c->levelstate->num_spawners_current = c->levelstate->num_spawners_current + 1;
          c->data[i].played_spawnsound = true;
        }

        c->data[i].fadeout_spawn = c->data[i].fadeout_spawn - 0.003*dt;
        if (c->data[i].fadeout_spawn < 0 )
        {
          c->data[i].fadeout_spawn = 0;
        }
        fn_mat4 fdmat = fn_makescale(fn_createVec3s(1.0 - fn_clamp(c->data[i].fadeout_spawn ,0.0,1.0)));
        c->transforms[i] = fn_multMat4(fdmat,c->transforms[i]);
        positionHatch(c,i);

        for (int j = 0 ; j < 12;j++)
        {

          LEG_ASSIGNMENT(i*12 + j) = fn_multMat4(fdmat,LEG_ASSIGNMENT(i*12 + j));
        }

        for (int j = 0 ; j < 6;j++)
        {
          c->transforms_gems[i*6 + j] = fn_multMat4(fdmat,c->transforms_gems[i*6 + j]);
        }
      }
    }

    if (th_time() > c->data[i].spawn_children_timer + CHILD_SPAWN_INTERVAL - 600.0)
    {
      c->hatch_interp[i] = c->hatch_interp[i] + dt*(1.0/500.0);

      if (c->hatch_interp[i] > 1.0)
      {
        c->hatch_interp[i] = 1.0;
      }
    }
    else
    {
      c->hatch_interp[i] = c->hatch_interp[i] - dt*(1.0/(CHILD_SPAWN_INTERVAL - 1500.0));

      if (c->hatch_interp[i] < 0.0)
      {
        c->hatch_interp[i] = 0.0;
      }
    }


    if (th_time() > c->data[i].spawn_children_timer + CHILD_SPAWN_INTERVAL && ENABLE_SPAWNING && spawned)
    {
      int spawn_choice = c->data[i].spawn_children_flipflop;
      c->data[i].spawn_children_flipflop = (c->data[i].spawn_children_flipflop + 1) % 2;
      th_playSoundIfNotPlaying(&c->data[i].spawn_sound,c->entities[i].aabb.position,sound_horse_givebirth,1.0 );
      a_setVSRolloff(c->data[i].spawn_sound,0.2);

      if (!c->data[i].easymode)
      {
        if (spawn_choice == 0)
        {

          bool success_spawn = true;
          fn_vec3 spos = selectSpawnPosition(c->entities[i].aabb.position,fn_addVec3(c->entities[i].aabb.position,fn_multVec3s(c->data[i].old_up,100.0)),c,164,&success_spawn);
          if (success_spawn)
          {

            int sham_id = th_shamblersSpawn(c->levelstate->shamblers,spos);
            if (sham_id >= 0)
            {
              c->levelstate->shamblers->entities[sham_id].velocity = fn_multVec3s(c->data[i].old_up,1.0);
              c->levelstate->shamblers->entities[sham_id].velocity = fn_addVec3(c->levelstate->shamblers->entities[sham_id].velocity,fn_multVec3s(c->data[i].old_forward,-0.3));
            }
          }


        }
        else
        {

          bool success_spawn = true;
          fn_vec3 spos = selectSpawnPosition(c->entities[i].aabb.position,fn_addVec3(c->entities[i].aabb.position,fn_multVec3s(c->data[i].old_up,100.0)),c,75,&success_spawn);
          if (success_spawn)
          {

            int eye_id = th_eyeballSpawn(c->levelstate->eyeball,spos);
            if (eye_id >= 0)
            {
              c->levelstate->eyeball->entities[eye_id].velocity = fn_multVec3s(c->data[i].old_up,0.5);
              c->levelstate->eyeball->entities[eye_id].velocity = fn_addVec3(c->levelstate->eyeball->entities[eye_id].velocity,fn_multVec3s(c->data[i].old_forward,-0.3));
            }
          }

        }
      }


      // th_boidsSpawn(c->levelstate->boidgroups,fn_addVec3(c->entities[i].aabb.position,fn_createVec3(1,0,0)));
      // th_boidsSpawn(c->levelstate->boidgroups,fn_addVec3(c->entities[i].aabb.position,fn_createVec3(1,1,0)));

      fn_vec3 vortexCenter = fn_addVec3(c->entities[i].aabb.position,fn_multVec3s(c->data[i].old_up,0));

      th_spawnMetalNoSound(vortexCenter,-1);

      int dim = 3;

      if (c->data[i].easymode)
      {
        dim = 2;
      }

      for (int r = 0 ; r < dim*dim; r++)
      {
        for (int z = 0; z < dim;z++)
        {

          float angle = (float)r / (float)(dim*dim);
          angle = angle*2*3.14159;
          float radius = 40.0;
          float z_increment = 40.0;

          fn_vec3 pos;
          pos.x = -sin(angle) * radius; // Negative to create a clockwise rotation
          pos.y = -z_increment*z;
          pos.z = cos(angle) * radius; // Assuming 2D motion in x-y plane

          fn_vec3 tangent = fn_normalizeVec3(fn_cross(fn_createVec3(0,1,0),fn_createVec3(pos.x,0,pos.z)));
          fn_vec3 velocity = fn_multVec3s(tangent,0.2);

          float pos_len = fn_length(pos);
          float vel_len = fn_length(velocity);

          pos = fn_transformNormal(fn_normalizeVec3(pos),m_prime);
          velocity = fn_transformNormal(fn_normalizeVec3(velocity),m_prime);
          pos = fn_multVec3s(pos,pos_len);
          velocity = fn_multVec3s(velocity,vel_len);

          pos = fn_addVec3(pos,vortexCenter);

          fn_vec3 p_boid = pos;

          bool success_spawn = true;
          fn_vec3 spos = selectSpawnPosition(vortexCenter,p_boid,c,35,&success_spawn);
          if (success_spawn)
          {
            int index = th_boidsSpawn(&c->levelstate->boidgroups[0],spos);
            if (index >= 0)
            {
              c->levelstate->boidgroups[0].entities[index].aabb.position = spos;

              velocity = fn_addVec3(velocity,fn_multVec3s(c->data[i].old_forward,-0.3));

              velocity = fn_addVec3(velocity,fn_multVec3s(c->data[i].old_up,0.25));

              c->levelstate->boidgroups[0].entities[index].velocity = velocity;// fn_normalizeVec3(fn_createVec3(x*60,-y*60,z*60));
              c->levelstate->boidgroups[0].prime_velocity[index] = velocity;
              c->levelstate->boidgroups[0].prime_target[index] = vortexCenter;
            }
          }


          //fn_printVec3(velocity);
        }
      }

      c->data[i].spawn_children_timer = th_time();
    }







  }

  //c->count
  unsigned int vert_chunk = c->wire_verts_count/c->num_wire_bundles;

  for (int idx = 0 ; idx < c->num_wire_bundles;idx++)
  {
    //find "i" by checking each possible i, make sure its alive, and then pick the one with the most recent spawntime
    //if you couldnt find anything, pick the one with the soonest spawntime
    //compare target "i" to the established one, if different, then trigger switch

    float selected_spawntime = 0.0;
    int target_idx = -1;

    for (int j = 0 ; j < c->wire_bundle_horseid_counts[idx];j++)
    {
      int i_test = c->wire_bundle_horse_ids[idx][j];
      bool spawned = c->data[i_test].played_spawnsound;

      if (!spawned)
      {
        continue;
      }
      float spawntime = c->data[i_test].spawn_when;

      if (target_idx == -1 || spawntime > selected_spawntime )
      {
        selected_spawntime = spawntime;
        target_idx = i_test;
      }
    }

    if (target_idx == -1)
    {
      for (int j = 0 ; j < c->wire_bundle_horseid_counts[idx];j++)
      {
        int i_test = c->wire_bundle_horse_ids[idx][j];
        float spawntime = c->data[i_test].spawn_when;
        if (target_idx == -1 || spawntime < selected_spawntime)
        {
          selected_spawntime = spawntime;
          target_idx = i_test;
        }
      }
    }

    if (c->wire_bundle_target_idx[idx] == -1)
    {
      c->wire_bundle_target_idx[idx] = target_idx;
    }

    if (target_idx != c->wire_bundle_target_idx[idx])
    {
      c->wire_states[idx] = TH_HORSE_WIRE_SWITCH;
    }

    int i = c->wire_bundle_target_idx[idx];
    //int i = c->wire_bundle_horse_ids[idx][0];

    fn_vec3 master_start = c->wire_plug_origins[idx];//fn_createVec3(133.901901, 132.388123, -471.449463);



    fn_vec3 temp_up_vec = c->wire_plug_normals[idx];



    if (!c->data[i].gibbed && c->wire_states[idx] != TH_HORSE_WIRE_RETRACT && c->wire_states[idx] != TH_HORSE_WIRE_SWITCH )
    {


      if (c->data[i].played_spawnsound )
      {
        c->wire_orients[idx] = c->transforms[i];
      }
      else
      {
        fn_vec3 start_pos = fn_addVec3(master_start,fn_multVec3s(temp_up_vec,600));

        fn_vec3 xbasis = fn_createVec3(1,0,0);
        fn_vec3 ybasis = fn_createVec3(0,1,0);
        fn_vec3 zbasis = fn_createVec3(0,0,1);


        fn_mat4 sqmat = fn_createMat4(xbasis.x,xbasis.y,xbasis.z,0,ybasis.x,ybasis.y,ybasis.z,0,zbasis.x,zbasis.y,zbasis.z,0,0,0,0,1);

        fn_quat start_q = fn_mat4toquat(sqmat);

        c->wire_orients[idx] = fn_translaterotatescaleq(start_pos,start_q,fn_createVec3s(10));
      }
    }


    if (c->data[i].gibbed && c->wire_states[idx] != TH_HORSE_WIRE_SWITCH)
    {
      c->wire_states[idx] = TH_HORSE_WIRE_RETRACT;
    }



    fn_mat4 tr_backconnect = fn_maketranslate(fn_createVec3(-4.5,1.95,38));
    tr_backconnect = fn_multMat4(tr_backconnect,c->wire_orients[idx]);

    int wire_bundle = 4;


    fn_vec3 master_end = fn_transformVec3(fn_createVec3(0,0,0),tr_backconnect);

    float wire_dist = fn_distance(master_end,master_start);
    if (wire_dist > 850 && c->wire_states[idx] == TH_HORSE_WIRE_EXTEND)
    {
      if (wire_dist > 850 + 35)
      {
        c->wire_states[idx] = TH_HORSE_WIRE_RETRACT;
      }
      //master_end = fn_addVec3(master_start,fn_multVec3s(fn_normalizeVec3(fn_subVec3(master_end,master_start)),850.0));
    }

    fn_vec3 end_forward = fn_normalizeVec3(fn_transformNormal(fn_createVec3(0,0,-1),c->wire_orients[idx]));
    fn_vec3 end_right = fn_normalizeVec3(fn_transformNormal(fn_createVec3(1,0,0),c->wire_orients[idx]));

    // fn_vec3 master_end_normal = fn_subVec3(master_end,master_start);
    // master_end_normal.y = 0;
    // master_end_normal = fn_normalizeVec3(master_end_normal);

    fn_vec3 master_end_normal = end_forward;// fn_multVec3s(end_forward,1);

    fn_vec3 up_temp,right_temp;

    if (fn_equalVec3(c->wirebase_orients_up[idx],fn_createVec3s(0)))
    {
      //float forward_len = fn_length(fn_subVec3(master_end,master_start));
      fn_vec3 forward_temp = fn_normalizeVec3(fn_subVec3(c->entities[i].aabb.position,master_start));

      right_temp = fn_normalizeVec3(fn_cross(temp_up_vec,forward_temp));

      right_temp = fn_subVec3(right_temp,fn_multVec3s(temp_up_vec,fn_dot(right_temp,temp_up_vec)));
      right_temp = fn_normalizeVec3(right_temp);

      //right_temp = temp_up_vec;

       //up_temp = fn_normalizeVec3(fn_cross(right_temp,forward_temp));
      up_temp = temp_up_vec;

      // fn_printVec3(right_temp);
      // fn_printVec3(up_temp);
      // fn_printVec3(forward_temp);

       c->wirebase_orients_right[idx] = right_temp;
       c->wirebase_orients_up[idx] = up_temp;
    }
    else
    {
      right_temp = c->wirebase_orients_right[idx];
      up_temp = c->wirebase_orients_up[idx];
    }



    fn_vec3 starts[4];
    fn_vec3 mids[4];
    fn_vec3 mid2s[4];
    fn_vec3 ends[4];


    master_end = fn_addVec3(master_end,fn_multVec3s(master_end_normal,-30));


    starts[0] = master_start;
    starts[1] = fn_addVec3(master_start,fn_multVec3s(right_temp,14*2));
    starts[2] = fn_addVec3(master_start,fn_multVec3s(right_temp,14*4));
    starts[3] = fn_addVec3(master_start,fn_multVec3s(right_temp,14*6));

    for (int k = 0 ; k < 4;k++)
    {
      th_pushOccluderFrame(fn_createVec4Vec3(fn_addVec3(starts[k],fn_multVec3s(temp_up_vec,10.0)),50.0));
    }

    mids[0] = fn_addVec3(starts[0],fn_multVec3s(temp_up_vec,250));
    mids[1] = fn_addVec3(starts[1],fn_multVec3s(temp_up_vec,250));
    mids[2] = fn_addVec3(starts[2],fn_multVec3s(temp_up_vec,250));
    mids[3] = fn_addVec3(starts[3],fn_multVec3s(temp_up_vec,250));


    mid2s[0] = master_end;
    mid2s[1] = fn_addVec3(master_end,fn_multVec3s(end_right,14*2));
    mid2s[2] = fn_addVec3(master_end,fn_multVec3s(end_right,14*4));
    mid2s[3] = fn_addVec3(master_end,fn_multVec3s(end_right,14*6));

    for (int k = 0; k < 4;k++)
    {
      mid2s[k] = fn_addVec3(mid2s[k],fn_multVec3s(master_end_normal,-100));
    }


    ends[0] = master_end;
    ends[1] = fn_addVec3(master_end,fn_multVec3s(end_right,14*2));
    ends[2] = fn_addVec3(master_end,fn_multVec3s(end_right,14*4));
    ends[3] = fn_addVec3(master_end,fn_multVec3s(end_right,14*6));

    th_SplineFrame frames[4];

    float alpha_spawn = 0.0;


    if (c->wire_states[idx] == TH_HORSE_WIRE_START)
    {
      alpha_spawn = 0;

      if (th_time() - c->levelstate->level_start_time > c->data[i].spawn_when + 1000.0)
      {
        c->wire_states[idx] = TH_HORSE_WIRE_EXTEND;
      }
    }
    else if (c->wire_states[idx] == TH_HORSE_WIRE_EXTEND)
    {
      c->wire_extens[idx] = c->wire_extens[idx] + (1.0/2000.0)*dt;
      c->wire_extens[idx] = fn_clamp(c->wire_extens[idx],0.0,1.0);
      alpha_spawn = c->wire_extens[idx];
    }
    else if (c->wire_states[idx] == TH_HORSE_WIRE_RETRACT)
    {
      c->wire_extens[idx] = c->wire_extens[idx] - (1.0/5000.0)*dt;
      c->wire_extens[idx] = fn_clamp(c->wire_extens[idx],0.2,1.0);

      alpha_spawn = c->wire_extens[idx];
    }
    else if (c->wire_states[idx] == TH_HORSE_WIRE_SWITCH)
    {
      c->wire_extens[idx] = c->wire_extens[idx] - (1.0/5000.0)*dt;
      c->wire_extens[idx] = fn_clamp(c->wire_extens[idx],0.0,1.0);

      alpha_spawn = c->wire_extens[idx];

      if (alpha_spawn == 0.0)
      {
        c->wire_states[idx] = TH_HORSE_WIRE_START;
        c->wire_bundle_target_idx[idx] = target_idx;
      }
    }

    //printf("%i %i\n",c->wire_states[idx],idx);


    th_GpuData* wire_datas = th_generateWireBundle(temp_alloc,starts,mids,mid2s,ends,4,temp_up_vec,master_end_normal,up_temp,alpha_spawn,frames);

    fn_vec3 right_vec_tip = fn_normalizeVec3(fn_subVec3(frames[0].position,frames[3].position));
    // fn_vec3 up_vec_tip = frames[0].up;
    // fn_vec3 forward_vec_tip = fn_normalizeVec3(fn_cross(right_vec_tip,up_vec_tip));
    //up_vec_tip = fn_normalizeVec3(fn_cross(forward_vec_tip,right_vec_tip));
    fn_vec3 forward_vec_tip = frames[0].forward;
    fn_vec3 up_vec_tip = fn_normalizeVec3(fn_cross(forward_vec_tip,right_vec_tip));
    forward_vec_tip = fn_normalizeVec3(fn_cross(right_vec_tip,up_vec_tip));

    fn_vec3 xbasis = right_vec_tip;
    fn_vec3 ybasis = up_vec_tip;
    fn_vec3 zbasis = forward_vec_tip;

    fn_mat4 sqmat = fn_createMat4(xbasis.x,xbasis.y,xbasis.z,0,ybasis.x,ybasis.y,ybasis.z,0,zbasis.x,zbasis.y,zbasis.z,0,0,0,0,1);

    fn_vec3 plugpos = frames[0].position;
    plugpos = fn_addVec3(plugpos,fn_multVec3s(forward_vec_tip,60));
    plugpos = fn_addVec3(plugpos,fn_multVec3s(right_vec_tip,-40));

    c->connector_mats[idx] = fn_translaterotatescaleq(plugpos,fn_mat4toquat(sqmat),fn_createVec3s(10));


    th_GpuDataOffsets wire_offsets = th_mergeGpuData(temp_alloc,wire_datas,4,TH_INCREMENT_INDICES);
    memcpy(&c->wire_verts[vert_chunk*idx],wire_offsets.data.verts,sizeof(th_Vertex)*vert_chunk);




  }


  vert_chunk = c->liquid_vert_count/c->count;

  for (int i = 0 ; i < c->count ; i ++)
  {
    if ( !c->data[i].gibbed && c->data[i].played_spawnsound )
    {
      unsigned int my_frame = i % 4;//amortize over 4 frames
      my_frame = (th_frame() % 4 == my_frame);
      if (!c->simmed_fluid_once[i] || my_frame)
      {
        th_updateLiquidSim(&c->fsim[i],dt*4);
        c->simmed_fluid_once[i] = true;
      }

    }

    if (c->data[i].spawn_children_timer == th_time())
    {
      c->liquid_sim_timers[i] = th_time() - (CHILD_SPAWN_INTERVAL + 0.001 );
    }

    if (th_time() - c->liquid_sim_timers[i] > CHILD_SPAWN_INTERVAL && !c->data[i].gibbed && c->data[i].played_spawnsound )
    {
      const float liquid_ms = 100.0;

      if (th_time() - c->liquid_sim_timers[i] > CHILD_SPAWN_INTERVAL + liquid_ms )
      {
        c->liquid_sim_timers[i] = th_time();
      }
      else
      {
        //float liquid_ms = th_time() - c->liquid_sim_timers[0] - 7000;
        th_addCentalLiftLiquidSim(&c->fsim[i],(40.0/liquid_ms)*dt,3);
      }




    }

    fn_mat4 local_liquid = fn_translaterotatescale(fn_createVec3(0,0.0,2.0),fn_radians(90.0),fn_createVec3(0,1,0),fn_createVec3(8.5,9.0,9.6));

    th_GpuData liquid_mesh = th_generateLiquidMesh(temp_alloc,&c->fsim[i],local_liquid);



    fn_mat4 ltrans = c->transforms[i];
    r_transformThorMesh(&liquid_mesh,ltrans);

    memcpy(&c->liquid_verts[i*vert_chunk],liquid_mesh.verts,sizeof(th_Vertex)*vert_chunk);
  }



  //th_free(&temp_alloc);
  th_finishAllocatorTemporary(temp_alloc);
}

int th_horseDamageCallback(void* ep,void* pep,float dt,void* world)
{
  th_Entity* e = (th_Entity*)ep;
  th_Entity* player = (th_Entity*)pep;



  if (th_time() > e->time_of_damage + 3000)
  {
    fn_vec3 dir = fn_normalizeVec3(fn_subVec3(player->aabb.position,e->aabb.position));
    player->velocity = fn_addVec3(player->velocity,fn_multVec3s(dir,2));
    player->velocity = fn_addVec3(player->velocity,e->velocity);
    e->time_of_damage = th_time();
    return 10;
  }
  else
  {
    return 0;
  }


}

void th_horseInitialize(th_Allocator* alloc,th_HorseGroup* c,int count,fn_vec3* positions,float* times,th_LevelState* levelstate)
{
  th_createAllocatorTemporary(alloc,&c->temp_alloc);

  c->alloc = alloc;
  const float gem_starting_health = 120.0;

  c->levelstate = levelstate;
  c->count = count;
  c->data = th_alloc(alloc,sizeof(th_HorseData)*count);
  c->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  c->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->hatch_transforms = th_alloc(alloc,sizeof(fn_mat4)*count);

  c->hatch_interp = th_alloc(alloc,sizeof(float)*count);

  int legs_per_count = 12;
  int legs_per_count_div2 = 6;

  c->leg_count = count*legs_per_count;
  c->leg_count_div2 = count*(legs_per_count/2);

  c->legs_per_count = legs_per_count;
  c->entities_legs = th_alloc(alloc,sizeof(th_Entity)*count*legs_per_count);

  // c->transforms_legs = th_alloc(alloc,sizeof(fn_mat4)*count*legs_per_count);
  c->transforms_legs_upper = th_alloc(alloc,sizeof(fn_mat4)*count*legs_per_count_div2);
  c->transforms_legs_lower = th_alloc(alloc,sizeof(fn_mat4)*count*legs_per_count_div2);




  c->transforms_gems = th_alloc(alloc,sizeof(fn_mat4)*count*6);
  c->gem_count = count*6;
  c->entities_gems = th_alloc(alloc,sizeof(th_Entity)*count*6);





  //find duplicated wires
  c->num_wire_bundles = 0;
  c->wire_bundle_horse_ids = NULL;
  c->wire_bundle_horseid_counts = NULL;
  fn_vec3* used_spawnpositions = malloc(sizeof(fn_vec3)*count);
  int spawnpos_unique = 0;

  for (int i = 0 ; i < count ; i++)
  {
    int found_idx = -1;
    for (int j = 0 ; j < spawnpos_unique;j++)
    {
      if (fn_equalVec3(positions[i],used_spawnpositions[j]))
      {
        found_idx = j;
        break;
      }
    }

    if (found_idx == -1)
    {
      spawnpos_unique = spawnpos_unique + 1;
      used_spawnpositions = realloc(used_spawnpositions,sizeof(fn_vec3)*spawnpos_unique);
      used_spawnpositions[spawnpos_unique - 1] = positions[i];

      c->wire_bundle_horse_ids = realloc(c->wire_bundle_horse_ids,sizeof(int*)*(spawnpos_unique));
      c->wire_bundle_horseid_counts = realloc(c->wire_bundle_horseid_counts,sizeof(int)*(spawnpos_unique));
      c->wire_bundle_horse_ids[spawnpos_unique - 1] = malloc(sizeof(int)*1);
      c->wire_bundle_horse_ids[spawnpos_unique - 1][0] = i;
      c->wire_bundle_horseid_counts[spawnpos_unique - 1] = 1;
    }
    else
    {
      c->wire_bundle_horseid_counts[found_idx] = c->wire_bundle_horseid_counts[found_idx] + 1;
      c->wire_bundle_horse_ids[found_idx] = realloc(c->wire_bundle_horse_ids[found_idx],sizeof(int)*c->wire_bundle_horseid_counts[found_idx]);
      c->wire_bundle_horse_ids[found_idx][c->wire_bundle_horseid_counts[found_idx] - 1] = i;
    }
  }

  c->num_wire_bundles = spawnpos_unique;

  c->wire_bundle_horseid_counts = th_arenaManage(alloc,c->wire_bundle_horseid_counts,sizeof(int)*c->num_wire_bundles);
  for (int i = 0 ; i < c->num_wire_bundles;i++)
  {
    c->wire_bundle_horse_ids[i] = th_arenaManage(alloc,c->wire_bundle_horse_ids[i],sizeof(int)*c->wire_bundle_horseid_counts[i]);
  }
  c->wire_bundle_horse_ids = th_arenaManage(alloc,c->wire_bundle_horse_ids,sizeof(int*)*c->num_wire_bundles);

  free(used_spawnpositions);


  c->connector_count = c->num_wire_bundles;
  c->connector_mats = th_alloc(alloc,sizeof(fn_mat4)*c->num_wire_bundles);

  c->wire_orients = th_alloc(alloc,sizeof(fn_mat4)*c->num_wire_bundles);
  c->wirebase_orients_right = th_alloc(alloc,sizeof(fn_vec3)*c->num_wire_bundles);
  c->wirebase_orients_up = th_alloc(alloc,sizeof(fn_vec3)*c->num_wire_bundles);

  c->wire_extens = th_alloc(alloc,sizeof(float)*c->num_wire_bundles);
  c->wire_states = th_alloc(alloc,sizeof(th_HorseWireState)*c->num_wire_bundles);

  c->wire_plug_origins = th_alloc(alloc,sizeof(fn_vec3)*c->num_wire_bundles);
  c->wire_plug_normals = th_alloc(alloc,sizeof(fn_vec3)*c->num_wire_bundles);

  c->wire_bundle_target_idx = th_alloc(alloc,sizeof(fn_vec3)*c->num_wire_bundles);

  for (int i = 0 ; i < c->num_wire_bundles;i++)
  {
    c->wire_bundle_target_idx[i] = -1;

    c->wirebase_orients_right[i] = fn_createVec3(0,0,0);
    c->wirebase_orients_up[i] = fn_createVec3(0,0,0);

    c->wire_orients[i] = fn_identityMat4();

    c->connector_mats[i] = fn_identityMat4();

    c->wire_extens[i] = 0.0;
    c->wire_states[i] = TH_HORSE_WIRE_START;
  }

  c->liquid_sim_timers = th_alloc(alloc,sizeof(th_timer_t)*count);

  c->fsim = th_alloc(alloc,sizeof(th_LiquidHeights)*count);

  c->simmed_fluid_once = th_alloc(alloc,sizeof(bool)*count);

  for (int i = 0; i < count; i++) {
   c->simmed_fluid_once[i] = false;
   c->liquid_sim_timers[i] = th_time();
   th_initLiquidSim(alloc,&c->fsim[i]);
   th_updateLiquidSim(&c->fsim[i],1.0);



    c->hatch_interp[i] = 0.0;

    c->data[i].easymode = false;
    c->data[i].stun_timer = 0.0;
    c->data[i].stun_direction = fn_createVec3(1,0,0);

    c->data[i].fadeout_spawn = 1.0;
    c->data[i].spawn_when = times[i];
    c->data[i].spawn_init = false;
    c->data[i].spawn_finished = false;
    c->data[i].spawn_stalled = false;

    c->data[i].course_data = NULL;
    c->data[i].course_count = 0;
    c->data[i].old_body = fn_identityMat4();
    c->data[i].fadeout_body = fadeout_init;
    c->data[i].spawn_children_flipflop = 0;
    c->data[i].spawn_children_timer = th_time();
    c->data[i].path = NULL;
    c->data[i].orientation = fn_createVec4(0,0,0,1);
    c->data[i].orientation_direction = fn_createVec3(0,-1,0);
    c->data[i].facing = fn_createVec3(0,0,-1);
    c->data[i].set_up = false;
    c->data[i].fully_extended = true;
    c->data[i].timer_nfextended = 0.0;
    c->data[i].timer_move = 0.0;
    c->data[i].angle_adjust = 0.0;
    c->data[i].pitch_adjust = 0.0;
    c->data[i].height_adjust = 0.0;

    c->data[i].mech1 = NULL;
    c->data[i].mech2 = NULL;
    c->data[i].mechindex = 0;

    c->data[i].gibbed = false;
    c->data[i].spawn_sound = NULL;
    c->data[i].setposition = false;
    c->data[i].played_spawnsound = false;
    c->data[i].unstall_count = 0;

    for (int j = 0; j < 6; j++) {
      c->data[i].impact_sounds[j] = NULL;
    }

    for (size_t j = 0; j < 6; j++) {
        c->data[i].stepping[j] = false;
        c->data[i].target_positions[j] = fn_createVec3(0,0,0);
        c->data[i].t[j] = 0;
        c->data[i].previously_free[j] = false;

        c->data[i].step_linear[j] = false;
        c->data[i].previously_stepping[j] = false;
        c->data[i].footnormals[j] = fn_createVec3(0,-1,0);
        c->data[i].footcontacted[j] = false;
    }
    c->data[i].driven = false;
    c->data[i].driver_normal = fn_createVec3(0,-1,0);

    c->data[i].interp = 0;
    c->data[i].cprog = 0;

    c->entities[i] = TH_DEFAULT_ENTITY;
    c->entities[i].aabb.position = positions[i];
    c->entities[i].aabb.hwidth = fn_createVec3(150,150,150);
    c->entities[i].velocity = fn_createVec3s(0);
    c->entities[i].grounded = false;
    c->entities[i].aabb.mode = BOX;
    c->entities[i].alive = true;
    c->entities[i].collided = false;
    c->entities[i].impact = false;
    c->entities[i].delete_me = false;
    c->entities[i].impact_count = 0;

    c->data[i].horse_body_gib = TH_DEFAULT_ENTITY;
    c->data[i].horse_body_gib.aabb.position = positions[i];
    c->data[i].horse_body_gib.aabb.hwidth = fn_createVec3(150,150,150);
    c->data[i].horse_body_gib.velocity = fn_createVec3s(0);
    c->data[i].horse_body_gib.grounded = false;
    c->data[i].horse_body_gib.aabb.mode = BOX;
    c->data[i].horse_body_gib.alive = true;
    c->data[i].horse_body_gib.collided = false;
    c->data[i].horse_body_gib.impact = false;
    c->data[i].horse_body_gib.delete_me = false;
    c->data[i].horse_body_gib.impact_count = 0;




    c->transforms[i] = fn_translaterotatescale(positions[i],0,fn_createVec3(0,1,0),fn_createVec3s(10));
    positionHatch(c,i);

    fn_vec3 leggoffset[12];
    fn_quat legq[12];
    for (size_t k = 0; k < 12; k++) {
      leggoffset[k] = fn_createVec3s(10000);
      legq[k] = fn_createVec4(0,0,0,1);
    }

    leggoffset[0] = fn_createVec3(2*10*6.4,0,-0.25*10*6.4);
    // leggoffset[1] = fn_createVec3(-2*10*6.4,0,-0.25*10*6.4);
    // leggoffset[2] = fn_createVec3(2*10*6.4,0,-0.25*10*6.4 + 3*10*6.4);
    // leggoffset[3] = fn_createVec3(-2*10*6.4,0,-0.25*10*6.4+ 3*10*6.4);
    // leggoffset[4] = fn_createVec3(2*10*6.4,0,-0.25*10*6.4 - 3*10*6.4);
    // leggoffset[5] = fn_createVec3(-2*10*6.4,0,-0.25*10*6.4 - 3*10*6.4);


    float offset_v = 3.04*10*6.4;
    // leggoffset[6] = fn_createVec3(2*10*6.4,offset_v,-0.25*10*6.4);
    // leggoffset[7] = fn_createVec3(-2*10*6.4,offset_v,-0.25*10*6.4);
    // leggoffset[8] = fn_createVec3(2*10*6.4,offset_v,-0.25*10*6.4 + 3*10*6.4);
    // leggoffset[9] = fn_createVec3(-2*10*6.4,offset_v,-0.25*10*6.4+ 3*10*6.4);
    // leggoffset[10] = fn_createVec3(2*10*6.4,offset_v,-0.25*10*6.4 - 3*10*6.4);
    // leggoffset[11] = fn_createVec3(-2*10*6.4,offset_v,-0.25*10*6.4 - 3*10*6.4);


    fn_vec3 nodes[3];
    nodes[0] = fn_createVec3(2*10*6.4,0,-0.25*10*6.4);
    nodes[1] = fn_createVec3(2*10*6.4 + 1,offset_v,-0.25*10*6.4);
    nodes[2] = fn_createVec3(2*10*6.4,offset_v*1.5,-0.25*10*6.4);

    solveJointPosition(nodes[2],nodes,offset_v);
    leggoffset[0] = fn_multVec3s(fn_addVec3(nodes[0],nodes[1]),0.5);
    leggoffset[1] = fn_multVec3s(fn_addVec3(nodes[1],nodes[2]),0.5);
    legq[0] = fn_getRotationQuaternion(fn_createVec3(0,1,0),fn_normalizeVec3(fn_subVec3(nodes[0],nodes[1])));
    legq[1] = fn_getRotationQuaternion(fn_createVec3(0,1,0),fn_normalizeVec3(fn_subVec3(nodes[1],nodes[2])));



    for (int j = 0; j < legs_per_count; j++) {
      LEG_ASSIGNMENT(i*legs_per_count + j) = fn_translaterotatescaleq(fn_addVec3(positions[i],leggoffset[j]),legq[j],fn_createVec3s(10));

      c->entities_legs[i*legs_per_count + j] = TH_DEFAULT_ENTITY;
      c->entities_legs[i*legs_per_count + j].aabb.position = positions[i];
      c->entities_legs[i*legs_per_count + j].aabb.hwidth = j%2 == 0 ? fn_createVec3(25,25,25) : fn_createVec3(30,30,30);
      c->entities_legs[i*legs_per_count + j].velocity = fn_createVec3s(0);
      c->entities_legs[i*legs_per_count + j].grounded = false;
      c->entities_legs[i*legs_per_count + j].aabb.mode = BOX;
      c->entities_legs[i*legs_per_count + j].alive = false;
      c->entities_legs[i*legs_per_count + j].collided = false;
      c->entities_legs[i*legs_per_count + j].impact = false;
      c->entities_legs[i*legs_per_count + j].delete_me = false;
      c->entities_legs[i*legs_per_count + j].impact_count = 0;
      c->entities_legs[i*legs_per_count + j].time_of_damage = 0;





    }

    for (size_t j = 0; j < 6; j++) {

      for (size_t k = 0; k < 2; k++) {
        c->data[i].fadeout_legs[j*2 + k] = fadeout_init;
        c->data[i].old_legs[j*2 + k] = fn_identityMat4();
        for (size_t l = 0; l < 2; l++) {
          c->data[i].horse_leg_gib[j*2 + k][l] = TH_DEFAULT_ENTITY;
          c->data[i].horse_leg_gib[j*2 + k][l].aabb.position = positions[i];
          c->data[i].horse_leg_gib[j*2 + k][l].aabb.hwidth = fn_createVec3(75,75,75);
          c->data[i].horse_leg_gib[j*2 + k][l].velocity = fn_createVec3s(0);
          c->data[i].horse_leg_gib[j*2 + k][l].grounded = false;
          c->data[i].horse_leg_gib[j*2 + k][l].aabb.mode = BOX;
          c->data[i].horse_leg_gib[j*2 + k][l].alive = true;
          c->data[i].horse_leg_gib[j*2 + k][l].collided = false;
          c->data[i].horse_leg_gib[j*2 + k][l].impact = false;
          c->data[i].horse_leg_gib[j*2 + k][l].delete_me = false;
          c->data[i].horse_leg_gib[j*2 + k][l].impact_count = 0;
        }
      }

      c->transforms_gems[i*6 + j] = fn_translaterotatescale(nodes[0],0,fn_createVec3(0,1,0),fn_createVec3s(10));

      c->entities_gems[i*6 + j] = TH_DEFAULT_ENTITY;
      c->entities_gems[i*6 + j].aabb.position = positions[i];
      c->entities_gems[i*6 + j].aabb.hwidth = fn_createVec3(60,60,60);
      c->entities_gems[i*6 + j].velocity = fn_createVec3s(0);
      c->entities_gems[i*6 + j].grounded = false;
      c->entities_gems[i*6 + j].aabb.mode = BOX;
      c->entities_gems[i*6 + j].alive = false;
      c->entities_gems[i*6 + j].collided = false;
      c->entities_gems[i*6 + j].impact = false;
      c->entities_gems[i*6 + j].delete_me = false;
      c->entities_gems[i*6 + j].impact_count = 0;
      c->entities_gems[i*6 + j].time_of_damage = 0;
      c->entities_gems[i*6 + j].damage_callback = th_horseDamageCallback;

      c->data[i].hasgem[j] = true;
      c->data[i].gemhealth[j] = gem_starting_health;
      c->data[i].gem_jitter_t[j] = 0.0;
      c->data[i].gem_jitter_x[j] = fn_createVec3s(0.0);
    }
  }


}


static bool evaluateTraceCone(th_World* world,fn_vec3 a,fn_vec3 b,float min_hit_dist)
{
  fn_vec3 a_normal = fn_createVec3(0,0,0);
  bool a_hit = false;
  fn_vec3 a_pos = th_traceVolume(world,a,b,150,&a_normal,&a_hit,th_getPhysicsMemory(world,0));

  if (!a_hit)
  {
    return true;
  }

  //printf("%f\n",fn_distance(a,a_pos));

  if (fn_distance(a,a_pos) > min_hit_dist)
  {
    return true;
  }

  return false;
}

void th_horsePositionPlugs(th_HorseGroup* c)
{
  for (int idx = 0 ; idx < c->num_wire_bundles;idx++)
  {
    int i = c->wire_bundle_horse_ids[idx][0];

    fn_vec3 forward = c->data[i].old_forward;

    fn_vec3 up = c->data[i].old_up;

    fn_vec3 pos = c->entities[i].aabb.position;


    fn_vec3 explore_back = fn_addVec3(pos,fn_multVec3s(forward,-400));

    //trace a cone in the direction of "down"

    fn_vec3 downdir = fn_multVec3s(up,-1);

    fn_vec3* conesamples = malloc(sizeof(fn_vec3)*256);

    fn_SampleCone(downdir, fn_radians(40.0),
                  16, 16,
                  conesamples);

    bool found_origin = false;
    for (int j = 0 ; j < 256;j++)
    {
      fn_vec3 a_normal = fn_createVec3(0,0,0);
      bool a_hit = false;
      fn_vec3 target = fn_addVec3(explore_back,fn_multVec3s(conesamples[j],700));
      fn_vec3 a_pos = th_traceVolume(c->levelstate->world,explore_back,target,10,&a_normal,&a_hit,th_getPhysicsMemory(c->levelstate->world,0));

      if (a_hit && fn_distance(a_pos,pos) < 600)
      {
        c->wire_plug_origins[idx] = fn_addVec3(a_pos,fn_multVec3s(a_normal,-35));
        c->wire_plug_normals[idx] = a_normal;
        found_origin = true;
        break;
      }
    }

    if (!found_origin)
    {
      c->wire_plug_origins[idx] = fn_createVec3(0,0,0);
      c->wire_plug_normals[idx] = fn_createVec3(0,-1,0);
      printf("COULD NOT FIND PLUG POSITION! \n");
    }
  }
}



void th_horseProcessCourse(th_HorseGroup* c,int i)
{
  if (!c->data[i].set_up)
  {
    fn_vec3 start_pos = c->entities[i].aabb.position;
    const float radius_trace = (3.04*10*6.4*0.6*2.0*1.2*0.5);
    fn_vec3* points = NULL;
    int pc = 0;
    for (int j = 0; j < c->data[i].course_count; j++) {
       int pc1 = 0;
       fn_vec3* points1 = th_horsePlanMotion(start_pos,c->data[i].course_data[j],c->levelstate->world,radius_trace,&pc1,false);
       if (pc1 > 1)
       {
         points = realloc(points,sizeof(fn_vec3)*(pc + pc1 - 1));
         memcpy(&points[pc],&points1[1],sizeof(fn_vec3)*(pc1 - 1));
         pc += pc1 - 1;
       }



       free(points1);
       if (pc > 0)
       {
         start_pos = points[pc - 1];
       }

      if (pc1 - 1 == 0)
       {
          //start_pos = c->data[i].course_data[j];//kick forward
          printf("Horse Path Couldnt Trace at index %i\n",j);
        }

    }
    if (points == NULL)
    {
      printf("Couldnt make horse course\n");
    }

    c->data[i].path = points;
    c->data[i].path = th_arenaManage(c->alloc,c->data[i].path,sizeof(fn_vec3)*(pc));
    points = c->data[i].path;//this is stupid, but necissary
    c->data[i].path_count = pc;
    c->data[i].facing = fn_createVec3(1,0,0);

    int cstart = c->data[i].cprog;
    float interp = c->data[i].interp;
    fn_vec3 pos = CalculatePosition(c->data[i].path[cstart],c->data[i].path[cstart + 1],c->data[i].path[cstart + 2],c->data[i].path[cstart + 3],interp);
    fn_vec3 forward = CalculateTangent(c->data[i].path[cstart],c->data[i].path[cstart + 1],c->data[i].path[cstart + 2],c->data[i].path[cstart + 3],interp);
    c->entities[i].aabb.position = pos;

    fn_vec3 u = fn_createVec3(0,-1,0);


    fn_vec3 s = fn_cross(forward,u);
    fn_vec3 up_v = fn_cross(s,forward);

    c->data[i].old_forward = forward;
    c->data[i].old_up = up_v;
    c->data[i].old_right = s;

    c->data[i].ups = malloc(sizeof(fn_vec3)*pc*2);

    computeUpVectors(points,c->data[i].ups,fn_createVec3(0,-1,0),c->data[i].old_right,c->data[i].old_forward,pc);

    memcpy(th_getDefaultDebugger()->points_default,points,sizeof(fn_vec3)*(pc));
    th_getDefaultDebugger()->point_count_default = pc;
    // free(points);
    c->data[i].ups = th_arenaManage(c->alloc,c->data[i].ups,sizeof(fn_vec3)*pc*2);

  }
}

void th_horseSetCourse(th_HorseGroup* c,int i,fn_vec3* target_points,int target_count)
{
  c->data[i].course_data = target_points;
  c->data[i].course_count = target_count;

  th_horseProcessCourse(c, i);
}

void th_horseSetEasyMode(th_HorseGroup* c,int i)
{
  c->data[i].easymode = true;

  c->data[i].hasgem[0] = true;
  c->data[i].hasgem[1] = true;
  c->data[i].hasgem[2] = false;
  c->data[i].hasgem[3] = false;
  c->data[i].hasgem[4] = false;
  c->data[i].hasgem[5] = false;

  c->data[i].gemhealth[0] = 104.0;
  c->data[i].gemhealth[1] = 104.0;
}
