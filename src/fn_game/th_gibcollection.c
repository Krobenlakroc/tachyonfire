#include "th_gibcollection.h"
#include "th_builtins.h"

static const int GIB_LIFE = 1.0;
void th_gibCreateInstances(th_Allocator* alloc,th_GibCollection* gib,int num_instances)
{
    gib->instances = th_alloc(alloc,sizeof(th_GibInstance)*num_instances);
    gib->instance_count = num_instances;
    pthread_mutex_init(&gib->giblock, NULL);
}

void th_gibInit(th_Allocator* alloc,th_GibCollection* gib,int count,th_LevelState* levelstate)
{
    for (int i = 0 ; i < gib->instance_count;i++)
    {
        th_GibInstance* instance = &gib->instances[i];
        instance->entity_count = count;
        instance->levelstate = levelstate;
        instance->num_used = 0;
        instance->current_count = 0;
        instance->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);

        instance->positions = th_alloc(alloc,sizeof(fn_vec3)*count);
        instance->velocity = th_alloc(alloc,sizeof(fn_vec3)*count);
        instance->originals = th_alloc(alloc,sizeof(fn_mat4)*count);
        instance->lifes = th_alloc(alloc,sizeof(float)*count);
        for (int j = 0 ; j < count;j++)
        {
            instance->transforms[j] = fn_makescale(fn_createVec3s(0));

            instance->positions[j] = fn_createVec3s(0);
            instance->velocity[j] = fn_createVec3s(0);
            instance->originals[j] = fn_identityMat4();
            instance->lifes[j] = 0;
        }
    }
}

void th_gibUpdate(th_GibCollection* gib,float dt)
{
    for (int i = 0 ; i < gib->instance_count;i++)
    {
        th_GibInstance* instance = &gib->instances[i];
        for (uint j = 0; j <instance->num_used;j++)
        {
            if (instance->lifes[j] < 0)
            {
                instance->transforms[j] = fn_makescale(fn_createVec3s(0));
                continue;
            }
            instance->positions[j] = fn_addVec3(instance->positions[j],fn_multVec3s(instance->velocity[j],dt));
            instance->velocity[j] = fn_addVec3(instance->velocity[j],fn_createVec3(0,0.001*dt,0));

            float s = instance->lifes[j]/GIB_LIFE;
            s = s* 4.45*1.25;
            fn_mat4 tx = fn_translatescale(instance->positions[j],fn_createVec3s(s));
            instance->transforms[j] = fn_multMat4(instance->originals[j],tx);
            instance->lifes[j] -= dt*0.001;
        }
    }
}

void th_gibSpawn(th_GibCollection* gib,fn_vec3 position,fn_mat4 original_orient)
{
    pthread_mutex_lock(&gib->giblock);
    for (int i = 0 ; i < gib->instance_count;i++)
    {
        th_GibInstance* instance = &gib->instances[i];
        instance->positions[instance->current_count] = position;
        instance->originals[instance->current_count] = original_orient;
        instance->velocity[instance->current_count] = fn_multVec3s(th_sampleRandomSphere(),0.45);

        instance->lifes[instance->current_count] = GIB_LIFE;

        instance->current_count++;

        if (instance->num_used < (uint)instance->entity_count)
        {
            instance->num_used++;
        }

        if (instance->current_count == instance->entity_count)
        {
            instance->current_count = 0;
        }
    }
    pthread_mutex_unlock(&gib->giblock);
}
