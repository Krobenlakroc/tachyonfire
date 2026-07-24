#include "th_iqm.h"
#include "../fn_game/th_debugger.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "th_decal.h"
#include "th_occlusion.h"
#include "../th_fopen.h"

/*
Adapted from :https://github.com/exezin/exengine
*/

static char* readBytes(const char *path, size_t *len)
{

  FILE* fp = th_fopen(path,"rb");


  // get file length
  fseek(fp, 0L, SEEK_END);
  long int res = ftell(fp);
  if (res == -1)
  {
    printf("Error Reading Bytes!\n");
    return NULL;
  }
  fseek(fp, 0L, SEEK_SET);
  if (len)
    *len = res;

  // allocate space for file data
  char *buff = malloc(res+1);

  // read file contents into buffer
  fread(buff,1,res,fp);

  // null-terminate the buffer
  buff[res] = '\0';

  fclose(fp);

  return buff;
}


static  fn_mat4  rotate_quat(fn_vec4 q) {
  fn_mat4 m = fn_identityMat4();
  float qxx = q.x * q.x;
  float qyy = q.y * q.y;
  float qzz = q.z * q.z;
  float qxz = q.x * q.z;
  float qxy = q.x * q.y;
  float qyz = q.y * q.z;
  float qwx = q.w * q.x;
  float qwy = q.w * q.y;
  float qwz = q.w * q.z;

  float m00  = 1 - 2 * (qyy +  qzz);
  float m10  = 2 * (qxy + qwz);
  float m20  = 2 * (qxz - qwy);
  float m01  = 2 * (qxy - qwz);
  float m11  = 1 - 2 * (qxx +  qzz);
  float m21  = 2 * (qyz + qwx);
  float m02  = 2 * (qxz + qwy);
  float m12  = 2 * (qyz - qwx);
  float m22  = 1 - 2 * (qxx +  qyy);

  m.m[0] = m00;
  m.m[1] = m01;
  m.m[2] = m02;
  m.m[3] = 0;
  m.m[4] = m10;
  m.m[5] = m11;
  m.m[6] = m12;
  m.m[7] = 0;
  m.m[8] = m20;
  m.m[9] = m21;
  m.m[10] = m22;
  m.m[11] = 0;
  m.m[12] = 0;
  m.m[13] = 0;
  m.m[14] = 0;
  m.m[15] = 1;
  return m;
}

 fn_mat4 calcBoneMatrix(fn_vec3 pos, fn_vec4 rot, fn_vec3 scale)
{

 fn_mat4 mat;
//  fn_mat4 m;
//
// m = fn_identityMat4();

 fn_mat4 s = (fn_makescale(fn_createVec3(scale.x,scale.y,scale.z)));
 // m = (fn_multMat4(m,mat));

 fn_mat4 r = fn_transpose(rotate_quat(fn_createVec4(rot.x,rot.y,rot.z,rot.w)));
  // m = (fn_multMat4(m,mat));

 fn_mat4 t = (fn_maketranslate(fn_createVec3(pos.x,pos.y,pos.z)));
  // m = (fn_multMat4(m,mat));

  return fn_multMat4(fn_multMat4(r,s),t);

}

fn_quat fn_extractTwist(fn_quat q, fn_vec3 axis) {
  // Project quaternion onto the axis
  fn_vec3 qv = q.xyz;
  fn_vec3 projection = fn_multVec3s(axis, fn_dot(qv, axis));

  fn_quat twist = fn_createVec4(projection.x, projection.y, projection.z, q.w);
  return fn_normalizeVec4(twist);
}

// Calculate swing (perpendicular rotation) and twist (around axis)
void fn_swingTwistDecomposition(fn_quat q, fn_vec3 axis, fn_quat* swing, fn_quat* twist) {
  *twist = fn_extractTwist(q, axis);
  *swing = fn_multquat(q, fn_inverseq(*twist));
}

fn_mat4 th_orientBone(fn_vec3* positions,int i,int j, int k)
{
  fn_vec3 primary = fn_normalizeVec3(fn_subVec3(positions[j],positions[i]));
  fn_vec3 to_ref = fn_subVec3(positions[k], positions[i]);

  fn_vec3 secondary = fn_subVec3(to_ref,
                                 fn_multVec3s(primary, fn_dot(to_ref, primary)));

  fn_vec3 tertiary = fn_cross(primary, secondary);

  if (fn_length(tertiary) < 0.01)
  {
    tertiary = th_getDecalTangentVector(primary);
    secondary = fn_cross(tertiary,primary);
  }

  fn_vec3 b_x_basis = fn_normalizeVec3(secondary);
  fn_vec3 b_y_basis = fn_normalizeVec3(tertiary);
  fn_vec3 b_z_basis = fn_normalizeVec3(primary);
  fn_mat4 ret = fn_createMat4(b_x_basis.x,b_x_basis.y,b_x_basis.z,0,b_y_basis.x,b_y_basis.y,b_y_basis.z,0,b_z_basis.x,b_z_basis.y,b_z_basis.z,0,0,0,0,1);
  return ret;
}

void updateModelMatsRagdoll(th_Model *m, GLuint instance)
{
  fn_mat4* transform = m->transform_cache;//[m->bones_count];
  th_Ragdoll* ragdoll = &m->instances[instance].ragdoll_data;

  // First pass: build world transforms directly from ragdoll
  //printf("Model\n");
  for (uint i = 0; i < m->bones_count; i++) {

    th_Bone b = m->instances[instance].bones[i];

    if (ragdoll->masses[i] != 0.0)
    {
      fn_vec3 world_pos = ragdoll->bones_position[i];
      fn_quat world_rot = ragdoll->bones_rot[i];
      fn_vec3 world_scale = ragdoll->bones_scale[i];

      if (b.parent >= 0) {


        fn_vec3 parent_pos = ragdoll->bones_position[b.parent];
        fn_vec3 to_parent = fn_subVec3(parent_pos,world_pos );
        float bone_length = fn_length(to_parent);

        if (bone_length > 0.0001f) {
          fn_vec3 current_dir = fn_normalizeVec3(to_parent);

          fn_quat new_q = fn_mat4toquat(th_orientBone(ragdoll->bones_position,i,b.parent,ragdoll->reference_bones[i]));



          world_rot =  fn_multquat(fn_multquat( ragdoll->bones_rot[i], ragdoll->inverse_phys[i]) ,new_q);


        }

        if (ragdoll->constraint_angles[i] != -1.0)
        {
          th_PoseElement* pose = m->instances[instance].pose;
          fn_mat4 original_local  = calcBoneMatrix(pose[i].translate, pose[i].rotate, pose[i].scale);
          fn_mat4 transform_kine = fn_multMat4(original_local, transform[b.parent]);

          fn_vec3 kine_pos = fn_createVec3(transform_kine.m[12],transform_kine.m[13],transform_kine.m[14]);

          fn_vec3 parent_pos = ragdoll->bones_position[b.parent];

          ragdoll->constraint_axes[i] = fn_normalizeVec3(fn_subVec3(kine_pos,parent_pos));
        }
      }

      transform[i] = fn_translaterotatescaleq(world_pos,world_rot,world_scale);



    }
    else
    {
      // Kinematic bone
      if (b.parent >= 0) {
        // Get original local transform (offset from parent)

        th_PoseElement* pose = m->instances[instance].pose;
        fn_mat4 original_local  = calcBoneMatrix(pose[i].translate, pose[i].rotate, pose[i].scale);

        // Apply parent's current world transform
        transform[i] = fn_multMat4(original_local, transform[b.parent]);
      } else {
        // Root kinematic bone

        transform[i] = m->instances[instance].skeleton_world[i];
      }
    }


  }

  //compute final skeleton matrices
  for (uint i = 0; i < m->bones_count; i++) {
    fn_mat4 result = fn_multMat4(m->inverse_base[i], transform[i]);

    m->instances[instance].bones[i].transform = transform[i];
    m->instances[instance].skeleton[i] = result;

    //ragdoll->bones_position_original[i] = ragdoll->bones_position[i];
  }
}


void updateModelMats(th_Model *m,GLuint instance)
{
  fn_mat4* transform = m->transform_cache;
  th_PoseElement* pose = m->instances[instance].pose;

  for (uint i=0; i<m->bones_count; i++) {
    th_Bone b = m->instances[instance].bones[i];

    fn_mat4 mat, result;
    mat  = calcBoneMatrix(pose[i].translate, pose[i].rotate, pose[i].scale);
    result = fn_identityMat4();

    if (b.parent >= 0) {
      transform[i] = fn_multMat4(mat, transform[b.parent]);
      result = fn_multMat4( m->inverse_base[i], transform[i]);
    } else {
      transform[i] = mat;
      result = fn_multMat4(m->inverse_base[i], mat);
    }

    m->instances[instance].bones[i].transform =  transform[i];
    m->instances[instance].skeleton[i] =  result;
  }
}

void updateModelMatsWorld(th_Model *m,GLuint instance)
{
  fn_mat4* transform = m->transform_cache;
  th_PoseElement* pose = m->instances[instance].pose;

  for (uint i=0; i<m->bones_count; i++) {
    th_Bone b = m->instances[instance].bones[i];

    fn_mat4 mat, result;
    mat  = calcBoneMatrix(pose[i].translate, pose[i].rotate, pose[i].scale);
    result = fn_identityMat4();

    if (b.parent >= 0) {
      transform[i] = fn_multMat4(mat, transform[b.parent]);
      result = transform[i];//fn_multMat4( m->inverse_base[i], transform[i]);
    } else {
      transform[i] = mat;
      result = transform[i];//fn_multMat4(m->inverse_base[i], mat);
    }

    m->instances[instance].skeleton_world[i] =  result;
  }
}

fn_mat4 th_getSkeletonElement(th_Model *m,const char *id,GLuint instance)
{
  for (uint i=0; i<m->bones_count; i++) {
    th_Bone b = m->instances[instance].bones[i];
    if (strcmp(b.name,id) == 0)
    {
      return b.transform;
    }
  }

  return fn_identityMat4();
}

void th_modelMixPose(th_Model *m, th_PoseElement* a, th_PoseElement* b, float weight,GLuint instance)
{
  weight = fn_min(fn_max(weight, 0.0f), 1.0f);
  for (GLuint i=0; i<m->bones_count; i++) {
    fn_vec3 t;
    t = fn_lerpVec3( a[i].translate, b[i].translate, weight);

    fn_vec4 r;
    r = fn_slerpVec4(fn_normalizeVec4(a[i].rotate), fn_normalizeVec4(b[i].rotate), weight);
    r= fn_normalizeVec4(r);

    fn_vec3 s;
    s = fn_lerpVec3(a[i].scale, b[i].scale, weight);

    memcpy(&m->instances[instance].pose[i].translate,  &t, sizeof(fn_vec3));
    memcpy(&m->instances[instance].pose[i].rotate,     &r, sizeof(fn_vec4));
    memcpy(&m->instances[instance].pose[i].scale,      &s, sizeof(fn_vec3));
  }
}

static void th_modelMixPoseOutput(th_Model *m, th_PoseElement* a, th_PoseElement* b, float weight,th_PoseElement* output)
{
  weight = fn_min(fn_max(weight, 0.0f), 1.0f);
  for (GLuint i=0; i<m->bones_count; i++) {
    fn_vec3 t;
    t = fn_lerpVec3( a[i].translate, b[i].translate, weight);

    fn_vec4 r;
    r = fn_slerpVec4(fn_normalizeVec4(a[i].rotate), fn_normalizeVec4(b[i].rotate), weight);
    r= fn_normalizeVec4(r);

    fn_vec3 s;
    s = fn_lerpVec3(a[i].scale, b[i].scale, weight);

    memcpy(&output[i].translate,  &t, sizeof(fn_vec3));
    memcpy(&output[i].rotate,     &r, sizeof(fn_vec4));
    memcpy(&output[i].scale,      &s, sizeof(fn_vec3));
  }
}

void th_setAnim(th_Model *m,const char *id,GLuint instance)
{
  for (GLuint i = 0; i < m->anim_count; i++) {
    if(!strcmp(m->anims[i].name, id)) {
      m->instances[instance].current_anim = &m->anims[i];
      break;
    }
  }

  if(m->instances[instance].current_anim == NULL)
    return;

  m->instances[instance].current_time  = 0;
  m->instances[instance].current_frame = m->instances[instance].current_anim->first;
  m->instances[instance].anim_finished = 0;
}

void th_setAnimID(th_Model *m, int id,GLuint instance)
{
  m->instances[instance].current_anim = &m->anims[id];
  m->instances[instance].current_time  = 0;
  m->instances[instance].current_frame = m->instances[instance].current_anim->first;
  m->instances[instance].anim_finished = 0;
}


static void capturePose(th_Model *m, GLuint instance, th_PoseElement *out_pose)
{
  for (uint i = 0; i < m->bones_count; i++) {
    out_pose[i] = m->instances[instance].pose[i];
  }
}

// Set animation with smooth blending
void th_setAnimBlend(th_Model *m, const char *id, float blend_duration, GLuint instance)
{
  th_ModelInstance *inst = &m->instances[instance];

  // Find the target animation
  th_Animation *target_anim = NULL;
  for (GLuint i = 0; i < m->anim_count; i++) {
    if (!strcmp(m->anims[i].name, id)) {
      target_anim = &m->anims[i];
      break;
    }
  }

  if (target_anim == NULL)
    return;

  // If we're already on this animation and not blending, no need to blend
  if (inst->current_anim == target_anim && !inst->blend_state.is_blending) {
    return;
  }

  // Initialize blend state
  inst->blend_state.is_blending = true;
  inst->blend_state.source_anim = inst->current_anim;
  inst->blend_state.target_anim = target_anim;
  inst->blend_state.blend_time = 0.0f;
  inst->blend_state.blend_duration = blend_duration;

  // Capture current pose state
  inst->blend_state.source_time = inst->current_time;
  inst->blend_state.source_frame = inst->current_frame;
  inst->blend_state.source_next_frame = inst->next_frame;
  inst->blend_state.source_position = inst->position;

  // Allocate and capture the current pose
  if (inst->blend_state.source_pose == NULL) {
    inst->blend_state.source_pose = th_alloc(m->alloc, sizeof(th_PoseElement) * m->bones_count);
  }
  capturePose(m, instance, inst->blend_state.source_pose);

  // Start the target animation from the beginning
  inst->current_anim = target_anim;
  inst->current_time = 0;
  inst->current_frame = target_anim->first;
  inst->anim_finished = 0;
}

// Set animation by ID with smooth blending
void th_setAnimIDBlend(th_Model *m, int id, float blend_duration, GLuint instance)
{
  th_ModelInstance *inst = &m->instances[instance];
  th_Animation *target_anim = &m->anims[id];

  // If we're already on this animation and not blending, no need to blend
  if (inst->current_anim == target_anim && !inst->blend_state.is_blending) {
    return;
  }

  // Initialize blend state
  inst->blend_state.is_blending = true;
  inst->blend_state.source_anim = inst->current_anim;
  inst->blend_state.target_anim = target_anim;
  inst->blend_state.blend_time = 0.0f;
  inst->blend_state.blend_duration = blend_duration;

  // Capture current pose state
  inst->blend_state.source_time = inst->current_time;
  inst->blend_state.source_frame = inst->current_frame;
  inst->blend_state.source_next_frame = inst->next_frame;
  inst->blend_state.source_position = inst->position;

  // Allocate and capture the current pose
  if (inst->blend_state.source_pose == NULL) {
    inst->blend_state.source_pose = th_alloc(m->alloc, sizeof(th_PoseElement) * m->bones_count);
  }
  capturePose(m, instance, inst->blend_state.source_pose);

  // Start the target animation from the beginning
  inst->current_anim = target_anim;
  inst->current_time = 0;
  inst->current_frame = target_anim->first;
  inst->anim_finished = 0;
}

int th_getSkeletonID(th_Model *m,const char *id,GLuint idx)
{
  th_ModelInstance* instance = &m->instances[idx];
  for (GLuint i = 0 ; i < m->bones_count;i++)
  {
    if (strcmp(instance->bones[i].name,id) == 0)
    {
      return i;
    }
  }
  printf("NOT FOUND %s\n",id);
  return 0;
}

bool name_in_list(const char* name,const char** list,size_t elems)
{
  for (size_t ex = 0; ex < elems;ex++)
  {
    if (strcmp(name,list[ex]) == 0 )
    {
      return true;
    }
  }
  return false;
}

void th_pushModelOccluders(th_Model* m,int idx,float radius,fn_mat4 a)
{
  updateModelMatsWorld(m,idx);

  th_ModelInstance* instance = &m->instances[idx];
  for (GLuint j = 0 ; j < m->bones_count;j++)
  {

    fn_vec3 pos = fn_createVec3(instance->skeleton_world[j].m[12],instance->skeleton_world[j].m[13],instance->skeleton_world[j].m[14]);



    pos = fn_transformVec3(pos,a);

    th_pushOccluderFrame(fn_createVec4Vec3(pos,radius));

  }
}

void th_updateModelInstance(th_Model *m, float delta_time,th_World* world,int idx)
{

  // const char* included_core[] = {"bip01","pelvis","SPINNER","chest","spine2","neck","rthigh","lthigh","rclavicle","lclavicle","rupperArm","lupperArm"
  // };

  // const char* included_core[] = {"mixamorig:Hips","mixamorig:Spine","mixamorig:Spine1","chest","mixamorig:Spine2","mixamorig:Neck","mixamorig:mixamorig:RightUpLeg","mixamorig:LeftUpLeg","mixamorig:RightShoulder","mixamorig:LeftShoulder","mixamorig:RightArm","mixamorig:LeftArm"
  // };

  // const char* included_core[] = {"ixamorig:Hips",  "mixamorig:Spine",  "mixamorig:LeftUpLeg",  "mixamorig:RightUpLeg", "mixamorig:Spine1", "mixamorig:LeftLeg", "mixamorig:RightLeg", "mixamorig:Spine2", "mixamorig:LeftFoot",  "mixamorig:RightFoot",  "mixamorig:Neck",  "mixamorig:LeftShoulder",  "mixamorig:RightShoulder", "mixamorig:LeftToeBase", "mixamorig:RightToeBase", "mixamorig:Head",  "mixamorig:LeftArm",  "mixamorig:RightArm",  "mixamorig:LeftForeArm",  "mixamorig:RightForeArm",  "mixamorig:LeftHand", "mixamorig:RightHand"
  // };

  const char* included_core[] = {"ixamorig:Hips",  "mixamorig:Spine",  "mixamorig:LeftUpLeg",  "mixamorig:RightUpLeg", "mixamorig:Spine1", "mixamorig:Spine2",  "mixamorig:Neck",  "mixamorig:LeftShoulder",  "mixamorig:RightShoulder",
  };

  int i = idx;
  th_ModelInstance* instance = &m->instances[i];

  if (instance->ragdoll)
  {
    if (!instance->ragdoll_setpose)
    {
      //decompose matrix
      fn_mat4 a = instance->ragdoll_mat;


      updateModelMats(m,i);
      updateModelMatsWorld(m,i);

      fn_vec3* bonepositions = th_alloc(m->alloc,sizeof(fn_vec3)*m->bones_count);
      fn_quat* bonerots = th_alloc(m->alloc,sizeof(fn_quat)*m->bones_count);
      fn_vec3* bonescales = th_alloc(m->alloc,sizeof(fn_vec3)*m->bones_count);
      th_RagdollConstraint* constraints = th_alloc(m->alloc,sizeof(th_RagdollConstraint)*2048);
      int constraint_count = 0;


      float* masses = th_alloc(m->alloc,sizeof(float)*m->bones_count);


      int* reference_bones = th_alloc(m->alloc, sizeof(int) * m->bones_count);
      for (GLuint j=0; j<m->bones_count; j++) {
        reference_bones[j] = th_getSkeletonID(m, "mixamorig:Hips", 0);

        instance->skeleton_world[j] = fn_multMat4(instance->skeleton_world[j],a);
        instance->skeleton[j] = fn_multMat4(instance->skeleton[j],a);//fn_makescale(fn_createVec3s(0));//

        th_Bone b = instance->bones[j];



        if (strcmp(b.name, "mixamorig:Hips") == 0) {
          reference_bones[j] = th_getSkeletonID(m, "mixamorig:RightUpLeg", 0);
        }


        if (strcmp(b.name, "mixamorig:Spine") == 0) {
          reference_bones[j] = th_getSkeletonID(m, "mixamorig:RightUpLeg", 0);
        }

        if (strcmp(b.name, "mixamorig:Spine1") == 0) {
          reference_bones[j] = th_getSkeletonID(m, "mixamorig:RightUpLeg", 0);
        }

        if (strcmp(b.name, "mixamorig:Spine2") == 0) {
          reference_bones[j] = th_getSkeletonID(m, "mixamorig:RightUpLeg", 0);
        }

        if (strcmp(b.name, "mixamorig:RightUpLeg") == 0) {
          reference_bones[j] = th_getSkeletonID(m, "mixamorig:Spine", 0);
        }

        if (strcmp(b.name, "mixamorig:LeftUpLeg") == 0) {
          reference_bones[j] = th_getSkeletonID(m, "mixamorig:Spine", 0);
        }
/*
        if (strcmp(b.name, "bip01") == 0) {
          reference_bones[j] = th_getSkeletonID(m, "rthigh", 0);
        }*/

//"mixamorig:RightHand" , "mixamorig:LeftHand"
        const char* excluded[] = {"mixamorig:HeadTop_End" , "mixamorig:LeftToeBase", "mixamorig:RightToeBase", "mixamorig:RightToe_End", "mixamorig:LeftToe_End","mixamorig:Head"
        };



        masses[j] = 0.0;



        if (name_in_list(b.name,included_core,sizeof(included_core)/sizeof(included_core[0])))
        {
          masses[j] = 100;
        }
        else if (!name_in_list(b.name,excluded,sizeof(excluded)/sizeof(excluded[0])))
        {
          masses[j] = 25;
        }

        if (strcmp(b.name,"mixamorig:RightLeg") == 0)
        {
          masses[j] = 20;
        }
        if (strcmp(b.name,"mixamorig:RightFoot") == 0)
        {
          masses[j] = 5;
        }

        if (strcmp(b.name,"mixamorig:LeftLeg") == 0)
        {
          masses[j] = 20;
        }
        if (strcmp(b.name,"mixamorig:LeftFoot") == 0)
        {
          masses[j] = 5;
        }

        if (strcmp(b.name,"mixamorig:LeftArm") == 0)
        {
          masses[j] = 10;
        }
        if (strcmp(b.name,"mixamorig:LeftForeArm") == 0)
        {
          masses[j] = 5;
        }
        if (strcmp(b.name,"mixamorig:LeftHand") == 0)
        {
          masses[j] = 3;
        }

        if (strcmp(b.name,"mixamorig:RightArm") == 0)
        {
          masses[j] = 10;
        }
        if (strcmp(b.name,"mixamorig:RightForeArm") == 0)
        {
          masses[j] = 5;
        }
        if (strcmp(b.name,"mixamorig:RightHand") == 0)
        {
          masses[j] = 3;
        }

        // if (strcmp(b.name,"mixamorig:Head") == 0)
        // {
        //   masses[j] = 25;
        // }




         //"mixamorig:LeftArm",  "mixamorig:RightArm",  "mixamorig:LeftForeArm",  "mixamorig:RightForeArm",  "mixamorig:LeftHand", "mixamorig:RightHand"



        bonepositions[j] = fn_createVec3(instance->skeleton_world[j].m[12],instance->skeleton_world[j].m[13],instance->skeleton_world[j].m[14]);

        fn_mat4 c = instance->skeleton_world[j];

        fn_vec4 b_x_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(c.m[0],c.m[1],c.m[2]));
        fn_vec4 b_y_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(c.m[4],c.m[5],c.m[6]));
        fn_vec4 b_z_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(c.m[8],c.m[9],c.m[10]));
        fn_vec3 s_b = fn_createVec3(b_x_basis_mag.w,b_y_basis_mag.w,b_z_basis_mag.w);
        fn_vec3 b_x_basis = b_x_basis_mag.xyz;
        fn_vec3 b_y_basis = b_y_basis_mag.xyz;
        fn_vec3 b_z_basis = b_z_basis_mag.xyz;
        fn_quat qb = fn_mat4toquat(fn_createMat4(b_x_basis.x,b_x_basis.y,b_x_basis.z,0,b_y_basis.x,b_y_basis.y,b_y_basis.z,0,b_z_basis.x,b_z_basis.y,b_z_basis.z,0,0,0,0,1));

        bonerots[j] = qb;
        bonescales[j] = s_b;


        if (b.parent >= 0)
        {
          if (masses[j] != 0.0 && masses[b.parent] != 0.0)
          {
            constraints[constraint_count] = th_createConstraint(j,b.parent,fn_distance(bonepositions[j],bonepositions[b.parent]));

            constraint_count++;

          }
        }
      }

      #define CREATE_BONE_CONSTRAINTS(arr) \
      do { \
        for (GLuint j=0; j<m->bones_count; j++) { \
          th_Bone ba = instance->bones[j]; \
          if (masses[j] != 0 && name_in_list(ba.name, arr, sizeof(arr)/sizeof(arr[0]))) \
          { \
            for (GLuint k=0; k<j; k++) { \
              th_Bone bb = instance->bones[k]; \
              if (masses[k] != 0 && j!= k && name_in_list(bb.name, arr, sizeof(arr)/sizeof(arr[0]))) \
              { \
                constraints[constraint_count] = th_createConstraint(j,k,fn_distance(bonepositions[j],bonepositions[k])); \
                constraint_count++; \
              } \
            } \
          } \
        } \
      } while(0)

      CREATE_BONE_CONSTRAINTS(included_core);

      // const char* included_top[] = {"chest","spine2","neck","rclavicle","lclavicle","rupperArm","lupperArm"
      // };

      // CREATE_BONE_CONSTRAINTS(included_top);


      th_initRagdoll(world,m->alloc,&instance->ragdoll_data,bonepositions,bonescales,bonerots,masses,m->bones_count,constraints,constraint_count,reference_bones);
      // th_getDefaultDebugger()->points_default = bonepositions;
      // th_getDefaultDebugger()->point_count_default = m->bones_count;

      th_Ragdoll* ragdoll = &instance->ragdoll_data;

      //ragdoll->constraint_angles[th_getSkeletonID(m, "rupperArm", 0)] = 9;
      // ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:Neck", 0)] = 2;
      //
      // ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:RightForeArm", 0)] = 9;
      // ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:RightHand", 0)] = 9;
      //
      // // ragdoll->constraint_angles[th_getSkeletonID(m, "lupperArm", 0)] = 9;
      // ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftForeArm", 0)] = 9;
      // ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftHand", 0)] = 9;
      //
      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:RightLeg", 0)] = 20;
      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:RightFoot", 0)] = 9;

      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftLeg", 0)] = 20;
      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftFoot", 0)] = 9;

      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftArm", 0)] = 20;
      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftForeArm", 0)] = 15;
      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftHand", 0)] = 9;

      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:RightArm", 0)] = 20;
      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:RightForeArm", 0)] = 15;
      ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:RightHand", 0)] = 9;

      //ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:Head", 0)] = 1;



      // if (strcmp(b.name,"mixamorig:RightArm") == 0)
      // {
      //   masses[j] = 10;
      // }
      // if (strcmp(b.name,"mixamorig:RightForeArm") == 0)
      // {
      //   masses[j] = 7;
      // }
      // if (strcmp(b.name,"mixamorig:RightHand") == 0)
      // {
      //   masses[j] = 2;
      // }
      //
      // ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftLeg", 0)] = 9;
      // ragdoll->constraint_angles[th_getSkeletonID(m, "mixamorig:LeftFoot", 0)] = 9;



      for (GLuint j=0; j<m->bones_count; j++)
      {
        th_Bone b = instance->bones[j];
        if (b.parent >= 0)
        {
          fn_quat new_q = fn_mat4toquat(th_orientBone(bonepositions,j,b.parent,reference_bones[j]));
          ragdoll->inverse_phys[j] = fn_inverseq(new_q);
          ragdoll->parents[j] = b.parent;
        }
        else
        {
          //fn_quat new_q = fn_mat4toquat(th_orientBone(bonepositions,j,b.parent,reference_bones[j]));
          ragdoll->inverse_phys[j] = fn_createVec4(0,0,0,1);//fn_inverseq(new_q);

        }
        ragdoll->parents[j] = b.parent;


      }

      updateModelMatsRagdoll(m,i);


      for (GLuint k = 0 ; k < m->bones_count;k++)
      {
        instance->ragdoll_data.bodies[k].e.velocity = instance->ragdoll_velocity;
      }

      //ragdoll->bodies[th_getSkeletonID(m, "chest", 0)].e.velocity = fn_addVec3(instance->ragdoll_velocity_center,instance->ragdoll_velocity);
      ragdoll->bodies[th_getSkeletonID(m, "mixamorig:Neck", 0)].e.velocity = fn_addVec3(instance->ragdoll_velocity_center,instance->ragdoll_velocity);
      ragdoll->bodies[th_getSkeletonID(m, "mixamorig:Spine2", 0)].e.velocity = fn_addVec3(instance->ragdoll_velocity_center,instance->ragdoll_velocity);
      ragdoll->bodies[th_getSkeletonID(m, "mixamorig:Spine1", 0)].e.velocity = fn_addVec3(instance->ragdoll_velocity_center,instance->ragdoll_velocity);


      instance->ragdoll_setpose = true;
    }
    else
    {


      th_simulateRagdoll(&instance->ragdoll_data,delta_time*1000);
      updateModelMatsRagdoll(m,i);

      // for (GLuint j=0; j<m->bones_count; j++) {
      //   instance->skeleton[j] = fn_makescale(fn_createVec3s(0));//
      // }
    }
    return;
  }


  // handle animations
  th_Animation *anim = instance->current_anim;

  if (anim == NULL)
    return;

  // get current frame
  uint32_t current_frame = instance->current_time * anim->rate;
  uint32_t len = anim->last + anim->first;
  float position = instance->current_time * anim->rate;

  if (current_frame > len && !anim->loop)
    return;

  // increase frame time
  instance->current_time += delta_time*instance->timescale;
  instance->current_frame = anim->first + current_frame;
  uint32_t next_frame = instance->current_frame+1;

  // check frame bounds
  if (instance->current_frame >= len) {
    if (anim->loop) {
      //instance->current_time -= len / anim->rate;
      instance->current_time  = 0;
      instance->current_frame = anim->first;// + instance->current_time * anim->rate;
      instance->anim_finished = 1;
    } else {
      instance->current_frame = anim->last;
      instance->anim_finished = 1;

      next_frame = anim->last;
    }
  }

  if (next_frame >= len) {
    next_frame = anim->first;
    instance->anim_finished = 1;
  }

  instance->next_frame = next_frame;
  instance->position = position - (float)floor(position);


  if (instance->blend_state.is_blending) {
    instance->blend_state.blend_time += delta_time * instance->timescale;
    float blend_alpha = instance->blend_state.blend_time / instance->blend_state.blend_duration;

    if (blend_alpha >= 1.0f) {
      // Blending complete
      blend_alpha = 1.0f;
      instance->blend_state.is_blending = false;
    }

    // Clamp blend alpha
    blend_alpha = fn_min(fn_max(blend_alpha, 0.0f), 1.0f);

    // Get the target animation pose (current animation is already set to target)
    th_PoseElement *target_pose_a = m->frames[instance->current_frame];
    th_PoseElement *target_pose_b = m->frames[next_frame];

    // Create temporary storage for interpolated target pose
    if (instance->blend_state.target_pose == NULL)
    {
      instance->blend_state.target_pose = malloc(sizeof(th_PoseElement) * m->bones_count);
    }
    th_PoseElement *target_pose = instance->blend_state.target_pose;
    th_modelMixPoseOutput(m, target_pose_a, target_pose_b, instance->position, target_pose);

    // Blend from source pose to target pose
    th_modelMixPose(m, instance->blend_state.source_pose, target_pose, blend_alpha, i);

  } else {

  // update skeleton matrices
  th_modelMixPose(m, m->frames[instance->current_frame], m->frames[next_frame], position - (float)floor(position),i);

  }

  updateModelMats(m,i);
}


void th_updateModel(th_Model *m, float delta_time,th_World* world)
{



  for (GLuint i = 0; i < m->instanceCount;i++)
  {
    th_updateModelInstance(m,delta_time,world,i);

  }

}

void th_captureModel(th_Model *m,float* current_times,uint32_t* current_frames,uint32_t* next_frames,char* is_ragdoll,fn_vec3* vel_center,fn_vec3* vel)
{
  for (GLuint i = 0; i < m->instanceCount;i++)
  {

    th_ModelInstance* instance = &m->instances[i];
    // handle animations
    th_Animation *anim = instance->current_anim;

    // get current frame
    uint32_t current_frame = instance->current_time * anim->rate;
    uint32_t len = anim->last + anim->first;
    float position = instance->current_time * anim->rate;

    current_frames[i] = instance->current_frame;

    current_times[i] = instance->position;


    next_frames[i] = instance->next_frame;

    is_ragdoll[i] = instance->ragdoll;

    vel_center[i] = instance->ragdoll_velocity_center;

    vel[i] = instance->ragdoll_velocity;
  }
}

void th_updateModelPlayback(th_Model *m, float* current_time,uint32_t* current_frame,uint32_t* next_frame,float* current_time_b,uint32_t* current_frame_b,uint32_t* next_frame_b,float f,char* is_ragdoll,fn_vec3* vel_center,fn_vec3* vel,float delta_time,th_World* world)
{
  for (GLuint i = 0; i < m->instanceCount;i++)
  {

    if (is_ragdoll[i])
    {
      th_ModelInstance* instance = &m->instances[i];
      instance->ragdoll = true;
      instance->ragdoll_velocity_center = vel_center[i];
      instance->ragdoll_velocity = vel[i];
      //fn_printVec3(instance->ragdoll_velocity_center);
      th_updateModelInstance(m,delta_time,world,i);
    }
    else
    {
      th_ModelInstance* instance = &m->instances[i];


      th_PoseElement* frame_a = malloc(sizeof(th_PoseElement)*m->bones_count);
      th_PoseElement* frame_b = malloc(sizeof(th_PoseElement)*m->bones_count);

      th_modelMixPoseOutput(m, m->frames[current_frame[i]], m->frames[next_frame[i]], current_time[i],frame_a);

      th_modelMixPoseOutput(m, m->frames[current_frame_b[i]], m->frames[next_frame_b[i]], current_time_b[i],frame_b);


      th_modelMixPose(m, frame_a, frame_b, f,i);
      // update skeleton matrices
      free(frame_a);
      free(frame_b);

      updateModelMats(m,i);
    }


  }
}

void th_setModelPose(th_Model *m, th_PoseElement* frame,GLuint instance)
{
  for (uint i=0; i<m->bones_count; i++) {
    th_PoseElement f = frame[i];

    fn_vec4 rotate;
    rotate  = f.rotate;//memcpy(&rotate, &f.rotate, sizeof(fn_vec4));
    rotate = fn_normalizeVec4(rotate);

    m->instances[instance].pose[i].translate  = f.translate;
    m->instances[instance].pose[i].rotate     = rotate    ;
    m->instances[instance].pose[i].scale      = f.scale   ;
  }

  updateModelMats(m,instance);
}

static void* duplicate(th_Allocator* alloc,void* data,int count,size_t type)
{
  void* new_mem = th_alloc(alloc,type*count);
  memcpy(new_mem,data,type*count);
  return new_mem;
}

void th_setTimeScale(th_Model *m, float ts,GLuint instance)
{
  m->instances[instance].timescale = ts;
}

void th_resetModelRagdoll(th_Model *m,int idx)
{
  th_ModelInstance* instance = &m->instances[idx];
  instance->ragdoll = false;
  instance->ragdoll_mat = fn_identityMat4();
  instance->ragdoll_setpose = false;
  instance->ragdoll_data = TH_DEFAULT_RAGDOLL;
  instance->ragdoll_velocity = fn_createVec3s(0);
  instance->ragdoll_velocity_center = fn_createVec3s(0);

  // Reset blend state
  instance->blend_state.is_blending = false;
  instance->blend_state.source_anim = NULL;
  instance->blend_state.target_anim = NULL;
  instance->blend_state.blend_time = 0.0f;
  instance->blend_state.blend_duration = 0.0f;
  instance->blend_state.source_pose = NULL;
  instance->blend_state.target_pose = NULL;
}

th_Model th_loadIQM(th_Allocator* alloc,const char *path, uint8_t flags,GLuint instances,fn_vec3 escale)
{

  //fn_vec3 escale = fn_createVec3s(1.5);
  // read in the file data
  uint8_t *data = (uint8_t*)readBytes(path,NULL);
  if (data == NULL) {
    printf("Failed to load IQM model file %s\n", path);
  //  return NULL;
  }

  // the header contents
  th_IQMHeader header;

  // check magic string and version
  memcpy(header.magic, data, 16);
  // uint *head = (uint *)&data[16];
  // header.version = head[0];

  uint version;
  memcpy(&version, &data[16], sizeof(uint));
  header.version = version;

  if (strcmp(header.magic, IQM_MAGIC) != 0 || header.version != IQM_VERSION) {
    printf("Loaded IQM model version is not 2.0\nFailed loading %s\n", path);
    free(data);
  //  return NULL;
  }

  // get the rest of the header weeeeee
  memcpy(&header, data, sizeof(th_IQMHeader));

  // th_IQMMesh *meshes = (th_IQMMesh *)&data[header.ofs_meshes];
  th_IQMMesh *meshes = malloc(sizeof(th_IQMMesh) * header.num_meshes);
  memcpy(meshes, &data[header.ofs_meshes], sizeof(th_IQMMesh) * header.num_meshes);


  const char *file_text = header.ofs_text ? (char *)&data[header.ofs_text] : "";

  // set the vertices
  th_AnimVertex *vertices  = th_alloc(alloc,sizeof(th_AnimVertex)*(header.num_vertexes));
  fn_vec4* temp_tangents = malloc(sizeof(fn_vec4)*(header.num_vertexes));
  //float *position, *uv, *normal, *tangent;
  uint8_t *blend_indexes, *blend_weights;
  // th_IQMVertexArray *vas = (th_IQMVertexArray *)&data[header.ofs_vertexarrays];
  th_IQMVertexArray *vas = malloc(sizeof(th_IQMVertexArray) * header.num_vertexarrays);
  memcpy(vas, &data[header.ofs_vertexarrays], sizeof(th_IQMVertexArray) * header.num_vertexarrays);


  for (uint i=0; i<header.num_vertexarrays; i++) {
    th_IQMVertexArray va = vas[i];

    switch (va.type) {
      case IQM_POSITION: {
        //position = (float *)&data[va.offset];
        for (uint x=0; x<header.num_vertexes; x++)
        {
          memcpy(&vertices[x].position, &data[va.offset + x*va.size*sizeof(float)], va.size*sizeof(float));
          vertices[x].position = fn_multVec3(vertices[x].position,escale);
        }

        break;
      }
      case IQM_TEXCOORD: {
        //uv = (float *)&data[va.offset];
        for (uint x=0; x<header.num_vertexes; x++)
          memcpy(&vertices[x].texCoord, &data[va.offset + x*va.size*sizeof(float)], va.size*sizeof(float));
        break;
      }
      case IQM_NORMAL: {
        //normal = (float *)&data[va.offset];
        for (uint x=0; x<header.num_vertexes; x++)
          memcpy(&vertices[x].normal, &data[va.offset + x*va.size*sizeof(float)], va.size*sizeof(float));
        break;
      }
      case IQM_TANGENT: {
        //tangent = (float *)&data[va.offset];
        for (uint x=0; x<header.num_vertexes; x++)
          memcpy(&temp_tangents[x], &data[va.offset + x*va.size*sizeof(float)], va.size*sizeof(float));
        break;
      }
      case IQM_BLENDINDEXES: {
        blend_indexes = (uint8_t *)&data[va.offset];
        for (uint x=0; x<header.num_vertexes; x++)
          memcpy(&vertices[x].boneindex, &blend_indexes[x*va.size], va.size*sizeof(uint8_t));
        break;
      }
      case IQM_BLENDWEIGHTS: {
        blend_weights = (uint8_t *)&data[va.offset];
        for (uint x=0; x<header.num_vertexes; x++)
          memcpy(&vertices[x].boneweights, &blend_weights[x*va.size], va.size*sizeof(uint8_t));
        break;
      }
      case IQM_COLOR: {
        //unused
      }
    }
  }

  for (uint x = 0; x <header.num_vertexes;x++)
  {
    memcpy(&vertices[x].tangent, &temp_tangents[x].xyz, 3*sizeof(float));
    memcpy(&vertices[x].bitangent, &temp_tangents[x].xyz, 3*sizeof(float));
  }
  free(temp_tangents);

  // bones and joints
  th_Bone *bones      = NULL;
  th_PoseElement* bind_pose  = NULL;
  th_PoseElement* pose       = NULL;
  // th_IQMJoint *joints  = (th_IQMJoint *)&data[header.ofs_joints];

  th_IQMJoint *joints = malloc(sizeof(th_IQMJoint) * header.num_joints);
  memcpy(joints, &data[header.ofs_joints], sizeof(th_IQMJoint) * header.num_joints);

  if (header.ofs_joints > 0) {
    bones     = th_alloc(alloc,sizeof(th_Bone)*header.num_joints);
    bind_pose = th_alloc(alloc,sizeof(th_PoseElement)*header.num_joints);
    pose      = th_alloc(alloc,sizeof(th_PoseElement)*header.num_joints);
    printf("%i Bones: ",header.num_joints);
    for (uint i=0; i<header.num_joints; i++) {
      th_IQMJoint *j   = &joints[i];
      strncpy(bones[i].name, &file_text[j->name], 64);
      printf("%s %i ", bones[i].name,i);
      bones[i].parent = j->parent;
      memcpy(&bones[i].position, j->translate, sizeof(fn_vec3));
      memcpy(&bones[i].rotation, j->rotate,    sizeof(fn_vec4));
      memcpy(&bones[i].scale,    j->scale,     sizeof(fn_vec3));
      memcpy(&bind_pose[i].translate,  j->translate, sizeof(fn_vec3));
      memcpy(&bind_pose[i].rotate,     j->rotate,    sizeof(fn_vec4));
      memcpy(&bind_pose[i].scale,      j->scale,     sizeof(fn_vec3));

      bones[i].position = fn_multVec3(bones[i].position,escale);
    }

    // for (uint i=0; i<header.num_joints; i++) {
    //   if (bones[i].parent < 0 || (uint32_t)bones[i].parent > header.num_joints )
    //   {
    //     printf("%s Parent (%s)", bones[i].name,"noparent");
    //   }
    //   else
    //   {
    //     printf("%s Parent (%s)", bones[i].name,bones[bones[i].parent].name);
    //   }
    //
    //
    // }
    printf("\n");
  }

  // anims
  th_Animation *anims = NULL;
  // th_IQMAnim *animdata = (th_IQMAnim *)&data[header.ofs_anims];


  th_IQMAnim *animdata = malloc(sizeof(th_IQMAnim) * header.num_anims);
  memcpy(animdata, &data[header.ofs_anims], sizeof(th_IQMAnim) * header.num_anims);

  printf("Anims: ");
  if (header.ofs_anims > 0) {
    anims = th_alloc(alloc,sizeof(th_Animation)*header.num_anims);
    for (uint i=0; i<header.num_anims; i++) {
      th_IQMAnim *a    = &animdata[i];

      /* get anim name */
      uint32_t ofs_name = a->name;
      const char *name = &file_text[ofs_name];
      uint8_t len = strlen(name);
      anims[i].name   = th_alloc(alloc,sizeof(char) * (len+1));

      strcpy(anims[i].name, name);
      anims[i].name[len] = '\0';
      printf("%s ",name );
      anims[i].first  = a->first_frame;
      anims[i].last   = a->num_frames;
      anims[i].rate   = a->framerate;
      anims[i].loop   = 1;//a->flags || (1<<0);
      anims[i].id = i;
    }
  }
  printf("\n");

  // poses
  uint16_t *framedata = NULL;
  th_PoseElement** frames = NULL;
  // th_IQMPose *posedata = (th_IQMPose *)&data[header.ofs_poses];
  th_IQMPose *posedata = malloc(sizeof(th_IQMPose) * header.num_poses);
  memcpy(posedata, &data[header.ofs_poses], sizeof(th_IQMPose) * header.num_poses);

  if (header.ofs_poses > 0) {
    frames = th_alloc(alloc,sizeof(th_PoseElement*)*header.num_frames);
    //framedata = (unsigned short *)&data[header.ofs_frames];
    size_t framedata_offset = header.ofs_frames;

    for (uint i=0; i<header.num_frames; i++) {
      th_PoseElement *frame = th_alloc(alloc,header.num_poses*sizeof(th_PoseElement));
      for (uint p=0; p<header.num_poses; p++) {
        th_IQMPose *pose = &posedata[p];

        float v[10];
        for (int o=0; o<10; o++) {
          float val = pose->channeloffset[o];
          uint mask = (1 << o);
          if ((pose->channelmask & mask) > 0) {

            uint16_t frame_val;
            memcpy(&frame_val, &data[framedata_offset], sizeof(uint16_t));

            val += frame_val * pose->channelscale[o];
            framedata_offset += sizeof(uint16_t);
            //framedata++;
          }
          v[o] = val;
        }

        frame[p].translate = fn_createVec3(v[0],v[1],v[2]);
        frame[p].rotate = fn_createVec4(v[3],v[4],v[5],v[6]);
        frame[p].scale = fn_createVec3(v[7],v[8],v[9]);
        frame[p].translate = fn_multVec3(frame[p].translate,escale);
      }
      frames[i] = frame;
    }
  }

  // indices
  GLuint *indices = th_alloc(alloc,sizeof(GLuint)*(header.num_triangles*3));
  memcpy(indices, &data[header.ofs_triangles], (header.num_triangles*3)*sizeof(GLuint));
  uint i, a;
  for (i=0; i<header.num_triangles*3; i+=3) {
    a = indices[i+0];
    indices[i+0] = indices[i+2];
    indices[i+2] = a;
  }

  // create the model
  th_Model model;
  model.instances = th_alloc(alloc,sizeof(th_ModelInstance)*instances);
  model.instanceCount = instances;
  model.anims       = anims;
  model.frames      = frames;
  model.bones_count   = header.num_joints;
  model.anim_count   = header.num_anims;
  model.frame_count  = header.num_frames;
  model.bind_pose   = bind_pose;

  model.transform_cache = th_alloc(alloc,model.bones_count*sizeof(fn_mat4));

  for (GLuint i = 0 ; i < instances;i++)
  {
    th_ModelInstance* instance = &model.instances[i];
    instance->anim_finished = 0;
    instance->bones       = duplicate(alloc,bones,model.bones_count,sizeof(th_Bone));
    instance->pose        = duplicate(alloc,pose,model.bones_count,sizeof(th_PoseElement));
    instance->skeleton     = NULL;
    instance->timescale = 1;
    // instance->current_anim = &model.anims[0];
    //
    // instance->current_time  = 0;
    // instance->current_frame = instance->current_anim->first;
    th_resetModelRagdoll(&model,i);
  }



  // calc inverse base pose
  model.inverse_base = NULL;


  for (uint i=0; i<header.num_joints; i++) {
    if (header.ofs_joints < 1)
      break;

    if (!i) {
      model.inverse_base = th_alloc(alloc,sizeof(fn_mat4)*header.num_joints);
      for (GLuint i = 0 ; i < instances;i++)
      {
        th_ModelInstance* instance = &model.instances[i];
        instance->skeleton = th_alloc(alloc,sizeof(fn_mat4)*header.num_joints);
        instance->skeleton_world = th_alloc(alloc,sizeof(fn_mat4)*header.num_joints);
      }

    }

    th_Bone b = bones[i];

    fn_mat4 mat, inv;
    mat = calcBoneMatrix( b.position, b.rotation, b.scale);
    inv = fn_inverse(mat);

    if (b.parent >= 0) {
      model.inverse_base[i] = fn_multMat4(model.inverse_base[b.parent], inv);
    } else {
      model.inverse_base[i] = inv;
    }


  }

  // if (i >= header.num_joints-1)
  for (GLuint i = 0 ; i < instances;i++)
  {
    updateModelMats(&model,i);
  }


  // add the meshes to the model
  printf("MESHES: %i\n",header.num_meshes );
  GLuint index_offset = 0;
  model.meshcount = header.num_meshes;
  model.meshes = th_alloc(alloc,sizeof(th_GpuData)*header.num_meshes);
  for (uint i=0; i<header.num_meshes; i++) {
    th_AnimVertex *vert = &vertices[meshes[i].first_vertex];
    GLuint *ind       = &indices[meshes[i].first_triangle*3];

    // negative offset indices
    GLuint offset = 0;
    for (uint k=0; k<meshes[i].num_triangles*3; k++) {
      ind[k] -= index_offset;

      if (ind[k] > offset)
        offset = ind[k];
    }
    index_offset += ++offset;

    // get material and texture names
    // char *tex_name = &file_text[meshes[i].material];
    // char *is_file = strpbrk(tex_name, ".");

    // create mesh
  //  ex_mesh_t *m = ex_mesh_new(vert, meshes[i].num_vertexes, ind, meshes[i].num_triangles*3, 0);
  model.meshes[i].animverts = vert;
  model.meshes[i].vertcount = meshes[i].num_vertexes;
  model.meshes[i].indices = ind;
  model.meshes[i].indicecount = meshes[i].num_triangles*3;


  }


  // cleanup data
  printf("Finished loading IQM model %s\n", path);
  // free(vertices);
  // free(indices);
  free(vas);
  free(meshes);
  free(joints);
  free(animdata);
  free(posedata);
  free(data);


  model.alloc = alloc;

  return model;

}

void th_transformModel(th_Model* m,fn_mat4 tr)
{
  for (GLuint i = 0 ; i < m->meshcount;i++)
  {
    for (GLuint j = 0 ; j < m->meshes[i].vertcount;j++)
    {
      m->meshes[i].animverts[j].position = fn_transformVec3(m->meshes[i].animverts[j].position,tr);
      m->meshes[i].animverts[j].normal = fn_transformNormal(m->meshes[i].animverts[j].normal,tr);
      m->meshes[i].animverts[j].tangent = fn_transformNormal(m->meshes[i].animverts[j].tangent,tr);
      m->meshes[i].animverts[j].bitangent = fn_transformNormal(m->meshes[i].animverts[j].bitangent,tr);
    }
  }

  for (GLuint i = 0 ; i < m->frame_count;i++)
  {
    for (GLuint j= 0 ; j < m->bones_count;j++)
    {
      m->frames[i][j].translate = fn_transformVec3(m->frames[i][j].translate,tr);
    }
  }


}
