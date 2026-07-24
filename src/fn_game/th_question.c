#include "th_question.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_time.h"
#include "th_builtins.h"
#include "../fn_engine/th_level.h"


void th_questionInit(th_Allocator* alloc,th_Question* o,int count,th_LevelState* levelstate)
{
    o->num_used = 0;
    o->current_count = 0;
    o->levelstate = levelstate;
    o->entity_count = count;
    o->entities = th_alloc(alloc,sizeof(th_Entity)*count);
    o->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
    o->lifes = th_alloc(alloc,sizeof(float)*count);
    o->is_collected = th_alloc(alloc,sizeof(bool)*count);
    for (int i = 0 ; i < count;i++)
    {
        o->entities[i] = TH_DEFAULT_ENTITY;
        o->entities[i].robust_collisions = false;
        o->entities[i].mode = TH_SLIDE_MODE;
        o->entities[i].aabb.position = fn_createVec3(10000000,10000000,10000000);
        o->entities[i].aabb.hwidth = fn_createVec3(30,30,30);
        o->entities[i].velocity = fn_createVec3s(0);
        o->entities[i].grounded = false;
        o->entities[i].collided = false;
        o->entities[i].aabb.mode = SPHERE;
        o->transforms[i] = fn_makescale(fn_createVec3(0,0,0));
        o->lifes[i] = -1.0;
        o->is_collected[i] = false;
    }
    o->index_last_touched = -1;
}

void th_questionUpdate(th_Question* o,float dt)
{
    const float scup = 2.25;
    const float hcross_scale = 1.75*scup;

    fn_vec3 target = o->levelstate->player_e.aabb.position;
    th_World* world = o->levelstate->world;
    int count = o->entity_count;
    th_Entity* entities = o->entities;
    fn_mat4* transforms = o->transforms;


    for (uint i = 0; i <o->num_used;i++)
    {
        fn_vec3 vpos = fn_addVec3(entities[i].aabb.position,fn_createVec3(0,sinf(th_time()*0.001*2.0)*45 - hcross_scale*9.0 - 45.0 ,0  ));
        if (o->lifes[i] < 0)
        {
            transforms[i] = fn_makescale(fn_createVec3(0,0,0));

            o->lifes[i] = o->lifes[i] + dt;

            if (o->lifes[i] >= -1000)
            {

                float alpha = fn_clamp(1.0 - (-o->lifes[i]/1000),0.0,1.0);
                transforms[i] = fn_translaterotatescale(vpos,th_time()*0.001*1.7,fn_createVec3(0,1,0),fn_createVec3s(hcross_scale*alpha));
            }

            if (o->lifes[i] >= 0)
            {
                o->lifes[i] = 1.0;
            }
            continue;
        }


        if (o->lifes[i] > 0 && fn_distance2(target,entities[i].aabb.position) <= 200*200*scup*scup && o->levelstate->player->hp < 200 )
        {
            o->lifes[i] = -60000;
            o->index_last_touched = i;
            o->is_collected[i] = true;
            // th_incrementPlayerHealth(o->levelstate->player,5);
            th_setGameGlow(fn_createVec3(2,1,0),0.2);
            //printf("%s\n","GOT GEM" );
        }



        // fn_quat q_temp = fn_getRotationQuaternion(fn_createVec3(0,-1,0),gem->orientations[i]);


        transforms[i] = fn_translaterotatescale(vpos,th_time()*0.001*1.7,fn_createVec3(0,1,0),fn_createVec3s(hcross_scale));




    }
}

void th_questionSpawn(th_Question* o,fn_vec3 position)
{

    o->entities[o->current_count].aabb.position = position;
    o->entities[o->current_count].grounded = false;
    o->lifes[o->current_count] = 1.0;


    o->current_count++;

    if (o->num_used < (uint)o->entity_count)
    {
        o->num_used++;
    }

    if (o->current_count == o->entity_count)
    {
        o->current_count = 0;
    }
}
