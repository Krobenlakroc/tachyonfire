#include "th_brass.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_time.h"

#include "../fn_engine/th_level.h"

void th_brassInit(th_Allocator* alloc,th_BrassObject* brass,int count,th_LevelState* levelstate)
{
  brass->levelstate = levelstate;
  brass->num_used = 0;
  brass->current_count = 0;
  brass->entity_count = count;
  brass->entities = th_alloc(alloc,sizeof(th_Entity)*count);
  brass->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  brass->orientations = th_alloc(alloc,sizeof(fn_vec3)*count);
  brass->scale = th_alloc(alloc,sizeof(float)*count);
  brass->angles = th_alloc(alloc,sizeof(float)*count);
  brass->angular_vel = th_alloc(alloc,sizeof(float)*count);
  for (int i = 0 ; i < count;i++)
  {
    brass->scale[i] = 1.0;
    brass->entities[i] = TH_DEFAULT_ENTITY;
    brass->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
    brass->entities[i].aabb.hwidth = fn_createVec3(5,5,5);
    brass->entities[i].velocity = fn_createVec3s(0);
    brass->entities[i].grounded = false;
    brass->entities[i].collided = false;
    brass->entities[i].aabb.mode = SPHERE;
    brass->transforms[i] = fn_makescale(fn_createVec3s(0));
    brass->orientations[i] = fn_createVec3(1,0,0);

    brass->angles[i] = 0.0;
    brass->angular_vel[i] = 0.0;
  }
}


void th_brassUpdateTransform(th_BrassObject* brass,int i)
{
  fn_mat4* transforms = brass->transforms;
  th_Entity* entities = brass->entities;

  fn_vec3 ybasis = fn_normalizeVec3(brass->orientations[i]);
  fn_vec3 xbasis = fn_normalizeVec3(fn_cross(fabs(fn_dot(ybasis,fn_createVec3(0,1,0))) > 0.95 ? fn_createVec3(1,0,0) : fn_createVec3(0,1,0) ,ybasis));

  fn_vec3 zbasis = fn_normalizeVec3(fn_cross(xbasis,ybasis));

  fn_mat4 q_temp = fn_createMat4(xbasis.x,xbasis.y,xbasis.z,0,ybasis.x,ybasis.y,ybasis.z,0,zbasis.x,zbasis.y,zbasis.z,0,0,0,0,1);

  fn_mat4 rot_angle = fn_makerotate(brass->angles[i],xbasis);

  transforms[i] = fn_translaterotatescalem(entities[i].aabb.position,fn_multMat4(rot_angle,q_temp),fn_createVec3s(brass->scale[i]));
}

void th_brassUpdate(th_BrassObject* brass,float dt)
{
  th_World* world = brass->levelstate->world;
  int count = brass->entity_count;
  th_Entity* entities = brass->entities;
  fn_mat4* transforms = brass->transforms;


  for (int i = 0; i <brass->num_used;i++)
  {
    fn_vec3 oldvelocity = entities[i].velocity;
    bool oldground = entities[i].grounded;
    if (entities[i].grounded)
    {
      continue;
    }
    th_updateEntity(&entities[i],world,dt,th_getPhysicsMemory(world,0));
    if (!entities[i].grounded)
    {
      entities[i].velocity.y += 0.001*dt;
    }
    if (entities[i].collided)
    {
      entities[i].velocity.x = 0;
      entities[i].velocity.y = fn_max(entities[i].velocity.y,0);
      entities[i].velocity.z = 0;

      brass->angular_vel[i] = 0.0;
    }
    if (!oldground && entities[i].grounded && oldvelocity.y > 0.1)
    {

      {
        a_VirtualSource* s = a_playVirtualSource(5,-2,entities[i].aabb.position,NULL);
        a_setVSLoop(s,false);
        a_setVSPos(s,entities[i].aabb.position);
        a_setVSVel(s,fn_createVec3s(0));
        a_setVSGain(s,0.3);
      }



    }

    brass->angles[i] = brass->angles[i] + brass->angular_vel[i]*dt;


    // fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,1,0),brass->orientations[i]);


    // fn_vec3 ybasis = fn_normalizeVec3(brass->orientations[i]);
    // fn_vec3 xbasis = fn_normalizeVec3(fn_cross(fabs(fn_dot(ybasis,fn_createVec3(0,1,0))) > 0.95 ? fn_createVec3(1,0,0) : fn_createVec3(0,1,0) ,ybasis));
    //
    // fn_vec3 zbasis = fn_normalizeVec3(fn_cross(xbasis,ybasis));
    //
    // fn_mat4 q_temp = fn_createMat4(xbasis.x,xbasis.y,xbasis.z,0,ybasis.x,ybasis.y,ybasis.z,0,zbasis.x,zbasis.y,zbasis.z,0,0,0,0,1);
    //
    // fn_mat4 rot_angle = fn_makerotate(brass->angles[i],xbasis);
    //
    // transforms[i] = fn_translaterotatescalem(entities[i].aabb.position,fn_multMat4(rot_angle,q_temp),fn_createVec3s(brass->scale[i]));

    th_brassUpdateTransform(brass,i);


  }
}

int th_brassSpawn(th_BrassObject* brass,fn_vec3 position,fn_vec3 velocity,fn_vec3 orientation)
{
  int ret = brass->current_count;
  brass->scale[brass->current_count] = 1.0;
  brass->entities[brass->current_count].aabb.position = position;

  brass->entities[brass->current_count].velocity = velocity;
  brass->entities[brass->current_count].grounded = false;

  brass->orientations[brass->current_count] = orientation;

  brass->current_count++;

  if (brass->num_used < brass->entity_count)
  {
    brass->num_used++;
  }

  if (brass->current_count == brass->entity_count)
  {
    brass->current_count = 0;
  }
  return ret;
}


int th_brassSpawnScaled(th_BrassObject* brass,fn_vec3 position,fn_vec3 velocity,fn_vec3 orientation,float scale)
{
  int ret = brass->current_count;
  brass->scale[brass->current_count] = scale;
  brass->entities[brass->current_count].aabb.position = position;

  brass->entities[brass->current_count].velocity = velocity;
  brass->entities[brass->current_count].grounded = false;

  brass->orientations[brass->current_count] = orientation;

  brass->current_count++;

  if (brass->num_used < brass->entity_count)
  {
    brass->num_used++;
  }

  if (brass->current_count == brass->entity_count)
  {
    brass->current_count = 0;
  }

  return ret;
}
