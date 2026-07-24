#include "th_ragdoll.h"

#include <string.h>
#include <stdio.h>

th_RagdollConstraint th_createConstraint(int i0,int i1,float d)
{
    th_RagdollConstraint ret;
    ret.index0 = i0;
    ret.index1 = i1;
    ret.d = d;
    return ret;
}

th_AngleConstraint th_createAngleConstraint(int i0,int i1,int i2,float min_angle,float max_angle)
{
    th_AngleConstraint ret;
    ret.parent_idx = i0;
    ret.child_idx = i1;
    ret.grandchild_idx = i2;
    ret.min_angle = min_angle;
    ret.max_angle = max_angle;

    return ret;
}



void th_initRagdoll(th_World* world,th_Allocator* alloc,th_Ragdoll* ragdoll,fn_vec3* bones_p,fn_vec3* bones_s,fn_quat* bones_r,float* masses,int count, th_RagdollConstraint* constraints,int constraint_count,int* reference_bones)
{
    ragdoll->parents = th_alloc(alloc,sizeof(int)*count);
    ragdoll->constraint_axes = th_alloc(alloc,sizeof(fn_vec3)*count);
    ragdoll->constraint_angles = th_alloc(alloc,sizeof(float)*count);
    ragdoll->reference_bones = reference_bones;
    ragdoll->world = world;
    ragdoll->bones_position = bones_p;
    ragdoll->bones_scale = bones_s;
    ragdoll->bones_rot = bones_r;
    ragdoll->masses = masses;
    ragdoll->count = count;


    ragdoll->upvectors = th_alloc(alloc,sizeof(fn_vec3)*count);
    ragdoll->inverse_phys = th_alloc(alloc,sizeof(fn_quat)*count);

    ragdoll->bones_position_original = th_alloc(alloc,sizeof(fn_vec3)*count);
    memcpy(ragdoll->bones_position_original,ragdoll->bones_position,sizeof(fn_vec3)*count);

    ragdoll->constraints = constraints;
    ragdoll->constraint_count = constraint_count;


    ragdoll->bodies = th_alloc(alloc,sizeof(th_RagdollBody)*count);
    for (int i = 0 ; i < count;i++)
    {
        ragdoll->parents[i] = 0;
        ragdoll->constraint_angles[i] = -1.0;
        ragdoll->constraint_axes[i] = fn_createVec3(0,0,0);
        ragdoll->bodies[i].e = TH_DEFAULT_ENTITY;
        ragdoll->bodies[i].e.aabb.position = bones_p[i];
        ragdoll->bodies[i].e.aabb.hwidth = fn_createVec3(25,25,25);
        ragdoll->bodies[i].e.velocity = fn_createVec3s(0);
        ragdoll->bodies[i].e.grounded = false;
        ragdoll->bodies[i].e.aabb.mode = BOX;
        //ragdoll->bodies[i].e.mode = TH_SLIDE_MODE;
        ragdoll->bodies[i].e.alive = true;
        ragdoll->bodies[i].e.collided = false;
        ragdoll->bodies[i].e.impact = false;
        ragdoll->bodies[i].e.delete_me = false;
        ragdoll->bodies[i].e.impact_count = 0;

        ragdoll->bodies[i].prev_position = bones_p[i];
        ragdoll->bodies[i].target_position = bones_p[i];

        if (ragdoll->masses[i] != 0.0)
        {
            ragdoll->masses[i] = 1.0/ragdoll->masses[i];
        }

    }
}


 void solve_distance_constraints_rigid(th_Ragdoll* sys) {
    for (int i = 0; i < sys->constraint_count; i++) {
        th_RagdollConstraint* c = &sys->constraints[i];
        th_RagdollBody* a = &sys->bodies[c->index0];
        th_RagdollBody* b = &sys->bodies[c->index1];

        th_Entity* ea = &sys->bodies[c->index0].e;

        th_Entity* eb = &sys->bodies[c->index1].e;

        fn_vec3 delta = fn_subVec3(b->target_position, a->target_position);
        float current_length = fn_length(delta);

        if (current_length < 1e-6f) continue;

        fn_vec3 dir = fn_multVec3s(delta, 1.0f / current_length);
        float error = current_length - c->d;

        float w_sum = sys->masses[c->index0] + sys->masses[c->index1];
        if (w_sum < 1e-6f) continue;

        // Simple correction: split error proportionally by mass
        float correction_mag = error / w_sum;

        // if (!ea->collided && !eb->collided)
        // {
            a->target_position = fn_addVec3(a->target_position,
                                            fn_multVec3s(dir, correction_mag * sys->masses[c->index0]));
            b->target_position = fn_subVec3(b->target_position,
                                            fn_multVec3s(dir, correction_mag * sys->masses[c->index1]));
        // }
        // else if (!ea->collided)
        // {
        //     a->target_position = fn_addVec3(a->target_position,
        //                                     fn_multVec3s(dir, error));
        // }
        // else if (!eb->collided)
        // {
        //     b->target_position = fn_subVec3(b->target_position,
        //                                     fn_multVec3s(dir, error));
        // }


    }
}

void solve_collision_constraints(th_Ragdoll* sys)
{
    float dt = 1.0;
    for (int i = 0; i < sys->count; i++) {
        if (sys->masses[i] == 0.0)
        {
            continue;
        }

        th_Entity* e = &sys->bodies[i].e;
        fn_vec3 old_velocity = e->velocity;
        e->velocity = fn_multVec3s(fn_subVec3(sys->bodies[i].target_position, e->aabb.position),1.0/dt);
        th_updateEntity(e,sys->world,dt,th_getPhysicsMemory(sys->world,0));

        sys->bodies[i].target_position = e->aabb.position;
        e->velocity = old_velocity;
    }
}

// void solve_angle_constraints_old(th_Ragdoll* sys) {
//     for (int i = 0; i < sys->count; i++) {
//         if (sys->constraint_angles[i] != -1.0 && !fn_equalVec3(sys->constraint_axes[i],fn_createVec3s(0)))
//         {
//             int pidx = sys->parents[i];
//             fn_vec3 dir = fn_normalizeVec3(fn_subVec3(sys->bodies[i].target_position,sys->bodies[pidx].target_position));
//             fn_vec3 constraint_axis = sys->constraint_axes[i];
//
//             float dot = fn_dot(dir,constraint_axis);
//             float angle = acosf(fn_dot(dir,constraint_axis))*(180.0/3.14159);
//             if (angle > sys->constraint_angles[i])
//             {
//                 fn_vec3 rvec = fn_normalizeVec3(fn_cross(dir,constraint_axis));
//
//                 fn_quat qtrans = fn_makeQuaternion(sys->constraint_angles[i]*(3.14159/180.0),rvec);
//
//                 fn_vec3 ndir = fn_rotatePointQuat(constraint_axis,qtrans);
//
//                 float d = fn_distance(sys->bodies[i].target_position,sys->bodies[pidx].target_position);
//                 fn_vec3 newpos = fn_addVec3(sys->bodies[pidx].target_position,fn_multVec3s(ndir,d));
//                 //sys->bodies[i].target_position = newpos;
//                 float relaxation = 0.5;
//
//                 fn_vec3 correction = fn_normalizeVec3(fn_subVec3(newpos, sys->bodies[i].target_position));
//                 sys->bodies[i].target_position = fn_addVec3(sys->bodies[i].target_position,
//                                                             fn_multVec3s(correction, relaxation));
//             }
//
//         }
//     }
// }


void solve_angle_constraints(th_Ragdoll* sys) {
    for (int i = 0; i < sys->count; i++) {
        if (sys->constraint_angles[i] != -1.0 && !fn_equalVec3(sys->constraint_axes[i], fn_createVec3s(0)))
        {
            int pidx = sys->parents[i];

            fn_vec3 dir = fn_subVec3(sys->bodies[i].target_position, sys->bodies[pidx].target_position);
            float d = fn_length(dir);
            if (d < 1e-6) continue; // Skip if bodies overlap
            dir = fn_multVec3s(dir, 1.0f / d); // Normalize

            fn_vec3 constraint_axis = fn_normalizeVec3(sys->constraint_axes[i]);
            float cos_angle = fn_dot(dir, constraint_axis);

            // Clamp to avoid numerical issues with acos
            cos_angle = fmaxf(-1.0f, fminf(1.0f, cos_angle));
            float angle = acosf(cos_angle);
            float max_angle = sys->constraint_angles[i] * (3.14159f / 180.0f);

            if (angle > max_angle)
            {
                // Fix: rotation axis should be constraint_axis × dir
                fn_vec3 rvec = fn_normalizeVec3(fn_cross(constraint_axis, dir));

                // Rotate dir toward constraint_axis by the excess angle
                float excess = angle - max_angle;
                fn_quat qtrans = fn_makeQuaternion(excess, rvec);
                fn_vec3 corrected_dir = fn_rotatePointQuat(dir, qtrans);

                // Calculate correction with proper mass weighting
                fn_vec3 target_pos = fn_addVec3(sys->bodies[pidx].target_position, fn_multVec3s(corrected_dir, d));
                fn_vec3 correction = fn_subVec3(target_pos, sys->bodies[i].target_position);

                // Apply correction with relaxation and mass ratios
                float stiffness = 0.1f; // Adjust this (0-1) to tune stability
                float w1 = sys->masses[i];
                float w2 = sys->masses[pidx];
                float w_sum = w1 + w2;

                if (w_sum > 1e-6) {
                    sys->bodies[i].target_position = fn_addVec3(sys->bodies[i].target_position,
                                                                fn_multVec3s(correction, stiffness * w1 / w_sum));
                    sys->bodies[pidx].target_position = fn_subVec3(sys->bodies[pidx].target_position,
                                                                   fn_multVec3s(correction, stiffness * w2 / w_sum));
                }
            }
        }
    }
}

//
void pbd_step(th_Ragdoll* sys, float dt) {
    //Predict positions from velocities
    for (int i = 0; i < sys->count; i++) {
        if (sys->masses[i] == 0.0)
        {
            continue;
        }
        sys->bodies[i].prev_position = sys->bodies[i].e.aabb.position;

        sys->bodies[i].e.velocity = fn_addVec3(
            sys->bodies[i].e.velocity,
            fn_multVec3s(fn_createVec3(0, 0.001f, 0), dt)  // gravity
        );

        sys->bodies[i].target_position = fn_addVec3(
            sys->bodies[i].e.aabb.position,
            fn_multVec3s(sys->bodies[i].e.velocity, dt)
        );

    }

    //Project constraints (Gauss-Seidel iterations)
    for (int iter = 0; iter < 5; iter++) {
        solve_distance_constraints_rigid(sys);
        solve_angle_constraints(sys);
        solve_collision_constraints(sys);
    }

    //Update velocities from position changes
    for (int i = 0; i < sys->count; i++) {
        if (sys->masses[i] == 0.0)
        {
            continue;
        }
        sys->bodies[i].e.velocity = fn_multVec3s(
            fn_subVec3(sys->bodies[i].e.aabb.position, sys->bodies[i].prev_position),
                                             1.0f / dt
        );


        float damping = 0.995f;//powf(0.99f, dt);
        sys->bodies[i].e.velocity = fn_multVec3s(sys->bodies[i].e.velocity, damping);
    }
}


void th_simulateRagdoll(th_Ragdoll* ragdoll,float dt)
{
    if (dt < 0.001f)
    {
        return;
    }
    pbd_step(ragdoll,dt);
    for (int i = 0; i < ragdoll->count; i++) {

        //printf("%f\n",ragdoll->masses[i]);
        if (ragdoll->masses[i] == 0.0)
        {
            ragdoll->bones_position[i] = fn_createVec3s(0);
        }
        else
        {
            ragdoll->bones_position[i] = ragdoll->bodies[i].target_position;
        }





    }


}



void solve_distance_constraints_simple(th_SimpleRagdoll* sys) {
    for (int i = 0; i < sys->num_constraints; i++) {
        th_RagdollConstraint* c = &sys->constraints[i];
        fn_vec3* a = &sys->positions[c->index0];
        fn_vec3* b = &sys->positions[c->index1];

        fn_vec3 delta = fn_subVec3(*b, *a);
        float current_length = fn_length(delta);

        if (current_length < 1e-6f) continue;

        fn_vec3 dir = fn_multVec3s(delta, 1.0f / current_length);
        float error = current_length - c->d;

        float w_sum = sys->masses[c->index0] + sys->masses[c->index1];
        if (w_sum < 1e-6f) continue;

        // Simple correction: split error proportionally by mass
        float correction_mag = error / w_sum;


        *a = fn_addVec3(*a,
                                        fn_multVec3s(dir, correction_mag * sys->masses[c->index0]));
        *b = fn_subVec3(*b,
                                        fn_multVec3s(dir, correction_mag * sys->masses[c->index1]));

    }
}

void solve_angle_constraints_simple(th_SimpleRagdoll* sys) {
    for (int i = 0; i < sys->num_angle_constraints; i++) {

        th_SimpleAngularConstraint ac = sys->angle_constraints[i];


        fn_vec3 dir = fn_subVec3(sys->positions[ac.index0], sys->positions[ac.index1]);
        float d = fn_length(dir);
        if (d < 1e-6) continue; // Skip if bodies overlap
        dir = fn_multVec3s(dir, 1.0f / d); // Normalize

        fn_vec3 constraint_axis = fn_normalizeVec3(ac.axis);
        float cos_angle = fn_dot(dir, constraint_axis);

        // Clamp to avoid numerical issues with acos
        cos_angle = fmaxf(-1.0f, fminf(1.0f, cos_angle));
        float angle = acosf(cos_angle);
        float max_angle = ac.angle * (3.14159f / 180.0f);

        if (angle > max_angle)
        {
            // Fix: rotation axis should be constraint_axis × dir
            fn_vec3 rvec = fn_normalizeVec3(fn_cross(constraint_axis, dir));

            // Rotate dir toward constraint_axis by the excess angle
            float excess = angle - max_angle;
            fn_quat qtrans = fn_makeQuaternion(excess, rvec);
            fn_vec3 corrected_dir = fn_rotatePointQuat(dir, qtrans);

            // Calculate correction with proper mass weighting
            fn_vec3 target_pos = fn_addVec3(sys->positions[ac.index1], fn_multVec3s(corrected_dir, d));
            fn_vec3 correction = fn_subVec3(target_pos, sys->positions[ac.index0]);

            // Apply correction with relaxation and mass ratios
            float stiffness = sys->stiffness; // Adjust this (0-1) to tune stability
            float w1 = sys->masses[ac.index0];
            float w2 = sys->masses[ac.index1];
            float w_sum = w1 + w2;

            if (w_sum > 1e-6) {
                sys->positions[ac.index0] = fn_addVec3(sys->positions[ac.index0],
                                                            fn_multVec3s(correction, stiffness * w1 / w_sum));
                sys->positions[ac.index1] = fn_subVec3(sys->positions[ac.index1],
                                                                fn_multVec3s(correction, stiffness * w2 / w_sum));
            }
        }

    }
}

void th_simulateSimpleRagdoll(th_SimpleRagdoll* ragdoll)
{
    for (int iter = 0; iter < 5; iter++) {
        solve_distance_constraints_simple(ragdoll);
        solve_angle_constraints_simple(ragdoll);
    }

}
