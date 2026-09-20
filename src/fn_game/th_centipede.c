#include "th_centipede.h"
#include  <math.h>
#include <stdio.h>
#include "../fn_engine/th_time.h"
#include <string.h>
#include "th_builtins.h"
#include "../fn_engine/th_threads.h"
#include "../fn_engine/th_level.h"
#include "../fn_engine/th_system.h"
#include "../fn_engine/th_globals.h"
#include "../fn_engine/th_ragdoll.h"
#include "../fn_engine/th_occlusion.h"
#include "../fn_engine/th_hitmarker.h"
#include "th_splineutils.h"

static const float SNAKE_ALPHA = 0.75;
static const th_timer_t SNAKE_PERTURB_TIME = 1500;

fn_vec3 stepforward(th_AgentInfo* info,float distance,int iterations)
{
  float interp_delta = 0;
  fn_vec3 d = fn_createVec3s(0);
  for (int i = 0; i < iterations;i++)
  {
    float speed = distance/((float)iterations);
    fn_vec3 d_old = d;
     d = CalculateTangentUnNorm(info->course[info->cprog],info->course[info->cprog + 1],info->course[info->cprog + 2],info->course[info->cprog + 3],info->interp);
     if ( d.x*d.x + d.y*d.y + d.z*d.z < 0.001)
     {
       printf("%i %i %f\n",info->cprog,info->course_count,info->interp);
       fn_printVec3(d);
      fn_printVec3(info->course[info->cprog]);
      fn_printVec3(info->course[info->cprog + 1]);
      fn_printVec3(info->course[info->cprog + 2]);
      fn_printVec3(info->course[info->cprog + 3]);


    }
    else
    {
      interp_delta = speed / sqrtf( d.x*d.x + d.y*d.y + d.z*d.z );
    }

    info->interp += interp_delta;
    if (info->interp > 1)
    {
      info->cprog += (int)info->interp;
      info->interp = info->interp - (int)info->interp;
      if (info->cprog > info->course_count - 4)
      {
        info->cprog = 0;
      }
    }
  }
  return d;
}

typedef struct
{
  fn_vec3 position;
  int start;
  int range;
  int k;
  th_CentipedeGroup* c;
  fn_vec3* velocity_delta;
}th_centiavoidThreadData;

static void thread_centiAvoid( void* id)
{
  th_centiavoidThreadData* data = (th_centiavoidThreadData*) id;
  th_CentipedeGroup* c = data->c;
  int k = data->k;
  fn_vec3 position = data->position;
  for (int j = data->start ; j < data->start + data->range;j++)
  {
    fn_vec3 cur_point = c->agents[c->masters[k].start].course[j];

    float dist = fn_distance2(cur_point,position);
    // if (dist < old_dist || j == 0)
    // {
    //   old_dist = dist;
    //   old_index = j;
    // }

    fn_vec3 point = c->agents[c->masters[k].start].course[j];
    bool passed = c->agents[c->masters[k].start].cprog > j;
    if (dist < 700*700 && !passed)
    {
      float  mag = (700*700 - dist)/(700*700);
      mag *= 15;
      fn_vec3 delta = fn_multVec3s(fn_normalizeVec3(fn_subVec3(position,point)),mag);
      // velocity = fn_addVec3(velocity,delta);
      *data->velocity_delta = fn_addVec3(*data->velocity_delta,delta);
      // velocity = fn_normalizeVec3(velocity);
      // velocity = fn_multVec3s(velocity,velocity_goal);
    }
  }
}

static fn_vec3* temp_memory = NULL;

static void CentiPlotCourse(th_CentipedeGroup* c,fn_vec3 target,int master)
{
  //return;
  th_MasterInfo* info = &c->masters[master];
  th_AgentInfo* agent = &c->agents[info->start];//
  th_AgentInfo* agent_end = &c->agents[info->start + info->count - 1];
  th_AgentInfo* agent_middle = &c->agents[info->start + info->count/2];
//  printf("%s\n","FOLLOW" );
  info->start_time = th_time();
  int ostart = agent->cprog;
  int oend = agent_end->cprog + 4;

  int upstart = (ostart )*2;
  upstart = (agent->interp >= 0.5) ? upstart + 1 : upstart;

  int upend = (oend - ostart )*2 + 1;

  if (agent->course_count - ostart > 0)
  {
    memmove ( &info->angles[0], &info->angles[ostart], sizeof(float)*((agent->course_count - ostart) ) );

    memmove ( &info->course[0], &info->course[ostart], sizeof(fn_vec3)*((agent->course_count - ostart)) );
  }


  //memmove ( &info->upvectors[0], &info->upvectors[upstart], sizeof(fn_vec3)*((agent->course_count*2 - upstart)) );
  //assert(ostart > 0 && ostart < agent->course_count);

  //info->old_up = fn_NlerpVec3(info->upvectors[upindex],info->upvectors[upindex + 1],fmod(interp,0.5)*2);

  int diff = (oend - ostart);

  //assert(diff > 0);

  if (diff <= 0 || !(ostart > 0 && ostart < agent->course_count) )
  {
    return;
  }

  int cprogdiff = agent->cprog;
  agent->cprog = 0;
  agent->course = info->course;

      int course_count = MAX_COURSE_SIZE - 10;
  agent->course_count = course_count;

  fn_vec3 start_pos = info->course[diff  ];
  info->t = info->angles[diff ];

  float s = 0;
  fn_vec3 position = start_pos;

  //fn_printVec3(CalculateTangentUnNorm(info->course[diff -3],info->course[diff -2],info->course[diff- 1],info->course[diff],1));
//  printf("%f\n",fn_length(CalculateTangentUnNorm(info->course[diff -3],info->course[diff -2],info->course[diff- 1],info->course[diff],1)) );
  //fn_vec3 velocity = CalculateTangentUnNorm(info->course[diff -3],info->course[diff -2],info->course[diff- 1],info->course[diff],1);
  fn_vec3 velocity =  fn_subVec3(info->course[diff ],info->course[diff-1]);

  int diff_offset = diff;
  while (fn_length2(velocity) < 0.1 && diff_offset >= 1)
  {
    diff_offset = diff_offset - 1;
    velocity =  fn_subVec3(info->course[diff_offset ],info->course[diff_offset-1]);
  }

  if (fn_length2(velocity) < 0.1)
  {
    //pick a directon and run with it:
    velocity = fn_createVec3(0,-1,0);
  }

  // fn_vec3 velocity = CalculateTangentUnNorm(
  //   info->course[diff - 2],
  //   info->course[diff - 1],
  //   info->course[diff],
  //   info->course[diff + 1], //check this
  //   1.0f
  // );

  velocity = fn_multVec3s(fn_normalizeVec3(velocity),700);
  //fn_printVec3(fn_subVec3(info->course[diff],info->course[diff-1]));
  fn_vec3 direction_of_motion = fn_normalizeVec3(velocity);
  float accel_drift = 50;
  float accel = 400;
  float velocity_goal = 200;
  bool found_guy = false;
  int index_of_guy = MAX_COURSE_SIZE - 15;
  fn_vec3 oldvelocity = velocity;
  float turning_speed = 0.1;
  //printf("%s\n","Start" );
  bool flip = false;
  fn_vec3 gemdir_primal = fn_transformNormal(fn_createVec3(0,-1,0),c->transforms_body[info->start + info->count - 1]);

  fn_vec3 direction_primal = fn_createVec3(0,-1,0);//fn_multVec3s(fn_normalizeVec3(fn_subVec3(target,position)),-1.0);
  if ( fn_dot(gemdir_primal,direction_primal) > 0 )
  {
    //printf("%s\n","flip" );
    flip = true;
  }

  for (int i = diff + 1; i < course_count ;i++)
  {
    accel += 50;
    accel = fn_clamp(accel,400,650);
    turning_speed += 0.1;
    turning_speed = fn_clamp(turning_speed,0,0.6);
    fn_vec3 wishdir = fn_normalizeVec3(fn_subVec3(target,position));

    fn_vec3 avoid_dir = fn_createVec3s(0);
    for (int k = 0; k < c->num_masters; k++) {
      if (k != master && c->masters[k].state != TH_HASPHYSICS && c->masters[k].state != TH_ALL_HASPHYSICS  )
      {
        float old_dist = 0.0;
        int old_index = 0;
        //TODO multithread
        // for (int j = 0 ; j < c->agents[c->masters[k].start].course_count;j++)
        // {
        static fn_vec3* accum_memory = NULL;
        if (accum_memory == NULL)
        {
          accum_memory = malloc(sizeof(fn_vec3)*th_getNumThreads());
        }


        TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_centiavoidThreadData,c->agents[c->masters[k].start].course_count)
        data[th_thread_id].start = start_pos;
        data[th_thread_id].range = add;
        data[th_thread_id].c = c;
        data[th_thread_id].k = k;
        data[th_thread_id].position = position;
        data[th_thread_id].velocity_delta = &accum_memory[th_thread_id];
        *data[th_thread_id].velocity_delta = fn_createVec3s(0);
        TH_SCHEDULING_FUNC
        th_setThread(thread_centiAvoid,(void*)&data[th_thread_id],th_thread_id);
        TH_END_SCHEDULING

        for (int tr = 0 ; tr < th_getNumThreads();tr++)
        {
          avoid_dir = fn_addVec3(avoid_dir,accum_memory[tr]);
        }





      }
    }


    th_TricolumnGroup* tricol = c->levelstate->tricolumn;

    th_HorseGroup* horse = c->levelstate->horse;

    if (horse != NULL)
    {
      for (int j = 0; j < c->levelstate->horse->count;j++)
      {

        if (!horse->data[j].spawn_finished || horse->data[j].gibbed)
        {
          continue;
        }

        fn_vec3 cur_point = horse->entities[j].aabb.position;

        float dist = fn_distance2(cur_point,position);


        fn_vec3 point = cur_point;

        if (dist < 700*700)
        {
          float  mag = (700*700 - dist)/(700*700);
          mag *= 30;
          fn_vec3 delta = fn_multVec3s(fn_normalizeVec3(fn_subVec3(position,point)),mag);

          avoid_dir = fn_addVec3(avoid_dir,delta);

        }
      }
    }

    if (tricol != NULL)
    {
      for (int j = 0; j < tricol->count;j++)
      {

        if (!tricol->data[j].spawn_finished || tricol->data[j].state == TRICOL_GIBBED)
        {
          continue;
        }

        fn_vec3 cur_point = tricol->data[j].position;

        float dist = fn_distance2(cur_point,position);


        fn_vec3 point = cur_point;

        if (dist < 700*700)
        {
          float  mag = (700*700 - dist)/(700*700);
          mag *= 30;
          fn_vec3 delta = fn_multVec3s(fn_normalizeVec3(fn_subVec3(position,point)),mag);

          avoid_dir = fn_addVec3(avoid_dir,delta);

        }
      }
    }

    if (fn_length2(avoid_dir) > 0.1)
    {
      avoid_dir = fn_normalizeVec3(avoid_dir);
    }


    wishdir = fn_addVec3(wishdir,fn_multVec3s(avoid_dir,0.65));
    wishdir = fn_normalizeVec3(wishdir);

    direction_of_motion = fn_slerpVec3(direction_of_motion,wishdir,turning_speed);
    direction_of_motion = fn_normalizeVec3(direction_of_motion);



  //  fn_printVec3(position);
    if (((fn_distance(target,position) > 300) && !found_guy))
    {
      oldvelocity = velocity;
      velocity = fn_normalizeVec3(velocity);
      velocity = fn_multVec3s(velocity,700);

      velocity.x += accel * direction_of_motion.x;
      velocity.y += accel * direction_of_motion.y;
      velocity.z += accel * direction_of_motion.z;
      oldvelocity= fn_subVec3(velocity,oldvelocity);

      velocity = fn_normalizeVec3(velocity);
      velocity = fn_multVec3s(velocity,velocity_goal);

      if (flip)
      {
        info->t += (3.14159265359*0.25)/12.0;
      }
      //

    }
    else {
      if (!found_guy)
      {
        index_of_guy = i;
       //printf("%i\n",i - diff );
      }
      found_guy = true;
      // if (index_of_guy > diff + 1)
      // {
        //velocity = fn_addVec3(velocity,oldvelocity);

        velocity.x += accel_drift * direction_of_motion.x;
        velocity.y += accel_drift * direction_of_motion.y;
        velocity.z += accel_drift * direction_of_motion.z;
        //
        velocity = fn_normalizeVec3(velocity);
        velocity = fn_multVec3s(velocity,velocity_goal);
    //  }


      // if (!fn_almostEqualf(cosf(info->t),1,0.01) && !fn_almostEqualf(sinf(info->t),0,0.01) )
      // {
      //   info->t += 3.14159265359/12.0;
      // }
      if (flip)
      {
        info->t += (3.14159265359*0.25)/12.0;
      }


    }

    info->t += c->config.turn_rate;

    if (fn_length2(velocity) < 0.1)
    {
      //pick a directon and run with it:
      velocity = fn_createVec3(0,-velocity_goal,0);
    }


    position  = fn_addVec3(position,velocity);


     fn_vec3 offset = fn_createVec3(0,0,0);
    // offset = fn_createVec3(0,sinf(info->t)*100 - 100,0);
    info->course[i] = fn_addVec3(position,offset);
    info->angles[i] = info->t;
    //info->angles[i] = 0.0;
  }


  info->target_index = index_of_guy;


  //update agents
  for (int i = info->start + 1 ; i < info->start + info->count;i++)
  {
    c->agents[i].course = agent->course;
    c->agents[i].course_count = course_count;
    c->agents[i].cprog -= cprogdiff;
  }
  computeUpVectors(&info->course[0],&info->upvectors[0],c->agents[info->start].old_up,c->agents[info->start].old_right,c->agents[info->start].old_forward,course_count);
}


static inline int64_t atmc_ld(int64_t* addr,int memorder)
{
  int64_t ret = 0;
  __atomic_load(addr,&ret,memorder);

  return ret;
}

typedef struct
{
  th_CentipedeGroup* c;
  int offset;
  int start;
  int range;
  int groupstart;
  th_World* world;
  int thread_id;
  float dt;
}th_centiupdateThreadsData;

static void thread_centiupdate( void* id)
{
  float scaling = 11.5;
  th_centiupdateThreadsData* data = (th_centiupdateThreadsData*)id;
  th_CentipedeGroup* c = data->c;
  int offset = data->offset;
  int start = data->groupstart;

  scaling = scaling*c->config.scale;


  for (int j =   data->start + offset ; j < data->start + data->range + offset;j++)
  {
    th_AgentInfo* info = &c->agents[j];

    c->agents[j].cprog = c->agents[start].cprog;
    c->agents[j].interp = c->agents[start].interp;

    // c->agents[j].cprog = c->agents[j -1].cprog;
    // c->agents[j].interp = c->agents[j - 1].interp;
  //  fn_vec3 jerk = CalculateJerk(info->course[info->cprog],info->course[info->cprog + 1],info->course[info->cprog + 2],info->course[info->cprog + 3],info->interp);
    //printf("%f\n",fn_lerp(32,200,(fn_clamp(fn_length(jerk),1000,6000)- 1000) /5000) );

    // fn_vec3 d = stepforward(&c->agents[j],150,1000);
      fn_vec3 d = stepforward(&c->agents[j],c->config.scale*150.0 * (j - start),1000);

    int cstart = info->cprog;
    float interp = info->interp;
    fn_vec3 pos = CalculatePosition(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);
    fn_vec3 forward = CalculateTangent(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);
    c->agents[j].position = pos;

    //float theta = fn_lerp(info->angles[cstart + 1],info->angles[cstart + 2],interp);
    float theta = CalculatePosition1D(info->angles[cstart + 0],info->angles[cstart + 1],info->angles[cstart + 2],info->angles[cstart + 3],interp);
    fn_mat4 r = fn_makerotate(theta,fn_createVec3(0,0,1));

    if (info->hasphysics)
    {
      continue;
    }
    int upindex = (cstart )*2;
    upindex = (interp >= 0.5) ? upindex + 1 : upindex;
    info->old_up = fn_NlerpVec3(info->upvectors[upindex],info->upvectors[upindex + 1],fmod(interp,0.5)*2);
    // if (start == 20)
    // {
    //   fn_printVec3(info->old_up);
    // }
    fn_vec3 old_up =info->old_up;

    //fn_mat4 m = th_6dofCamera(&info->old_forward,&info->old_right,&info->old_up,forward,pos);//fn_lookat(pos,fn_addVec3(pos,forward),fn_createVec3(0,-1,0));
    fn_mat4 m = th_6dofCamera(&info->old_forward,&info->old_right,&info->old_up,forward,fn_createVec3s(0));//fn_lookat(pos,fn_addVec3(pos,forward),fn_createVec3(0,-1,0));

    fn_mat4 temp_m = fn_inverse(fn_multMat4(m,r));
    fn_vec3 up_dir = fn_createVec3(temp_m.m[4],temp_m.m[5],temp_m.m[6]);

    fn_vec3 p_trans = fn_addVec3(pos,fn_multVec3s(up_dir,c->normal_perturb[j]));
    c->normal_perturb_offset[j] = fn_multVec3s(up_dir,c->normal_perturb[j]);
    p_trans = fn_multVec3s(p_trans,-1);
    m = fn_multMat4(fn_maketranslate(p_trans),m);

    m = fn_multMat4(m,r);

    //4 5 6
    // fn_printVec3(fn_normalizeVec3(fn_transformNormal(info->old_up,fn_inverse(r))));
    // fn_printMat4(fn_inverse(m));


    m = fn_multMat4(fn_makescale(fn_createVec3s(scaling)),fn_inverse(m));
    c->transforms_body[j] = m;
    info->old_matrix = m;




    if (info->hasgem)
    {
      fn_vec3 gem_jitter = fn_multVec3s(c->agents[j].gem_jitter_x ,sin(c->agents[j].gem_jitter_t*0.05)*c->agents[j].gem_jitter_t*0.25 );
      c->agents[j].gem_jitter_t -= data->dt;
      if (c->agents[j].gem_jitter_t <= 0.0)
      {
        c->agents[j].gem_jitter_t = 0.0;
      }
      fn_mat4 m = th_6dofCamera(&info->old_forward,&info->old_right,&old_up,forward,fn_addVec3(pos,gem_jitter));//fn_lookat(pos,fn_addVec3(pos,forward),fn_createVec3(0,-1,0));

      m = fn_multMat4(m,r);
      m = fn_multMat4(fn_makescale(fn_createVec3s(scaling)),fn_inverse(m));


      c->transforms_gems[j] = m;
    }
    else {
      c->transforms_gems[j] = fn_makescale(fn_createVec3(0,0,0));
    }



    c->frustum_data->min_x[c->frustum_offset + j] = pos.x - c->cullradius;
    c->frustum_data->min_y[c->frustum_offset + j] = pos.y - c->cullradius;
    c->frustum_data->min_z[c->frustum_offset + j] = pos.z - c->cullradius;

    c->frustum_data->max_x[c->frustum_offset + j] = pos.x + c->cullradius;
    c->frustum_data->max_y[c->frustum_offset + j] = pos.y + c->cullradius;
    c->frustum_data->max_z[c->frustum_offset + j] = pos.z + c->cullradius;

    c->frustum_data->min_x[c->frustum_offset_gem + j] = pos.x - c->cullradius;
    c->frustum_data->min_y[c->frustum_offset_gem + j] = pos.y - c->cullradius;
    c->frustum_data->min_z[c->frustum_offset_gem + j] = pos.z - c->cullradius;

    c->frustum_data->max_x[c->frustum_offset_gem + j] = pos.x + c->cullradius;
    c->frustum_data->max_y[c->frustum_offset_gem + j] = pos.y + c->cullradius;
    c->frustum_data->max_z[c->frustum_offset_gem + j] = pos.z + c->cullradius;


    c->entities_gems[j].aabb.position = pos;
    fn_vec3 dxds = CalculateTangentUnNorm(info->course[info->cprog],info->course[info->cprog + 1],info->course[info->cprog + 2],info->course[info->cprog + 3],info->interp);
    c->entities_gems[j].velocity = fn_multVec3s(dxds,info->speed/sqrt(dxds.x*dxds.x + dxds.y*dxds.y + dxds.z*dxds.z ));
    c->agents[j].velocity = c->entities_gems[j].velocity;

    // fn_vec3 tr_n = fn_createVec3s(0);
    // fn_vec3 tr=  th_trace(data->world,c->agents[j].position,c->agents[j].velocity,&tr_n,bool* is_hit,th_CollisionMemory* memory,float* out_t);
    if (j >= offset + c->length_of_snake - 2 && !c->agents[j].hasphysics && c->agents[j].spawned)
    {
      fn_vec3 n;
      bool col_hit;
      fn_vec3 col_pos =  th_trace(data->world,c->agents[j].position,fn_addVec3(c->agents[j].position,fn_multVec3s(fn_normalizeVec3(c->agents[j].velocity),100)),&n,&col_hit,th_getPhysicsMemory(data->world,data->thread_id),NULL);

      if (col_hit)
      {
        //fetch atomic

        int64_t last_smoke_time = atmc_ld(&c->agents[j].master->smoke_time,__ATOMIC_RELAXED);
        if ((int64_t)th_time() > last_smoke_time)
        {
          __atomic_store_n(&c->agents[j].master->smoke_time,(int64_t)th_time() + 10,__ATOMIC_RELAXED);
          th_spawnSmokePuffs(col_pos,data->thread_id);
        }

        //(c->agents[j].crashsource == NULL || !a_isPlaying(c->agents[j].crashsource))
        if (!c->agents[j].prevhit && fn_distance2(c->agents[j].hit,col_pos) > 300*300)
        {
          //fn_printVec3(col_pos);
          {
            a_VirtualSource* s = a_playVirtualSource(19,-1,col_pos,NULL );
            a_setVSLoop(s,false);
            a_setVSPos(s,col_pos);
            a_setVSVel(s,fn_createVec3s(0));
            a_setVSGain(s,0.85);
          }

          if (fn_distance2(c->levelstate->player_e.aabb.position,col_pos) < 400*400)
          {
            c->levelstate->player->screenshake_f = 0.13*0.3;
            c->levelstate->player->screenshake_t = 0;
            c->levelstate->player->screenshake_amplitude = 0.03;
          }
        }
        c->agents[j].prevhit = true;
        c->agents[j].hit = col_pos;
      }
      else
      {
        c->agents[j].prevhit = false;
      }


    }
    else if (!c->agents[j].hasphysics)
    {
      fn_vec3 n;
      bool col_hit;
      fn_vec3 col_pos =  th_trace(data->world,c->agents[j].position,fn_addVec3(c->agents[j].position,fn_multVec3s(fn_normalizeVec3(c->agents[j].velocity),100)),&n,&col_hit,th_getPhysicsMemory(data->world,data->thread_id),NULL);

      if (col_hit)
      {

        if (!c->agents[j].prevhit && fn_distance2(c->agents[j].hit,col_pos) > 300*300)
        {
          if (fn_distance2(c->levelstate->player_e.aabb.position,col_pos) < 400*400)
          {
            c->levelstate->player->screenshake_f = 0.13*0.3;
            c->levelstate->player->screenshake_t = 0;
            c->levelstate->player->screenshake_amplitude = 0.03;
          }
        }
        c->agents[j].prevhit = true;
        c->agents[j].hit = col_pos;
      }
      else
      {
        c->agents[j].prevhit = false;
      }


    }



  }
}

//TODO multithread
static void updateAgents(th_CentipedeGroup* c,int start,int end,float dt,th_World* world)
{
  float scaling = 11.5*c->config.scale;

  for (int j = start ;j < end;j++ )
  {



    if (c->normal_perturb_timer[j] > th_time())
    {
      float frac = (c->normal_perturb_timer[j] - th_time())/SNAKE_PERTURB_TIME;

      // if (frac > SNAKE_ALPHA)
      // {
      //   c->normal_perturb[j] = fn_lerp(c->normal_perturb_target[j],0,(frac - SNAKE_ALPHA)/(1.0 - SNAKE_ALPHA));
      // }
      // else
      // {
      //   c->normal_perturb[j] = fn_lerp(0,c->normal_perturb_target[j],(frac)/(SNAKE_ALPHA));
      // }

      c->normal_perturb[j] = sin(frac*3.14159*7)*c->normal_perturb_target[j]*frac;
    }
    else
    {
      c->normal_perturb[j] = 0.0;
    }
  }



  for (int j = start ;j < start+1;j++ )
  {
    th_AgentInfo* info = &c->agents[j];


    fn_vec3 d = CalculateTangentUnNorm(info->course[info->cprog],info->course[info->cprog + 1],info->course[info->cprog + 2],info->course[info->cprog + 3],info->interp);
    float speed = dt*info->speed;//dt*0.7;
    float interp_delta = 0.0;
    if (d.x*d.x + d.y*d.y + d.z*d.z < 0.01)
    {
        interp_delta = 1.2;//speed/0.001;
    }
    else
    {
        interp_delta = speed / sqrtf( d.x*d.x + d.y*d.y + d.z*d.z );
    }

    info->interp += interp_delta;
    if (info->interp > 1)
    {
      // info->cprog++;
      // info->interp = info->interp - 1;
      info->cprog += (int)info->interp;
      info->interp = info->interp - (int)info->interp;
      if (info->cprog > info->course_count - 4)
      {
        info->cprog = 0;
      }
    }

    int cstart = info->cprog;
    float interp = info->interp;
    fn_vec3 pos = CalculatePosition(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);
    fn_vec3 forward = CalculateTangent(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);
    c->agents[j].position = pos;

  //  float theta = fn_lerp(info->angles[cstart + 1],info->angles[cstart + 2],interp);
    float theta = CalculatePosition1D(info->angles[cstart + 0],info->angles[cstart + 1],info->angles[cstart + 2],info->angles[cstart + 3],interp);

    //printf("%f\n",theta );
    fn_mat4 r = fn_makerotate(theta,fn_createVec3(0,0,1));

    if (info->hasphysics)
    {
      continue;
    }

    int upindex = (cstart)*2;
    upindex = (interp >= 0.5) ? upindex + 1 : upindex;
    info->old_up = fn_NlerpVec3(info->upvectors[upindex],info->upvectors[upindex + 1],fmod(interp,0.5)*2);
    fn_vec3 old_up = info->old_up;
    fn_mat4 m = th_6dofCamera(&info->old_forward,&info->old_right,&info->old_up,forward,fn_createVec3s(0));//fn_lookat(pos,fn_addVec3(pos,forward),fn_createVec3(0,-1,0));
    fn_mat4 temp_m = fn_inverse(fn_multMat4(m,r));
    fn_vec3 up_dir = fn_createVec3(temp_m.m[4],temp_m.m[5],temp_m.m[6]);

    fn_vec3 p_trans = fn_addVec3(pos,fn_multVec3s(up_dir,c->normal_perturb[j]));
    c->normal_perturb_offset[j] = fn_multVec3s(up_dir,c->normal_perturb[j]);
    p_trans = fn_multVec3s(p_trans,-1);
    m = fn_multMat4(fn_maketranslate(p_trans),m);
    m = fn_multMat4(m,r);
    m = fn_multMat4(fn_makescale(fn_createVec3s(scaling)),fn_inverse(m));
    c->transforms_body[j] = m;
    info->old_matrix = m;
    if (info->hasgem)
    {

      fn_vec3 gem_jitter = fn_multVec3s(c->agents[j].gem_jitter_x ,sin(c->agents[j].gem_jitter_t*0.05)*c->agents[j].gem_jitter_t*0.25 );
      c->agents[j].gem_jitter_t -= dt;
      if (c->agents[j].gem_jitter_t <= 0.0)
      {
        c->agents[j].gem_jitter_t = 0.0;
      }
      fn_mat4 m = th_6dofCamera(&info->old_forward,&info->old_right,&old_up,forward,fn_addVec3(pos,gem_jitter));//fn_lookat(pos,fn_addVec3(pos,forward),fn_createVec3(0,-1,0));

      m = fn_multMat4(m,r);
      m = fn_multMat4(fn_makescale(fn_createVec3s(scaling)),fn_inverse(m));


      c->transforms_gems[j] = m;
    }
    else {
      c->transforms_gems[j] = fn_makescale(fn_createVec3(0,0,0));
      c->frustum_data->skip_culling_flag[c->frustum_offset_gem + j] = true;
    }

    c->entities_gems[j].aabb.position = pos;

    fn_vec3 dxds = CalculateTangentUnNorm(info->course[info->cprog],info->course[info->cprog + 1],info->course[info->cprog + 2],info->course[info->cprog + 3],info->interp);
    c->entities_gems[j].velocity = fn_multVec3s(dxds,info->speed/sqrt(dxds.x*dxds.x + dxds.y*dxds.y + dxds.z*dxds.z ));
    c->rollmat = r;
    c->agents[j].velocity = c->entities_gems[j].velocity;

    c->forward = forward;
    c->pos = pos;

    c->frustum_data->min_x[c->frustum_offset + j] = pos.x - c->cullradius;
    c->frustum_data->min_y[c->frustum_offset + j] = pos.y - c->cullradius;
    c->frustum_data->min_z[c->frustum_offset + j] = pos.z - c->cullradius;

    c->frustum_data->max_x[c->frustum_offset + j] = pos.x + c->cullradius;
    c->frustum_data->max_y[c->frustum_offset + j] = pos.y + c->cullradius;
    c->frustum_data->max_z[c->frustum_offset + j] = pos.z + c->cullradius;

    c->frustum_data->min_x[c->frustum_offset_gem + j] = pos.x - c->cullradius;
    c->frustum_data->min_y[c->frustum_offset_gem + j] = pos.y - c->cullradius;
    c->frustum_data->min_z[c->frustum_offset_gem + j] = pos.z - c->cullradius;

    c->frustum_data->max_x[c->frustum_offset_gem + j] = pos.x + c->cullradius;
    c->frustum_data->max_y[c->frustum_offset_gem + j] = pos.y + c->cullradius;
    c->frustum_data->max_z[c->frustum_offset_gem + j] = pos.z + c->cullradius;

  }

  TH_BEGIN_SCHEDULING(((uint)th_getNumThreads()),th_centiupdateThreadsData,(end - (start + 1 )))
  data[th_thread_id].start = start_pos;
  data[th_thread_id].range = add;
  data[th_thread_id].offset = start + 1;
  data[th_thread_id].c = c;
  data[th_thread_id].groupstart = start;
  data[th_thread_id].thread_id = th_thread_id;
  data[th_thread_id].world = world;
  data[th_thread_id].dt = dt;
  TH_SCHEDULING_FUNC
  th_setThread(thread_centiupdate,(void*)&data[th_thread_id],th_thread_id);
  TH_END_SCHEDULING

}

//TODO multithread
static bool updateAgentsPhysics(th_CentipedeGroup* c,int start,int end,float dt,th_World* world)
{
  bool allhasphysics = true;
  for (int j = start  ; j < end;j++)
  {

    th_AgentInfo* info = &c->agents[j];
    th_Entity* e = &info->entity;
    if (th_time() < info->physics_time)
    {
      allhasphysics = false;
      continue;
    }
    else if (!info->hasphysics)
    {

      {
        a_VirtualSource* s = a_playVirtualSource(21,-1, c->agents[j].position,NULL);
        a_setVSLoop(s,false);
        a_setVSPos(s,c->agents[j].position);
        a_setVSVel(s,fn_createVec3s(0));
        a_setVSGain(s,0.4);
      }
      e->aabb.position = fn_addVec3(c->agents[j].position,c->normal_perturb_offset[j]);;
      fn_vec3 gemdir = fn_transformNormal(fn_createVec3(0,1,0),c->transforms_body[j]);
      fn_vec3 momentum = fn_multVec3s(info->old_forward,info->speed*0.5);
      e->velocity = fn_addVec3(momentum,fn_multVec3s(gemdir,info->speed*0.5));
      e->grounded = false;
      info->hasphysics = true;
      info->has_gravity = true;

      fn_vec3 particle_pos = fn_addVec3(fn_multVec3s(fn_normalizeVec3(momentum),70),c->agents[j].position);
      th_spawnSparks(gemdir,particle_pos,-1,NULL);
      th_spawnSmokeRocket(particle_pos,-1);
    }

    if (!e->alive)
    {
      c->transforms_body[j] = fn_multMat4(th_fadeoutMatrix(&info->fadeout,0.003*dt),c->agents[j].old_matrix);
      continue;
    }

    fn_vec3 oldvelocity = e->velocity;
    bool oldground = e->grounded;

    th_updateEntity(e,world,dt,th_getPhysicsMemory(world,0));
    if (!e->grounded )
    {
      e->velocity.y += 0.001*dt*0.5;
    }
    if (e->collided){

      if (fn_length2(e->velocity) > 0.05*0.05){
        a_VirtualSource* s = a_playVirtualSource(54,-4,e->aabb.position,NULL );
        a_setVSLoop(s,false);
        a_setVSPos(s,e->aabb.position);
        a_setVSVel(s,e->velocity);
        a_setVSGain(s,fn_remap(fn_length(e->velocity),0,0.5,0.2,0.3));
        a_setVSPitch(s,fn_remap(fn_length(e->velocity),0,0.5,0.8,1.3));
      }

      info->has_gravity = true;
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

    fn_mat4 m = th_6dofCamera(&info->old_forward,&info->old_right,&info->old_up,fn_normalizeVec3(info->old_forward),e->aabb.position);
    int cstart = info->cprog;
    float interp = info->interp;
    float theta = CalculatePosition1D(info->angles[cstart + 0],info->angles[cstart + 1],info->angles[cstart + 2],info->angles[cstart + 3],interp);
    fn_mat4 r = fn_makerotate(theta,fn_createVec3(0,0,1));
    m = fn_multMat4(m,r);
      float scaling = 11.5*c->config.scale;
    m = fn_multMat4(fn_makescale(fn_createVec3s(scaling)),fn_inverse(m));
    c->transforms_body[j] = m;
    c->agents[j].old_matrix = m;
    c->agents[j].position = e->aabb.position;

    c->frustum_data->min_x[c->frustum_offset + j] = e->aabb.position.x - c->cullradius;
    c->frustum_data->min_y[c->frustum_offset + j] = e->aabb.position.y - c->cullradius;
    c->frustum_data->min_z[c->frustum_offset + j] = e->aabb.position.z - c->cullradius;

    c->frustum_data->max_x[c->frustum_offset + j] = e->aabb.position.x + c->cullradius;
    c->frustum_data->max_y[c->frustum_offset + j] = e->aabb.position.y + c->cullradius;
    c->frustum_data->max_z[c->frustum_offset + j] = e->aabb.position.z + c->cullradius;



  }

  return allhasphysics;
}

void th_centipedeUpdate(th_CentipedeGroup* c,float dt,fn_RawInput* input)
{
  fn_vec3 target = c->levelstate->player_e.aabb.position;
  th_World* world = c->levelstate->world;

  float imag_dt = 5;
  //update masters
  for (int i = 0 ; i < c->num_masters;i++)
  {
    th_MasterInfo* info = &c->masters[i];
    th_AgentInfo* agent = &c->agents[info->start];//
    th_AgentInfo* agent_end = &c->agents[info->start + info->count - 1];
    th_AgentInfo* agent_middle = &c->agents[info->start + info->count/2];
    bool hasonegem = false;
    bool spawned = true;
    if (th_time() - c->levelstate->level_start_time < info->spawn_when)
    {
      spawned = false;
    }
    else if (!info->spawn_finished)
    {
      //activate entities
      for (int j = info->start;j < info->start + info->count;j++)
      {
          c->entities_gems[j].alive = true;
          c->agents[j].spawned = true;
      }
      info->spawn_finished = true;
    }
    //handle impacts

    for (int j = info->start;j < info->start + info->count;j++)
    {
      if (c->entities_gems[j].impact)
      {
        float gem_old_health = c->agents[j].gem_health;
        //TODO accoutn for velocity of segments? and delta time
        for (int k = 0 ; k < c->entities_gems[j].impact_count;k++ )
        {
          th_Entity* projectile = (th_Entity*)c->entities_gems[j].impacts[k].entity;
          fn_vec3 vel = fn_multVec3s(fn_normalizeVec3(projectile->velocity),-1.0);

          // fn_vec3 hitdir = fn_subVec3(c->entities_gems[j].impacts[k].pos,c->agents[j].position);
          // hitdir = fn_normalizeVec3(hitdir);
          //fn_vec3 gemdir = c->agents[j].old_up;


          fn_vec3 gemdir = fn_transformNormal(fn_createVec3(0,-1,0),c->transforms_body[j]);
          bool gem_impacted = (fn_dot(gemdir,vel) > -0.1  || projectile->type == TH_HAMMER_PROJECTILE )&& c->agents[j].hasgem;

          if (projectile->type == TH_HAMMER_PROJECTILE || gem_impacted)
          {
            float mag = 3.0;
            float time_mag = 1.0;
            if (projectile->type != TH_HAMMER_PROJECTILE)
            {
              mag = 2.0;
              time_mag = 0.6666;
            }

            float target_a = fn_dot(gemdir,vel) < 0 ? -100*mag : 100*mag;
            float target_b = fn_dot(gemdir,vel) < 0 ? -50*mag : 50*mag;
            float target_c = fn_dot(gemdir,vel) < 0 ? -25*mag : 25*mag;
            float target_d = fn_dot(gemdir,vel) < 0 ? -12.5*mag : 12.5*mag;
            c->normal_perturb_target[j] = target_a;
            c->normal_perturb_timer[j] = th_time() + SNAKE_PERTURB_TIME*time_mag;
            if (j - 1 >= info->start )
            {
              c->normal_perturb_target[j - 1] = target_b;
              c->normal_perturb_timer[j - 1] = th_time() + SNAKE_PERTURB_TIME*time_mag;
            }
            if (j + 1 < info->start + info->count)
            {
              c->normal_perturb_target[j + 1] = target_b;
              c->normal_perturb_timer[j + 1] = th_time() + SNAKE_PERTURB_TIME*time_mag;
            }

            if (j - 2 >= info->start )
            {
              c->normal_perturb_target[j - 2] = target_c;
              c->normal_perturb_timer[j - 2] = th_time() + SNAKE_PERTURB_TIME*time_mag;
            }
            if (j + 2 < info->start + info->count)
            {
              c->normal_perturb_target[j + 2] = target_c;
              c->normal_perturb_timer[j + 2] = th_time() + SNAKE_PERTURB_TIME*time_mag;
            }

            if (j - 3 >= info->start )
            {
              c->normal_perturb_target[j - 3] = target_d;
              c->normal_perturb_timer[j - 3] = th_time() + SNAKE_PERTURB_TIME*time_mag;
            }
            if (j + 3 < info->start + info->count)
            {
              c->normal_perturb_target[j + 3] = target_d;
              c->normal_perturb_timer[j + 3] = th_time() + SNAKE_PERTURB_TIME*time_mag;
            }
          }
        //  float attack_angle = fn_angle(hitdir,gemdir);
          if ( gem_impacted ) //&& fn_angle(gemdir,vel) < fn_radians(90)
          {

            th_Hitmarker hmarker;
            hmarker.entity_pos_ref = NULL;
            hmarker.entity_transform_ref = &c->transforms_gems[j];
            hmarker.alive_ref = &c->agents[j].hasgem;
            hmarker.timer = th_time();
            hmarker.last_good_pos = c->agents[j].position;
            hmarker.is_alive = true;
            hmarker.radius = 65.0*c->config.scale;
            th_pushHitmarker(hmarker);

            //c->entities_gems[j].alive = false;
            if (projectile->type == TH_MACHINEGUN_BULLET)
            {
              c->agents[j].gem_health = c->agents[j].gem_health - 4.0;
            }
            else if (projectile->type == TH_SHOTGUN_SHELL)
            {
              c->agents[j].gem_health = c->agents[j].gem_health - 5.0;
            }
            else
            {
              c->agents[j].gem_health = c->agents[j].gem_health - 100.0;
            }

            if (c->agents[j].gem_health <= 0.0)
            {
              c->agents[j].hasgem = false;
              th_gemSpawn(c->levelstate->gems,c->agents[j].position,fn_multVec3s(gemdir,0.5));

              if (gem_old_health > 2.0)
              {
                {
                  a_VirtualSource* s = a_playVirtualSource(sound_gem_breakfree,0,c->agents[j].position,NULL );
                  a_setVSLoop(s,false);
                  a_setVSPos(s,c->agents[j].position);
                  a_setVSVel(s,fn_createVec3s(0));
                  a_setVSGain(s,1.0);
                  a_setVSPitch(s,th_randomFloat(0.85,1.15));
                }
              }
            }

            th_spawnBloodNoSound(c->agents[j].position,0);

            fn_vec3 b1 = fn_multVec3s(fn_createVec3(1,0,0),th_randomFloat(-1.0,1.0));
            fn_vec3 b2 = fn_multVec3s(fn_createVec3(0,1,0),th_randomFloat(-1.0,1.0));
            fn_vec3 b3 = fn_multVec3s(fn_createVec3(0,0,1),th_randomFloat(-1.0,1.0));
            if (c->agents[j].gem_jitter_t <= ((1.0/0.05)*3.14159)*5*0.5)
            {
              c->agents[j].gem_jitter_x = fn_normalizeVec3(fn_addVec3(b1,fn_addVec3(b2,b3)));
              c->agents[j].gem_jitter_t = ((1.0/0.05)*3.14159)*5;//10*3.14159*9;

              if (c->agents[j].hasgem)
              {
                {
                  a_VirtualSource* s = a_playVirtualSource(sound_gem_impact,0,c->agents[j].position,NULL );
                  a_setVSLoop(s,false);
                  a_setVSPos(s,c->agents[j].position);
                  a_setVSVel(s,fn_createVec3s(0));
                  a_setVSGain(s,0.8);
                  a_setVSPitch(s,th_randomFloat(0.85,1.15));
                }
              }
            }
          }
          else
          {
             float neg = fn_dot(gemdir,vel) > 0 ? 1 : -1;
             th_spawnSparks(fn_multVec3s(gemdir,neg),c->entities_gems[j].impacts[k].pos,-1,c->levelstate->general_light_query);
             th_spawnSparks(fn_multVec3s(gemdir,neg),c->entities_gems[j].impacts[k].pos,-1,c->levelstate->general_light_query);
             // th_spawnSparks(fn_multVec3s(gemdir,neg),c->entities_gems[j].impacts[k].pos,-1);
             // th_spawnSparks(fn_multVec3s(gemdir,neg),c->entities_gems[j].impacts[k].pos,-1);

             {
               a_VirtualSource* s = a_playVirtualSource(15 + th_random() % 3,-1,c->entities_gems[j].impacts[k].pos,NULL );
               a_setVSLoop(s,false);
               a_setVSPos(s,c->entities_gems[j].impacts[k].pos);
               a_setVSVel(s,fn_createVec3s(0));
               a_setVSGain(s,0.55);
             }

          }


        }
        c->entities_gems[j].impact_count = 0;

        //do something based on angle of the hit
        //take dot product(angle) of direction of gem(down basis) and vector from center to hit

      }
      hasonegem = hasonegem || c->agents[j].hasgem;

      if (c->entities_gems[j].alive)
      {
        th_pushOccluderFrame(fn_createVec4Vec3(c->agents[j].position,150.0*c->agents[j].master->config.scale));
      }
    }
    //|| input->currentKeyStates[SDL_SCANCODE_Q]
    if ((!hasonegem  ) && info->state != TH_HASPHYSICS && info->state != TH_ALL_HASPHYSICS)
    {
      th_markEnemyDeath(1);
      th_setGameplayTimeScale(fn_createVec3(0.45,0.00000,0.0000002));
      info->state = TH_HASPHYSICS;
      th_timer_t phy_time = th_time();
      for (int j = info->start;j < info->start + info->count;j++)
      {
        c->entities_gems[j].alive = false;
        c->agents[j].entity.aabb.position = fn_addVec3(c->agents[j].position,c->normal_perturb_offset[j]);
        fn_vec3 gemdir = fn_transformNormal(fn_createVec3(0,-1,0),c->transforms_body[j]);
        fn_vec3 momentum = fn_multVec3s(c->agents[j].old_forward,c->agents[j].speed);
        c->agents[j].entity.velocity = fn_addVec3(momentum,fn_multVec3s(gemdir,c->agents[j].speed*0.25));
        c->agents[j].entity.grounded = false;
        c->agents[j].physics_time = phy_time;
        phy_time += 45.0;
      }
    }

    if (info->state == TH_SPAWNLOOP && th_time() - c->levelstate->level_start_time > info->spawnloop_time + info->spawn_when  && agent->cprog < agent_end->cprog)
    {
      info->state = TH_FOLLOW;
      info->start_time = th_time();
      CentiPlotCourse(c,target,i);
    }
    else if (info->state == TH_FOLLOW)
    {

      int max_course_pos = MAX_COURSE_SIZE - 35;
      //this shouldnt happen, but it does and its not worth hunting down the original cause
      if (!(agent->cprog < agent_end->cprog) )
      {
        printf("CPROG OUT OF ORDER\n");
        //reset the centi
        agent->cprog = agent->cprog - agent_end->cprog;
        if (agent->cprog < 4)
        {
          agent->cprog = 4;
        }

        agent->interp = 0.0;
        //stepforward(agent,0,1000);

        //back the centipede up, march segments forward
        for (int j = info->start + 1;j < info->start + info->count;j++)
        {
          c->agents[j].cprog = agent->cprog;
          c->agents[j].interp = 0;
          stepforward(&c->agents[j],c->config.scale*150*(j - info->start),1000);
        }

        info->start_time = th_time();
        CentiPlotCourse(c,target,i);
      }
      else if ( agent->cprog < agent_end->cprog && (agent_end->cprog > max_course_pos || (agent_end->cprog >= info->target_index ) || th_time() - info->start_time > (info->config.retrack_interval/(info->config.speed/0.8)))  )
      {
        //plot new course
        info->start_time = th_time();
        CentiPlotCourse(c,target,i);
      }
      if (agent->speed < c->config.speed)
      {
        agent->speed += 0.00005*dt;
      }
      if (agent->speed > c->config.speed)
      {
        agent->speed = c->config.speed;
      }
      //agent->speed = 0;
    }



    //update agents
    if (info->state != TH_ALL_HASPHYSICS)
    {
      if (spawned || (!spawned && !info->spawn_init))
      {
        updateAgents(c,info->start,info->start + info->count,dt,world);
        info->spawn_init = true;
      }

    }

    if (!spawned)
    {
      //fadein
      for (int j = info->start;j < info->start + info->count;j++)
      {
        if (th_time() - c->levelstate->level_start_time > info->spawn_when - 1000)
        {
          c->transforms_body[j] = fn_multMat4(th_fadeinMatrix(&c->agents[j].fadeout_spawn,0.003*dt),c->agents[j].old_matrix);
          c->transforms_gems[j] = c->transforms_body[j];
        }
        else
        {
          c->transforms_body[j] = fn_makescale(fn_createVec3s(0));
          c->transforms_gems[j] = fn_makescale(fn_createVec3s(0));
        }

      }
    }

    if (info->state == TH_HASPHYSICS || info->state == TH_ALL_HASPHYSICS)
    {
      bool allhasphysics = updateAgentsPhysics(c,info->start,info->start + info->count,dt,world);
      if (allhasphysics)
      {
        info->state = TH_ALL_HASPHYSICS;
      }
    }

    if (spawned)
    {
      for (int ag = info->start; ag < info->start + info->count; ag+=2) {

        if (info->state != TH_ALL_HASPHYSICS)
        {
          if (info->state != TH_HASPHYSICS || (info->state == TH_HASPHYSICS && !c->agents[ag].hasphysics))
          {
            if (info->audio_sources[ag - info->start] == NULL)
            {
              info->audio_sources[ag - info->start] = a_playVirtualSource(18,-1,c->agents[ag].position,NULL);
              a_VirtualSource* s = info->audio_sources[ag - info->start];
              a_setVSLoop(s,true);
              a_setVSPos(s,c->agents[ag].position);
              a_setVSVel(s,c->agents[ag].velocity);
              a_setVSGain(s,0.35);
              float offset = (float)th_random()/(float)(RAND_MAX/(8.5));
              a_setVSOffset(s,offset);
            }
            else
            {
              a_VirtualSource* s = info->audio_sources[ag - info->start];
              a_setVSPos(s,c->agents[ag].position);
              a_setVSVel(s,c->agents[ag].velocity);
            }
          }
          else if (info->state == TH_HASPHYSICS && c->agents[ag].hasphysics)
          {
            if (info->audio_sources[ag - info->start] != NULL)
            {
              a_VirtualSource* s = info->audio_sources[ag - info->start];
              a_stopVS(s);
              info->audio_sources[ag - info->start] = NULL;
            }
          }
        }
        else
        {
          if (info->audio_sources[ag - info->start] != NULL)
          {
            a_VirtualSource* s = info->audio_sources[ag - info->start];
            a_stopVS(s);
            info->audio_sources[ag - info->start] = NULL;
          }
        }


      }




    }

    if (spawned)
    {
      for (int j = info->start;j < info->start + info->count;j++)
      {
        const float lgscl = 20;
        const float dimens = 1.519*20*5*c->config.scale;



        if (c->agents[j].hasphysics && !c->agents[j].entity.alive)
        {
          float fdtemp = c->agents[j].fadeout + 0.003*dt;
          fn_mat4 fdout = th_fadeoutMatrix(&fdtemp,0.003*dt);

          c->transforms_legs_a[j*2 + 0] = fn_multMat4(fdout,c->transforms_legs_a_old[j*2 + 0]);
          c->transforms_legs_a[j*2 + 1] = fn_multMat4(fdout,c->transforms_legs_a_old[j*2 + 1]);
          c->transforms_legs_b[j*2 + 0] = fn_multMat4(fdout,c->transforms_legs_b_old[j*2 + 0]);
          c->transforms_legs_b[j*2 + 1] = fn_multMat4(fdout,c->transforms_legs_b_old[j*2 + 1]);
        }
        else
        {

          float time = th_time(); // Assuming you have a time variable
          float wave_speed = 0.001*4.0;
          float wave_frequency = 0.3*2.0;
          float leg_phase_offset = j; // Each segment slightly out of phase
          float side_phase_offset = 3.14159; // Opposite sides move in opposite directions

          float osc_0 = sin(time * wave_speed + leg_phase_offset * wave_frequency);
          float osc_1 = sin(time * wave_speed + leg_phase_offset * wave_frequency + side_phase_offset);

          osc_0 = osc_0*0.5 + 0.5;
          osc_1 = osc_1*0.5 + 0.5;

          float swing_amplitude = -10.0; // How much the legs swing
          float osco_o = -0.3;

          fn_vec3 bone_basedir_a_0 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(1, osco_o + osc_0 * swing_amplitude, 0)), c->transforms_body[j]);
          fn_vec3 bone_basedir_b_0 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(-0.75, osco_o + osc_0 * swing_amplitude * 0.7, 0)), c->transforms_body[j]);

          fn_vec3 bone_basedir_a_1 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(-1, osco_o + osc_1 * swing_amplitude, 0)), c->transforms_body[j]);
          fn_vec3 bone_basedir_b_1 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(0.75, osco_o + osc_1 * swing_amplitude * 0.7, 0)), c->transforms_body[j]);

          // fn_vec3 bone_basedir_a_0 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(1,-0.5,0)),c->transforms_body[j]);
          // fn_vec3 bone_basedir_b_0 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(-0.75,-0.5,0)),c->transforms_body[j]);
          //
          // fn_vec3 bone_basedir_a_1 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(-1,-0.5,0)),c->transforms_body[j]);
          // fn_vec3 bone_basedir_b_1 = fn_transformNormal(fn_normalizeVec3(fn_createVec3(0.75,-0.5,0)),c->transforms_body[j]);

          fn_vec3 bone_basedir = fn_transformNormal(fn_createVec3(1,0,0),c->transforms_body[j]);

          //c->agents[j].position
          fn_vec3 agent_position = fn_createVec3(c->transforms_body[j].m[12],c->transforms_body[j].m[13],c->transforms_body[j].m[14]);

          fn_vec3 bone_base0 = fn_addVec3(agent_position,fn_multVec3s(bone_basedir,50));
          fn_vec3 bone_base1 = fn_addVec3(agent_position,fn_multVec3s(bone_basedir,-50));

          if (!info->spawned_legs)
          {
            c->bone_point_a[j*2 + 0] = fn_addVec3(bone_base0,fn_multVec3s(bone_basedir_a_0,dimens));
            c->bone_point_b[j*2 + 0] = fn_addVec3(c->bone_point_a[j*2 + 0],fn_multVec3s(bone_basedir_b_0,dimens));

            c->bone_point_a[j*2 + 1] = fn_addVec3(bone_base1,fn_multVec3s(bone_basedir_a_1,dimens));
            c->bone_point_b[j*2 + 1] = fn_addVec3(c->bone_point_a[j*2 + 1],fn_multVec3s(bone_basedir_b_1,dimens));
          }
          else
          {
            //ragdoll sim
            //distance constraints
            //angle constrains
            fn_vec3 ragdoll_positions[3];
            ragdoll_positions[0] = bone_base0;
            ragdoll_positions[1] = c->bone_point_a[j*2 + 0];
            ragdoll_positions[2] = c->bone_point_b[j*2 + 0];
            float ragdoll_masses[3];
            ragdoll_masses[0] = 0;
            ragdoll_masses[1] = 1.0/10;
            ragdoll_masses[2] = 1.0/20;
            th_RagdollConstraint ragdoll_constraints[2];
            ragdoll_constraints[0].index0 = 0;
            ragdoll_constraints[0].index1 = 1;
            ragdoll_constraints[0].d = dimens;
            ragdoll_constraints[1].index0 = 1;
            ragdoll_constraints[1].index1 = 2;
            ragdoll_constraints[1].d = dimens;

            th_SimpleAngularConstraint ragdoll_constraints_theta[2];
            ragdoll_constraints_theta[0].index0 = 1;
            ragdoll_constraints_theta[0].index1 = 0;
            ragdoll_constraints_theta[0].angle = 20.0;
            ragdoll_constraints_theta[0].axis = bone_basedir_a_0;
            ragdoll_constraints_theta[1].index0 = 2;
            ragdoll_constraints_theta[1].index1 = 1;
            ragdoll_constraints_theta[1].angle = 7.0;
            ragdoll_constraints_theta[1].axis = bone_basedir_b_0;

            th_SimpleRagdoll rgdll;
            rgdll.num_positions = 3;
            rgdll.num_constraints = 2;
            rgdll.num_angle_constraints = 2;
            rgdll.positions = ragdoll_positions;
            rgdll.masses = ragdoll_masses;
            rgdll.constraints = ragdoll_constraints;
            rgdll.angle_constraints = ragdoll_constraints_theta;
            rgdll.stiffness = 0.7f;

            th_simulateSimpleRagdoll(&rgdll);

            c->bone_point_a[j*2 + 0] = ragdoll_positions[1];
            c->bone_point_b[j*2 + 0] = ragdoll_positions[2];

            ragdoll_positions[0] = bone_base1;
            ragdoll_positions[1] = c->bone_point_a[j*2 + 1];
            ragdoll_positions[2] = c->bone_point_b[j*2 + 1];


            ragdoll_constraints_theta[0].axis = bone_basedir_a_1;

            ragdoll_constraints_theta[1].axis = bone_basedir_b_1;

            th_simulateSimpleRagdoll(&rgdll);

            c->bone_point_a[j*2 + 1] = ragdoll_positions[1];
            c->bone_point_b[j*2 + 1] = ragdoll_positions[2];

          }



          fn_vec3 bone_a_0 = fn_normalizeVec3(fn_subVec3(c->bone_point_a[j*2 + 0],bone_base0));
          fn_quat tr_a_0 = fn_mat4toquat(fn_inverse(th_6dofCamera(NULL,NULL,&c->legs_a_up[j*2 + 0],bone_a_0,fn_createVec3(0,0,0))));
          fn_vec3 l_a_0 = fn_multVec3s(fn_addVec3(c->bone_point_a[j*2 + 0],bone_base0),0.5);

          fn_vec3 bone_b_0 = fn_normalizeVec3(fn_subVec3(c->bone_point_b[j*2 + 0],c->bone_point_a[j*2 + 0]));
          fn_quat tr_b_0 = fn_mat4toquat(fn_inverse(th_6dofCamera(NULL,NULL,&c->legs_b_up[j*2 + 0],bone_b_0,fn_createVec3(0,0,0))));
          fn_vec3 l_b_0 = fn_multVec3s(fn_addVec3(c->bone_point_b[j*2 + 0],c->bone_point_a[j*2 + 0]),0.5);

          fn_vec3 bone_a_1 = fn_normalizeVec3(fn_subVec3(c->bone_point_a[j*2 + 1],bone_base1));
          fn_quat tr_a_1 = fn_mat4toquat(fn_inverse(th_6dofCamera(NULL,NULL,&c->legs_a_up[j*2 + 1],bone_a_1,fn_createVec3(0,0,0))));
          fn_vec3 l_a_1 = fn_multVec3s(fn_addVec3(c->bone_point_a[j*2 + 1],bone_base1),0.5);

          fn_vec3 bone_b_1 = fn_normalizeVec3(fn_subVec3(c->bone_point_b[j*2 + 1],c->bone_point_a[j*2 + 1]));
          fn_quat tr_b_1 = fn_mat4toquat(fn_inverse(th_6dofCamera(NULL,NULL,&c->legs_b_up[j*2 + 1],bone_b_1,fn_createVec3(0,0,0))));
          fn_vec3 l_b_1 = fn_multVec3s(fn_addVec3(c->bone_point_b[j*2 + 1],c->bone_point_a[j*2 + 1]),0.5);


          c->transforms_legs_a[j*2 + 0] = fn_translaterotatescaleq(l_a_0,tr_a_0,fn_createVec3s(c->config.scale*lgscl));
          c->transforms_legs_a[j*2 + 1] = fn_translaterotatescaleq(l_a_1,tr_a_1,fn_createVec3s(c->config.scale*lgscl));
          c->transforms_legs_b[j*2 + 0] = fn_translaterotatescaleq(l_b_0,tr_b_0,fn_createVec3s(c->config.scale*lgscl));
          c->transforms_legs_b[j*2 + 1] = fn_translaterotatescaleq(l_b_1,tr_b_1,fn_createVec3s(c->config.scale*lgscl));

          c->transforms_legs_a_old[j*2 + 0] = c->transforms_legs_a[j*2 + 0];
          c->transforms_legs_a_old[j*2 + 1] = c->transforms_legs_a[j*2 + 1];
          c->transforms_legs_b_old[j*2 + 0] = c->transforms_legs_b[j*2 + 0];
          c->transforms_legs_b_old[j*2 + 1] = c->transforms_legs_b[j*2 + 1];
        }


      }
      info->spawned_legs = true;
    }




  }



}

int th_centipedeDamageCallback(void* ep,void* pep,float dt,void* world)
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

void th_centipedeInitialize(th_Allocator* alloc,th_CentipedeGroup* c,int count,int length,int count_masters,float* spawnloop_times,th_CentipedeCourse* courses,th_LevelState* levelstate,th_CentiConfig config)
{
  c->config = config;
  c->levelstate = levelstate;
  // if (temp_memory == NULL)
  // {
    temp_memory = th_alloc(alloc,sizeof(fn_vec3)*MAX_COURSE_SIZE);
  //
  // }
  // int course_count = 23 ;
  // fn_vec3* course = th_alloc(alloc,sizeof(fn_vec3)*MAX_COURSE_SIZE);
  // float* angles = th_alloc(alloc,sizeof(float)*MAX_COURSE_SIZE);
  // fn_vec3* ups = th_alloc(alloc,sizeof(fn_vec3)*(MAX_COURSE_SIZE + MAX_COURSE_SIZE - 1)) ;
  // for (int i = 0 ; i < MAX_COURSE_SIZE;i++)
  // {
  //   angles[i] = 0;
  //   course[i] = fn_createVec3(0,0,0);
  // }
  //
  // for (int i = 0 ; i < 23;i++)
  // {
  //   float theta = (i/22.0)*2*3.14159265359*5;
  //   course[i] = fn_createVec3(sin(theta)*500.0,600 - (i/22.0)*2000,cos(theta)*500.0);
  // }
  //
  // fn_vec3* course2 = th_alloc(alloc,sizeof(fn_vec3)*MAX_COURSE_SIZE);
  // float* angles2 = th_alloc(alloc,sizeof(float)*MAX_COURSE_SIZE);
  // fn_vec3* ups2 = th_alloc(alloc,sizeof(fn_vec3)*(MAX_COURSE_SIZE + MAX_COURSE_SIZE - 1)) ;
  // for (int i = 0 ; i < MAX_COURSE_SIZE;i++)
  // {
  //   angles2[i] = 0;
  //   course2[i] = fn_createVec3(0,0,0);
  // }
  //
  // for (int i = 0 ; i < 23;i++)
  // {
  //   float theta = (i/22.0)*2*3.14159265359*5;
  //   course2[i] = fn_createVec3(cos(theta)*500.0,600 - (i/22.0)*2000,sin(theta)*500.0);
  // }
  c->cullradius = 200*c->config.scale;
  c->length_of_snake = length;
  //hitboxes
  c->entities_gems = th_alloc(alloc,sizeof(th_Entity)*count);
  c->entities_body = th_alloc(alloc,sizeof(th_Entity)*count);
  for (int i= 0 ; i < count;i++)
  {
    c->entities_gems[i] = TH_DEFAULT_ENTITY;
    c->entities_gems[i].mode = TH_SLIDE_MODE;
    c->entities_gems[i].aabb.position = fn_createVec3(0,0,0);
    c->entities_gems[i].aabb.hwidth = fn_createVec3(30*c->config.scale,30*c->config.scale,30*c->config.scale);
    c->entities_gems[i].velocity = fn_createVec3s(0);
    c->entities_gems[i].grounded = false;
    c->entities_gems[i].aabb.mode = BOX;
    c->entities_gems[i].alive = false;
    c->entities_gems[i].impact = false;
    c->entities_gems[i].impact_count = 0;
    c->entities_gems[i].damage_callback = th_centipedeDamageCallback;

    c->entities_body[i] = TH_DEFAULT_ENTITY;
    c->entities_body[i].mode = TH_SLIDE_MODE;
    c->entities_body[i].aabb.position = fn_createVec3(0,0,0);
    c->entities_body[i].aabb.hwidth = fn_createVec3(30*c->config.scale,30*c->config.scale,30*c->config.scale);
    c->entities_body[i].velocity = fn_createVec3s(0);
    c->entities_body[i].grounded = false;
    c->entities_body[i].aabb.mode = BOX;
    c->entities_body[i].alive = true;
    c->entities_body[i].impact = false;
    c->entities_body[i].impact_count = 0;
  }

  c->agents = th_alloc(alloc,sizeof(th_AgentInfo)*count);
  for (int k = 0 ; k <count_masters;k++)
  {
    for (int i = k*length;i < (k+1)*length;i++)
    {
      c->agents[i].spawned = false;
      c->agents[i].has_gravity = false;
      c->agents[i].course = courses[k].course;
      c->agents[i].angles = courses[k].angles;
      c->agents[i].course_count = courses[k].course_count;
      c->agents[i].cprog = 0;
      c->agents[i].interp = 0;
      c->agents[i].position = courses[k].course[0];
      c->agents[i].speed = c->config.spawn_speed;//0.7;
      c->agents[i].upvectors = courses[k].ups;
      c->agents[i].hasgem = true;
      c->agents[i].entity = TH_DEFAULT_ENTITY;
      c->agents[i].entity.aabb.position = fn_createVec3(10000000,10000000,10000000);
      c->agents[i].entity.aabb.hwidth = fn_createVec3(30*c->config.scale,30*c->config.scale,30*c->config.scale);
      c->agents[i].entity.velocity = fn_createVec3s(0);
      c->agents[i].entity.grounded = false;
      c->agents[i].entity.collided = false;
      c->agents[i].entity.aabb.mode = SPHERE;
      c->agents[i].entity.alive = true;
      c->agents[i].hasphysics = false;
      c->agents[i].hit = fn_createVec3s(1000000);
      c->agents[i].prevhit = false;
      c->agents[i].crashsource = NULL;
      c->agents[i].old_matrix = fn_identityMat4();
      c->agents[i].fadeout = 3.0;
      c->agents[i].fadeout_spawn = 1.0;
      c->agents[i].gem_health = c->config.gem_health;
      c->agents[i].gem_jitter_t = 0.0;
      c->agents[i].gem_jitter_x = fn_createVec3(0,0,0);

      // if (i >= 20)
      // {
      //   c->agents[i].course = course2;
      //   c->agents[i].angles = angles2;
      //   c->agents[i].course_count = course_count;
      //   c->agents[i].upvectors = ups2;
      //   c->agents[i].position = course2[0];
      // }
    }

    for (int i = 1 + k*length ; i < (k + 1)*length;i++)
    {
      stepforward(&c->agents[i],c->config.scale*150*(i - k*length),1000);
    }
    // for (int i = 21 ; i < 40;i++)
    // {
    //   stepforward(&c->agents[i],150*(i - 21),1000);
    // }
  }

  // for (int i = 1 ; i < 20;i++)
  // {
  //   stepforward(&c->agents[i],150*i,1000);
  // }
  // for (int i = 21 ; i < 40;i++)
  // {
  //   stepforward(&c->agents[i],150*(i - 21),1000);
  // }
  for (int i = 0 ; i < count;i++)
  {
    th_AgentInfo* info = &c->agents[i];
    float interp = info->interp;
    int cstart = info->cprog;
    fn_vec3 pos = CalculatePosition(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);
    fn_vec3 forward = CalculateTangent(info->course[cstart],info->course[cstart + 1],info->course[cstart + 2],info->course[cstart + 3],interp);

    fn_vec3 f;
    fn_vec3 s;
    fn_vec3 u;

    f = forward;

    s = fn_cross(f, fn_createVec3(0,-1,0));
    s = fn_normalizeVec3(s);

    u = fn_cross(s, f);

    info->old_forward = f;
    info->old_up = u;
    info->old_right = s;
  }


  for (int i = 0 ; i < count_masters;i++)
  {
  //  computeUpVectors(course,ups,fn_createVec3(0,-1,0),c->agents[0].old_right,c->agents[0].old_forward,course_count);
    computeUpVectors(courses[i].course,courses[i].ups,fn_createVec3(0,-1,0),c->agents[i*length].old_right,c->agents[i*length].old_forward,courses[i].course_count);
  }
  // computeUpVectors(course,ups,fn_createVec3(0,-1,0),c->agents[0].old_right,c->agents[0].old_forward,course_count);
  // computeUpVectors(course2,ups2,fn_createVec3(0,-1,0),c->agents[20].old_right,c->agents[20].old_forward,course_count);

  //stepforward(&c->agents[1],150);
  //stepforward(&c->agents[2],300);
  c->leg_count = count*2;
  c->transforms_legs_a = th_alloc(alloc,sizeof(fn_mat4)*c->leg_count);

  c->transforms_legs_b = th_alloc(alloc,sizeof(fn_mat4)*c->leg_count);

  c->transforms_legs_a_old = th_alloc(alloc,sizeof(fn_mat4)*c->leg_count);
  c->transforms_legs_b_old = th_alloc(alloc,sizeof(fn_mat4)*c->leg_count);

  c->legs_a_up = th_alloc(alloc,sizeof(fn_vec3)*c->leg_count);
  c->legs_b_up = th_alloc(alloc,sizeof(fn_vec3)*c->leg_count);

  c->bone_point_a = th_alloc(alloc,sizeof(fn_vec3)*c->leg_count);
  c->bone_point_b = th_alloc(alloc,sizeof(fn_vec3)*c->leg_count);

  for (int i = 0 ; i < c->leg_count;i++)
  {
    c->transforms_legs_a_old[i] = fn_makescale(fn_createVec3s(0));
    c->transforms_legs_b_old[i] = fn_makescale(fn_createVec3s(0));

    c->transforms_legs_a[i] = fn_makescale(fn_createVec3s(0));
    c->transforms_legs_b[i] = fn_makescale(fn_createVec3s(0));
    c->legs_a_up[i] = fn_createVec3(1,0,0);
    c->legs_b_up[i] = fn_createVec3(1,0,0);

    c->bone_point_a[i] = fn_createVec3(0,0,0);
    c->bone_point_b[i] = fn_createVec3(0,0,0);

  }

  c->transforms_gems = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->transforms_body = th_alloc(alloc,sizeof(fn_mat4)*count);
  c->normal_perturb = th_alloc(alloc,sizeof(float)*count);
  c->normal_perturb_offset = th_alloc(alloc,sizeof(fn_vec3)*count);
  c->normal_perturb_target = th_alloc(alloc,sizeof(float)*count);
  c->normal_perturb_timer = th_alloc(alloc,sizeof(th_timer_t)*count);
  for (int i = 0 ; i < count;i++)
  {
    c->transforms_body[i] = fn_makescale(fn_createVec3s(0));
    c->transforms_gems[i] = fn_makescale(fn_createVec3s(0));
    c->normal_perturb[i] = 0;
    c->normal_perturb_target[i] = 0.0;
    c->normal_perturb_timer[i] = 0.0;
    c->normal_perturb_offset[i] = fn_createVec3s(0.0);


  }

  c->count = count;
  c->forward = fn_createVec3(0,0,0);
  c->pos = fn_createVec3(0,0,0);
  c->rollmat = fn_identityMat4();

  c->num_masters = count_masters;
  c->masters = th_alloc(alloc,sizeof(th_MasterInfo)*c->num_masters);
  // c->masters[0].start = 0;
  // c->masters[0].count = 20;
  // c->masters[0].course = course;
  // c->masters[0].upvectors = ups;
  // c->masters[0].angles = angles;
  // c->masters[0].state = TH_SPAWNLOOP;
  // c->masters[0].t = 0;

  // c->masters[1].start = 20;
  // c->masters[1].count = 20;
  // c->masters[1].course = course2;
  // c->masters[1].upvectors = ups2;
  // c->masters[1].angles = angles2;
  // c->masters[1].state = TH_SPAWNLOOP;
  // c->masters[1].t = 0;

  for (int i = 0; i < count_masters; i++) {
    c->masters[i].spawn_finished = false;
    c->masters[i].spawn_init = false;
    c->masters[i].spawn_when = courses[i].spawn_when;
    c->masters[i].start = i*length;
    c->masters[i].count = length;
    c->masters[i].course = courses[i].course;
    c->masters[i].upvectors = courses[i].ups;
    c->masters[i].angles = courses[i].angles;
    c->masters[i].state = TH_SPAWNLOOP;
    c->masters[i].t = 0;
    c->masters[i].audio_sources = th_alloc(alloc,sizeof(a_VirtualSource*)*length);
    for (int j = 0; j < length; j++) {
      c->masters[i].audio_sources[j] = NULL;
    }
    c->masters[i].smoke_time = 0;
    c->masters[i].spawned_legs = false;

    c->masters[i].spawnloop_time = spawnloop_times[i];
    c->masters[i].config = config;
  }

  for (int k = 0 ; k <count_masters;k++)
  {
    for (int i = k*length;i < (k+1)*length;i++)
    {
      c->agents[i].master = &c->masters[k];
    }
  }
}
