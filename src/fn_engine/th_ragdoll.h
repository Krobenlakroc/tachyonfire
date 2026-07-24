#pragma once
#include "th_physics.h"
#include "th_allocator.h"

typedef struct
{
  int index0;
  int index1;
  float d;
}th_RagdollConstraint;


typedef struct
{
  int parent_idx;
  int child_idx;
  int grandchild_idx;
  float min_angle;  // e.g., 0 degrees (straight)
  float max_angle;  // e.g., 160 degrees (slightly bent)
} th_AngleConstraint;

typedef struct
{
  th_Entity e;
  fn_vec3 prev_position;
  fn_vec3 target_position;
}th_RagdollBody;

typedef struct
{
  fn_vec3* upvectors;
  fn_quat* inverse_phys;

  th_RagdollBody* bodies;
  float* masses;


  fn_vec3* bones_position_original;

  fn_vec3* bones_position;
  fn_vec3* bones_scale;
  fn_quat* bones_rot;
  int count;

  th_RagdollConstraint* constraints;
  int constraint_count;

  th_World* world;

  th_AngleConstraint* angle_constraints;
  int angle_constraint_count;

  int* reference_bones;

  fn_vec3* constraint_axes;
  float* constraint_angles;
  int* parents;

}th_Ragdoll;

typedef struct
{
  int index0;
  int index1;
  float angle;
  fn_vec3 axis;
}th_SimpleAngularConstraint;

typedef struct
{
  fn_vec3* positions;
  float* masses;
  int num_positions;

  th_RagdollConstraint* constraints;
  int num_constraints;
  th_SimpleAngularConstraint* angle_constraints;
  int num_angle_constraints;

  float stiffness;
}th_SimpleRagdoll;

#define TH_DEFAULT_RAGDOLL (th_Ragdoll){.bodies = NULL,.masses = NULL,.bones_position = NULL,.bones_scale = NULL,.bones_rot = NULL,.count = 0}



void th_initRagdoll(th_World* world,th_Allocator* alloc,th_Ragdoll* ragdoll,fn_vec3* bones_p,fn_vec3* bones_s,fn_quat* bones_r,float* masses,int count, th_RagdollConstraint* constraints,int constraint_count,int* reference_bones);

th_RagdollConstraint th_createConstraint(int i0,int i1,float d);


void th_simulateRagdoll(th_Ragdoll* ragdoll,float dt);

void th_simulateSimpleRagdoll(th_SimpleRagdoll* ragdoll);
