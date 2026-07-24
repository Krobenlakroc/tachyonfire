#include "th_constraintmotion.h"

#include "../fn_engine/th_level.h"
#include "th_splineutils.h"
#include "th_builtins.h"
#include "../fn_engine/th_globals.h"
/*
 * CONSTRAINTS
 * angle between any 2 legs must not be less than 35
 * leg length (dist to world) between 970 and 1250
 * A volume trace from origin to tip must only intersect the world at the tip
 *
 * One leg must not move between states (one of the legs planted in a previous state must be planted in this state)
 * Two legs must be planted on the ground in a state, the other one can be free floating
 */

/*
 * Dissuade searching the space if you make 5 steps without getting closer to B
 */

/*
 * Position change delta between states
 */
#define POSITION_STEP 50.0

#define NUM_LEGS 3

#define FOOT_LENGTH 150.0

#define LEG_MAX 1070

#define LEG_MIN 840

#define ANGLE_MIN 55.0

#define ADD_DIR_TRIES 10000

#define ANGLE_TRIES 10000


void th_deepCopyMotionState(th_MotionState* dest,th_MotionState* src)
{
    dest->center_pos = src->center_pos;


    memcpy(dest->leg_lengths,src->leg_lengths,sizeof(float)*NUM_LEGS);//times 3

    memcpy(dest->leg_grounded,src->leg_grounded,sizeof(bool)*NUM_LEGS);

    memcpy(dest->foot_directions,src->foot_directions,sizeof(fn_vec3)*NUM_LEGS);

    memcpy(dest->leg_directions,src->leg_directions,sizeof(fn_vec3)*NUM_LEGS);

}


fn_vec3 th_getContactPointMotion(th_MotionState* state,int index,fn_vec3* end_joint)
{
    fn_vec3 contact_b = fn_addVec3(state->center_pos,fn_multVec3s(state->leg_directions[index],state->leg_lengths[index]));

    if (end_joint != NULL)
    {
        *end_joint = contact_b;
    }
    fn_vec3 contact = fn_addVec3(contact_b,fn_multVec3s(state->foot_directions[index],FOOT_LENGTH));
    return contact;
}


static float angleConstraint(fn_vec3 a,fn_vec3 b)
{
    float alpha = acos(fn_clamp(fn_dot(a,b),-1.0,1.0));
    return fn_degrees(alpha) - ANGLE_MIN;
}

/*
static fn_vec3 randomSphere()
{
    float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
    float z =  (float)th_random()/(float)(RAND_MAX/2.0);
    z -= 1;
    float x = cos(theta);
    float y = sin(theta);
    fn_vec3 n = fn_normalizeVec3(fn_createVec3(x,y,z));
    return n;
}*/

static fn_vec3 sphereFibonacci(int i, int N)
{
    const float PHI = 1.61803399;

    float theta = 2.0f * 3.14159f * (i / PHI);

    float z_unscaled = (float)i / (float)N;

    float z = z_unscaled*2.0 - 1.0;

    float x = cos(theta);
    float y = sin(theta);
    fn_vec3 n = fn_normalizeVec3(fn_createVec3(x,y,z));

    return n;
}

bool evaluateTrace(th_World* world,fn_vec3 a,fn_vec3 b,float min_hit_dist)
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

static fn_vec3 sampleDirConstrained(fn_vec3* dirs,int num_dirs,bool* success,int offset)
{
    *success = false;
    for (int iter = 0; iter < ANGLE_TRIES;iter++)
    {
        int fib_idx = (iter + offset) % ANGLE_TRIES;
        fn_vec3 attempt_dir = sphereFibonacci(fib_idx ,ANGLE_TRIES );

        bool pass = true;
        for (int j = 0 ; j < num_dirs;j++)
        {
            if (angleConstraint(attempt_dir,dirs[j]) < 0.001)
            {
                pass = false;
            }
        }

        if (!pass)
        {
            continue;
        }

        *success = true;
        return attempt_dir;

    }

    return fn_createVec3(1,0,0);
}


static void tryAddDir(th_World* world,fn_vec3 pos,int* resolved_directions,fn_vec3* directions,fn_vec3* foot_directions,float* leg_lengths,bool try_plant)
{

    //printf("TRY ADD DIR %i\n",*resolved_directions);
    for (int iter = 0;iter < ADD_DIR_TRIES;iter++)
    {
        //printf("ITER %i\n",iter);
        if (*resolved_directions < 3)
        {
            //printf("ITER %i %i\n",iter,*resolved_directions);

            bool good_dir = false;
            fn_vec3 attempt_dir = sampleDirConstrained(directions,*resolved_directions ,&good_dir,(float)iter * ((float)ANGLE_TRIES / (float)ADD_DIR_TRIES));

            if (!good_dir)
            {
                continue;
            }



            fn_vec3 target;
            if (try_plant)
            {
                target = fn_addVec3(pos,fn_multVec3s(attempt_dir,LEG_MAX));
            }
            else
            {
                target = fn_addVec3(pos,fn_multVec3s(attempt_dir,LEG_MIN));
            }


            fn_vec3 a_normal = fn_createVec3(0,0,0);
            bool a_hit = false;
            fn_vec3 a_pos = th_traceVolume(world,pos,target,0.5,&a_normal,&a_hit,th_getPhysicsMemory(world,0));

            if (a_hit && try_plant)
            {

                //construct origin - leg - foot triangle
                // a - b - c

                fn_vec3 b = fn_addVec3(a_pos,fn_multVec3s(a_normal,FOOT_LENGTH));

                if (fn_distance(pos,b) < LEG_MAX && fn_distance(pos,b) > LEG_MIN)
                {
                    bool usable = evaluateTrace(world,pos,b,fn_distance(pos,b) - 170.0);

                    if (usable)
                    {
                        directions[*resolved_directions] = fn_normalizeVec3(fn_subVec3(b,pos));
                        foot_directions[*resolved_directions] = fn_normalizeVec3(fn_subVec3(a_pos,b));
                        leg_lengths[*resolved_directions] = fn_distance(pos,b);
                        *resolved_directions = *resolved_directions + 1;
                    }
                }
            }

            if (!a_hit && !try_plant)
            {
                directions[*resolved_directions] = fn_normalizeVec3(fn_subVec3(target,pos));
                foot_directions[*resolved_directions] = directions[*resolved_directions];
                leg_lengths[*resolved_directions] = LEG_MIN;
                *resolved_directions = *resolved_directions + 1;
            }
        }
    }
}

static void buildState(th_Allocator* alloc,th_MotionState* out_state,fn_vec3 pos,fn_vec3* directions,fn_vec3* foot_directions,float* leg_lengths,bool* leg_grounded)
{
    out_state->center_pos = pos;
    out_state->leg_lengths = th_alloc(alloc,sizeof(float)*NUM_LEGS);
    memcpy(out_state->leg_lengths,leg_lengths,sizeof(float)*NUM_LEGS);

    out_state->leg_directions = th_alloc(alloc,sizeof(fn_vec3)*NUM_LEGS);
    memcpy(out_state->leg_directions,directions,sizeof(fn_vec3)*NUM_LEGS);

    out_state->foot_directions = th_alloc(alloc,sizeof(fn_vec3)*NUM_LEGS);
    memcpy(out_state->foot_directions,foot_directions,sizeof(fn_vec3)*NUM_LEGS);

    out_state->leg_grounded = th_alloc(alloc,sizeof(bool)*NUM_LEGS);
    memcpy(out_state->leg_grounded,leg_grounded,sizeof(bool)*NUM_LEGS);

}

void shuffleState(th_MotionState* out_state,int* indices)
{
    th_MotionState new_state;

    new_state.center_pos = out_state->center_pos;

    new_state.leg_lengths = malloc(sizeof(float)*NUM_LEGS);
    for (int i = 0 ; i < NUM_LEGS;i++)
    {
        new_state.leg_lengths[indices[i]] = out_state->leg_lengths[i];
    }
    // memcpy(out_state->leg_lengths,leg_lengths,sizeof(float)*NUM_LEGS);


    new_state.leg_directions = malloc(sizeof(fn_vec3)*NUM_LEGS);
    for (int i = 0 ; i < NUM_LEGS;i++)
    {
        new_state.leg_directions[indices[i]] = out_state->leg_directions[i];
    }


    new_state.foot_directions = malloc(sizeof(fn_vec3)*NUM_LEGS);
    for (int i = 0 ; i < NUM_LEGS;i++)
    {
        new_state.foot_directions[indices[i]] = out_state->foot_directions[i];
    }

    new_state.leg_grounded = malloc(sizeof(bool)*NUM_LEGS);
    for (int i = 0 ; i < NUM_LEGS;i++)
    {
        new_state.leg_grounded[indices[i]] = out_state->leg_grounded[i];
    }



    memcpy(out_state->leg_lengths,new_state.leg_lengths,sizeof(float)*NUM_LEGS);
    memcpy(out_state->leg_directions,new_state.leg_directions,sizeof(fn_vec3)*NUM_LEGS);
    memcpy(out_state->foot_directions,new_state.foot_directions,sizeof(fn_vec3)*NUM_LEGS);
    memcpy(out_state->leg_grounded,new_state.leg_grounded,sizeof(bool)*NUM_LEGS);


    free(new_state.leg_lengths);
    free(new_state.leg_directions);
    free(new_state.foot_directions);
    free(new_state.leg_grounded);
}

bool findState(th_Allocator* alloc,th_World* world,fn_vec3 pos,th_MotionState* out_state,int resolved,fn_vec3* dirs,fn_vec3* foot_dirs,float* leg_lens_in,int* indices )
{
    fn_vec3 foot_directions[NUM_LEGS];
    fn_vec3 directions[NUM_LEGS];
    float leg_lengths[NUM_LEGS];

    int resolved_directions = 0;

    if (resolved)
    {
        resolved_directions = resolved;
        // foot_directions[0] = foot_dir;
        // directions[0] = dir;
        // leg_lengths[0] = leg_length;

        memcpy(foot_directions,foot_dirs,sizeof(fn_vec3)*resolved);
        memcpy(directions,dirs,sizeof(fn_vec3)*resolved);
        memcpy(leg_lengths,leg_lens_in,sizeof(float)*resolved);
    }




    //try and plant all 3 feet on the ground
    tryAddDir(world,pos,&resolved_directions,directions,foot_directions,leg_lengths,true);

    if (resolved_directions == 3)
    {
        bool grounded[NUM_LEGS];
        grounded[0] = true;
        grounded[1] = true;
        grounded[2] = true;
        buildState(alloc,out_state,pos,directions,foot_directions,leg_lengths,grounded);

        if (resolved)
        {
            shuffleState(out_state,indices);
        }

        return true;
    }

    if (resolved_directions <= 1)
    {
        return false;
    }

    //if we can plant 2, try and fit the third one in somewhere
    tryAddDir(world,pos,&resolved_directions,directions,foot_directions,leg_lengths,false);

    if (resolved_directions == 2)
    {
        bool grounded[NUM_LEGS];
        grounded[0] = true;
        grounded[1] = true;
        grounded[2] = false;
        buildState(alloc,out_state,pos,directions,foot_directions,leg_lengths,grounded);

        if (resolved)
        {
            shuffleState(out_state,indices);
        }
        return true;
    }


    return false;

}


static fn_vec3 getPlantPos(fn_vec3 origin,fn_vec3 dir,fn_vec3 footdir,float length,fn_vec3* above_foot)
{
    if (above_foot != NULL)
    {
        *above_foot = fn_addVec3(origin,fn_multVec3s(dir,length));
    }
    return fn_addVec3(fn_addVec3(origin,fn_multVec3s(dir,length)),fn_multVec3s(footdir,FOOT_LENGTH) );
}

static void fillIndices(int* indices,int origin)
{
    int next = 0;

    for (int i = 0; i < 3; i++)
    {
        if (i < origin)
            continue; // already filled in

        indices[i] = -1;
    }

    for (int i = 0; i < 3; i++)
    {
        if (i < origin)
        {
            continue; // already filled in
        }


        // find the next unused index
        while (next == indices[0] || next == indices[1] || next == indices[2])
            next++;

        indices[i] = next++;
    }
}


static bool reconstructState(th_MotionState* prev,fn_vec3 candidate_pos,int* use_idx,int num_idx,fn_vec3* dir_vec,fn_vec3* fdir_vec,float* len_vec)
{

    for (int idx = 0 ; idx < num_idx;idx++)
    {
        int j = use_idx[idx];
        fn_vec3 above_foot;
        fn_vec3 planted = getPlantPos(prev->center_pos,prev->leg_directions[j],prev->foot_directions[j],prev->leg_lengths[j],&above_foot);

        if (fn_distance(above_foot,candidate_pos) > LEG_MAX ||  fn_distance(above_foot,candidate_pos) < LEG_MIN)
        {
            return false;
        }

        fn_vec3 diff = fn_subVec3(above_foot,candidate_pos);

        dir_vec[idx] = fn_normalizeVec3(diff);
        fdir_vec[idx] = prev->foot_directions[j];
        len_vec[idx] = fn_length(diff);
    }

    return true;

}

th_MotionState* th_constraintPlan(th_Allocator* alloc,fn_vec3 A,fn_vec3 B,th_World* world,int* out_states,th_MotionState* pre_state,int step_idx)
{

    //Find a valid state for Position A
    //generate a candidate state, check if it satisfies the constraint
    //if unsatisfied, perturb using error
    //if error perturbation fails, try a random direction
    //if random directions fail, go back to last known good path
    //if last known good node is root, then fail and return null
    //each new state should at least be sort of closer, we arent pathfinding, we are finding good states
    th_MotionState start_state;

    if (pre_state != NULL)
    {
        start_state = *pre_state;
    }
    else
    {
        bool good_start = findState(alloc,world,A,&start_state,0,NULL,NULL,NULL,NULL );

        if (!good_start)
        {
            *out_states = 0;
            return NULL;
        }
    }


    //printf("START STATE\n");


    th_MotionState* states = malloc(sizeof(th_MotionState)*1);
    int num_states = 1;

    states[0] = start_state;

    fn_vec3 current_pos = A;


    while(fn_distance(current_pos,B) > 1.0*POSITION_STEP)
    {


        float dist = fn_distance(current_pos,B);
        float step = fn_min(dist,POSITION_STEP);

        fn_vec3 candidate_pos = fn_addVec3(current_pos,fn_multVec3s(fn_normalizeVec3(fn_subVec3(B,current_pos)),step));


        th_MotionState next_state;
        bool found_new_state = false;


        int grounded_indices[NUM_LEGS];
        int num_pre_grounded = 0;
        for (int j = 0 ; j < NUM_LEGS;j++)
        {
            if (states[num_states - 1].leg_grounded[j])
            {
                grounded_indices[num_pre_grounded] = j;
                num_pre_grounded++;
            }

        }


        if (num_pre_grounded == 3)
        {


            int use_idx[NUM_LEGS];
            use_idx[0] = 0;
            use_idx[1] = 1;
            use_idx[2] = 2;
            fn_vec3 dir_vec[3];
            fn_vec3 fdir_vec[3];
            float len_vec[3];
            bool success_recon = reconstructState(&states[num_states - 1],candidate_pos,use_idx,3,dir_vec,fdir_vec,len_vec);
            if (success_recon)
            {
                int indices[NUM_LEGS];
                indices[0] = 0;
                indices[1] = 1;
                indices[2] = 2;


                bool good_state = findState(alloc,world,candidate_pos,&next_state,3,dir_vec,fdir_vec,len_vec,indices);

                if (good_state)
                {
                    //printf("3 Fixed\n");
                    states = realloc(states,sizeof(th_MotionState)*(num_states + 1));
                    states[num_states] = next_state;
                    num_states++;

                    found_new_state = true;
                }
            }



        }


        if (!found_new_state && num_pre_grounded == 3)
        {
            int use_idx[NUM_LEGS];
            use_idx[0] = 0;
            use_idx[1] = 1;
            fn_vec3 dir_vec[2];
            fn_vec3 fdir_vec[2];
            float len_vec[2];
            bool success_recon = reconstructState(&states[num_states - 1],candidate_pos,use_idx,2,dir_vec,fdir_vec,len_vec);
            if (success_recon)
            {
                int indices[NUM_LEGS];
                indices[0] = 0;
                indices[1] = 1;
                fillIndices(indices,2);

                bool good_state = findState(alloc,world,candidate_pos,&next_state,2,dir_vec,fdir_vec,len_vec,indices);

                if (good_state)
                {
                    //printf("2 Fixed a\n");
                    states = realloc(states,sizeof(th_MotionState)*(num_states + 1));
                    states[num_states] = next_state;
                    num_states++;

                    found_new_state = true;
                }
            }
        }


        if (!found_new_state && num_pre_grounded == 3)
        {
            int use_idx[NUM_LEGS];
            use_idx[0] = 1;
            use_idx[1] = 2;
            fn_vec3 dir_vec[2];
            fn_vec3 fdir_vec[2];
            float len_vec[2];
            bool success_recon = reconstructState(&states[num_states - 1],candidate_pos,use_idx,2,dir_vec,fdir_vec,len_vec);
            if (success_recon)
            {
                int indices[NUM_LEGS];
                indices[0] = 1;
                indices[1] = 2;
                fillIndices(indices,2);

                bool good_state = findState(alloc,world,candidate_pos,&next_state,2,dir_vec,fdir_vec,len_vec,indices);

                if (good_state)
                {
                    //printf("2 Fixed b\n");
                    states = realloc(states,sizeof(th_MotionState)*(num_states + 1));
                    states[num_states] = next_state;
                    num_states++;

                    found_new_state = true;
                }
            }
        }

        if (!found_new_state && num_pre_grounded == 3)
        {
            int use_idx[NUM_LEGS];
            use_idx[0] = 0;
            use_idx[1] = 2;
            fn_vec3 dir_vec[2];
            fn_vec3 fdir_vec[2];
            float len_vec[2];
            bool success_recon = reconstructState(&states[num_states - 1],candidate_pos,use_idx,2,dir_vec,fdir_vec,len_vec);
            if (success_recon)
            {
                int indices[NUM_LEGS];
                indices[0] = 0;
                indices[1] = 2;
                fillIndices(indices,2);

                bool good_state = findState(alloc,world,candidate_pos,&next_state,2,dir_vec,fdir_vec,len_vec,indices);

                if (good_state)
                {
                    //printf("2 Fixed c\n");
                    states = realloc(states,sizeof(th_MotionState)*(num_states + 1));
                    states[num_states] = next_state;
                    num_states++;

                    found_new_state = true;
                }
            }
        }

        if (num_pre_grounded == 2)
        {
            //try the only pair of 2

            fn_vec3 dir_vec[2];
            fn_vec3 fdir_vec[2];
            float len_vec[2];
            bool success_recon = reconstructState(&states[num_states - 1],candidate_pos,grounded_indices,2,dir_vec,fdir_vec,len_vec);
            if (success_recon)
            {

                bool good_state = findState(alloc,world,candidate_pos,&next_state,2,dir_vec,fdir_vec,len_vec,grounded_indices);

                if (good_state)
                {
                    //printf("2 Fixed\n");
                    states = realloc(states,sizeof(th_MotionState)*(num_states + 1));
                    states[num_states] = next_state;
                    num_states++;

                    found_new_state = true;
                }
            }
        }


        if (!found_new_state)
        {
            //try every possible pair of 1
            for (int j = 0 ; j < NUM_LEGS;j++)
            {
                if (states[num_states - 1].leg_grounded[j])
                {
                    int use_idx = j;
                    fn_vec3 dir_vec,fdir_vec;
                    float len_vec;
                    bool success_recon = reconstructState(&states[num_states - 1],candidate_pos,&use_idx,1,&dir_vec,&fdir_vec,&len_vec);
                    if (!success_recon)
                    {
                        continue;
                    }

                    int indices[NUM_LEGS];
                    indices[0] = j;
                    fillIndices(indices,1);
                    //fn_normalizeVec3(diff),states[num_states - 1].foot_directions[j],fn_length(diff)

                    bool good_state = findState(alloc,world,candidate_pos,&next_state,1,&dir_vec,&fdir_vec,&len_vec,indices);

                    if (good_state)
                    {
                        //printf("1 Fixed\n");
                        //test for swing
                        float swing_noswap = fn_angle(next_state.leg_directions[indices[1]],states[num_states - 1].leg_directions[indices[1]]) + fn_angle(next_state.leg_directions[indices[2]],states[num_states - 1].leg_directions[indices[2]]);

                        float swing_swap = fn_angle(next_state.leg_directions[indices[1]],states[num_states - 1].leg_directions[indices[2]]) + fn_angle(next_state.leg_directions[indices[2]],states[num_states - 1].leg_directions[indices[1]]);

                        if (swing_swap < swing_noswap)
                        {


                            float leg_lengths_temp = next_state.leg_lengths[indices[1]];
                            bool leg_grounded_temp = next_state.leg_grounded[indices[1]];

                            fn_vec3 foot_directions_temp = next_state.foot_directions[indices[1]];

                            fn_vec3 leg_directions_temp = next_state.leg_directions[indices[1]];


                            next_state.leg_lengths[indices[1]] = next_state.leg_lengths[indices[2]];
                            next_state.leg_grounded[indices[1]] = next_state.leg_grounded[indices[2]];
                            next_state.foot_directions[indices[1]] = next_state.foot_directions[indices[2]];
                            next_state.leg_directions[indices[1]] = next_state.leg_directions[indices[2]];


                            next_state.leg_lengths[indices[2]] = leg_lengths_temp;
                            next_state.leg_grounded[indices[2]] = leg_grounded_temp;
                            next_state.foot_directions[indices[2]] = foot_directions_temp;
                            next_state.leg_directions[indices[2]] = leg_directions_temp;
                        }


                        states = realloc(states,sizeof(th_MotionState)*(num_states + 1));
                        states[num_states] = next_state;
                        num_states++;

                        found_new_state = true;
                        break;
                    }
                }


            }
        }



        if (!found_new_state)
        {
            // states = th_arenaManage(alloc,states,sizeof(th_MotionState)*num_states);
            // *out_states = num_states;
            printf("Failed Plan at step %i, iter %i\nLast position: %f %f %f\n",step_idx,num_states,current_pos.x,current_pos.y,current_pos.z);
            free(states);
            *out_states = 0;
            return NULL;

            //return states;

        }
        else
        {
            current_pos = states[num_states - 1].center_pos;
        }

        //least travel from old state to new state

        //printf("%f\n",fn_distance(current_pos,B));

        //printf("%f %f %f\n",states[num_states - 1].leg_lengths[0],states[num_states - 1].leg_lengths[1],states[num_states - 1].leg_lengths[2]);

        //printf("%i %i %i\n",states[num_states - 1].leg_grounded[0],states[num_states - 1].leg_grounded[1],states[num_states - 1].leg_grounded[2]);

    }


    states = th_arenaManage(alloc,states,sizeof(th_MotionState)*num_states);
    *out_states = num_states;

    return states;




}


th_MotionState* th_constraintPlanCourse(th_Allocator* alloc,fn_vec3* course,int num_course,th_World* world,int* out_states)
{

    th_Allocator local_alloc;
    th_createAllocator(&local_alloc);


    th_MotionState* ret = NULL;
    int num_ret = 0;
    for (int i = 1 ; i < num_course;i++)
    {
        th_MotionState* pre_state = NULL;
        if (i >= 2)
        {
            pre_state = &ret[num_ret - 1];
        }
        int inter_states = 0;
        th_MotionState* intermediate = th_constraintPlan(&local_alloc,course[i - 1],course[i],world,&inter_states,pre_state,i);

        if (intermediate != NULL)
        {
            if (pre_state == NULL)
            {
                ret = realloc(ret,sizeof(th_MotionState)*(num_ret + inter_states));
            }
            else if (inter_states >= 2)
            {
                ret = realloc(ret,sizeof(th_MotionState)*(num_ret + inter_states - 1 ));
            }

            int j_start = 0;
            if (pre_state != NULL)
            {
                j_start = 1;
            }

            for (int j = j_start ; j < inter_states;j++)
            {
                float* leg_lengths_global = th_alloc(alloc,sizeof(float)*NUM_LEGS);
                memcpy(leg_lengths_global,intermediate[j].leg_lengths,sizeof(float)*NUM_LEGS);
                intermediate[j].leg_lengths = leg_lengths_global;

                bool* leg_grounded_global = th_alloc(alloc,sizeof(bool)*NUM_LEGS);
                memcpy(leg_grounded_global,intermediate[j].leg_grounded,sizeof(bool)*NUM_LEGS);
                intermediate[j].leg_grounded = leg_grounded_global;

                fn_vec3* foot_directions_global = th_alloc(alloc,sizeof(fn_vec3)*NUM_LEGS);
                memcpy(foot_directions_global,intermediate[j].foot_directions,sizeof(fn_vec3)*NUM_LEGS);
                intermediate[j].foot_directions = foot_directions_global;

                fn_vec3* leg_directions_global = th_alloc(alloc,sizeof(fn_vec3)*NUM_LEGS);
                memcpy(leg_directions_global,intermediate[j].leg_directions,sizeof(fn_vec3)*NUM_LEGS);
                intermediate[j].leg_directions = leg_directions_global;
            }

            if (pre_state == NULL)
            {
                memcpy(&ret[num_ret],intermediate,sizeof(th_MotionState)*inter_states);
                num_ret = num_ret + inter_states;
            }
            else if (inter_states >= 2)
            {
                memcpy(&ret[num_ret],&intermediate[1],sizeof(th_MotionState)*(inter_states - 1));
                num_ret = num_ret + (inter_states - 1);
            }

        }
    }



    th_free(&local_alloc);


    ret = th_arenaManage(alloc,ret,sizeof(th_MotionState)*num_ret);
    *out_states = num_ret;
    return ret;
}
