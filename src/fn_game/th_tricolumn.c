#include "th_tricolumn.h"
#include "../fn_engine/th_level.h"
#include "th_splineutils.h"
#include "th_builtins.h"
#include "../fn_engine/th_globals.h"
#include "../fn_engine/th_occlusion.h"
#include "../fn_engine/th_hitmarker.h"

static const float eye_offset_amt = -100;

int th_triColumnDamageCallback(void* ep,void* pep,float dt,void* world)
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

// Build orthonormal basis from central direction
void build_basis(fn_vec3 n, fn_vec3* u, fn_vec3* v) {
    fn_vec3 up = fn_createVec3(0.0, 0.0, 1.0);
    if (fabs(n.z) > 0.9) { // If near Z-axis, pick X as helper
        up.x = 1.0; up.y = 0.0; up.z = 0.0;
    }
    *u = fn_cross(up, n);
    *u = fn_normalizeVec3(*u);
    *v = fn_cross(n, *u);
    *v = fn_normalizeVec3(*v);
}

// Generate one random direction within cone
fn_vec3 sample_direction(fn_vec3 cental, float max_angle) {
    // uniform azimuth
    fn_vec3 out;
    float phi = 2.0 * 3.14159265 * ((float) th_random() / RAND_MAX);
    // correct polar distribution
    float cos_max = cos(max_angle);
    float cos_theta = (1.0 - cos_max) * ((float) th_random() / RAND_MAX) + cos_max;
    float sin_theta = sqrt(1.0 - cos_theta*cos_theta);

    fn_vec3 local = fn_createVec3(
        cos(phi) * sin_theta,
                                  sin(phi) * sin_theta,
                                  cos_theta);

    // Build basis
    fn_vec3 n = cental;
    n = fn_normalizeVec3(n);
    fn_vec3 u, v;
    build_basis(n, &u, &v);

    // Transform from local cone coords to world coords
    out.x = local.x*u.x + local.y*v.x + local.z*n.x;
    out.y = local.x*u.y + local.y*v.y + local.z*n.y;
    out.z = local.x*u.z + local.y*v.z + local.z*n.z;
    out = fn_normalizeVec3(out);
    return out;
}

static fn_quat update_orient(fn_vec3* from_vec,fn_vec3 target_vec,float ang_spd_deg,float dt,fn_vec3* old_forwards,fn_vec3* old_rights,fn_vec3* old_ups)
{
    fn_vec3 from_vector = *from_vec;
    fn_vec3 target_vector = target_vec;
    // fn_printVec3(from_vector);
    // fn_printVec3(target_vector);

    fn_vec3 axis = fn_normalizeVec3(fn_cross(from_vector,target_vector));
    //fn_printVec3(axis);

    float angle_to_rot = -acosf(fn_clamp(fn_dot(from_vector,target_vector),-1,1));
    float delta_angle = -dt*fn_radians(ang_spd_deg);
    bool snap = false;
    //printf("%f %f\n",fabs(delta_angle) , fabs(angle_to_rot));
    if (fabs(delta_angle) > fabs(angle_to_rot))
    {
        delta_angle = angle_to_rot;
        snap = true;
    }


    fn_quat rot = fn_makeQuaternion(delta_angle, axis);
    fn_vec3 dir_rotated = fn_normalizeVec3( fn_rotatePointQuat( from_vector,rot) );
    if (snap)
    {
        dir_rotated = target_vector;
    }


    *from_vec = dir_rotated;


    fn_vec3 bone_to = dir_rotated;

    //c->data[i].old_ups[j] = fn_createVec3(0,1,0);
    fn_mat4 cam = th_6dofCamera(old_forwards,old_rights,old_ups,fn_multVec3(bone_to,fn_createVec3(1,1,1)),fn_createVec3(0,0,0));

    fn_mat4 m = fn_inverse(cam);

    return fn_mat4toquat(m);
}


static void th_triColumnArmMatrices(th_TricolumnGroup* c,int i)
{
    for (int j = 0 ; j < 3;j++)
    {
        fn_mat4 tr_back = fn_maketranslate(fn_multVec3s(c->trapdoor_hinge,-1.0));

        //(sinf(th_time()*0.001*4) + 1.0)
        fn_mat4 rot_trap = fn_makerotate(fn_radians(75.0)*c->data[i].hinge_interp[j],fn_createVec3(-1,0,0));

        fn_mat4 tr_forward = fn_maketranslate(fn_multVec3s(c->trapdoor_hinge,1.0));

        fn_mat4 trapdoor = fn_multMat4(fn_multMat4(tr_back,rot_trap),tr_forward);

        trapdoor = fn_multMat4(trapdoor,c->transforms_cols[i*3 + j]);

        c->transforms_trapdoors[i*3 + j ] = trapdoor;//c->transforms_cols[i*3 + j];


        //30 in 280 out
        //negative is out positive is in (0, -28)
        float extension = c->extension[i*3 + j];
        fn_vec3 foot_dir = c->foot_dir[i*3 + j];


        fn_mat4 extend = fn_maketranslate(fn_createVec3(0,0,extension));

        c->transforms_leg[i*3 + j ] = fn_multMat4(extend,c->transforms_cols[i*3 + j]);


        tr_back = fn_maketranslate(fn_multVec3s(c->pivot_center,-1.0));
        tr_forward = fn_maketranslate(fn_multVec3s(c->pivot_center,1.0));

        // fn_vec3 old_up = fn_createVec3(0,1,0);
        fn_mat4 rot_foot = fn_inverse(th_6dofCamera(NULL,NULL,&c->old_up_foot[i*3 + j],foot_dir,fn_createVec3(0,0,0)));

        fn_mat4 foot = fn_multMat4(fn_multMat4(tr_back,rot_foot),tr_forward);


        foot = fn_multMat4(foot,fn_multMat4(extend,c->transforms_cols[i*3 + j]));

        c->transforms_foot[i*3 + j ] = foot;//fn_multMat4(extend,c->transforms_cols[i*3 + j]);
    }

}

static void th_triColumnPoseBaseplate(th_TricolumnGroup* c,int i,float dt,bool aggro,bool flip)
{
    fn_vec3 orient_trgt = fn_normalizeVec3(fn_subVec3( c->levelstate->player_e.aabb.position,c->data[i].position));
    if (flip)
    {
        orient_trgt = fn_multVec3s(orient_trgt,-1.0);
    }

    float speed = flip ? 0.2 : 0.02;

    c->data[i].orientation = update_orient(&c->data[i].direction_vector,orient_trgt,speed,dt,&c->data[i].old_forwards_orient,&c->data[i].old_rights_orient,&c->data[i].old_ups_orient);
    fn_quat orient = c->data[i].orientation;
    c->transforms[i] = fn_translaterotatescaleq(c->data[i].position,orient,fn_createVec3s(10));

    //set eye pos
    if (th_time() - c->data[i].aggrotimer < 3000 && aggro)
    {
        if (th_time() - c->data[i].orient_eye_timer > 300)
        {
            fn_vec3 playerpos = c->levelstate->player_e.aabb.position;
            playerpos = fn_normalizeVec3(fn_subVec3(playerpos,c->data[i].position));


            fn_vec3 x = fn_rotatePointQuat(fn_createVec3(1,0,0),orient);
            fn_vec3 y = fn_rotatePointQuat(fn_createVec3(0,1,0),orient);
            fn_vec3 z = fn_rotatePointQuat(fn_createVec3(0,0,1),orient);

            fn_vec3 result = fn_normalizeVec3(fn_createVec3(fn_dot(x,playerpos),fn_dot(y,playerpos),fn_dot(z,playerpos)));
            if (fn_dot(result,fn_createVec3(0,0,-1)) > 0.2 )
            {
                c->data[i].orient_trgt_eyedir = result;
                c->data[i].orient_eye_timer = th_time();
            }

        }

        c->data[i].orientation_eyedir = update_orient(&c->data[i].direction_vector_eyedir,c->data[i].orient_trgt_eyedir,0.045,dt,NULL,NULL,&c->data[i].old_ups_eyedir);

    }
    else
    {
        if (th_time() - c->data[i].orient_eye_timer > 450)
        {
            c->data[i].orient_trgt_eyedir = sample_direction(fn_createVec3(0,0,-1),fn_radians(90));
            c->data[i].orient_eye_timer = th_time();
        }

        c->data[i].orientation_eyedir = update_orient(&c->data[i].direction_vector_eyedir,c->data[i].orient_trgt_eyedir,0.2,dt,NULL,NULL,&c->data[i].old_ups_eyedir);
    }



    fn_quat eye_orr = fn_multquat(c->data[i].orientation_eyedir,orient);


    fn_vec3 offset_eye = fn_rotatePointQuat(fn_createVec3(0,0,eye_offset_amt),eye_orr);
    c->transforms_eye[i] = fn_translaterotatescaleq(fn_addVec3(c->data[i].position,offset_eye),eye_orr,fn_createVec3s(1.2));
}

static void th_triColumnPoseGems(th_TricolumnGroup* c,int i,float dt)
{
    fn_vec3 gem_jitter = fn_multVec3s(c->data[i].gem_jitter_x[0] ,sin(c->data[i].gem_jitter_t[0]*0.025)*c->data[i].gem_jitter_t[0]*0.21 );
    c->data[i].gem_jitter_t[0] -= dt;
    if (c->data[i].gem_jitter_t[0] <= 0.0)
    {
        c->data[i].gem_jitter_t[0] = 0.0;
    }

    if (c->data[i].hasgem[0])
    {
        c->transforms_gems[i*7 + 0] = fn_translaterotatescaleq(fn_addVec3(gem_jitter,c->data[i].position),c->data[i].orientation,fn_createVec3s(25));
    }
    else
    {
        c->transforms_gems[i*7 + 0] = fn_makescale(fn_createVec3s(0));
    }


    c->entities_gems[i*7 + 0].aabb.position = c->data[i].position;
    c->entities_gems[i*7 + 0].aabb.hwidth = fn_createVec3(70,70,70);
    c->entities_gems[i*7 + 0].velocity = fn_multVec3s(fn_createVec3s(0),1.0/(dt));

    fn_vec3 old_gem_pos[6];
    for (int j = 1 ;j < 7;j++)
    {
        old_gem_pos[j - 1] =  c->entities_gems[i*7 + j].aabb.position;
    }


    for (int j = 1 ; j < 7;j++)
    {
        fn_vec3 gem_point =  c->data[i].col_points[j%3];

        if ( j % 2 == 0)
        {
            gem_point  = fn_addVec3(gem_point,fn_rotatePointQuat(fn_createVec3(100,0,0),c->data[i].col_orientations[j%3]));
        }
        else
        {
            gem_point  = fn_addVec3(gem_point,fn_rotatePointQuat(fn_createVec3(-100,0,0),c->data[i].col_orientations[j%3]));
        }

        fn_vec3 gem_jitter = fn_multVec3s(c->data[i].gem_jitter_x[j] ,sin(c->data[i].gem_jitter_t[j]*0.025)*c->data[i].gem_jitter_t[j]*0.21 );
        c->data[i].gem_jitter_t[j] -= dt;
        if (c->data[i].gem_jitter_t[j] <= 0.0)
        {
            c->data[i].gem_jitter_t[j] = 0.0;
        }

        if (c->data[i].hasgem[j])
        {
            c->transforms_gems[i*7 + j] = fn_translaterotatescaleq(fn_addVec3(gem_jitter,gem_point),c->data[i].col_orientations[j%3],fn_createVec3s(20));
        }
        else
        {
            c->transforms_gems[i*7 + j] = fn_makescale(fn_createVec3s(0));
        }
        c->entities_gems[i*7 + j].aabb.position = gem_point;
        c->entities_gems[i*7 + j].aabb.hwidth = fn_createVec3(60,60,60);
        c->entities_gems[i*7 + j].velocity = fn_multVec3s(fn_subVec3(c->entities_gems[i*7 + j].aabb.position,old_gem_pos[j - 1]),1.0/(dt));
    }
}

static void th_triColumnPose(th_MotionState mstate,th_TricolumnGroup* c,int i,float dt,th_World* world)
{
    //set center pos
    c->data[i].position = mstate.center_pos;

    th_triColumnPoseBaseplate(c,i ,dt,true,false);


    //set legs

    for (int j = 0 ; j < 3 ;j ++)
    {
        //(1000.0 - mstate.leg_lengths[j])/10.0 ;
        c->extension[i*3 + j] = mstate.leg_lengths[j] - 760.0;
        c->extension[i*3 + j] = -c->extension[i*3 + j]/10.0;

        //c->extension[i*3 + j] = -31.0;


        fn_vec3 bone_to = mstate.leg_directions[j];

        //c->data[i].old_ups[j] = fn_createVec3(0,1,0);
        fn_mat4 cam = th_6dofCamera(&c->data[i].old_forwards[j],&c->data[i].old_rights[j],&c->data[i].old_ups[j],fn_multVec3(bone_to,fn_createVec3(1,1,1)),fn_createVec3(0,0,0));

        fn_mat4 m = fn_inverse(cam);

        c->data[i].col_orientations[j] = fn_mat4toquat(m);

        fn_vec3 leg_pos = fn_addVec3(mstate.center_pos,fn_multVec3s(bone_to,550));
        c->transforms_cols[i*3 + j] = fn_translaterotatescaleq(leg_pos,c->data[i].col_orientations[j],fn_createVec3s(10));

        c->data[i].col_points[j] = leg_pos;

        c->data[i].old_legs[j] = c->transforms_cols[i*3 + j];


        fn_vec3 x = fn_rotatePointQuat(fn_createVec3(1,0,0),c->data[i].col_orientations[j] );
        fn_vec3 y = fn_rotatePointQuat(fn_createVec3(0,1,0),c->data[i].col_orientations[j] );
        fn_vec3 z = fn_rotatePointQuat(fn_createVec3(0,0,1),c->data[i].col_orientations[j] );

        fn_vec3 foot_world = mstate.foot_directions[j];
        fn_vec3 foot_basis = fn_normalizeVec3(fn_createVec3(fn_dot(x,foot_world),fn_dot(y,foot_world),fn_dot(z,foot_world)));

        c->foot_dir[i*3 + j] = foot_basis;

    }




    //set gems
    th_triColumnPoseGems(c,i,dt);


    int rnd_temp = 0;
    if (th_time() - c->data[i].aggrotimer < 3000 &&  th_time() - c->data[i].aggrotimer > 200 )
    {
        fn_vec3 offset_eye = fn_rotatePointQuat(fn_createVec3(0,0,eye_offset_amt),fn_multquat(c->data[i].orientation_eyedir,c->data[i].orientation));

        fn_vec3 eyeloc = fn_addVec3(c->data[i].position,offset_eye);
        fn_vec3 laserdelt = fn_normalizeVec3(offset_eye);
        fn_vec3 laser_dir = laserdelt;
        laserdelt = fn_multVec3s(laserdelt,1500);
        laserdelt = fn_addVec3(laserdelt,eyeloc);

        fn_vec3 lasertarget = th_traceVolume(world,eyeloc,laserdelt,0.1,NULL,NULL,th_getPhysicsMemory(world,0));

        lasertarget  = fn_addVec3(lasertarget,fn_multVec3s(fn_normalizeVec3(fn_subVec3(lasertarget,eyeloc)),250));

        th_spawnLaserBeam(eyeloc,lasertarget,-1,c->levelstate->general_light_query,20,&rnd_temp,60.0,170);
        fn_vec3 playerpos = c->levelstate->player_e.aabb.position;

        if ( fn_distance2(c->data[i].position,playerpos) <= fn_distance2(c->data[i].position,lasertarget) && fn_dot(fn_normalizeVec3(fn_subVec3(playerpos,c->data[i].position)),laser_dir) > 0.995  )
        {
            if (c->data[i].lightning_hit_timer == -1.0 || th_time() > c->data[i].lightning_hit_timer )
            {
                th_Entity* player = &c->levelstate->player_e;
                // fn_vec3 d = fn_subVec3(entity->aabb.position,player->aabb.position);
                fn_vec3 av = fn_multVec3s(laser_dir,0.1f);
                player->velocity = fn_addVec3(av,player->velocity);

                th_setGameGlow(fn_createVec3(1,0,0),0.25);

                th_PlayerObject* po = c->levelstate->player;
                th_decrementPlayerHealth(po,2);
                th_decrementPlayerGem(po,2);
                c->data[i].lightning_hit_timer = th_time() + 50;
            }

        }
    }



    //set foot based off of basis

    th_triColumnArmMatrices(c,i);
}
// a->leg_directions[j]


static fn_vec3 gettangent(fn_vec3 look)
{
    fn_vec3 up_decal = fn_createVec3(0,-1,0);
    if (fn_almostequalVec3(fn_createVec3(0,-1,0),look,0.001) || fn_almostequalVec3(fn_createVec3(0,1,0),look,0.001))
    {
        up_decal = fn_createVec3(1,0,0);
    }
    return fn_normalizeVec3(fn_cross(up_decal,look));
}

static bool update_orient_direction(fn_vec3* history,fn_vec3 target_vector,float motion_angular_speed_deg,float dt)
{
    fn_vec3 from_vector = *history;//

    fn_vec3 cross = fn_cross(from_vector,target_vector);
    if (fn_length(cross) < 0.01) // vectors (anti)parallel
    {
        cross = fn_cross(from_vector, gettangent(from_vector));
    }

    fn_vec3 axis = fn_normalizeVec3(cross);

    float angle_to_rot = -acosf(fn_clamp(fn_dot(from_vector,target_vector),-1,1));
    float delta_angle = -dt*fn_radians(motion_angular_speed_deg);
    bool snap = false;
    if (fabs(delta_angle) > fabs(angle_to_rot))
    {
        delta_angle = angle_to_rot;
        snap = true;
    }

    fn_quat rot = fn_makeQuaternion(delta_angle, axis);
    fn_vec3 dir_rotated = fn_normalizeVec3( fn_rotatePointQuat( from_vector,rot) );
    if (snap)
    {
        dir_rotated = target_vector;
    }

    *history = dir_rotated;

    return snap;

}

static bool tricolumnInterpState(th_TricolumnGroup* c,int i,th_MotionState* a,th_MotionState* b,th_MotionState* out,float dt,th_TricolumnInterpData* state_machine)
{
    //position moves in finite time
    //leg angles take indeterminite time
    //float dx = 50.0;
    float dt_motion = 700.0; //how long to go from 0 to 1 aka 50.0

    float dx_dt = 1.0/22.0; //how fast to retract and extend

    float motion_angular_speed_deg = 0.027;

    if (c->fastmode[i])
    {
        dt_motion *= 0.25;

        dx_dt *= 4.0;

        motion_angular_speed_deg *= 4.0;
    }

    bool play_begin_sound = false;
    if (state_machine->alpha == 0.0)
    {
        play_begin_sound = true;
        for (int j = 0 ; j < 3;j++)
        {



            state_machine->lengths[j] = a->leg_lengths[j];
            state_machine->states[j] = TRICOL_RETRACT;

            state_machine->done_rotate[j][0] = false;
            state_machine->done_rotate[j][1] = false;
        }
    }

    state_machine->alpha = state_machine->alpha + (1.0/dt_motion)*dt;

    state_machine->alpha = fn_clamp(state_machine->alpha,0.0,1.0);

    fn_vec3 center_interped = fn_lerpVec3(a->center_pos,b->center_pos,state_machine->alpha);
    out->center_pos = center_interped;
    //go from a to b in indeterminite time

    //interp position according to dx/dt constant


    //interp legs according to whether the old position is the same or not

    for (int j = 0 ; j < 3;j++)
    {
        fn_vec3 joint_a,joint_b;
        fn_vec3 contacta = th_getContactPointMotion(a,j,&joint_a);
        fn_vec3 contactb = th_getContactPointMotion(b,j,&joint_b);

        if (fn_distance(contacta,contactb) < 0.001 && a->leg_grounded[j] && b->leg_grounded[j] )
        {
            //dont retract, interp the center, keep foot direction, change length and dir

            out->foot_directions[j] = b->foot_directions[j];

            fn_vec3 dir_interped = fn_subVec3(joint_b,center_interped);

            out->leg_directions[j] = fn_normalizeVec3(dir_interped);
            out->leg_lengths[j] = fn_length(dir_interped);

            out->leg_grounded[j] = true;

            state_machine->states[j] = TRICOL_END;
        }
        else
        {
            //retract and re-extend

            if (play_begin_sound)
            {
                th_playSoundIfNotPlaying(&c->data[i].mech3,contacta,sound_tricol_groan3,0.6 );
            }

            if (state_machine->states[j] == TRICOL_RETRACT)
            {
                float delta_length = 840.0 - state_machine->lengths[j];
                if (delta_length >= 0.0)
                {
                    state_machine->states[j] = TRICOL_ROTATE;
                    state_machine->lengths[j] = 840.0;

                    th_playSoundIfNotPlaying(&c->data[i].mech2,joint_a,sound_tricol_groan2,0.6 );
                }
                else
                {
                    delta_length = fn_max(delta_length,-dx_dt*dt);
                    state_machine->lengths[j] = state_machine->lengths[j] + delta_length;
                }





                out->foot_directions[j] = a->foot_directions[j];
                out->leg_directions[j] = a->leg_directions[j];
                out->leg_lengths[j] = state_machine->lengths[j];
                out->leg_grounded[j] = false;

                state_machine->old_dir[j] = out->leg_directions[j];
                state_machine->old_dir_foot[j] = out->foot_directions[j];

                state_machine->done_rotate[j][0] = false;
                state_machine->done_rotate[j][1] = false;
            }
            else if (state_machine->states[j] == TRICOL_ROTATE)
            {


                state_machine->done_rotate[j][0] = state_machine->done_rotate[j][0] || update_orient_direction(&state_machine->old_dir[j],b->leg_directions[j],motion_angular_speed_deg,dt);
                state_machine->done_rotate[j][1] = state_machine->done_rotate[j][1] || update_orient_direction(&state_machine->old_dir_foot[j],b->foot_directions[j],motion_angular_speed_deg,dt);

                if (state_machine->done_rotate[j][0] && state_machine->done_rotate[j][1])
                {
                    state_machine->states[j] = TRICOL_EXTEND;

                    th_playSoundIfNotPlaying(&c->data[i].mech1,joint_b,sound_tricol_groan1,0.6 );
                }

                out->foot_directions[j] = state_machine->old_dir_foot[j];
                out->leg_directions[j] = state_machine->old_dir[j];
                out->leg_lengths[j] = state_machine->lengths[j];
                out->leg_grounded[j] = false;
            }
            else if (state_machine->states[j] == TRICOL_EXTEND)
            {
                out->leg_grounded[j] = false;
                float delta_length = b->leg_lengths[j] - state_machine->lengths[j];
                if (delta_length <= 0.0)
                {
                    //TODO END
                    //state_machine->states[j] = TRICOL_ROTATE;
                    out->leg_grounded[j] = b->leg_grounded[j];
                    state_machine->lengths[j] = b->leg_lengths[j];
                    state_machine->states[j] = TRICOL_END;
                }
                else
                {
                    delta_length = fn_min(delta_length,dx_dt*dt);
                    state_machine->lengths[j] = state_machine->lengths[j] + delta_length;
                }





                out->foot_directions[j] = b->foot_directions[j];
                out->leg_directions[j] = b->leg_directions[j];
                out->leg_lengths[j] = state_machine->lengths[j];

            }
            else if (state_machine->states[j] == TRICOL_END)
            {
                out->foot_directions[j] = b->foot_directions[j];
                out->leg_directions[j] = b->leg_directions[j];
                out->leg_lengths[j] = b->leg_lengths[j];
                out->leg_grounded[j] = b->leg_grounded[j];
            }
        }
    }


    bool all_ended = state_machine->alpha == 1.0;
    for (int j = 0 ; j < 3;j++)
    {
        all_ended = all_ended && (state_machine->states[j] == TRICOL_END);
    }

    return all_ended;


}

static const float offset_v = 1000;
static const float fadeout_decay_rate = 0.002;
static const float fadeout_init = 3.0;
static const int sample_count = 35;

static const float SPAWN_INTERVAL = 12000;
static const float SPAWN_DURATION = 5000;
static const float SPAWN_INTERVAL_INITIAL = 7000;


static bool checkSpawn(th_World* world,fn_vec3 center,fn_vec3 spawnpos)
{
    fn_vec3 n = fn_createVec3(0,0,0);
    bool hit = false;
    fn_vec3 pos = th_traceVolume(world,center,spawnpos,45,&n,&hit,th_getPhysicsMemory(world,0));
    return !hit;
}


void th_tricolumnSetCourse(th_TricolumnGroup* c,int i,fn_vec3* target_points,int target_count)
{
    c->data[i].course = target_points;
    c->data[i].course_count = target_count;

    th_World* world = c->levelstate->world;

    printf("motion states %i!\n",i);

    c->data[i].motion_states = th_constraintPlanCourse(c->alloc,c->data[i].course,c->data[i].course_count,world,&c->data[i].num_motion_states);

    if (c->data[i].num_motion_states == 0)
    {
        printf("FAILED TO GEN MOTION STATES!\n");
    }
}

void th_tricolumnInitialize(th_Allocator* alloc,th_TricolumnGroup* c,int count,fn_vec3* positions,float* times,bool* fastmode,th_LevelState* levelstate)
{
    c->alloc = alloc;
    c->entities_gems = th_alloc(alloc,sizeof(th_Entity)*count*7);

    c->levelstate = levelstate;
    c->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
    c->transforms_eye = th_alloc(alloc,sizeof(fn_mat4)*count);
    c->transforms_gems = th_alloc(alloc,sizeof(fn_mat4)*count*7);
    c->transforms_cols = th_alloc(alloc,sizeof(fn_mat4)*count*3);

    c->transforms_trapdoors = th_alloc(alloc,sizeof(fn_mat4)*count*3);

    c->transforms_leg = th_alloc(alloc,sizeof(fn_mat4)*count*3);
    c->transforms_foot = th_alloc(alloc,sizeof(fn_mat4)*count*3);

    c->data = th_alloc(alloc,sizeof(th_TricolumnData)*count);
    c->alloc = alloc;
    c->count = count;
    c->gem_count = count*7;
    c->col_count = count*3;


    c->extension = th_alloc(alloc,sizeof(float)*count*3);
    c->foot_dir = th_alloc(alloc,sizeof(fn_vec3)*count*3);
    c->old_up_foot = th_alloc(alloc,sizeof(fn_vec3)*count*3);

    c->fastmode = fastmode;

    for (int i = 0 ; i < count;i++)
    {
        c->data[i].spawn_heartbeat_index = 0;
        c->data[i].interp_state_machine.alpha = 0.0;
        c->data[i].interp_state_machine.frame = 0;
        c->data[i].interp_state_machine.reverse = false;

        c->data[i].motion_states = NULL;
        c->data[i].num_motion_states = 0;


        c->data[i].motion_state_interp.leg_lengths = th_alloc(alloc,sizeof(float)*3);
        c->data[i].motion_state_interp.leg_grounded = th_alloc(alloc,sizeof(bool)*3);
        c->data[i].motion_state_interp.foot_directions = th_alloc(alloc,sizeof(fn_vec3)*3);
        c->data[i].motion_state_interp.leg_directions = th_alloc(alloc,sizeof(fn_vec3)*3);

        c->data[i].fadeout_spawn = 1.0;
        c->data[i].spawn_when = times[i];
        c->data[i].spawn_init = false;
        c->data[i].spawn_finished = false;
        c->data[i].course = NULL;
        c->data[i].course_count = 0;
        c->data[i].course_progress = 0;

        c->data[i].baseplate_gib = TH_DEFAULT_ENTITY;
        c->data[i].baseplate_gib.aabb.position = positions[i];
        c->data[i].baseplate_gib.aabb.hwidth = fn_createVec3(70,70,70);
        c->data[i].baseplate_gib.velocity = fn_createVec3s(0);
        c->data[i].baseplate_gib.grounded = false;
        c->data[i].baseplate_gib.aabb.mode = BOX;
        c->data[i].baseplate_gib.alive = true;
        c->data[i].baseplate_gib.collided = false;
        c->data[i].baseplate_gib.impact = false;
        c->data[i].baseplate_gib.delete_me = false;
        c->data[i].baseplate_gib.impact_count = 0;

        c->data[i].fadeout_body = fadeout_init;
        c->data[i].old_body = fn_identityMat4();
        c->data[i].old_eye = fn_identityMat4();
        c->data[i].lightning_hit_timer = -1;




        c->data[i].state = TRICOL_FINDGROUND;
        c->data[i].set_up = false;
        c->data[i].position = positions[i];
        c->data[i].orientation = fn_createVec4(1,0,0,0);
        c->data[i].direction_vector = fn_createVec3(0,0,1);
        c->data[i].old_forwards_orient = fn_createVec3(0,0,1);
        c->data[i].old_rights_orient = fn_createVec3(1,0,0);
        c->data[i].old_ups_orient =  fn_createVec3(0,1,0);

        c->data[i].orientation_eyedir = fn_createVec4(1,0,0,0);
        c->data[i].direction_vector_eyedir = fn_createVec3(0,0,-1);
        c->data[i].old_ups_eyedir = fn_createVec3(0,1,0);
        c->data[i].orient_trgt_eyedir = fn_createVec3(0,0,-1);
        c->data[i].orient_trgt_basedir = fn_createVec3(0,0,-1);
        c->data[i].orient_eye_timer = 0;
        c->data[i].spawn_heartbeat = 0;
        c->data[i].spawn_flip_flop = 0;

        c->data[i].aggrotimer = -10000;
        c->data[i].spawn_sound = NULL;

        c->data[i].mech1 = NULL;
        c->data[i].mech2 = NULL;
        c->data[i].mech3 = NULL;
        c->data[i].lazer = NULL;

        for (int j = 0 ; j < 3;j++)
        {
            c->data[i].hinge_interp[j] = 0.0;


            c->extension[i*3 + j] = 0;
            c->foot_dir[i*3 + j] = fn_createVec3(0,0,-1);
            c->old_up_foot[i*3 + j] = fn_createVec3(0,1,0);


            c->data[i].fadeout_legs[j] = fadeout_init;
            c->data[i].old_legs[j] = fn_identityMat4();
            for (size_t l = 0; l < 2; l++) {
                c->data[i].leg_gib[j][l] = TH_DEFAULT_ENTITY;
                c->data[i].leg_gib[j][l].aabb.position = positions[i];
                c->data[i].leg_gib[j][l].aabb.hwidth = fn_createVec3(75,75,75);
                c->data[i].leg_gib[j][l].velocity = fn_createVec3s(0);
                c->data[i].leg_gib[j][l].grounded = false;
                c->data[i].leg_gib[j][l].aabb.mode = BOX;
                c->data[i].leg_gib[j][l].alive = true;
                c->data[i].leg_gib[j][l].collided = false;
                c->data[i].leg_gib[j][l].impact = false;
                c->data[i].leg_gib[j][l].delete_me = false;
                c->data[i].leg_gib[j][l].impact_count = 0;
            }



            c->data[i].fadeout_legs[j] = fadeout_init;

            c->data[i].col_orientations[j] = fn_createVec4(1,0,0,0);
            c->data[i].col_points[j] = fn_createVec3(0,0,0);
            c->data[i].old_ups[j] = fn_createVec3(1,0,0);
            c->data[i].old_rights[j] = fn_createVec3(1,0,0);
            c->data[i].old_forwards[j] = fn_createVec3(1,0,0);


        }


        c->data[i].spawntimer = th_time() + SPAWN_INTERVAL;

        fn_vec3 offset_eye = fn_createVec3(0,0,eye_offset_amt);
        c->transforms_eye[i] = fn_translaterotatescale(fn_addVec3(positions[i],offset_eye),0,fn_createVec3(1,0,0),fn_createVec3s(1.2));
        c->transforms[i] = fn_translaterotatescale(positions[i],0,fn_createVec3(1,0,0),fn_createVec3s(8));
        for (int j = 0 ; j < 7 ; j ++)
        {
            c->transforms_gems[i*7 + j] = fn_translaterotatescale(positions[i],0,fn_createVec3(1,0,0),fn_createVec3s(20));
            c->entities_gems[i*7 + j] = TH_DEFAULT_ENTITY;
            c->entities_gems[i*7 + j].aabb.position = positions[i];
            c->entities_gems[i*7 + j].aabb.hwidth = fn_createVec3(60,60,60);
            c->entities_gems[i*7 + j].velocity = fn_createVec3s(0);
            c->entities_gems[i*7 + j].grounded = false;
            c->entities_gems[i*7 + j].aabb.mode = BOX;
            c->entities_gems[i*7 + j].alive = false;
            c->entities_gems[i*7 + j].collided = false;
            c->entities_gems[i*7 + j].impact = false;
            c->entities_gems[i*7 + j].delete_me = false;
            c->entities_gems[i*7 + j].impact_count = 0;
            c->entities_gems[i*7 + j].time_of_damage = 0;
            c->entities_gems[i*7 + j].damage_callback = th_triColumnDamageCallback;

            c->data[i].impact_sounds[j] = NULL;
            c->data[i].hasgem[j] = true;
            c->data[i].gemhealth[j] = 104.0;
            c->data[i].gem_jitter_t[j] = 0.0;
            c->data[i].gem_jitter_x[j] = fn_createVec3s(0.0);


        }
        fn_vec3 offsets_col[3];
        offsets_col[0] = fn_createVec3(0,500,0);
        offsets_col[1] = fn_createVec3(100,-500,0);
        offsets_col[2] = fn_createVec3(-100,-500,0);

        for (int j = 0 ; j < 3 ; j ++)
        {
            c->transforms_cols[i*3 + j] = fn_translaterotatescale(fn_addVec3(positions[i],offsets_col[j]),0,fn_createVec3(1,0,0),fn_createVec3s(10));
        }

        th_triColumnArmMatrices(c,i);
    }


}




//find where the tricol's legs can intersec the world
static fn_vec3 tricol_traceContact(th_World* world,fn_vec3 center, fn_vec3 direction_in,float angle_max,float leg_length,int num_samples,bool* hit_one )
{
    *hit_one = false;
    for (int i = 0; i < num_samples;i++)
    {

        fn_vec3 direction = sample_direction(direction_in,angle_max);
        //fn_printVec3(direction);

        float radius = 1;
        th_Entity tracer = TH_DEFAULT_ENTITY;
        tracer.aabb.position = center;
        tracer.aabb.hwidth = fn_createVec3(radius,radius,radius);
        tracer.radius = fn_createVec3(radius,radius,radius);
        tracer.velocity = fn_createVec3s(0);
        tracer.grounded = false;
        tracer.collided = false;
        tracer.aabb.mode = SPHERE;
        tracer.mode = TH_IMPACT_MODE;
        float dt = 20.0;

        fn_vec3 end = fn_multVec3s(direction,leg_length - 50);

        tracer.velocity = end;//fn_subVec3(end,tracer.aabb.position);
        // tracer.velocity = fn_normalizeVec3(tracer.velocity);
        tracer.velocity  = fn_multVec3s(tracer.velocity,1.0/dt);

        th_updateEntity(&tracer,world,dt,th_getPhysicsMemory(world,0));

        if (tracer.collided)
        {
            float leg_dist = fn_distance(center,tracer.collision_position);
            //printf("%f %f\n",leg_dist,leg_length);
            if (leg_dist > leg_length - 150  && leg_dist < leg_length - 100  )
            {
                //printf("%f\n",fn_degrees(acosf(fn_dot(direction,direction_in))));
                *hit_one = true;
                return fn_addVec3(fn_multVec3s(direction,leg_length),center);
            }
        }


    }
    return  fn_addVec3(fn_multVec3s(direction_in,leg_length),center);

}

static int pick_random_index(bool* arr1, bool* arr2) {
    int indices[3];
    int n = 0;

    if (arr1[0] && !arr2[0]) indices[n++] = 0;
    if (arr1[1] && !arr2[1]) indices[n++] = 1;
    if (arr1[2] && !arr2[2]) indices[n++] = 2;

    if (n == 0) return -1; // none true

    return indices[th_random() % n];
}

static fn_vec3 rotate_point_about(fn_vec3 p,fn_vec3 r,fn_vec3 n,float a)
{
    fn_vec3 tr = fn_subVec3(p,r);
    fn_vec3 rot = fn_rotatePointQuat(tr, fn_makeQuaternion(a,n));
    fn_vec3 rot_tr = fn_addVec3(rot,r);
    return rot_tr;

}



static bool can_progress(th_TricolumnGroup* c,int idx,fn_vec3 position,fn_vec3 direction)
{
    for (int i = 0; i < c->count; i++) {
        if (i == idx || !c->data[i].spawn_finished || c->data[i].state == TRICOL_GIBBED)
        {
            continue;
        }
        if (fn_distance2(position,c->data[i].position) < 700*700)
        {
            fn_vec3 dvec = fn_normalizeVec3(fn_subVec3(c->data[i].position,position));
            if (fn_dot(dvec,direction) > 0.5)
            {
                return false;
            }
        }
    }

    return true;
}

static bool can_spawn(th_TricolumnGroup* c,int idx,fn_vec3 position)
{
    for (int i = 0; i < c->count; i++) {
        if (i == idx || !c->data[i].spawn_finished || c->data[i].state == TRICOL_GIBBED)
        {
            continue;
        }
        if (fn_distance2(position,c->data[i].position) < 700*700)
        {
            return false;
        }
    }

    return true;
}

static bool checkGemInWorld(th_World* world,th_Entity* ent)
{
    return th_checkCollisionWorld(world,ent,th_getPhysicsMemory(world,0));
}

void th_tricolumnUpdate(th_TricolumnGroup* c,float dt)
{
    th_World* world = c->levelstate->world;
    const float leg_length = 1000;
    fn_vec3 tr = fn_createVec3(1,-1,1);

    for (int i = 0 ; i < c->count;i++)
    {

        bool spawned = true;
        if (th_time() - c->levelstate->level_start_time < c->data[i].spawn_when)
        {
            spawned = false;
        }
        else if (!c->data[i].spawn_finished)
        {
            //activate entities
            for (int j = i*7;j < i*7 + 7;j++)
            {
                c->entities_gems[j].alive = true;
            }
            c->data[i].spawn_finished = true;
            c->data[i].spawntimer = th_time() + SPAWN_INTERVAL_INITIAL;
            c->data[i].state = TRICOL_EXPLORE;
        }



        if (!(th_time() - c->levelstate->level_start_time > c->data[i].spawn_when - 1000))
        {

            c->transforms[i] =fn_makescale(fn_createVec3s(0));
            c->transforms_eye[i] =fn_makescale(fn_createVec3s(0));

            for (int j = 0 ; j < 7;j++)
            {
                c->transforms_gems[i*7 + j] = fn_makescale(fn_createVec3s(0));
            }

            for (int j = 0 ; j < 3;j++)
            {
                c->transforms_cols[i*3 + j] = fn_makescale(fn_createVec3s(0));
            }
            th_triColumnArmMatrices(c,i);

            continue;
        }
        else if (!spawned)
        {
            bool non_conflicting = can_spawn(c,i,c->data[i].position);
            if (!non_conflicting)
            {
                //can't spawn because you'd telefrag? just wait 5 seconds
                c->data[i].spawn_when = c->data[i].spawn_when + 5000;
                for (int j = i*7;j < i*7 + 7;j++)
                {
                    c->entities_gems[j].alive = false;
                }
                c->data[i].spawn_finished = false;

                c->transforms[i] =fn_makescale(fn_createVec3s(0));
                c->transforms_eye[i] =fn_makescale(fn_createVec3s(0));
                //printf("Delayed %i\n",i);
                c->data[i].state = TRICOL_EXPLORE;
                c->data[i].spawntimer = th_time() + SPAWN_INTERVAL_INITIAL;


                for (int j = 0 ; j < 7;j++)
                {
                    c->transforms_gems[i*7 + j] = fn_makescale(fn_createVec3s(0));
                }

                for (int j = 0 ; j < 3;j++)
                {
                    c->transforms_cols[i*3 + j] = fn_makescale(fn_createVec3s(0));
                }
                th_triColumnArmMatrices(c,i);

                continue;
            }
        }

        if (!c->data[i].set_up)
        {







            c->data[i].set_up = true;
            c->data[i].state = TRICOL_EXPLORE;


        }




         fn_vec3 motion_target = c->data[i].position;
         if (c->data[i].course != NULL)
         {
            motion_target = c->data[i].course[c->data[i].course_progress];
            if (fn_distance2(motion_target,c->data[i].position) < 300*300)
            {
                if (c->data[i].course_progress < c->data[i].course_count - 1)
                {
                    c->data[i].course_progress++;
                }
            }
         }




        //manage state
        switch (c->data[i].state)
        {
            case TRICOL_EXPLORE:
            {

                if (th_time() - c->levelstate->level_start_time > c->data[i].spawn_when + 500.0)
                {
                    int frame = c->data[i].interp_state_machine.frame;

                    // printf("%s %i %i/%i %f\n",c->data[i].interp_state_machine.reverse ? "Reverse" : "Forward",i,frame,c->data[i].num_motion_states - 1,c->data[i].interp_state_machine.alpha);

                    if (frame >= c->data[i].num_motion_states - 1 && !c->data[i].interp_state_machine.reverse)
                    {
                        th_triColumnPose(c->data[i].motion_states[c->data[i].num_motion_states - 1],c,i,dt,world);

                        th_deepCopyMotionState(&c->data[i].motion_state_interp,&c->data[i].motion_states[c->data[i].num_motion_states - 1]);

                        if (c->fastmode[i])
                        {

                            c->data[i].interp_state_machine.reverse = true;
                            c->data[i].interp_state_machine.frame = c->data[i].num_motion_states - 2;
                            c->data[i].interp_state_machine.alpha = 1.0;
                        }
                    }
                    else if (c->data[i].interp_state_machine.reverse && frame < 0 )
                    {
                        th_triColumnPose(c->data[i].motion_states[0],c,i,dt,world);

                        th_deepCopyMotionState(&c->data[i].motion_state_interp,&c->data[i].motion_states[0]);

                        if (c->fastmode[i])
                        {
                            c->data[i].interp_state_machine.reverse = false;
                            c->data[i].interp_state_machine.frame = 0;
                            c->data[i].interp_state_machine.alpha = 0.0;
                        }
                    }
                    else
                    {
                        fn_vec3 motion_bias = fn_normalizeVec3(fn_subVec3(c->data[i].motion_states[frame + 1].center_pos,c->data[i].motion_states[frame].center_pos));
                        bool canmove = can_progress(c,i,c->data[i].position,motion_bias);

                        float interp_dt = dt;
                        if (!canmove)
                        {
                            interp_dt = 0.0;
                        }

                        if (c->data[i].interp_state_machine.reverse)
                        {

                            c->data[i].interp_state_machine.alpha = 1.0 - c->data[i].interp_state_machine.alpha;
                            c->data[i].interp_state_machine.alpha = fn_clamp(c->data[i].interp_state_machine.alpha,0.0,1.0);

                            bool done = tricolumnInterpState(c,i,&c->data[i].motion_states[frame + 1],&c->data[i].motion_states[frame],&c->data[i].motion_state_interp,interp_dt,&c->data[i].interp_state_machine);

                            c->data[i].interp_state_machine.alpha = 1.0 - c->data[i].interp_state_machine.alpha;
                            c->data[i].interp_state_machine.alpha = fn_clamp(c->data[i].interp_state_machine.alpha,0.0,1.0);

                            if (done)
                            {
                                c->data[i].interp_state_machine.frame = c->data[i].interp_state_machine.frame - 1;
                                c->data[i].interp_state_machine.alpha = 1.0;
                                //printf("Frame %i\n",c->data[i].interp_state_machine.frame);
                            }


                        }
                        else
                        {
                            bool done = tricolumnInterpState(c,i,&c->data[i].motion_states[frame],&c->data[i].motion_states[frame + 1],&c->data[i].motion_state_interp,interp_dt,&c->data[i].interp_state_machine);
                            if (done)
                            {
                                c->data[i].interp_state_machine.frame = c->data[i].interp_state_machine.frame + 1;
                                c->data[i].interp_state_machine.alpha = 0.0;
                                //printf("Frame %i\n",c->data[i].interp_state_machine.frame);
                            }
                        }



                        //printf("%i %i %i\n",c->data[i].interp_state_machine.states[0],);

                        th_triColumnPose(c->data[i].motion_state_interp,c,i,dt,world);
                    }



                }
                else
                {
                    th_triColumnPose(c->data[i].motion_states[0],c,i,dt,world);



                    th_deepCopyMotionState(&c->data[i].motion_state_interp,&c->data[i].motion_states[0]);
                }



                if (th_time() > c->data[i].spawntimer  )
                {
                    if (c->data[i].state == TRICOL_EXPLORE)
                    {
                        //no new contact found, pick a different pivot direction

                        if (!c->fastmode[i])
                        {
                            th_playSoundIfNotPlaying(&c->data[i].spawn_sound,c->data[i].position,sound_tricol_givebirth,1.0 );
                            a_setVSRolloff(c->data[i].spawn_sound,0.2);
                            c->data[i].state = TRICOL_SPAWN;
                            c->data[i].orient_trgt_basedir = sample_direction(fn_createVec3(0,0,-1),fn_radians(179.0));
                            c->data[i].spawntimer = th_time() + SPAWN_DURATION;
                            c->data[i].spawn_heartbeat = th_time() + 200;
                        }
                        else
                        {
                            c->data[i].spawntimer = th_time() + SPAWN_DURATION;
                        }

                    }

                }



                for (int j = 2; j >= 0;j--)
                {
                    if (c->data[i].hinge_interp[j] > 0.0)
                    {

                        if (c->data[i].hinge_interp[j] == 1.0)
                        {
                            {
                                fn_vec3 pos = fn_rotatePointQuat(fn_createVec3(0,110,0),c->data[i].col_orientations[j]);
                                pos = fn_addVec3(c->data[i].col_points[j],pos);
                                a_VirtualSource* s = a_playVirtualSource(sound_tricol_dooropen,-1,pos,NULL );
                                a_setVSLoop(s,false);
                                a_setVSPos(s,pos);
                                a_setVSVel(s,fn_createVec3s(0));
                                a_setVSGain(s,1.0);
                                a_setVSPitch(s,th_randomFloat(0.85,1.15));
                            }
                        }
                        c->data[i].hinge_interp[j] = c->data[i].hinge_interp[j] - (1.0/200.0)*dt;

                        if (c->data[i].hinge_interp[j] < 0.0)
                        {
                            c->data[i].hinge_interp[j] = 0.0;
                        }

                        break;
                    }
                }





            }
            break;
            case TRICOL_GIBBED:{

                th_Entity* e = &c->data[i].baseplate_gib;
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

                    fn_vec3 pos_ent = e->aabb.position;

                    fn_quat orient = c->data[i].orientation;
                    c->transforms[i] = fn_translaterotatescaleq(pos_ent,orient,fn_createVec3s(10));



                    if (th_time() - c->data[i].orient_eye_timer > 450)
                    {
                        c->data[i].orient_trgt_eyedir = sample_direction(fn_createVec3(0,0,-1),fn_radians(90));
                        c->data[i].orient_eye_timer = th_time();
                    }

                    c->data[i].orientation_eyedir = update_orient(&c->data[i].direction_vector_eyedir,c->data[i].orient_trgt_eyedir,0.2,dt,NULL,NULL,&c->data[i].old_ups_eyedir);

                    fn_quat eye_orr = fn_multquat(c->data[i].orientation_eyedir,orient);


                    fn_vec3 offset_eye = fn_rotatePointQuat(fn_createVec3(0,0,eye_offset_amt),eye_orr);
                    c->transforms_eye[i] = fn_translaterotatescaleq(fn_addVec3(pos_ent,offset_eye),eye_orr,fn_createVec3s(1.2));

                    c->data[i].old_body = c->transforms[i];
                    c->data[i].old_eye = c->transforms_eye[i];
                }
                else {
                    float old_fade = c->data[i].fadeout_body;
                    c->transforms[i] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_body,fadeout_decay_rate*dt),c->data[i].old_body);

                    c->transforms_eye[i] = fn_multMat4(th_fadeoutMatrix(&old_fade,fadeout_decay_rate*dt),c->data[i].old_eye);

                }




                for (size_t p = 0; p < 3; p++) {
                    //simulate entity pair
                    th_Entity old_0 = c->data[i].leg_gib[p][0];
                    th_Entity old_1 = c->data[i].leg_gib[p][1];
                    for (size_t l = 0; l < 2; l++) {
                        th_Entity* e = &c->data[i].leg_gib[p][l];
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
                                a_setVSGain(s,fn_remap(fn_length(e->velocity),0,0.5,0.3,0.4));
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

                            c->data[i].leg_gib[p][0].alive = false;
                            c->data[i].leg_gib[p][1].alive = false;
                        }
                    }

                    if (!old_0.alive && !old_1.alive)
                    {
                        c->transforms_cols[i*3 + p ] = fn_multMat4(th_fadeoutMatrix(&c->data[i].fadeout_legs[p],fadeout_decay_rate*dt),c->data[i].old_legs[p ]);
                    }
                    else
                    {
                        //enforce constrint
                        float dist_current = fn_distance(c->data[i].leg_gib[p][0].aabb.position,c->data[i].leg_gib[p][1].aabb.position);
                        float error = dist_current - offset_v;
                        // printf("%f\n",error );
                        fn_vec3 direction = fn_subVec3(c->data[i].leg_gib[p][1].aabb.position,c->data[i].leg_gib[p][0].aabb.position);
                        direction = fn_normalizeVec3(direction);
                        fn_vec3 vel_correct_0 = fn_multVec3s(direction,error*0.5);
                        fn_vec3 vel_correct_1 = fn_multVec3s(direction,-error*0.5);

                        fn_vec3 canidate_0 = fn_addVec3(c->data[i].leg_gib[p][0].aabb.position,vel_correct_0);
                        fn_vec3 canidate_1 = fn_addVec3(c->data[i].leg_gib[p][1].aabb.position,vel_correct_1);
                        float radius = c->data[i].leg_gib[p][0].aabb.hwidth.x;

                        bool hit_0 = false;
                        fn_vec3 pos_correct_0 = th_traceVolume(c->levelstate->world,c->data[i].leg_gib[p][0].aabb.position,canidate_0,radius,NULL,&hit_0,th_getPhysicsMemory(c->levelstate->world,0));

                        bool hit_1 = false;
                        fn_vec3 pos_correct_1 = th_traceVolume(c->levelstate->world,c->data[i].leg_gib[p][1].aabb.position,canidate_1,radius,NULL,&hit_0,th_getPhysicsMemory(c->levelstate->world,0));

                        if (c->data[i].leg_gib[p][0].alive)
                            c->data[i].leg_gib[p][0].velocity = fn_addVec3(c->data[i].leg_gib[p][0].velocity,fn_multVec3s(vel_correct_0,1.0/32.0));
                        if (c->data[i].leg_gib[p][0].alive)
                            c->data[i].leg_gib[p][1].velocity = fn_addVec3(c->data[i].leg_gib[p][1].velocity,fn_multVec3s(vel_correct_1,1.0/32.0));


                        fn_vec3 n0 = c->data[i].leg_gib[p][0].aabb.position;
                        fn_vec3 n1 = c->data[i].leg_gib[p][1].aabb.position;
                        fn_vec3 offset_n = fn_multVec3s(fn_addVec3(n0,n1),0.5);


                        fn_vec3 tr = fn_createVec3(1,1,1);

                        fn_vec3 top_pos = fn_createVec3(0,-offset_v*0.5,0);
                        fn_vec3 bottom_pos = fn_createVec3(0,offset_v*0.5,0);

                        fn_vec3 bone_1_to = fn_multVec3(fn_normalizeVec3(fn_subVec3(n1 ,n0)),tr);


                        fn_mat4 cam = th_6dofCamera(&c->data[i].old_forwards[p],&c->data[i].old_rights[p],&c->data[i].old_ups[p],fn_multVec3(bone_1_to,fn_createVec3(1,1,1)),fn_createVec3(0,0,0));

                        fn_mat4 m = fn_inverse(cam);

                        c->data[i].col_orientations[p] = fn_mat4toquat(m);

                        c->transforms_cols[i*3 + p] = fn_translaterotatescaleq(offset_n,c->data[i].col_orientations[p],fn_createVec3s(10));
                        c->data[i].old_legs[p ] = c->transforms_cols[i*3 + p];
                    }






                }

                th_triColumnArmMatrices(c,i);
            }

            break;
            case TRICOL_SPAWN:
            {


                for (int j = 0; j < 3;j++)
                {
                    if (c->data[i].hinge_interp[j] < 1.0)
                    {

                        if (c->data[i].hinge_interp[j] == 0.0)
                        {
                            {
                                fn_vec3 pos = fn_rotatePointQuat(fn_createVec3(0,110,0),c->data[i].col_orientations[j]);
                                pos = fn_addVec3(c->data[i].col_points[j],pos);
                                a_VirtualSource* s = a_playVirtualSource(sound_tricol_dooropen,-1,pos,NULL );
                                a_setVSLoop(s,false);
                                a_setVSPos(s,pos);
                                a_setVSVel(s,fn_createVec3s(0));
                                a_setVSGain(s,1.0);
                                a_setVSPitch(s,th_randomFloat(0.85,1.15));
                            }
                        }
                        c->data[i].hinge_interp[j] = c->data[i].hinge_interp[j] + (1.0/200.0)*dt;

                        if (c->data[i].hinge_interp[j] > 1.0)
                        {
                            c->data[i].hinge_interp[j] = 1.0;
                        }

                        break;
                    }
                }


                int col_spawn_from = c->data[i].spawn_heartbeat_index % 3;
                fn_vec3 trap_point = fn_rotatePointQuat(fn_createVec3(0,110,0),c->data[i].col_orientations[col_spawn_from]);


                th_triColumnPoseBaseplate(c,i,dt,false,true);

                th_triColumnPoseGems(c,i,dt);

                // fn_quat eye_orr = fn_multquat(c->data[i].orientation_eyedir,c->data[i].orientation);
                // fn_vec3 offset_spawn = fn_rotatePointQuat(fn_createVec3(0,0,120),eye_orr);
                // offset_spawn = fn_addVec3(c->data[i].position,offset_spawn);

                fn_vec3  offset_spawn = fn_addVec3(c->data[i].col_points[col_spawn_from],trap_point);



                if (th_time() > c->data[i].spawntimer  )
                {
                    c->data[i].spawntimer = th_time() + SPAWN_INTERVAL;
                    c->data[i].state = TRICOL_EXPLORE;

                    c->data[i].spawn_flip_flop = (c->data[i].spawn_flip_flop + 1) % 2;

                    if (c->data[i].spawn_flip_flop == 0)
                    {
                        th_eyeballSpawn(c->levelstate->eyeball_super,c->data[i].position);
                    }

                }

                if (th_time() > c->data[i].spawn_heartbeat)
                {
                    c->data[i].spawn_heartbeat = th_time() + 200;

                    c->data[i].spawn_heartbeat_index = c->data[i].spawn_heartbeat_index + 1;

                    fn_vec3 velocity = fn_multVec3s(fn_normalizeVec3(trap_point),0.3);

                    bool canspawnboid = checkSpawn(c->levelstate->world,c->data[i].position,offset_spawn);
                    if (canspawnboid)
                    {
                        //spawn a boid
                        int index = th_boidsSpawnShield(&c->levelstate->boidgroups[0],offset_spawn);
                        if (index >= 0)
                        {
                            c->levelstate->boidgroups[0].entities[index].aabb.position = offset_spawn;
                            c->levelstate->boidgroups[0].entities[index].velocity = velocity;// fn_normalizeVec3(fn_createVec3(x*60,-y*60,z*60));
                            c->levelstate->boidgroups[0].prime_velocity[index] = velocity;
                            c->levelstate->boidgroups[0].prime_target[index] = c->data[i].position;
                        }
                    }

                }

                th_triColumnArmMatrices(c,i);



            }


            break;

            default:

            break;
        }

        if (c->data[i].state != TRICOL_GIBBED)
        {

            if (c->fastmode[i] && fn_distance2(c->data[i].position,c->levelstate->player_e.aabb.position) < 1500*1500 )
            {
                if (c->data[i].state == TRICOL_EXPLORE)
                {
                    c->data[i].aggrotimer = th_time() - 250;
                    th_playSoundIfNotPlaying(&c->data[i].lazer,c->data[i].position,sound_laser_tricol,1.0 );
                }
            }

            th_pushOccluderFrame(fn_createVec4Vec3(c->data[i].position,300.0));

            for (int j = 0 ; j < 3;j++)
            {
                th_pushOccluderFrame(fn_createVec4Vec3(fn_addVec3(c->data[i].position,fn_multVec3s(c->data[i].motion_state_interp.leg_directions[j],250)),200.0));
                th_pushOccluderFrame(fn_createVec4Vec3(fn_addVec3(c->data[i].position,fn_multVec3s(c->data[i].motion_state_interp.leg_directions[j],550)),300.0));

                float leg_len = c->data[i].motion_state_interp.leg_lengths[j];
                th_pushOccluderFrame(fn_createVec4Vec3(fn_addVec3(c->data[i].position,fn_multVec3s(c->data[i].motion_state_interp.leg_directions[j],leg_length)),120.0));
            }

            int num_gems = 0;
            for (int j = 0;j < 7;j++)
            {
                if (c->entities_gems[i*7 + j].impact)
                {
                    for (int k = 0 ; k < c->entities_gems[i*7 + j].impact_count;k++ )
                    {
                        th_Entity* projectile = (th_Entity*)c->entities_gems[i*7 + j].impacts[k].entity;
                        fn_vec3 vel = fn_multVec3s(fn_normalizeVec3(projectile->velocity),-1.0);

                        bool positive = j % 2 == 0;
                        float signed_dir = 1.0;
                        if (!positive)
                        {
                            signed_dir = -1.0;
                        }

                        fn_mat4 gem_tr_mat;
                        if (j == 0)
                        {
                            gem_tr_mat = c->transforms[i];
                            signed_dir = 1.0;
                        }
                        else
                        {
                            gem_tr_mat = c->transforms_cols[i*3 + j%3];

                        }
                        // fn_vec3 gemdir = fn_transformNormal(fn_createVec3(signed_dir,0,0),gem_tr_mat);
                        fn_vec3 gemdir;
                        if (j == 0)
                        {
                            gemdir = fn_transformNormal(fn_createVec3(0,0,signed_dir),gem_tr_mat);
                        }
                        else
                        {
                            gemdir = fn_transformNormal(fn_createVec3(signed_dir,0,0),gem_tr_mat);
                        }
                        //
                        //printf("%f %i\n",fn_dot(gemdir,vel),j);
                        if ( (fn_dot(gemdir,vel) > -0.15  || projectile->type == TH_HAMMER_PROJECTILE )&& c->data[i].hasgem[j] ) //&& fn_angle(gemdir,vel) < fn_radians(90)
                        {
                            th_Hitmarker hmarker;
                            hmarker.entity_pos_ref = NULL;
                            hmarker.entity_transform_ref = &c->transforms_gems[i*7 + j];
                            hmarker.alive_ref = &c->data[i].hasgem[j];
                            hmarker.timer = th_time();
                            hmarker.last_good_pos = fn_createVec3s(0.0);
                            hmarker.is_alive = true;
                            hmarker.radius = j == 0 ? 110.0 : 110.0;
                            th_pushHitmarker(hmarker);

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

                            if (c->data[i].hasgem[j] && c->data[i].gemhealth[j] <= 0.0)
                            {
                                if (c->data[i].state == TRICOL_EXPLORE && !c->fastmode[i])
                                {
                                    c->data[i].aggrotimer = th_time();
                                    th_playSoundIfNotPlaying(&c->data[i].lazer,c->data[i].position,sound_laser_tricol,1.0 );
                                }

                                c->data[i].hasgem[j] = false;
                                th_gemSpawn(c->levelstate->gems,c->entities_gems[i*7 + j].aabb.position,fn_multVec3s(gemdir,0.5));
                                th_gemSpawn(c->levelstate->gems,c->entities_gems[i*7 + j].aabb.position,fn_multVec3s(gemdir,-0.5));
                                th_spawnBloodNoSound(c->entities_gems[i*7 + j].aabb.position,0);



                                {
                                    a_VirtualSource* s = a_playVirtualSource(sound_gem_breakfree,0,c->entities_gems[i*7 + j].aabb.position,NULL );
                                    a_setVSLoop(s,false);
                                    a_setVSPos(s,c->entities_gems[i*7 + j].aabb.position);
                                    a_setVSVel(s,fn_createVec3s(0));
                                    a_setVSGain(s,1.0);
                                    a_setVSPitch(s,th_randomFloat(0.85,1.15));
                                }
                            }
                            else
                            {
                                th_spawnBloodSpurt(fn_multVec3s(vel,1),c->entities_gems[i*7 + j].impacts[k].pos,-1);
                                th_spawnBloodNoSound(c->entities_gems[i*7 + j].aabb.position,0);
                                fn_vec3 b1 = fn_multVec3s(fn_createVec3(signed_dir,0,0),th_randomFloat(0,1.0));
                                fn_vec3 b2 = fn_multVec3s(fn_createVec3(0,1,0),th_randomFloat(-1.0,1.0));
                                fn_vec3 b3 = fn_multVec3s(fn_createVec3(0,0,1),th_randomFloat(-1.0,1.0));
                                if (c->data[i].gem_jitter_t[j] <= ((1.0/0.05)*3.14159)*5*0.5)
                                {
                                    c->data[i].gem_jitter_x[j] = fn_normalizeVec3(fn_addVec3(b1,fn_addVec3(b2,b3)));
                                    c->data[i].gem_jitter_t[j] = ((1.0/0.05)*3.14159)*5;//10*3.14159*9;

                                    {
                                        a_VirtualSource* s = a_playVirtualSource(sound_gem_impact,0,c->entities_gems[i*7 + j].aabb.position,NULL );
                                        a_setVSLoop(s,false);
                                        a_setVSPos(s,c->entities_gems[i*7 + j].aabb.position);
                                        a_setVSVel(s,fn_createVec3s(0));
                                        a_setVSGain(s,0.8);
                                        a_setVSPitch(s,th_randomFloat(0.85,1.15));
                                    }
                                }


                            }



                        }
                        else
                        {
                            th_spawnSparks(fn_multVec3s(vel,1),c->entities_gems[i*7 + j].impacts[k].pos,-1,c->levelstate->general_light_query);
                            th_spawnSparks(fn_multVec3s(vel,1),c->entities_gems[i*7 + j].impacts[k].pos,-1,c->levelstate->general_light_query);
                            th_spawnSparks(fn_multVec3s(vel,1),c->entities_gems[i*7 + j].impacts[k].pos,-1,c->levelstate->general_light_query);


                            {
                                a_VirtualSource* s = a_playVirtualSource(15 + th_random() % 3,-1,c->entities_gems[i*7 + j].impacts[k].pos,NULL );
                                a_setVSLoop(s,false);
                                a_setVSPos(s,c->entities_gems[i*7 + j].impacts[k].pos);
                                a_setVSVel(s,fn_createVec3s(0));
                                a_setVSGain(s,0.67);
                            }

                        }


                    }
                    c->entities_gems[i*7 + j].impact_count = 0;
                    c->entities_gems[i*7 + j].impact = false;


                }
                if (c->data[i].hasgem[j])
                {
                    num_gems++;
                }
            }

            if (num_gems == 0 )
            {
                th_markEnemyDeath(1);
                c->data[i].state = TRICOL_GIBBED;
                c->levelstate->num_spawners_killed = c->levelstate->num_spawners_killed + 1;
                c->data[i].baseplate_gib.aabb.position = c->data[i].position;
                c->data[i].baseplate_gib.velocity = fn_multVec3s(fn_createVec3s(0.5),0.45);
                th_setGameplayTimeScale(fn_createVec3(0.45,0.00000,0.0000002));

                for (size_t p = 0; p < 7; p+= 1) {
                    c->entities_gems[i*7 + p].alive = false;
                    c->transforms_gems[i*7 + p] = fn_makescale(fn_createVec3s(0));
                }

                for (size_t p = 0; p < 3; p+= 1) {

                    fn_vec3 gemdir = sample_direction(fn_createVec3(0,1,0),fn_radians(175.0));
                    float gamma = 2.5;
                    // float beta = 1.0;

                    fn_vec3 axis = fn_normalizeVec3(fn_subVec3(c->data[i].col_points[p],c->data[i].position));
                    c->data[i].leg_gib[p][0].aabb.position = fn_addVec3(c->data[i].col_points[p],fn_multVec3s(axis,-500));
                    c->data[i].leg_gib[p][0].velocity = fn_multVec3s(gemdir,-gamma);
                    c->data[i].leg_gib[p][1].aabb.position = fn_addVec3(c->data[i].col_points[p],fn_multVec3s(axis,500));
                    c->data[i].leg_gib[p][1].velocity = fn_multVec3s(gemdir,gamma);


                    // c->data[i].leg_gib[p][0].velocity = fn_addVec3(c->data[i].leg_gib[p][0].velocity,fn_multVec3s(c->data[i].old_up,beta));
                    // c->data[i].leg_gib[p][1].velocity = fn_addVec3(c->data[i].leg_gib[p][1].velocity,fn_multVec3s(c->data[i].old_up,beta));


                    {
                        a_VirtualSource* s = a_playVirtualSource(21,-1, c->data[i].col_points[p],NULL);
                        a_setVSLoop(s,false);
                        a_setVSPos(s,c->data[i].col_points[p]);
                        a_setVSVel(s,fn_createVec3s(0));
                        a_setVSGain(s,0.5);
                    }
                }

            }

        }

        if (!spawned)
        {
            if (th_time() - c->levelstate->level_start_time > c->data[i].spawn_when - 1000)
            {


               // printf("Fading %i %i\n",i,c->data[i].state);
                if (c->data[i].fadeout_spawn == 1.0)
                {

                    a_VirtualSource* s = a_playVirtualSource(sound_spawn_creatureii,1,c->data[i].position,NULL );
                    a_setVSLoop(s,false);
                    a_setVSPos(s,c->data[i].position);
                    a_setVSVel(s,fn_createVec3s(0.0));
                    a_setVSGain(s,1.0);

                    a_duckVS(s,4500.0,0.20);
                    a_setVSRolloff(s,0.3);

                    c->levelstate->num_spawners_current = c->levelstate->num_spawners_current + 1;
                }
                c->data[i].fadeout_spawn = c->data[i].fadeout_spawn - 0.003*dt;
                if (c->data[i].fadeout_spawn < 0 )
                {
                    c->data[i].fadeout_spawn = 0;
                }
                fn_mat4 fdmat = fn_makescale(fn_createVec3s(1.0 - fn_clamp(c->data[i].fadeout_spawn ,0.0,1.0)));

                c->transforms[i] = fn_multMat4(fdmat,c->transforms[i]);
                c->transforms_eye[i] = fn_multMat4(fdmat,c->transforms_eye[i]);

                for (int j = 0 ; j < 7;j++)
                {
                    c->transforms_gems[i*7 + j] = fn_multMat4(fdmat,c->transforms_gems[i*7 + j]);
                }

                for (int j = 0 ; j < 3;j++)
                {
                    c->transforms_cols[i*3 + j] = fn_multMat4(fdmat,c->data[i].old_legs[j]);
                    //fn_printMat4(c->transforms_cols[i*3 + j]);
                }
                th_triColumnArmMatrices(c,i);
            }
        }

    }

}
