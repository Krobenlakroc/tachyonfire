#pragma once
#include "th_gpu.h"
#include "r_mesh.h"
#include "r_texture.h"
#include "th_harmonics.h"
#include "th_cubemap.h"
#include "th_iqm.h"
#include "th_physics.h"
#include "../fn_game/th_boids.h"
#include "../fn_game/th_shambler.h"
#include "../fn_game/th_plasma.h"
#include "../fn_game/th_weapon.h"
#include "../fn_game/th_hammer.h"
#include "../fn_game/th_centipede.h"
#include "../fn_game/th_debugger.h"
#include "../fn_game/th_shotgun.h"
#include "../fn_game/th_gems.h"
#include "../fn_game/th_player.h"
#include "../fn_game/th_eyeball.h"
#include "../fn_game/th_rocket.h"
#include "../fn_game/th_horse.h"
#include "../fn_game/th_tricolumn.h"
#include "../fn_game/th_gibcollection.h"
#include "../fn_game/th_builtins.h"
#include "../fn_game/th_tutorial.h"
#include "../fn_game/th_lifesphere.h"
#include "../fn_game/th_question.h"

// #include "../fn_game/th_interlink.h"
#include "th_light.h"

#include "th_spawnset.h"
#include "th_allocator.h"

#define TH_MATERIAL_PROPERTIES_DIV4 32

typedef enum
{
  TH_BLANK_MAT_FLAG = 0,
  TH_DYNAMIC_MAT_FLAG = 1, //bit 0
  TH_GLOW_MAT_FLAG = 2, // bit 1
  TH_IRIDESCENT_MAT_FLAG = 4, //bit 2
  TH_BARREL_GLOW_FLAG = 8 // bit 3
}th_MaterialFeatureFlag;

typedef enum
{
  TH_TUTORIAL_BAR,
  TH_BAR_ONE,
  TH_BAR_TWO,
  TH_BAR_THREE,
  TH_BAR_FOUR
}th_LevelProgress;


typedef struct
{
  int offset;
  int model_id;
  fn_mat4** mats;
  int* matcount;
  int stride;//controls how many times a matrix is reused (default would be 1)
  /*
  Example: stride of 2, stride of 1
 0, 0
 0, 1
 1, 2
 1, 3
  */


  /*
   * Dynamic geometry streaming
   */
  th_Vertex** backing_buffer;//source of the verts
  int num_verts;//verts to stream

  int elements_model;//how many verts in 1 model of the command, assumes identical #verts in several models, w/ different materials
  int num_models;//how many models
}th_RenderCommand;

#define TH_DEFAULT_RENDERCOMMAND (th_RenderCommand){.offset = 0,.model_id =0,.mats = NULL,.matcount = NULL,.stride = 0,.backing_buffer = NULL,.num_verts = 0,.verts_model = 0,.num_models = 1}

typedef struct
{
  int cpu_render_command_id;//our render command (used for copying matrices to dynamic memory)
  int gpu_render_command_id;//gpu render command array index
  int aabbs_start;//associated aabbs
  int aabbs_end;
  th_ObjectClass obj_class;
}th_FrustumCullCommand;

typedef struct
{
  char* string;
  fn_vec2 pos_tx;
  float size;
  float font;
  fn_vec3 color;
}th_TextCommand;

typedef struct th_LevelState
{
  //game state
  th_BoidGroup* boidgroups;
  th_ShamblerGroup* shamblers;
  th_PlasmaObject* plasma;
  th_BrassObject* brass;
  th_BrassObject* shotbrass;
  th_CentipedeGroup* centipede;
  th_Debugger* debugger_centi;
  th_ShotgunObject* shotgun;
  th_PlayerObject* player;
  th_EyeballGroup* eyeball;
  th_RocketObject* rocket;
  th_HorseGroup* horse;
  th_HammerObject* hammer;
  th_GemObject* gems;
  th_Weapon* weapon;
  th_Debugger* debugger_collision;
  th_TricolumnGroup* tricolumn;
  th_EyeballGroup* eyeball_super;
  th_CentipedeGroup* centipede_super;
  th_GibCollection* boid_gibs;
  th_TutorialObject* tutorial;
  th_LifeSphere* lifesphere;
  th_Question* question;
  th_CentipedeGroup* centipede_fast;

  //player physics
  th_Entity player_e;

  //physics world
  th_World* world;

  //Animated Models
  th_Model* animated_models;//included because these function as "render entities"
  int animated_meshes;//how many meshes (models can have multipel meshes) have been added
  int animated_models_count;

  //
  th_LightQuery* general_light_query;

  th_timer_t level_start_time;

  int num_spawners_total;
  int num_spawners_current;
  int num_spawners_killed;

  int num_enemies_highwater;
  int num_enemies_current;

  //for the glowing barrel
  float barrel_color_interp;
  fn_vec3 blackbody_barrel_pointa;
  fn_vec3 blackbody_barrel_pointb;
  float barrel_shape_min;
  float barrel_shape_max;
  float barrel_maxtemp;


  th_LevelProgress progress_state;
}th_LevelState;



typedef enum
{
  A_IMPACT = 0,
  A_MACHIN = 1,
  A_BRASS = 5,
  A_ONEBYONE = 9,
  A_RIC = 10,
  A_SHOTG = 14
}th_AudioValues;


//descibes level layout
typedef struct
{
  th_Allocator allocator;

  //game objects
  union
  {
    th_LevelState levelstate;
    th_LevelState ls;//alternate name
  };


  //game state

  th_RenderCommand* render_commands;
  th_RenderCommand* anim_render_commands;
  int boidgroups_count,render_commands_count,anim_render_commands_count;
  th_PointLight* pointlights;
  int pointlightcount;

  fn_vec3 spawnpoint;
  fn_vec2 spawnangles;

  fn_vec3 player_pos_titlescreen;

  th_TextCommand* text_commands;
  int text_commands_count;
  //dynamic light querying
  th_LightQuery* light_query;


  //cubemap
  fn_vec3* cube_positions;
  fn_vec3* cube_mins;
  fn_vec3* cube_maxs;
  int cube_count;

  //shadows
  fn_vec3* shadow_positions;
  int shadow_count;

  //SH GI
  fn_vec3 gridpos;
  fn_vec3 griddims ;
  float gridsize ;
  char* harmonics_file;
  bool autoGrid;

  //Probe Occlusion
  fn_vec3 occ_gridpos;// = l->gridpos;
  fn_vec3 occ_griddims;// = fn_multVec3s(l->griddims,2.0);
  float occ_gridsize;// = l->gridsize*0.5;
  float* occ_bitmasks;

  //materials
  char** all_materials;
  fn_vec2* handles;
  int handles_count;

  //meshes
  th_GpuData* meshes;
  int* staticdynamic;
  int* tesselated;
  int meshes_count,staticdynamic_count,tesselated_count;
  int meshes_dynamic_count;
  int* dynamic_shadowcaster;
  int dynamic_shadowcaster_count;

  int* lights;
  int light_count;
  int lights_reserved;


  //frustum_culling
  th_FrustumCullData* culldata;
  th_FrustumCullCommand* cull_commands;
  int cull_commands_count;

  //sunlight
  fn_vec3 sundir_shadow;
  fn_vec3 sundir_light;
  fn_vec3 suncolor_light;
  fn_mat4 orthomat_sunlight;
  fn_vec3 center_sunlight;

  //sky
  bool night_sky;
  fn_vec3 sundir_sky;

  //cubelights
  fn_vec3* omni_light_positions;
  fn_vec3* omni_light_atlascoords;
  int* omni_light_masks; //bitmask for which sides of the omni light are dynamically updated (6 bits used)
  int omni_light_count;

  //fog
  fn_vec3 fog_color;
  float fog_gain;
  float fog_density;
  float fog_ambient_density;
  float mu;
  float alpha;

  //atmosphere
  fn_vec3 atm_rayleigh;
  float atm_sun_intensity;
  fn_vec3 atm_sun_color;

  //spawnset
  int spawnset_entries;
  th_SpawnsetPair* spawnset;

  //victory standards
  th_VictoryStats brass_standard;
  th_VictoryStats silver_standard;
  th_VictoryStats gold_standard;

  float skyboost;

  const char* trackname;

  float horizon_height;
  float cloud_enable;

  int* material_properties;

  float shadow_radius_fog;
  float shadow_radius;

  float sun_shadow_scale; //use more of the atlas for the sun, default is 1, max is 4

  float omni_shadow_scale;
  float omni_light_jitter;
  float cube_nohit_sky;


  //streaming
  int render_commands_streamed_count;
  th_RenderCommand* render_commands_streamed;

  int streamed_meshes;
  th_GpuData* streamed;
}th_LevelDescriptor;

typedef enum
{
  TH_NONCOLLIDING = 1,
  TH_CUBEMAPMESH = 2,
  TH_COLLIDER = 4,
  TH_USE_NONCONVEX = 8,
}th_ObjProperties;

fn_mat4* th_make_matrices(fn_mat4 m,int count);
fn_vec2* th_make_handles(fn_vec2  m,int count);

fn_mat4* th_make_matrices_arena(th_Allocator* alloc,fn_mat4 m,int count);
fn_vec2* th_make_handles_arena(th_Allocator* alloc,fn_vec2  m,int count);

void th_setrecacheMode(bool recache_override);


void th_loadLevel(th_LevelDescriptor* l,bool gencubemaps,bool sharm,GLuint* cubemapDepth,GLuint* cubemapColor,const char* levelname,const char* spawnsetname,fn_vec3 scale,bool respawn,int fog_quality);
