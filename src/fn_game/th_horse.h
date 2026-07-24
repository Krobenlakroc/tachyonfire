#pragma once
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_gpu.h"
#include "../fn_engine/th_liquidgen.h"


struct th_LevelState;
typedef struct th_LevelState th_LevelState;

typedef struct
{
  fn_vec3 nodes[3];
}th_LegNodes;

typedef struct
{
  fn_quat leg_orientations[12];//6 legs times 2 bones
  fn_vec3 leg_offsets[12];
  fn_quat orientation;
  fn_vec3 orientation_direction;
  fn_vec3 facing;
  fn_vec3 footpositions[6];
  bool set_up;
  fn_vec3 footnormals[6];
  bool footcontacted[6];

  //foot information
  bool previously_free[6];
  bool stepping[6];
  bool step_linear[6];
  bool previously_stepping[6];
  fn_vec3 target_positions[6];
  float t[6];

  //legs
  th_LegNodes leg_history[6];

  //body target
//  fn_vec3 target_position;
  bool fully_extended;
  float timer_nfextended;
  float timer_move;

  float angle_adjust;
  float pitch_adjust;
  float height_adjust;

  //driver logic
  //gravity in driver direction
  //move "up"
  bool driven;
  fn_vec3 driver_normal;

  //spline driven
  fn_vec3* path;
  fn_vec3* ups;
  int path_count;

  float interp;
  int cprog;
  fn_vec3 old_up;
  fn_vec3 old_right;
  fn_vec3 old_forward;

  //sounds
  a_VirtualSource* spawn_sound;

  a_VirtualSource* mech1;
  a_VirtualSource* mech2;
  int mechindex;

  a_VirtualSource* impact_sounds[6];

  bool hasgem[6];
  float gemhealth[6];

  float gem_jitter_t[6]; //time
  fn_vec3 gem_jitter_x[6]; //direction of oscillation

  bool gibbed;
  th_Entity horse_body_gib;//one for the body
  th_Entity horse_leg_gib[6*2][2];//6 legs, 2 bones, 2 colliders
  //enforce a distance constraint afetr simulating the two independant bodies

  th_timer_t spawn_children_timer;
  int spawn_children_flipflop;

  float fadeout_body;
  fn_mat4 old_body;

  float fadeout_legs[6*2];
  fn_mat4 old_legs[6*2];

  fn_vec3* course_data;
  int course_count;


  float fadeout_spawn;
  float spawn_when;
  bool spawn_init;
  bool spawn_finished;
  bool spawn_stalled;

  th_timer_t stun_timer;
  fn_vec3 stun_direction;

  bool easymode;

  bool setposition;//used for spawn validity checking
  bool played_spawnsound;
  int unstall_count; //stupid hack to make spawning work, debounce the un-spawn signal n = 2
}th_HorseData;


typedef enum
{
  TH_HORSE_WIRE_START,
  TH_HORSE_WIRE_EXTEND,
  TH_HORSE_WIRE_RETRACT,
  TH_HORSE_WIRE_SWITCH //retract to 0, and go to extend
}th_HorseWireState;

typedef struct
{
  th_Entity* entities_gems;

  th_HorseData* data;

  th_LevelState* levelstate;
  th_Entity* entities;
  fn_mat4* transforms;
  fn_mat4* transforms_gems;
  int count;
  int gem_count;

  th_Entity* entities_legs;
  fn_mat4* transforms_legs_upper;
  fn_mat4* transforms_legs_lower;
  int leg_count_div2;
  int legs_per_count;

  int leg_count;

  th_FrustumCullData* frustum_data;
  int frustum_offset;
  int frustum_offset_legs;

  th_Allocator* alloc;

  fn_vec3 door_hinge;

  fn_mat4* hatch_transforms;
  float* hatch_interp;

  //wire plug connector
  th_Vertex* wire_verts;
  int wire_verts_count;

  int connector_count;
  fn_mat4* connector_mats;

  fn_mat4* wire_orients;
  fn_vec3* wirebase_orients_right;
  fn_vec3* wirebase_orients_up;

  float* wire_extens;
  th_HorseWireState* wire_states;

  //horses with the same spawn share the same umbilical
  int num_wire_bundles;
  int** wire_bundle_horse_ids;//list of horse ids to check per umbilical
  int* wire_bundle_horseid_counts;

  fn_vec3* wire_plug_origins;
  fn_vec3* wire_plug_normals;

  int* wire_bundle_target_idx;//keep track of which horse id the wire is "chasing"


  //fluid sim
  bool* simmed_fluid_once;
  th_LiquidHeights* fsim;
  th_Vertex* liquid_verts;
  int liquid_vert_count;

  th_timer_t* liquid_sim_timers;

  th_Allocator temp_alloc;//used for geometry generation
}th_HorseGroup;


void th_horseUpdate(th_HorseGroup* c,float dt);

void th_horseInitialize(th_Allocator* alloc,th_HorseGroup* c,int count,fn_vec3* positions,float* times,th_LevelState* levelstate);

void th_horseSetCourse(th_HorseGroup* c,int i,fn_vec3* target_points,int target_count);

void th_horseSetEasyMode(th_HorseGroup* c,int i);

void th_horseProcessCourse(th_HorseGroup* c,int i);

void th_horsePositionPlugs(th_HorseGroup* c);
