#include "r_shader.h"
#include "th_gpu.h"
#include "r_mesh.h"
#include <stdlib.h>
#include "../fn_window.h"
#include "r_texture.h"
#include "../fn_input.h"
#include "th_time.h"
#include "th_harmonics.h"
#include "th_cubemap.h"
// #define STB_IMAGE_IMPLEMENTATION
// #include "stb_image.h"
#include <math.h>
#include <pthread.h>
#include "th_iqm.h"
#include "th_physics.h"
#include <time.h>

#include <limits.h>

#include "th_clusters.h"
#include "th_level.h"
#include "../fn_game/th_boids.h"
#include "../fn_game/th_builtins.h"
#include "th_decal.h"
#include "th_particle.h"

#include "bluenoise.h"

#include "th_audio.h"
#include "th_program.h"


#include "../fn_math/fn_spline.h"
#include "th_system.h"

#define PI 3.14159265359

#include "th_brdf.h"
#include "th_replay.h"
#include <malloc.h>
#include "th_occlusion.h"
#include "th_globals.h"

#include "icosahedra.h"
#include "th_hitmarker.h"
#include "../th_fopen.h"
//#include <mcheck.h>
// static void print_allocated(const char *tag) {
//   struct mallinfo2 mi = mallinfo2();
//   printf("[%s] malloced bytes (uordblks) = %zu\n", tag, (size_t)mi.uordblks);
// }



static const float TWO_PI = 2.0*3.14159265359;

#define TH_SKY_BOOST_LIGHTBAKE 5.0

static fn_vec3 last_good_pos = {0};
static fn_vec2 last_good_angles = {0};
static fn_vec3 last_good_look = {0};
static fn_vec3 last_good_up = {0};

static th_Character character_map[256];

static const int maxnumEnvboxes = TH_MAX_CUBEMAPS;

static const int maxmaterials_div4 = TH_MATERIAL_PROPERTIES_DIV4;//128/4

static int save_screenshot(char* filename, int w, int h)
{
 //This prevents the images getting padded
// when the width multiplied by 3 is not a multiple of 4
 glPixelStorei(GL_PACK_ALIGNMENT, 1);

 int nSize = w*h*3;
 // First let's create our buffer, 3 channels per Pixel
 char* dataBuffer = (char*)malloc(nSize*sizeof(char));

 if (!dataBuffer) return 0;

  // Let's fetch them from the backbuffer
  // We request the pixels in GL_BGR format, thanks to Berzeger for the tip
  glReadPixels((GLint)0, (GLint)0,
   (GLint)w, (GLint)h,
    GL_BGR, GL_UNSIGNED_BYTE, dataBuffer);

  //Now the file creation
  FILE *filePtr = th_fopen(filename, "wb");
  if (!filePtr) return 0;


  unsigned char TGAheader[12]={0,0,2,0,0,0,0,0,0,0,0,0};
  unsigned char header[6] = { w%256,w/256,
            h%256,h/256,
            24,0};
  // We write the headers
  fwrite(TGAheader,	sizeof(unsigned char),	12,	filePtr);
  fwrite(header,	sizeof(unsigned char),	6,	filePtr);
  // And finally our image data
  fwrite(dataBuffer,	sizeof(GLubyte),	nSize,	filePtr);
  fclose(filePtr);

  free(dataBuffer);

 return 1;
}



static fn_mat4 r_camera(fn_vec3 pos,fn_vec2 angles)
{

  angles.y = fn_clamp(angles.y,-fn_radians(90),fn_radians(90));
  fn_mat4 modelView = fn_identityMat4();//glm::lookAt(-getLocation(),getLook(),glm::vec3(0,-1,0));
  //modelView = glm::lookAt(getLocation(),getLook(),glm::vec3(0,1,0));
  //  modelView = fn_rotate(modelView, -glm::radians(camRoll), glm::vec3(0.0f, 0.0f, 1.0f));
  modelView = fn_rotate(modelView, angles.y, fn_createVec3(1.0f, 0.0f, 0.0f));
  modelView = fn_rotate(modelView, angles.x, fn_createVec3(0.0f, 1.0f, 0.0f));
//  modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
  modelView = fn_translate(modelView,fn_multVec3(pos,fn_createVec3(1,1,1)));//*glm::vec3(-1,1,-1));
  modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
  return modelView;
}

fn_vec3 r_getLook(fn_vec2 angles)
{

  fn_mat4 m = fn_rotate(fn_identityMat4(), -angles.x, fn_createVec3(0.0f, 1.0f, 0.0f));
  m = fn_rotate(m, -angles.y, fn_createVec3(1.0f, 0.0f, 0.0f));

  fn_vec4 v = fn_multVec4Mat4(m,fn_createVec4(0,0,1,0.0));
  return fn_createVec3(v.x,v.y,v.z);
}

fn_vec3 r_getLookVector(fn_vec2 angles,fn_vec3 l)
{

  fn_mat4 m = fn_rotate(fn_identityMat4(), -angles.x, fn_createVec3(0.0f, 1.0f, 0.0f));
  m = fn_rotate(m, -angles.y, fn_createVec3(1.0f, 0.0f, 0.0f));

  fn_vec4 v = fn_multVec4Mat4(m,fn_createVec4(l.x,l.y,l.z,0.0));
  return fn_createVec3(v.x,v.y,v.z);
}

void th_resume_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->th_resume_flag = true;
}
void th_restart_level_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->th_load_new_level = true;
  state->th_respawn_flag = true;
}


void th_quit_game_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->gameState = STATE_QUIT;
}

void th_options_menu_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->gameState = STATE_OPTIONS_MENU;
}

void th_levelselect_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->gameState = STATE_LEVELSELECT;
}

void th_mainmenu_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->gameState = STATE_MAINMENU;
  state->gameState_return = STATE_MAINMENU;

  if (state->bardrone_source != NULL)
  {
    a_stopVS(state->bardrone_source);
    state->bardrone_source = NULL;
  }

  a_stopAllSources();
}

void th_return_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->gameState = state->gameState_return;
}

void th_modifyFov(void* data,int new_fov)
{
  th_RendererState* state = (th_RendererState*)data;

  state->config_global->fov = new_fov;

  th_updateRenderMatrices(state);
  th_updateClustersFOV(state->invProj);
}

void th_modifySensitivity(void* data,float new_sens)
{
  th_RendererState* state = (th_RendererState*)data;

  state->config_global->mouseSens = new_sens;
}

void th_modifyExposure(void* data,float new_exposure)
{
  th_RendererState* state = (th_RendererState*)data;

  state->exposure_display = new_exposure;
}

void th_modifyGamma(void* data,float new_gamma)
{
  th_RendererState* state = (th_RendererState*)data;

  state->gamma_display = new_gamma;
}

void th_rebindKey(void* data,SDL_Scancode scode)
{
  int* bpoint = (int*)data;
  *bpoint = scode;
}

void th_makeKeybindWriteback(th_keybind_writeback* wb_ptr,void** data_ptr,int* bind_point)
{
  *wb_ptr = th_rebindKey;
  *data_ptr = (void*)bind_point;
}

void th_modifySfxVolume(void* data,float new_volgain)
{
  a_setGain(new_volgain);
}

void th_modifyMusicVolume(void* data,float new_volgain)
{
  a_setGainMusic(new_volgain);
}



void th_launch_level_callback(void* data)
{
  th_RendererState* state = (th_RendererState*)data;
  state->th_load_new_level = true;

  th_uiNagbar("Loading.....",fn_createVec2(0,32),1.0,1);
}

void th_writeVictory(th_VictoryManifest list_of_victory,th_LevelManifest list_of_levels,char* path)
{
  //write to file victory.txt

  FILE* fp = th_fopen(path,"w");
  if (fp == NULL)
  {
    th_uiNagbar("Could not write victory.txt",fn_createVec2(0,0),0.333,5000);
    return;
  }
  else
  {
    th_uiNagbar("Writing victory.txt",fn_createVec2(0,16),0.333,500);
  }

  int cpy_count = list_of_victory.count > list_of_levels.count ? list_of_levels.count : list_of_victory.count;
  //memcpy(state->levelSelectLayout.victory_state,state->list_of_victory.victorystates,cpy_count*sizeof(th_LevelVictoryState));
  for (int i = 0 ; i < cpy_count; i++)
  {
    th_LevelVictoryState s = list_of_victory.victorystates[i];
    fprintf(fp, "level_%i %d %d %d %d\n",i,s.level_speed,s.level_airtime,s.level_damagetaken,s.flawless);
  }





  fclose(fp);
}

void th_registerVictory(th_RendererState* state,fn_vec3 flt_vec)
{
  //update levelselect UI
  //state->levelSelectLayout.victory_state
  //state->levelSelectLayout.selected_level
  int level_speed = ((int)flt_vec.y) + (int)1;
  int level_airtime = ((int)flt_vec.z) + (int)1;
  int level_damagetaken = ((int)flt_vec.x) + (int)1;

  int is_flawless = level_speed == 4 && level_airtime == 4 && level_damagetaken == 4;


  if (level_speed > state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_speed)
  {
      state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_speed = level_speed;
  }
  if (state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_airtime < level_airtime)
  {
    state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_airtime = level_airtime;
  }
  if (state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_damagetaken < level_damagetaken)
  {
    state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_damagetaken = level_damagetaken;
  }

  if (state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].flawless < is_flawless)
  {
    state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].flawless = is_flawless;
  }

  int cpy_count = state->list_of_victory.count > state->list_of_levels.count ? state->list_of_levels.count : state->list_of_victory.count;
  memcpy(state->list_of_victory.victorystates,state->levelSelectLayout.victory_state,cpy_count*sizeof(th_LevelVictoryState));

  th_writeVictory(state->list_of_victory,state->list_of_levels,th_getPathVictory());
}


void th_bindGlobalTextures(th_RendererState* state)
{
  // r_bindTextureForce(noshading_fb.textures[0],GL_TEXTURE_2D,0);//texinfo, irradiance
  r_bindTextureForce(state->noshading_fb.textures[1],GL_TEXTURE_2D,1);//normalroughness
  r_bindTextureForce(state->noshading_fb.depthTexture,GL_TEXTURE_2D,2);//depthtex
  r_bindTextureForce(state->noshading_fb.textures[2],GL_TEXTURE_2D,3);//basis, cubemap
  r_bindTextureForce(state->brdfLUTTexture,GL_TEXTURE_2D,4);
  r_bindTextureForce(state->noshading_fb.textures[0],GL_TEXTURE_2D,5);//albedo-metallic
  r_bindTextureForce(state->lightTex,GL_TEXTURE_2D,20);
  r_bindTextureForce(state->cubemapDepth,GL_TEXTURE_CUBE_MAP_ARRAY,21);
  r_bindTextureForce(state->cubemapColor,GL_TEXTURE_CUBE_MAP_ARRAY,22);
  r_bindTextureForce(state->skybox,GL_TEXTURE_CUBE_MAP,23);
  r_bindTextureForce(state->shadowBuffer.depthTexture,GL_TEXTURE_2D,24);
//  r_bindTextureForce(state->fontTex,GL_TEXTURE_2D,25);
  //r_bindTextureForce(state->scale_temp_store,GL_TEXTURE_2D,26);
  r_bindTextureForce(state->cubepass_fb.textures[0],GL_TEXTURE_2D,29);
  glBindImageTexture(	0,state->cubepass_fb.textures[0],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
  glBindImageTexture(	6,state->cubepass_fb.textures[1],0,GL_FALSE,0,GL_WRITE_ONLY,GL_R16F);
  r_bindTextureForce(state->cubepass_fb.textures[1],GL_TEXTURE_2D,0);
  // glBindImageTexture(	1,deferred_texture_fb.textures[0],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA8);
  // glBindImageTexture(	2,deferred_texture_fb.textures[1],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA8);

  r_bindTextureForce(state->final_image_texture,GL_TEXTURE_2D,35);
  glBindImageTexture(	3,state->final_image_texture,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA8);

  r_bindTextureForce(state->reflections_fb.textures[0],GL_TEXTURE_2D,31);
  glBindImageTexture(	5,state->reflections_fb.textures[0],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);

  r_bindTextureForce(state->blurpass_fb.textures[0],GL_TEXTURE_2D,32);
  glBindImageTexture(	4,state->blurpass_fb.textures[0],0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);

  r_bindTextureForce(state->forward_fb.textures[0],GL_TEXTURE_2D,33);
  r_bindTextureForce(state->fontcolortex,GL_TEXTURE_2D_ARRAY,34);
  if (state->th_temporalfilter)
  {
    r_bindTextureForce(state->old_reflectiondata,GL_TEXTURE_2D,27);
    r_bindTextureForce(state->old_depthtexture,GL_TEXTURE_2D,28);
    r_bindTextureForce(state->oldnormalTex,GL_TEXTURE_2D,30);
  }
  if (state->th_fog)
  {
    r_bindTextureForce(state->fogpass_fb.textures[0],GL_TEXTURE_2D,36);
    r_bindTextureForce(state->blurfog_a.textures[0],GL_TEXTURE_2D,37);
    r_bindTextureForce(state->blurfog_b.textures[0],GL_TEXTURE_2D,38);
  }

  if (state->th_directional_occlusion)
  {
    r_bindTextureForce(state->occlusion_fb.textures[0],GL_TEXTURE_2D,39);
    r_bindTextureForce(state->noiseTex,GL_TEXTURE_2D,40);
  }
  else if (state->th_ambient_occlusion)
  {
    r_bindTextureForce(state->cheap_ao_fb.textures[0],GL_TEXTURE_2D,39);
  }
  #define OCC_3D_TEX 0

  #if OCC_3D_TEX
    // r_bindTextureForce(state->occ_tex,GL_TEXTURE_3D,41);
  #endif

  r_bindTextureForce(state->forward_fb.depthTexture,GL_TEXTURE_2D,41);//depthtex
}

char* create_define_string(int slices, int memory, const char* prefix) {
    // Format of the final string:
    // "#define TH_Z_SLICES <slices>\n#define TH_Z_MEMORY <memory>\n"

    const char* define_format = "#define TH_Z_SLICES %d\n#define TH_Z_MEMORY %d\n";
    int buffer_size = snprintf(NULL, 0, define_format, slices, memory) + 1;

    // Allocate space for the final string, including the prefix length.
    char* result = malloc(strlen(prefix) + buffer_size);
    if (result == NULL) {
        return NULL; // Memory allocation failed
    }

    // Combine the prefix and the formatted define string
    strcpy(result, prefix);
    sprintf(result + strlen(prefix), define_format, slices, memory);

    return result;
}

char* create_define_string_shadow(int slices, int memory,int samples, const char* prefix) {
  // Format of the final string:
  // "#define TH_Z_SLICES <slices>\n#define TH_Z_MEMORY <memory>\n"

  const char* define_format = "#define TH_Z_SLICES %d\n#define TH_Z_MEMORY %d\n#define TH_SHADOW_NSAMPLES %d\n";
  int buffer_size = snprintf(NULL, 0, define_format, slices, memory,samples) + 1;

  // Allocate space for the final string, including the prefix length.
  char* result = malloc(strlen(prefix) + buffer_size);
  if (result == NULL) {
    return NULL; // Memory allocation failed
  }

  // Combine the prefix and the formatted define string
  strcpy(result, prefix);
  sprintf(result + strlen(prefix), define_format, slices, memory,samples);

  return result;
}

char* create_define_string_reflections(int slices, int memory,float bias_ql,float bias, const char* prefix) {
  // Format of the final string:
  // "#define TH_Z_SLICES <slices>\n#define TH_Z_MEMORY <memory>\n"

  const char* define_format = "#define TH_Z_SLICES %d\n#define TH_Z_MEMORY %d\n#define LOD_BIAS_QUICKLOOKUP %f\n#define LOD_BIAS %f\n";
  int buffer_size = snprintf(NULL, 0, define_format, slices, memory,bias_ql,bias) + 1;

  // Allocate space for the final string, including the prefix length.
  char* result = malloc(strlen(prefix) + buffer_size);
  if (result == NULL) {
    return NULL; // Memory allocation failed
  }

  // Combine the prefix and the formatted define string
  strcpy(result, prefix);
  sprintf(result + strlen(prefix), define_format, slices, memory,bias_ql,bias);

  return result;
}

char* create_define_string_tiled(int tiled_div4,int uints_div4,int tiles_x, const char* prefix) {
  // Format of the final string:
  // "#define TH_Z_SLICES <slices>\n#define TH_Z_MEMORY <memory>\n"





  const char* define_format = "#define NUM_TILED_DIV4 %d\n#define NUM_UINTS_DIV4 %d\n#define NUM_TILES_X %d\n";
  int buffer_size = snprintf(NULL, 0, define_format, tiled_div4,uints_div4,tiles_x) + 1;

  // Allocate space for the final string, including the prefix length.
  char* result = malloc(strlen(prefix) + buffer_size);
  if (result == NULL) {
    return NULL; // Memory allocation failed
  }

  // Combine the prefix and the formatted define string
  strcpy(result, prefix);
  sprintf(result + strlen(prefix), define_format, tiled_div4,uints_div4,tiles_x);

  return result;
}

//TH_SHADOW_NSAMPLES


void th_initShaders(th_RendererState* state,bool usepostprocesss)
{
  //TODO CLUSTER_Z_VIRTUAL , CLUSTER_Z_MEMORY
  char* zslice_def = create_define_string(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,"#version 460 core\n");//"#version 460 core\n#define TH_Z_SLICES 15\n#define TH_Z_MEMORY 10\n";
  r_initShader(&state->noshading_anim,"th1/shaders/noshad_anim.vert","th1/shaders/noshading.frag","NOSHADINGANIM","");
  r_initShader(&state->occluder_anim,"th1/shaders/occ_anim.vert","th1/shaders/shadow_occluder.frag","OCCANIM","");
  r_initShader(&state->deferredtex,"th1/shaders/global_pass1.vert","th1/shaders/deferred_texture.frag","DTEX",zslice_def);
  //r_initShaderCompute(&state->deferredtex,"th1/shaders/deferred_texture.comp","DTEX","");
  // r_initShader(&occlusion_upsample,"th1/shaders/global_pass1.vert","th1/shaders/occlusionupsample.frag","USAMPLE","");
  // r_initShader(&occlusion_blur,"th1/shaders/global_pass1.vert","th1/shaders/occlusionblur.frag","BLUR","");
  // r_initShader(&directional_occlusion,"th1/shaders/global_pass1.vert","th1/shaders/directionalocc.frag","DOCC","");


  int shad_samples[] = {10,14,32};
  int n_shadow_samples = shad_samples[fn_clampi(state->config_global->shadow_quality,0,2)];

  char* finalargs;
  if (usepostprocesss)
  {
    //finalargs = "#version 460 core\n#define NOSPECULAR\n#define TH_Z_SLICES 15\n#define TH_Z_MEMORY 10\n";
    if (state->th_batch_render)
    {
      finalargs = create_define_string_shadow(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,n_shadow_samples,"#version 460 core\n#define NOSPECULAR\n#define BATCH_MODE\n");
    }
    else
    {
      finalargs = create_define_string_shadow(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,n_shadow_samples,"#version 460 core\n#define NOSPECULAR\n");
    }

  }
  else
  {
    if (state->th_fog)
    {
      //finalargs = "#version 460 core\n#define POSTPROCESS\n#define USE_FOG_POST\n#define TH_Z_SLICES 15\n#define TH_Z_MEMORY 10\n";
      finalargs = create_define_string_shadow(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,n_shadow_samples,"#version 460 core\n#define POSTPROCESS\n#define USE_FOG_POST\n");
    }
    else
    {
      //finalargs = "#version 460 core\n#define POSTPROCESS\n#define TH_Z_SLICES 15\n#define TH_Z_MEMORY 10\n";
      finalargs = create_define_string_shadow(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,n_shadow_samples,"#version 460 core\n#define POSTPROCESS\n");
    }

  }

  char* finalargs_particle;
  if (state->config_global->particle_lighting == 1)
  {
    finalargs_particle = create_define_string_shadow(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,10,"#version 460 core\n#define PARTICLE_LIGHTING\n");
  }
  else
  {
    finalargs_particle = create_define_string_shadow(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,10,"#version 460 core\n");
  }

  //#define


//  r_initShaderCompute(&state->pbr_deferred,"th1/shaders/state->pbr_deferred.comp","PBRDEF",finalargs);
  r_initShader(&state->pbr_deferred,"th1/shaders/global_pass1.vert","th1/shaders/pbr_deferred.frag","PBRDEF",finalargs);

  r_initShader(&state->fog_shader,"th1/shaders/global_pass1.vert","th1/shaders/fog.frag","FOG",finalargs);
  //r_initShader(&state->pbr_deferred,"th1/shaders/global_pass1.vert","th1/shaders/state->pbr_deferred.frag","PBRDEF",finalargs);
  //r_initShader(&ssrshader,"th1/shaders/global_pass1.vert","th1/shaders/ssr.frag","SSR","");
  r_initShader(&state->prefiltershader,"th1/shaders/global_pass1.vert","th1/shaders/prefilter.frag","PREFILTR","");

  // r_initShaderTess(&tess_shader_occluder,"th1/shaders/displacement_occluder.vert","th1/shaders/shadow_occluder.frag","th1/shaders/displacement_occluder.tess_control","th1/shaders/displacement_occluder.tess_eval","DISPL_OCC");
  //r_initShaderTess(&tess_shader_noshading,"th1/shaders/displacement.vert","th1/shaders/noshading.frag","th1/shaders/displacement.tess_control","th1/shaders/displacement.tess_eval","DISPL_NOSHAD");
  //r_initShaderTess(&tess_shader,"th1/shaders/displacement.vert","th1/shaders/pbr.frag","th1/shaders/displacement.tess_control","th1/shaders/displacement.tess_eval","DISPL");
  r_initShader(&state->shadowOccluder,"th1/shaders/shadow_occluder.vert","th1/shaders/shadow_occluder.frag","SHADOW","");


  r_initShader(&state->skyshader,"th1/shaders/global_pass1.vert","th1/shaders/sky.frag","SKY","");

  r_initShader(&state->night_skyshader,"th1/shaders/global_pass1.vert","th1/shaders/sky_night.frag","SKYNIGHT","");

  //r_initShader(&post,"th1/shaders/post.vert","th1/shaders/post.frag","POST","");
  float biases[] = {3.0,1.0,0.0};
  float biases_ql[] = {1.0,0.0,0.0};

  float bias = biases[fn_clampi(state->config_global->reflection_quality,0,2)];
  float bias_ql = biases_ql[fn_clampi(state->config_global->reflection_quality,0,2)];

  char* cubemap_def = create_define_string_reflections(CLUSTER_Z_VIRTUAL,CLUSTER_Z_COUNT,bias_ql,bias,"#version 460 core\n");
  r_initShaderCompute(&state->cubemaps,"th1/shaders/cubemap.comp","CUBEMAP",cubemap_def);

  // r_initShader(&state->cubemaps,"th1/shaders/global_pass1.vert","th1/shaders/cubemap.frag","CUBEMAP","");
  // r_initShader(&spharmonics,"th1/shaders/global_pass1.vert","th1/shaders/global_pass1.frag","PASS1","");
  // r_initShader(&spharmonics2,"th1/shaders/global_pass1.vert","th1/shaders/global_pass2.frag","PASS2","");
  r_initShader(&state->noshading,"th1/shaders/noshading.vert","th1/shaders/noshading.frag","NOSHADING","");
  //r_initShader(&pbr,"th1/shaders/pbr.vert","th1/shaders/pbr.frag","PBR","");
  //r_initShader(&decal_apply,"th1/shaders/global_pass1.vert","th1/shaders/decal_apply.frag","DECALS","");
  //r_initShader(&gaussian_blur,"th1/shaders/global_pass1.vert","th1/shaders/gaussian_blur.frag","BLUR","");
  r_initShaderCompute(&state->temporal_shader,"th1/shaders/temporal.comp","TEMPORAL","");
  r_initShaderCompute(&state->median_blur,"th1/shaders/median.comp","MEDIAN","");
  r_initShader(&state->forward_particle,"th1/shaders/forward_particle.vert","th1/shaders/particle.frag","PARTICLE",finalargs_particle);
  //r_initShader(&text_shader,"th1/shaders/global_pass1.vert","th1/shaders/text.frag","TEXTUI","");
  //r_initShader(&forward_light,"th1/shaders/light_source.vert","th1/shaders/light_source.frag","LIGHTSOURCE","");

  r_initShader(&state->glyph_shader,"th1/shaders/glyph.vert","th1/shaders/glyph.frag","GLYPH","");

  r_initShader(&state->image_shader,"th1/shaders/glyph.vert","th1/shaders/image.frag","IMAGE","");

  r_initShader(&state->image_shader_nofade,"th1/shaders/glyph.vert","th1/shaders/image_nofade.frag","IMAGE_NOFADE","");

  r_initShader(&state->progress_shader,"th1/shaders/glyph.vert","th1/shaders/progress.frag","PROGRESS","");


  r_initShader(&state->wavytext_shader,"th1/shaders/glyph_wavy.vert","th1/shaders/glyph_wavy.frag","GLYPH_WAVY","");

  r_initShader(&state->gradient_shader,"th1/shaders/glyph.vert","th1/shaders/gradient.frag","GRAD_SHADER","");

  const char* finalargs_a = "#version 460 core\n#define IMAGE_A_BLUR\n";
  r_initShader(&state->fog_blur_shader_a,"th1/shaders/global_pass1.vert","th1/shaders/fastgaussian.frag","FOGBLURA",finalargs_a);

  const char* finalargs_b = "#version 460 core\n";
  r_initShader(&state->fog_blur_shader_b,"th1/shaders/global_pass1.vert","th1/shaders/fastgaussian.frag","FOGBLURB",finalargs_b);


  r_initShader(&state->cheap_ao_shader,"th1/shaders/cheap_ao.vert","th1/shaders/cheap_ao.frag","CHEAP_AO","");

  char* tiled_shading_defs = create_define_string_tiled(state->num_tiled_div4,state->num_uints_div4,state->num_tiles_x,"#version 460 core\n");
  r_initShaderCompute(&state->tiled_depth_bound_shader,"th1/shaders/cheap_ao_tile_prepare.comp","AOTILEPREP",tiled_shading_defs);
  r_initShader(&state->tiled_raster_shader,"th1/shaders/cheap_ao_tile_insert.vert","th1/shaders/cheap_ao_tile_insert.frag","AOTILERASTER",tiled_shading_defs);
  r_initShader(&state->tiled_ao_shading,"th1/shaders/global_pass1.vert","th1/shaders/cheap_ao_tile_shade.frag","AOTILESHADE",tiled_shading_defs);
}



//one call rendering functions
void th_initRenderingOnce(th_RendererState* state,bool sharm,bool gencubemaps,int resolution,fn_Config* config,bool iter,fn_RawInput* input)
{
  //framebuffers, fonts

  if (sharm || gencubemaps)
  {
    state->th_dynamic_objs = false;
    state->th_particles = false;
    // state->th_dynamiclights = false;
    state->th_reflections = false;
    state->th_fog = false;
  }

  th_modifyExposure(state,config->exposure);
  th_modifyGamma(state,config->gamma);

  /*
  * FRAMEBUFFERS
  */

  state->screen_dims = fn_createVec2(config->width,config->height);
  printf("SCREEN DIMS SET %f %f\n",state->screen_dims.x,state->screen_dims.y);

  th_makeQuery(&state->timer,"RenderTime");
  // th_createFramebufferGeneral(&state->noshading_fb,config->width,config->height,TH_VEC4 | TH_VEC3 | TH_16BIT | TH_32BITRGB | TH_DEPTHBUFFER,2);
  // th_createFramebufferGeneral(&deferred_texture_fb,config->width,config->height,TH_VEC4 | TH_8BIT,2);
  // th_createFrameTextureGeneral(&state->cubepass_fb,config->width*state->reflections_scale,config->height*state->reflections_scale,TH_VEC4 | TH_32BIT ,1);
  // th_createFrameTextureGeneral(&state->blurpass_fb,config->width,config->height,TH_VEC4 | TH_16BIT ,1);
  // th_createFrameTextureGeneral(&state->reflections_fb,config->width,config->height,TH_VEC4 | TH_16BIT ,1);
  // th_createFramebufferGeneral(&state->forward_fb,config->width,config->height,TH_VEC4 | TH_16BIT | TH_DEPTHBUFFER ,1);

  if(state->th_batch_render)
  {
      th_createFramebufferGeneral(&state->noshading_fb,state->batch_atlas_res,state->batch_atlas_res,TH_VEC4 | TH_8BIT | TH_RGBA16F | TH_DEPTHBUFFER,3);
  }
  else
  {
      th_createFramebufferGeneral(&state->noshading_fb,config->width,config->height,TH_VEC4 | TH_8BIT | TH_RGBA16F | TH_DEPTHBUFFER,3);
  }

  // th_createFramebufferGeneral(&deferred_texture_fb,config->width,config->height,TH_VEC4 | TH_8BIT,2);
  th_createFrameTextureGeneral(&state->cubepass_fb,config->width*state->reflections_scale,config->height*state->reflections_scale,TH_VEC4 | TH_16BIT | TH_VEC1  ,2);
  th_createFrameTextureGeneral(&state->blurpass_fb,config->width,config->height,TH_VEC4 | TH_16BIT ,1);
  th_createFrameTextureGeneral(&state->reflections_fb,config->width,config->height,TH_VEC4 | TH_16BIT ,1);
  th_createFramebufferGeneral(&state->forward_fb,config->width,config->height,TH_VEC4 | TH_16BIT | TH_DEPTHBUFFER ,1);
  if (state->th_fog)
  {
    //printf("FOG DEBUG %f %f\n",config->width*state->fog_scale,config->height*state->fog_scale);
    th_createFramebufferGeneral(&state->fogpass_fb,config->width*state->fog_scale,config->height*state->fog_scale,TH_VEC4 | TH_16BIT | TH_FILTERED  ,1);
    th_createFramebufferGeneral(&state->blurfog_a,config->width,config->height,TH_VEC4 | TH_16BIT | TH_FILTERED ,1);
    th_createFramebufferGeneral(&state->blurfog_b,config->width,config->height,TH_VEC4 | TH_16BIT  ,1);
  }
  if (state->th_directional_occlusion)
  {
    th_createFramebufferGeneral(&state->occlusion_fb,config->width*state->fog_scale,config->height*state->fog_scale,TH_VEC4 | TH_16BIT   ,1);
  }

  th_createFramebufferGeneral(&state->cheap_ao_fb,config->width,config->height,TH_16BIT | TH_VEC1,1);

  if (config->borderless_fullscreen && config->upscale_mode != TH_RAWOUTPUT)
  {
    th_createFramebufferGeneral(&state->internal_fb,config->width,config->height,TH_8BIT | TH_VEC3,1);
  }


  //old
  //512

  //TODO
  //Merge Noshading and deferred texture and do decals in seperate pass DONE
  //new geom-normal buffer RG16 DONE
  //change cubepass to RGBA16 and R16 DONE
  //Change forwrd buffer to 8bit and to tonemapping in forward shader
  //Add in synflag bit
  //Decal Normal texturing

  //GBUFFER
  //RGBA8 - albedo metallic
  //RGBA8 - normal roughness AO
  //RG16 - geometry normal

  //RAYTRACING
  //RGBA16 accum & weight
  //R16 hit distance
  //RGBA16 temporal
  //RGBA16 median

  //
  //RGBA8 Forward tonemapped
  //336 total

  if (sharm || gencubemaps)
  {
    if (state->th_batch_render)
    {
      th_createFramebufferGeneral(&state->cubetarget_fb,state->batch_atlas_res,state->batch_atlas_res,TH_VEC3  | TH_16BIT | TH_DEPTHBUFFER,1);
    }
    else
    {
      th_createFramebufferGeneral(&state->cubetarget_fb,config->width,config->height,TH_VEC3  | TH_16BIT | TH_DEPTHBUFFER,1);
    }

  }

  glGenTextures(1, &state->final_image_texture);
  glBindTexture(GL_TEXTURE_2D, state->final_image_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, config->width, config->height, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);



  /*
  * Uniform Buffer Objects & SSBO
  */
  state->lightsUBO = th_createMappedUbo(sizeof(th_PointLight)*256 ,2,(void**)&state->mapped_pointlights);

  state->accessUBO = th_createMappedUbo(sizeof(GLuint)*4*4096 ,3,(void**)&state->mapped_acessubo);

  state->offsetsUBO = th_createMappedUbo(sizeof(th_uvec4)*((16*8*CLUSTER_Z_COUNT)) ,4,(void**)&state->mapped_offsetsubo);

  state->accesscubesUBO = th_createMappedUbo(sizeof(GLuint)*4*4096 ,5,(void**)&state->mapped_acessubocubes);

  state->boneindicesUBO = th_createMappedUbo(sizeof(GLuint)*4*4096 ,6,(void**)&state->mapped_boneindices);

  state->bonesUBO = th_createMappedSSBO(state->max_bones*sizeof(float)*4*4 ,1,(void**)&state->mapped_AnimUBO,state->bonesSyncs);

  state->offsetsDecalUBO = th_createMappedUbo(sizeof(th_uvec4)*((16*8*CLUSTER_Z_COUNT)) ,9,(void**)&state->mapped_offsetsDecal);

  state->accessDecalUBO = th_createMappedUbo(sizeof(GLuint)*4*4096 ,8,(void**)&state->mapped_accessDecal);

  state->decalsUBO= th_createMappedUbo(sizeof(th_Decal)*800 ,7,(void**)&state->mappedDecals);



  /*
  * SHADOW BUFFERs
  */
  int SHADOW_GRID = 4;

  th_createFramebufferDepthOnly(&state->shadowBuffer,state->th_shadowmap_resolution*SHADOW_GRID,state->th_shadowmap_resolution*SHADOW_GRID);


  state->shadowCasterPos = fn_createVec3(-43.755478, -264.998047 ,446.177795);
  //-56.692692 ,-224.180786 ,355.918701
  state->shadowCasterDir = fn_createVec3(0.068358 ,0.488178 ,-0.870063);
  state->shadowSampled = false;
  state->staticSampled = false;
  for (int i = 0 ; i < MAX_POINT_SHADOWS;i++){
    state->staticSampledPoint[i] = false;
  }



  // glGenTextures(1, &state->shadowCache_tex);
  // glBindTexture(GL_TEXTURE_2D, state->shadowCache_tex);
  // // glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, state->th_shadowmap_resolution*SHADOW_GRID, state->th_shadowmap_resolution*SHADOW_GRID, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
  //
  // glTexStorage2D(	GL_TEXTURE_2D,
  //                   1,
  //                 GL_DEPTH_COMPONENT24,
  //                 state->th_shadowmap_resolution*SHADOW_GRID,
  //                 state->th_shadowmap_resolution*SHADOW_GRID);
  //
  // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  // // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  // // float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  // // glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
  //
  // GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
  // glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  // float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
  // glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

  th_createFramebufferDepthOnly(&state->shadowBufferCache,state->th_shadowmap_resolution*SHADOW_GRID,state->th_shadowmap_resolution*SHADOW_GRID);
  state->shadowCache_tex = state->shadowBufferCache.depthTexture;


  /*
  * BRDF LUT
  */
  th_loadLUT(512,"th1/brdflut.float",&state->brdfLUTTexture);


  /*
   *  CHEAP AO BUFERS INIT
   */

  // int ao_volume_verts = 12;
  // int ao_volume_indices = 20*3;
  // int ao_volume_instances = 1024;
  //
  //
  // state->cheap_ao_data.verts = malloc(sizeof(th_OccluderVertex)*ao_volume_verts);
  // state->cheap_ao_data.indices = malloc(sizeof(GLuint)*ao_volume_indices);
  // state->cheap_ao_data.instances = malloc(sizeof(fn_vec4)*ao_volume_instances);
  //
  // for (int i = 0 ; i < 12;i++)
  // {
  //   state->cheap_ao_data.verts[i].position = fn_createVec3(ico_verts[i][0],ico_verts[i][1],ico_verts[i][2]);
  // }
  //
  // for (int i = 0 ; i < 20*3;i++)
  // {
  //   state->cheap_ao_data.indices[i] = ico_indices[i] - 1;
  // }
  //
  // for (int i = 0 ; i < ao_volume_instances;i++)
  // {
  //   float x = th_randomFloat(-1000,1000);
  //   float y = th_randomFloat(-1000,1000);
  //   float z = th_randomFloat(-1000,1000);
  //     state->cheap_ao_data.instances[i] = fn_createVec4(x,y,z,0);//6.705734, 11.881274, -4.092169
  // }
  //
  //
  // state->cheap_ao_data.vertcount = ao_volume_verts;
  // state->cheap_ao_data.instancecount = ao_volume_instances;
  // state->cheap_ao_data.indicecount = ao_volume_indices;
  //
  // th_createVboOccluder(&state->cheap_ao_array,state->cheap_ao_data,TH_NOCOMMAND | TH_NONORMALTANGENT);

  /*
   * CHEAP AO TILED BUFFERS
   */



  glGenVertexArrays(1, &state->tiled_dummy_vao);

  state->tiledaoUBO = th_createMappedUbo(sizeof(fn_vec4)*2048 ,10,(void**)&state->mapped_ao_volumes);

  int NUM_TILED_DIV4 = state->num_tiled_div4;
  int NUM_UINTS_DIV4 = state->num_uints_div4;

  state->tiled_data = th_createSSBO(sizeof(fn_vec4)*9*NUM_TILED_DIV4,2);
  state->tiled_light_lists = th_createSSBO(sizeof(fn_vec4)*NUM_UINTS_DIV4,3);







  /*
  * POST PROCESSING & TEXT GLYPH VBOS
  */
  int s_w = state->screen_dims.x;
  int s_h = state->screen_dims.y;
  if (sharm || gencubemaps)
  {
    s_w = resolution;
    s_h = resolution;
  }

  state->post_data.verts = malloc(sizeof(th_Vertex)*3);
  state->post_data.indices = malloc(sizeof(GLuint)*3);
  state->post_data.vertcount = 3;
  state->post_data.indicecount = 3;
  state->post_data.indices[0] = 0;
  state->post_data.indices[1] = 1;
  state->post_data.indices[2] = 2;
  // state->post_data.indices[3] = 1;
  // state->post_data.indices[4] = 2;
  // state->post_data.indices[5] = 3;
  //state->post_data.verts[0].position = fn_createVec3(s_w*2,s_h*2,0);
  state->post_data.verts[0].position = fn_createVec3(s_w*2,0,0);
  state->post_data.verts[1].position = fn_createVec3(0,0,0);
  state->post_data.verts[2].position = fn_createVec3(0,s_h*2,0);
//  state->post_data.verts[0].texCoord = fn_createVec2(1*2,0);
  state->post_data.verts[0].texCoord = fn_createVec2(1*2,0);
  state->post_data.verts[1].texCoord = fn_createVec2(0,0);
  state->post_data.verts[2].texCoord = fn_createVec2(0,1*2);
  //state->post_data.verts[0].normal = fn_createVec3(1,1,1);
  state->post_data.verts[0].normal = fn_createVec3(1,1,1);
  state->post_data.verts[1].normal = fn_createVec3(1,1,1);
  state->post_data.verts[2].normal = fn_createVec3(1,1,1);
  //state->post_data.verts[0].tangent = fn_createVec3(1,1,1);
  state->post_data.verts[0].tangent = fn_createVec3(1,1,1);
  state->post_data.verts[1].tangent = fn_createVec3(1,1,1);
  state->post_data.verts[2].tangent = fn_createVec3(1,1,1);
  //state->post_data.verts[0].bitangent = fn_createVec3(1,1,1);
  state->post_data.verts[0].bitangent = fn_createVec3(1,1,1);
  state->post_data.verts[1].bitangent = fn_createVec3(1,1,1);
  state->post_data.verts[2].bitangent = fn_createVec3(1,1,1);

  th_createVbo(&state->post_gpu,state->post_data,TH_NOINSTANCE | TH_NOCOMMAND | TH_NONORMALTANGENT);



  state->text_data.verts = malloc(sizeof(th_Vertex)*4);
  state->text_data.indices = malloc(sizeof(GLuint)*6);
  state->text_data.vertcount = 4;
  state->text_data.indicecount = 6;
  state->text_data.indices[0] = 0;
  state->text_data.indices[1] = 1;
  state->text_data.indices[2] = 3;
  state->text_data.indices[3] = 1;
  state->text_data.indices[4] = 2;
  state->text_data.indices[5] = 3;
  state->text_data.verts[0].position = fn_createVec3(s_w,s_h*0.5,0);
  state->text_data.verts[1].position = fn_createVec3(s_w,0,0);
  state->text_data.verts[2].position = fn_createVec3(0,0,0);
  state->text_data.verts[3].position = fn_createVec3(0,s_h*0.5,0);
  state->text_data.verts[0].texCoord = fn_createVec2(1,1);
  state->text_data.verts[1].texCoord = fn_createVec2(1,0);
  state->text_data.verts[2].texCoord = fn_createVec2(0,0);
  state->text_data.verts[3].texCoord = fn_createVec2(0,1);
  state->text_data.verts[0].normal = fn_createVec3(1,1,1);
  state->text_data.verts[1].normal = fn_createVec3(1,1,1);
  state->text_data.verts[2].normal = fn_createVec3(1,1,1);
  state->text_data.verts[3].normal = fn_createVec3(1,1,1);
  state->text_data.verts[0].tangent = fn_createVec3(1,1,1);
  state->text_data.verts[1].tangent = fn_createVec3(1,1,1);
  state->text_data.verts[2].tangent = fn_createVec3(1,1,1);
  state->text_data.verts[3].tangent = fn_createVec3(1,1,1);
  state->text_data.verts[0].bitangent = fn_createVec3(1,1,1);
  state->text_data.verts[1].bitangent = fn_createVec3(1,1,1);
  state->text_data.verts[2].bitangent = fn_createVec3(1,1,1);
  state->text_data.verts[3].bitangent = fn_createVec3(1,1,1);

  th_createVbo(&state->text_gpu,state->text_data,TH_NOINSTANCE | TH_NOCOMMAND | TH_NONORMALTANGENT);


  /*
  * DRAWCOMMAND MEMORY ALLOC
  */
  state->drawcommands_light = malloc(sizeof(r_DrawElementsIndirectCommand)*256);
  state->light_commands_count = 0;

  state->drawcommands_particle = malloc(sizeof(r_DrawElementsIndirectCommand)*256);
  state->particle_commands_count = 0;


  state->drawcommands = malloc(sizeof(r_DrawElementsIndirectCommand)*256);
  state->drawcommands_count = 0;
  state->drawcommands_dynamic_count = 0;

  state->drawcommands_tess = malloc(sizeof(r_DrawElementsIndirectCommand)*256);
  state->drawcommands_count_tess= 0;

  state->drawcommands_tessshadow_indices = malloc(sizeof(GLuint)*256);
  state->drawcommands_tessshadow = malloc(sizeof(r_DrawElementsIndirectCommand)*256);
  state->drawcommands_count_tessshadow= 0;

  state->drawcommands_dynshadow_indices = malloc(sizeof(GLuint)*256);
  state->drawcommands_dynshadow = malloc(sizeof(r_DrawElementsIndirectCommand)*256);
  state->drawcommands_count_dynamicshadowcaster= 0;


  /*
  *CAMERA INIT
  */
  state->projection = fn_perspective(fn_radians(config->fov),state->screen_dims.x/state->screen_dims.y,state->zNear,state->zFar);

  // th_createFramebuffer(&framebuffer,state->screen_dims.x,state->screen_dims.y);



  state->angles= fn_createVec2(0,0);
  state->pos = fn_createVec3(0,-500,0);
  state->postmatrix = fn_ortho(0,state->screen_dims.x,0,state->screen_dims.y);


  /*
  * SOBOL, NOISE, FONTS
  */
  //create sobol buffers

  GLuint sobolSSBO = th_createSSBO(sizeof(GLuint)*128*128*8 ,0);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, sobolSSBO);
  //  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(unsigned int)*256*256, sobol_256spp_256d);

  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(unsigned int)*128*128*8, scramblingTile);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  fn_vec3 noise[16];
  noise[0] = fn_createVec3(-0.638208,-0.364921,0);
  noise[1] = fn_createVec3(0.773981,0.304117,0);
  noise[2] = fn_createVec3(-0.69933,0.362692,0);
  noise[3] = fn_createVec3(-0.228371,-0.224549,0);
  noise[4] = fn_createVec3(-0.000517964,-0.704934,0);
  noise[5] = fn_createVec3(0.174373,0.691151,0);
  noise[6] = fn_createVec3(0.180217,0.910818,0);
  noise[7] = fn_createVec3(0.112292,-0.703697,0);
  noise[8] = fn_createVec3(0.96661,-0.182467,0);
  noise[9] = fn_createVec3(-0.71636,0.129797,0);
  noise[10] = fn_createVec3(-0.495747,-0.0229709,0);
  noise[11] = fn_createVec3(-0.0719389,0.92219,0);
  noise[12] = fn_createVec3(-0.747938,-0.600486,0);
  noise[13] = fn_createVec3(-0.361501,0.258538,0);
  noise[14] = fn_createVec3(-0.746576,0.302508,0);
  noise[15] = fn_createVec3(0.243268,0.606146,0);

  glGenTextures(1, &state->noiseTex);
  glBindTexture(GL_TEXTURE_2D, state->noiseTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &noise[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  th_renderFontTextures(character_map,config);

  // state->fontTex = fn_loadTexture("th1/textures/font3.png");
  // const char** tarr = malloc(sizeof(char*)*2);
  // tarr[0] = "th1/textures/fontgrad2.png";
  // tarr[1] = "th1/textures/fontgrad.png";
  // state->fontcolortex = fn_loadTextureArray(tarr,2);

  printf("%s\n","loaded tex" );
  // glGenTextures(1, &state->fontTex);
  // glBindTexture(GL_TEXTURE_2D, state->fontTex);
  // // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, &noise[0]);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


  /*
  *  TEMPORAL FILTER
  */
  if (state->th_temporalfilter)
  {
    float fbscale = 1;
    glGenTextures(1, &state->old_reflectiondata);
    glBindTexture(GL_TEXTURE_2D, state->old_reflectiondata);
    float* blank = malloc(sizeof(float)*state->screen_dims.x*fbscale*state->screen_dims.y*fbscale*4);
    for (int i = 0 ; i <state->screen_dims.x*fbscale*state->screen_dims.y*fbscale*4;i++)
    {
      blank[i] = 0;
    }
    GLint iformat =GL_RGBA16F;
    if (state->reflections_fb.flags & TH_32BIT)
    {
      iformat = GL_RGBA32F;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, iformat, state->screen_dims.x*fbscale, state->screen_dims.y*fbscale, 0, GL_RGBA, GL_FLOAT, blank);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor2[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor2);

    glGenTextures(1, &state->old_depthtexture);
    glBindTexture(GL_TEXTURE_2D, state->old_depthtexture);
    float* blank2 = malloc(sizeof(float)*state->screen_dims.x*state->screen_dims.y);
    for (int i = 0 ; i <state->screen_dims.x*state->screen_dims.y;i++)
    {
      blank2[i] = 0;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, state->screen_dims.x, state->screen_dims.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, blank2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor2);


    glGenTextures(1, &state->oldnormalTex);
    glBindTexture(GL_TEXTURE_2D, state->oldnormalTex);
    float* blank3 = malloc(sizeof(float)*state->screen_dims.x*state->screen_dims.y*4);
    for (int i = 0 ; i <state->screen_dims.x*state->screen_dims.y*4;i++)
    {
      blank3[i] = 0;
    }
    GLint iformat2 = GL_RGBA8;
    if (state->noshading_fb.flags & TH_16BIT)
    {
      iformat2 = GL_RGBA16F;
    }
    else if (state->noshading_fb.flags & TH_32BIT)
    {
      iformat2 = GL_RGBA32F;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, iformat2, state->screen_dims.x, state->screen_dims.y, 0, GL_RGBA, GL_FLOAT, blank3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor2);
  }

  /*
   * UI
   */

  th_MainMenuCallbacks main_callbacks;
  main_callbacks.quit_callback = th_quit_game_callback;
  main_callbacks.quit_callback_data = state;
  main_callbacks.options_callback = th_options_menu_callback;
  main_callbacks.options_callback_data = state;
  main_callbacks.levelselect_callback = th_levelselect_callback;
  main_callbacks.levelselect_callback_data = state;

  th_createMainMenu(&state->mainMenuLayout,character_map,state->screen_dims,main_callbacks);


  th_OptionsCallbacks options_callbacks;
  options_callbacks.back_callback = th_return_callback;
  options_callbacks.back_callback_data = state;

  th_OptionsWritebacks options_writebacks = {0};
  options_writebacks.exposure_writeback = th_modifyExposure;
  options_writebacks.gamma_writeback = th_modifyGamma;

  options_writebacks.exposure_writeback_data = state;
  options_writebacks.gamma_writeback_data = state;

  options_writebacks.fov_writeback = th_modifyFov;
  options_writebacks.fov_writeback_data = state;
  options_writebacks.sensitivity_writeback = th_modifySensitivity;
  options_writebacks.sensitivity_writeback_data = state;
  th_makeKeybindWriteback(&options_writebacks.forward_writeback,&options_writebacks.forward_writeback_data,&input->binding_forward);
  th_makeKeybindWriteback(&options_writebacks.backward_writeback,&options_writebacks.backward_writeback_data,&input->binding_back);
  th_makeKeybindWriteback(&options_writebacks.left_writeback,&options_writebacks.left_writeback_data,&input->binding_left);
  th_makeKeybindWriteback(&options_writebacks.right_writeback,&options_writebacks.right_writeback_data,&input->binding_right);
  th_makeKeybindWriteback(&options_writebacks.jump_writeback,&options_writebacks.jump_writeback_data,&input->binding_jump);
  th_makeKeybindWriteback(&options_writebacks.crouch_writeback,&options_writebacks.crouch_writeback_data,&input->binding_crouch);
  th_makeKeybindWriteback(&options_writebacks.machinegun_writeback,&options_writebacks.machinegun_writeback_data,&input->binding_weapon1);
  th_makeKeybindWriteback(&options_writebacks.shotgun_writeback,&options_writebacks.shotgun_writeback_data,&input->binding_weapon2);
  th_makeKeybindWriteback(&options_writebacks.hammer_writeback,&options_writebacks.hammer_writeback_data,&input->binding_weapon3);
  options_writebacks.sfx_writeback = th_modifySfxVolume;
  options_writebacks.sfx_writeback_data = NULL;

  options_writebacks.music_writeback = th_modifyMusicVolume;
  options_writebacks.music_writeback_data = NULL;

  //lift from options file
  th_createOptionsMenu(&state->optionsMenuLayout,character_map,state->screen_dims,options_callbacks,options_writebacks);




  th_LevelSelectCallbacks levelselect_callbacks = {0};
  levelselect_callbacks.back_callback = th_mainmenu_callback;
  levelselect_callbacks.back_callback_data = state;
  levelselect_callbacks.start_callback = th_launch_level_callback;
  levelselect_callbacks.start_callback_data = state;



  th_createLevelSelectMenu(&state->levelSelectLayout,character_map,state->screen_dims,levelselect_callbacks,state->list_of_levels,&state->wavytext_shader);

  int cpy_count = state->list_of_victory.count > state->list_of_levels.count ? state->list_of_levels.count : state->list_of_victory.count;
  memcpy(state->levelSelectLayout.victory_state,state->list_of_victory.victorystates,cpy_count*sizeof(th_LevelVictoryState));
  // state->levelSelectLayout.victory_state[0].level_speed = 4;
  // state->levelSelectLayout.victory_state[0].level_airtime = 3;
  // state->levelSelectLayout.victory_state[0].level_damagetaken = 2;


  th_PauseMenuCallbacks pausemenu_callbacks = {0};
  pausemenu_callbacks.resume_callback = th_resume_callback;
  pausemenu_callbacks.resume_callback_data = state;
  pausemenu_callbacks.restart_callback = th_restart_level_callback;
  pausemenu_callbacks.restart_callback_data = state;
  pausemenu_callbacks.titlescreen_callback = th_mainmenu_callback;
  pausemenu_callbacks.titlescreen_callback_data = state;
  pausemenu_callbacks.options_callback = th_options_menu_callback;
  pausemenu_callbacks.options_callback_data = state;
  th_createPauseMenu(&state->pauseMenuLayout,character_map,state->screen_dims,pausemenu_callbacks);

  th_DeathMenuCallbacks deathmenu_callbacks = {0};
  deathmenu_callbacks.restart_callback = th_restart_level_callback;
  deathmenu_callbacks.restart_callback_data = state;
  deathmenu_callbacks.titlescreen_callback = th_mainmenu_callback;
  deathmenu_callbacks.titlescreen_callback_data = state;
  th_createDeathMenu(&state->deathMenuLayout,character_map,state->screen_dims,deathmenu_callbacks,&state->wavytext_shader);



  th_DeathMenuCallbacks victorymenu_callbacks = {0};
  victorymenu_callbacks.restart_callback = th_restart_level_callback;
  victorymenu_callbacks.restart_callback_data = state;
  victorymenu_callbacks.titlescreen_callback = th_mainmenu_callback;
  victorymenu_callbacks.titlescreen_callback_data = state;
  th_createVictoryMenu(&state->victoryMenuLayout,character_map,state->screen_dims,victorymenu_callbacks,&state->wavytext_shader,state->levelSelectLayout.victory_state);



  th_createHUD(&state->hudLayout,character_map,state->screen_dims);


  // th_DeathMenuCallbacks deathmenu_callbacks = {0};
  // deathmenu_callbacks.restart_callback = th_restart_level_callback;
  // deathmenu_callbacks.restart_callback_data = state;
  // deathmenu_callbacks.titlescreen_callback = th_mainmenu_callback;
  // deathmenu_callbacks.titlescreen_callback_data = state;
  // th_createDeathMenu(&state->victoryMenuLayout,character_map,state->screen_dims,deathmenu_callbacks,&state->wavytext_shader);
  //
  //
  //
  // th_DeathMenuCallbacks victorymenu_callbacks = {0};
  // victorymenu_callbacks.restart_callback = th_restart_level_callback;
  // victorymenu_callbacks.restart_callback_data = state;
  // victorymenu_callbacks.titlescreen_callback = th_mainmenu_callback;
  // victorymenu_callbacks.titlescreen_callback_data = state;
  // th_createVictoryMenu(&state->deathMenuLayout,character_map,state->screen_dims,victorymenu_callbacks,&state->wavytext_shader);


  state->EnvBoxesUbo = th_createUbo(maxnumEnvboxes*sizeof(float)*4*3 + sizeof(int),0);

  state->MaterialPropertiesUbo = th_createUbo(maxmaterials_div4*sizeof(float)*4,1);


  state->dynamic_shadowcaster_commands = th_createCommandBuffer(256);



}

//state->level depentant rendering functions
void th_initRenderingLevel(bool sharm, bool iter,th_RendererState* state)
{
  th_Allocator* alloc = &state->level.allocator;

  /*
   * Level grid and cubes
   */
  state->gridpos = state->level.gridpos;
  state->gridsize = state->level.gridsize;
  state->griddims = state->level.griddims;
  state->cube_count = state->level.cube_count;
  state->cubePositionsCount = state->level.cube_count;
  state->cubePositions = state->level.cube_positions;



  //harmonics
  //state->cubemaps
  //th_mergeGpuData
  /*
   * ANIMATED VBO MERGE
   */

  state->animcommands_count = 0;
  th_Model* models_anim = state->level.levelstate.animated_models;
  if (state->level.levelstate.animated_models_count)
  {
    int total_meshcount = 0;
    for (int i = 0 ; i < state->level.levelstate.animated_models_count;i++)
    {
      total_meshcount += models_anim[i].meshcount;
    }
    th_GpuData* combined = th_alloc(alloc,sizeof(th_GpuData)*total_meshcount);
    int index = 0;
    for (int i = 0 ; i < state->level.levelstate.animated_models_count;i++)
    {
      for (unsigned int j = 0 ; j < models_anim[i].meshcount;j++)
      {
        combined[index] = models_anim[i].meshes[j];
        index++;
      }
    }


    state->guy_data = th_mergeGpuData(alloc,combined,total_meshcount,TH_TEXTUREID | TH_ANIMATED);
    th_createVbo(&state->guy_array,state->guy_data.data,TH_TEXTUREID | TH_ANIMATED);

    state->animcommands_count = total_meshcount;
    state->animcommands = th_alloc(alloc,sizeof(r_DrawElementsIndirectCommand)*state->animcommands_count);

    for (int i = 0 ; i < total_meshcount;i++)
    {
      state->animcommands[i].count = state->guy_data.element_counts[i];
      state->animcommands[i].firstIndex = state->guy_data.offsets_indices[i];
      state->animcommands[i].instanceCount = state->guy_data.instance_counts[i];
      state->animcommands[i].baseVertex = state->guy_data.offsets_verts[i];
      state->animcommands[i].baseInstance = state->guy_data.offsets_instances[i] + state->guy_data.data.instancecount*(th_frame()%3);
    }

    printf("TOTAL MESHCOUNT %i\n",total_meshcount);
  }


  /*
   * SKY CUBEMAP
   */
  if (state->level.night_sky)
  {
    th_createSkyCubemap(1024,&state->night_skyshader,&state->skybox,state->level.sundir_sky,&state->prefiltershader,state->level.atm_rayleigh,state->level.atm_sun_intensity,state->level.atm_sun_color,false,0.0,0.0);
  }
  else
  {
    th_createSkyCubemap(1024,&state->skyshader,&state->skybox,state->level.sundir_sky,&state->prefiltershader,state->level.atm_rayleigh,state->level.atm_sun_intensity,state->level.atm_sun_color,true,state->level.cloud_enable,state->level.horizon_height);
  }

  // state->screen_dims = fn_createVec2(config->width,config->height);
  glViewport(0,0,state->screen_dims.x,state->screen_dims.y);


  /*
   * HARMONICS
   */

  int count;
  th_Harmonic* harmonics;
  bool exists_harmonics = th_fileExists(state->level.harmonics_file);
  if (!exists_harmonics || (sharm && !iter))
  {
    harmonics = th_blankHarmonics(alloc,&count,state->griddims);//th_loadHarmonics("th1/harmonics.float",&count);
  }
  else
  {
    // harmonics = th_loadHarmonics("th1/harmonics.float",&count);//th_loadHarmonicsBinary(state->level.harmonics_file,&count);
    // th_exportHarmonicsBinary(harmonics,count,"th1/harmonics.hwad");
    harmonics = th_loadHarmonicsBinary(alloc,state->level.harmonics_file,&count,state->griddims,state->gridpos,state->gridsize);
  }

  state->lightTex = th_createLightTexture(harmonics,count,&state->dims_harmtex);

  //printf("lut and harmonics #3%i\n",SDL_GetTicks()-cur_time );
  //cur_time = SDL_GetTicks();


  /*
   * STATIC DYNAMIC TESSELATED
   */

  state->stream_commands_count = 0;
  if (state->level.streamed_meshes > 0)
  {
    state->dynamic_geom_offsets = th_mergeGpuData(alloc,state->level.streamed,state->level.streamed_meshes,TH_TEXTUREID);
    th_createVbo(&state->dynamic_geom_array,state->dynamic_geom_offsets.data,TH_TEXTUREID | TH_MAPPED_VERTS);

    int total_models = 0;
    for (int i = 0 ; i < state->level.streamed_meshes;i++)
    {
      total_models = total_models + state->level.render_commands_streamed[i].num_models;
    }

    state->stream_commands_count = total_models;
    state->streamcommands = th_alloc(alloc,sizeof(r_DrawElementsIndirectCommand)*state->stream_commands_count);

    int streamcommands_cursor = 0;
    for (int i = 0 ; i < state->level.streamed_meshes;i++)
    {
      th_RenderCommand rc = state->level.render_commands_streamed[i];

      for (int j = 0 ; j < rc.num_models;j++)
      {
        state->streamcommands[streamcommands_cursor].count = rc.elements_model;//state->guy_data.element_counts[i];
        state->streamcommands[streamcommands_cursor].firstIndex = state->dynamic_geom_offsets.offsets_indices[i] + rc.elements_model*j;//;
        state->streamcommands[streamcommands_cursor].instanceCount = 1;
        state->streamcommands[streamcommands_cursor].baseVertex = state->dynamic_geom_offsets.offsets_verts[i];
        state->streamcommands[streamcommands_cursor].baseInstance = state->dynamic_geom_offsets.offsets_instances[i] + j;
        streamcommands_cursor = streamcommands_cursor + 1;
      }

    }
  }




  //3 is t
  th_GpuData* models = malloc(sizeof(th_GpuData)*state->level.staticdynamic_count);
  for (int i = 0 ; i < state->level.staticdynamic_count;i++)
  {
    models[i] = state->level.meshes[state->level.staticdynamic[i]];
  }
  state->final = th_mergeGpuData(alloc,models,state->level.staticdynamic_count,TH_TEXTUREID);
  th_createVbo(&state->model_data,state->final.data,TH_TEXTUREID);

  if (state->level.tesselated_count)
  {
    th_GpuData* tess_models = malloc(sizeof(th_GpuData)*state->level.tesselated_count);
    for (int i = 0 ; i < state->level.tesselated_count;i++)
    {
      tess_models[i] = state->level.meshes[state->level.tesselated[i]];
    }
    state->final_tess = th_mergeGpuData(alloc,tess_models,state->level.tesselated_count,TH_TEXTUREID);
    th_createVbo(&state->model_data_tess,state->final_tess.data,TH_TEXTUREID);
    free(tess_models);
  }
  else
  {
    state->th_tesselation = false;
  }


  /*
   * PARTICLE MESHES
   */
  int particle_count = 256;
  th_initParticles(alloc,particle_count);
  th_GpuData* particle_model = th_alloc(alloc,sizeof(th_GpuData)*1);

  particle_model[0] = r_loadThorMesh("th1/models/quad.obj",false,false,false);
  particle_model[0].verts = th_arenaManage(alloc,particle_model[0].verts,particle_model[0].vertcount*sizeof(th_Vertex));
  particle_model[0].indices = th_arenaManage(alloc,particle_model[0].indices,particle_model[0].indicecount*sizeof(GLuint));
  particle_model[0].ids = th_make_handles_arena(alloc,fn_createVec2(0,22),particle_count);
  particle_model[0].instances = th_make_matrices_arena(alloc,fn_translaterotatescale(fn_createVec3s(100000),0,fn_createVec3(1,0,0),fn_createVec3(100,100,100)),particle_count);
  particle_model[0].instancecount = particle_count;
  // th_Particle p;
  // p.velocity = fn_createVec3(0,0,0);
  // p.position = fn_createVec3(0,-700,0);
  // th_addParticle(p);
  // p.position = fn_createVec3(50,-700,50);
  // th_addParticle(p);
  // p.position = fn_createVec3(70,-700,0);
  // th_addParticle(p);


  state->final_particle = th_mergeGpuData(alloc,particle_model,1,TH_TEXTUREID);
  th_createVbo(&state->particle_data,state->final_particle.data,TH_TEXTUREID);

  state->particle_commands_count = 1;
  for (int i = 0 ; i < 1;i++)
  {
    state->drawcommands_particle[i].count = state->final_particle.element_counts[i];
    state->drawcommands_particle[i].firstIndex = state->final_particle.offsets_indices[i];
    state->drawcommands_particle[i].instanceCount = state->final_particle.instance_counts[i];
    state->drawcommands_particle[i].baseVertex = state->final_particle.offsets_verts[i];
    state->drawcommands_particle[i].baseInstance = state->final_particle.offsets_instances[i] + state->final_particle.data.instancecount*(th_frame()%3);
  }

  /*
   * ENV BOXES
   */
  fn_vec4* EnvBoxPos_uniform = th_alloc(alloc,sizeof(float)*4*state->level.cube_count);

  fn_vec4* EnvBoxMin_uniform = th_alloc(alloc,sizeof(float)*4*state->level.cube_count);

  fn_vec4* EnvBoxMax_uniform = th_alloc(alloc,sizeof(float)*4*state->level.cube_count);
  for (int i = 0 ; i < state->level.cube_count;i++)
  {
    EnvBoxPos_uniform[i] = fn_vec3Tovec4(state->level.cube_positions[i],0.0);
    EnvBoxMin_uniform[i] = fn_vec3Tovec4(state->level.cube_mins[i],0.0);
    EnvBoxMax_uniform[i] = fn_vec3Tovec4(state->level.cube_maxs[i],0.0);
  }

  if (state->level.cube_count > 32)
  {
    printf("ERROR, too many cubemaps!\n");
    exit(0);
  }

  glBindBuffer(GL_UNIFORM_BUFFER, state->EnvBoxesUbo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float)*4*state->level.cube_count, EnvBoxPos_uniform);
  glBufferSubData(GL_UNIFORM_BUFFER, maxnumEnvboxes*sizeof(float)*4*1, sizeof(float)*4*state->level.cube_count, EnvBoxMin_uniform);
  glBufferSubData(GL_UNIFORM_BUFFER, maxnumEnvboxes*sizeof(float)*4*2, sizeof(float)*4*state->level.cube_count, EnvBoxMax_uniform);
  glBufferSubData(GL_UNIFORM_BUFFER, maxnumEnvboxes*sizeof(float)*4*3, sizeof(int), &state->level.cube_count);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  glBindBuffer(GL_UNIFORM_BUFFER, state->MaterialPropertiesUbo);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(int)*4*maxmaterials_div4, state->level.material_properties);

  glBindBuffer(GL_UNIFORM_BUFFER, 0);


  /*
   * DRAWCOMMANDS
   */

  state->drawcommands_count = state->level.staticdynamic_count;
  printf("DRAW COMMANDS %i\n",state->drawcommands_count);
  state->drawcommands_dynamic_count = state->level.meshes_dynamic_count;
  for (int i = 0 ; i < state->level.staticdynamic_count;i++)
  {
    state->drawcommands[i].count = state->final.element_counts[i];
    state->drawcommands[i].firstIndex = state->final.offsets_indices[i];
    state->drawcommands[i].instanceCount = state->final.instance_counts[i];
    state->drawcommands[i].baseVertex = state->final.offsets_verts[i];
    state->drawcommands[i].baseInstance = state->final.offsets_instances[i] + state->final.data.instancecount*(th_frame()%3);
  }

  state->drawcommands_count_tess = state->level.tesselated_count;
  for (int i = 0 ; i < state->level.tesselated_count;i++)
  {
    state->drawcommands_tess[i].count = state->final_tess.element_counts[i];
    state->drawcommands_tess[i].firstIndex = state->final_tess.offsets_indices[i];
    state->drawcommands_tess[i].instanceCount = state->final_tess.instance_counts[i];
    state->drawcommands_tess[i].baseVertex = state->final_tess.offsets_verts[i];
    state->drawcommands_tess[i].baseInstance = state->final_tess.offsets_instances[i] + state->final_tess.data.instancecount*(th_frame()%3);
  }



  // tess_shadowcaster_commands = th_createCommandBuffer(256);
  // state->drawcommands_count_tessshadow = 1;
  // state->drawcommands_tessshadow[0].count = state->final_tess.element_counts[1];
  // state->drawcommands_tessshadow[0].firstIndex = state->final_tess.offsets_indices[1];
  // state->drawcommands_tessshadow[0].instanceCount = state->final_tess.instance_counts[1];
  // state->drawcommands_tessshadow[0].baseVertex = state->final_tess.offsets_verts[1];
  // state->drawcommands_tessshadow[0].baseInstance = state->final_tess.offsets_instances[1] + state->final_tess.data.instancecount*(th_frame()%3);
  // state->drawcommands_tessshadow_indices[0] = 1;


  state->drawcommands_count_dynamicshadowcaster = state->level.dynamic_shadowcaster_count;
  for (int i = 0 ; i < state->level.dynamic_shadowcaster_count;i++)
  {
    int index = state->level.dynamic_shadowcaster[i];
    state->drawcommands_dynshadow[i].count = state->final.element_counts[index];
    state->drawcommands_dynshadow[i].firstIndex = state->final.offsets_indices[index];
    state->drawcommands_dynshadow[i].instanceCount = state->final.instance_counts[index];
    state->drawcommands_dynshadow[i].baseVertex = state->final.offsets_verts[index];
    state->drawcommands_dynshadow[i].baseInstance = state->final.offsets_instances[index] + state->final.data.instancecount*(th_frame()%3);
    state->drawcommands_dynshadow_indices[i] = index;
  }

  state->drawcommands_fc = th_alloc(alloc,sizeof(r_DrawElementsIndirectCommand)*256);
  state->drawcommands_count_fc = 0;
  state->drawcommands_dynamic_count_fc = 0;

  state->shadowSampled = false;
  state->staticSampled = false;
  for (int i = 0 ; i < MAX_POINT_SHADOWS;i++){
    state->staticSampledPoint[i] = false;
  }

  state->angles = state->level.spawnangles;

  /*
   * Bind textures, finalize
   */
  fn_getGLError();
  th_bindGlobalTextures(state);
  free(models);
}

void th_LoadData(bool sharm,bool gencubemaps,int resolution,fn_Config* config,bool iter,th_RendererState* state,fn_RawInput* input,const char* levelname,const char* spawnsetname)
{

  state->th_dynamic_shadows = config->dynamic_shadows;

  float reflection_scales[] = {0.5,0.75,1.0};
  state->reflections_scale = reflection_scales[fn_clampi(config->reflection_scale,0,2)];


  GLuint shadowmap_resolutions[] = {512,1024,2048};
  state->th_shadowmap_resolution = shadowmap_resolutions[fn_clampi(config->shadow_quality,0,2)];


  state->config_global = config;
  fn_defaultSplineState(&state->camera_spline);

  state->th_skyRadianceBoost = 1.0;
  if (sharm)
  {
    state->th_skyRadianceBoost = TH_SKY_BOOST_LIGHTBAKE;
  }
  Uint32 load_start = SDL_GetTicks();
  Uint32 cur_time = load_start;
  th_initializeBuiltinMemory();
  printf("LOAding Data\n");

  state->old_mvp = fn_identityMat4();
  state->projMatrixInv_old = fn_identityMat4();
  state->viewMatrixInv_old = fn_identityMat4();
  th_initShaders(state,sharm || gencubemaps);
  th_initRenderingOnce(state,sharm,
                       gencubemaps,
                       resolution,
                       config,
                       iter,input);
  if (levelname == NULL)
  {
    th_loadLevel(&state->level,gencubemaps,sharm,&state->cubemapDepth,&state->cubemapColor,"levelmovementtutorial","spawnset_title",fn_createVec3(1.0,1.0,1.0),state->th_respawn_flag,fn_clampi(state->config_global->fog_quality,0,2));
  }
  else
  {

    fn_vec3 scale = fn_createVec3s(1);
    if (strcmp(levelname,"levelskatepark") == 0)
    {
      scale = fn_createVec3s(1.5);
    }

    if (spawnsetname == NULL)
    {
      th_loadLevel(&state->level,gencubemaps,sharm,&state->cubemapDepth,&state->cubemapColor,levelname,"spawnset_blank",scale,state->th_respawn_flag,fn_clampi(state->config_global->fog_quality,0,2));
    }
    else
    {
      th_loadLevel(&state->level,gencubemaps,sharm,&state->cubemapDepth,&state->cubemapColor,levelname,spawnsetname,scale,state->th_respawn_flag,fn_clampi(state->config_global->fog_quality,0,2));
    }

  }

  if (sharm)
  {
    state->th_skyRadianceBoost = state->th_skyRadianceBoost * state->level.skyboost;
  }

  th_getMatsPerFrame(state->rp,&state->level);
  th_initRenderingLevel(sharm, iter,state);


  GLint max_ubosize;
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE,&max_ubosize);
  printf("Maximum UBO SIZE: %i \n",max_ubosize );


  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,&max_ubosize);
  printf("UBO ALLIGNMENT: %i \n",max_ubosize );
  fn_getGLError();




  // int meminfo[5];
  // const char* memstrings[5] = {"GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX",
  //   "GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX",
  //   "GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX",
  //   "GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX",
  //   "GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX"};
  //   glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX,&meminfo[0]);
  //   glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX,&meminfo[1]);
  //   glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX,&meminfo[2]);
  //   glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX,&meminfo[3]);
  //   glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX,&meminfo[4]);
  //   for (int i = 0 ; i < 5;i++)
  //   {
  //     printf("%s %f GB\n",memstrings[i],(float)meminfo[i] * 0.000001 );
  //   }



    cur_time = SDL_GetTicks();
    printf("total load time %f Seconds\n",(float)(cur_time - load_start)/1000.f );
}


void th_renderPointShadows(th_RendererState* state,fn_vec3 point_shadow_pos,fn_mat4* shadow_viewproj_point,fn_vec2 offset,int offset_static_check,int mask,bool only_static)
{
  //omni light
  /*
  -----
  -1-2-
  -----
  -3-4-
  -----
  -5-6-
  */
  float scale = state->level.omni_shadow_scale;

  fn_mat4 mats[6];
  mats[1] = r_camera(point_shadow_pos,fn_createVec2(fn_radians(-90),fn_radians(0)));// negx
  mats[0] = r_camera(point_shadow_pos,fn_createVec2(fn_radians(90),fn_radians(0)));// posx
  mats[4] = r_camera(point_shadow_pos,fn_createVec2(fn_radians(0),fn_radians(0))); //posz
  mats[5] =  r_camera(point_shadow_pos,fn_createVec2(fn_radians(180),fn_radians(0)));//negz
  mats[3] = r_camera(point_shadow_pos,fn_createVec2(fn_radians(0),fn_radians(90)));//negy
  mats[2] = r_camera(point_shadow_pos,fn_createVec2(fn_radians(0),fn_radians(-90)));//posy
  fn_mat4 omni_proj = fn_perspective(fn_radians(90),1,state->zNear,state->zFar);
  fn_mat4 omni_viewproj;


  shadow_viewproj_point[0] = fn_multMat4(mats[0],omni_proj);
  shadow_viewproj_point[1] = fn_multMat4(mats[1],omni_proj);
  shadow_viewproj_point[2] = fn_multMat4(mats[2],omni_proj);
  shadow_viewproj_point[3] = fn_multMat4(mats[3],omni_proj);
  shadow_viewproj_point[4] = fn_multMat4(mats[4],omni_proj);
  shadow_viewproj_point[5] = fn_multMat4(mats[5],omni_proj);
  int x_vp = 0;
  int y_vp = 0;

  float iscale = 0.5*scale;
  float ires = state->th_shadowmap_resolution;

  fn_vec4 viewports[6];
  viewports[0] = fn_createVec4(ires,0,ires*0.5,ires*0.5);
  viewports[1] = fn_createVec4(ires*(1 + iscale),0,ires*0.5,ires*0.5);
  viewports[2] = fn_createVec4(ires,ires*0.5*scale,ires*0.5,ires*0.5);
  viewports[3] = fn_createVec4(ires*(1 + iscale),ires*0.5*scale,ires*0.5,ires*0.5);
  viewports[4] = fn_createVec4(ires,ires*scale,ires*0.5,ires*0.5);
  viewports[5] = fn_createVec4(ires*(1 + iscale),ires*scale,ires*0.5,ires*0.5);

  bool any_mask = false;
  for (int i = 0;i < 6;i++)
  {
    any_mask = any_mask || !(mask >> i & 1);
  }
  any_mask = any_mask && state->staticSampledPoint[0 + offset_static_check*6];

  //any_mask &&

  GLsizei width_copy = state->th_shadowmap_resolution*scale;
  GLsizei height_copy = state->th_shadowmap_resolution*1.5*scale ;

  if (state->staticSampledPoint[0 + offset_static_check*6])// && state->th_dynamic_objs && state->th_dynamic_shadows
  {
    x_vp = state->th_shadowmap_resolution + offset.x;
    y_vp = offset.y;



    // th_printlnDevConsole("%i %i %i %i",x_vp,y_vp,width_copy,height_copy);
    // th_printlnDevConsole("%i %i",(int)state->th_shadowmap_resolution*4,(int)state->th_shadowmap_resolution*4);

    glCopyImageSubData(state->shadowCache_tex,GL_TEXTURE_2D,0,x_vp,y_vp,0,state->shadowBuffer.depthTexture,GL_TEXTURE_2D,0,x_vp,y_vp,0,width_copy,height_copy,1);
  }

  for (int i = 0;i < 6;i++)
  {
    viewports[i].x += offset.x;
    viewports[i].y += offset.y;
  }

  if (!state->staticSampledPoint[0 + offset_static_check*6])
  {
    for (int i = 0;i < 6;i++)
    {
      glViewport(viewports[i].x,viewports[i].y,viewports[i].z*scale,viewports[i].w*scale);
      x_vp = viewports[i].x;
      y_vp = viewports[i].y;

      omni_viewproj = fn_multMat4(mats[i],omni_proj);

      if (!state->staticSampledPoint[i + offset_static_check*6])
      {


        r_bindShader(&state->shadowOccluder);
        r_sendmat4(&state->shadowOccluder,omni_viewproj,"modelViewprojection");
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->model_data.cmds.drawCommandBuffer);
        th_renderArrayCmdBuffer(&state->model_data,state->drawcommands_count-state->drawcommands_dynamic_count);

        state->staticSampledPoint[i + offset_static_check*6] = true;
      }
    }

    x_vp = state->th_shadowmap_resolution + offset.x;
    y_vp = offset.y;



  }

  //dont render dynamic geometry to cache
  if (only_static)
  {
    return;
  }

  for (int i = 0;i < 6;i++)
  {

    glViewport(viewports[i].x,viewports[i].y,viewports[i].z*scale,viewports[i].w*scale);
    x_vp = viewports[i].x;
    y_vp = viewports[i].y;

    omni_viewproj = fn_multMat4(mats[i],omni_proj);


    if ((mask >> i & 1))
    {
      continue;
    }




    if (state->th_dynamic_objs && state->th_dynamic_shadows)
    {
      if (state->animcommands_count)
      {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->guy_array.cmds.drawCommandBuffer);
        r_bindShader(&state->occluder_anim);
        r_sendmat4(&state->occluder_anim,omni_viewproj,"modelViewprojection");
        r_sendi(&state->occluder_anim,th_frame() %3,"frame_index");
        th_renderArrayCmdBuffer(&state->guy_array,state->animcommands_count);
      }


      r_bindShader(&state->shadowOccluder);
      r_sendmat4(&state->shadowOccluder,omni_viewproj,"modelViewprojection");
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->dynamic_shadowcaster_commands.buffer);
      th_renderArrayCmdBuffer(&state->model_data,state->drawcommands_count_dynamicshadowcaster);


      if (state->stream_commands_count > 0 && state->th_dynamic_objs)
      {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->dynamic_geom_array.cmds.drawCommandBuffer);
        th_renderArrayCmdBuffer(&state->dynamic_geom_array,state->stream_commands_count);
      }
    }
  }


}







// void th_interpFrame(fn_mat4* a,fn_mat4* b,float f, fn_mat4* dest,int count)
// {
//   for (size_t i = 0; i < count; i++) {
//     dest[i] = th_interpMat4(a[i],b[i],f);
//   }
// }
static bool debug_key = false;

void th_render(fn_mat4 modelViewprojection,fn_mat4 proj,fn_mat4 view,fn_vec2 screenSize,th_RendererState* state)
{
  int resolution = (int)screenSize.x;


  if (!state->th_do_rendering_flag)
  {
    printf("%s %i\n","Frame",th_frame() );
    return;
  }
  Uint32 start_time = SDL_GetTicks();

  th_waitTripleSync(state->bonesSyncs);

  th_performCapture(state->rp,&state->level);








  fn_vec4 planes_frust[6];
  fn_extractFrustum(planes_frust,modelViewprojection);
  th_cullVisible(state,planes_frust);

  if (state->th_dynamiclights && !state->th_batch_render)
  {
    th_assignPhysicalLights(state->level.light_query,planes_frust,state->pos);

    int num_hitmarkers = 0;
    th_PointLight* hitmarkers = th_getHitmarkerLights(&num_hitmarkers);

    th_mergePhysicalLights(state->level.light_query,hitmarkers,num_hitmarkers);

    // if (num_hitmarkers > 0)
    //   printf("%i hmarkers\n",num_hitmarkers);
  }




  state->shadowCasterPos = state->level.center_sunlight;
  //-56.692692 ,-224.180786 ,355.918701
  //state->shadowCasterDir = fn_createVec3(0.068358 ,0.488178 ,-0.870063);
  state->shadowCasterDir = state->level.sundir_shadow;
  //
  // course[0]=fn_createVec3(827.902039,-3330.415283,1546.699463);
  // course[1]=fn_createVec3(-434.229034,-136.181198,-1146.720215);

  // fn_mat4 shadow_proj = fn_ortho3D(-1280,1280,-1500,1500,3150,-3150);
  fn_mat4 shadow_view = fn_scale(fn_lookat(fn_multVec3(state->shadowCasterPos,fn_createVec3(-1,1,1)),fn_addVec3(fn_multVec3(state->shadowCasterPos,fn_createVec3(-1,1,1)),fn_multVec3(state->shadowCasterDir,fn_createVec3(-1,1,1))),fn_createVec3(0,-1,0)),fn_createVec3(-1,1,1));

  // fn_vec3 gma =fn_createVec3(-1531.168579,-2393.254395,879.690613);
  // fn_vec3 gmi =fn_createVec3(1391.097656,10.417622,-818.210022);

  fn_vec3 gmi =fn_createVec3(-2195.829834,-2513.986328,-1302.32);
  fn_vec3 gma =fn_createVec3(1265.751099,-91.827774,1975.616333);


  fn_vec3 corner[8];
  corner[0] = fn_createVec3(gmi.x,gmi.y,gma.z);
  corner[1] = fn_createVec3(gmi.x,gmi.y,gmi.z);
  corner[2] = fn_createVec3(gmi.x,gma.y,gma.z);
  corner[3] = fn_createVec3(gmi.x,gma.y,gmi.z);
  corner[4] = fn_createVec3(gma.x,gmi.y,gma.z);
  corner[5] = fn_createVec3(gma.x,gmi.y,gmi.z);
  corner[6] = fn_createVec3(gma.x,gma.y,gma.z);
  corner[7] = fn_createVec3(gma.x,gma.y,gmi.z);

  corner[0] = fn_transformVec3(corner[0],shadow_view);
  corner[1] = fn_transformVec3(corner[1],shadow_view);
  corner[2] = fn_transformVec3(corner[2],shadow_view);
  corner[3] = fn_transformVec3(corner[3],shadow_view);
  corner[4] = fn_transformVec3(corner[4],shadow_view);
  corner[5] = fn_transformVec3(corner[5],shadow_view);
  corner[6] = fn_transformVec3(corner[6],shadow_view);
  corner[7] = fn_transformVec3(corner[7],shadow_view);

  fn_vec3 mmi = corner[0];
  fn_vec3 mma = corner[0];
  for (int i = 0; i < 8;i++)
  {
    mmi.x = fn_min(mmi.x,corner[i].x);
    mma.x = fn_max(mma.x,corner[i].x);

    mmi.y = fn_min(mmi.y,corner[i].y);
    mma.y = fn_max(mma.y,corner[i].y);

    mmi.z = fn_min(mmi.z,corner[i].z);
    mma.z = fn_max(mma.z,corner[i].z);
  }
  //  printf("%s\n","shadowgovnt" );
  // fn_printVec3(mmi);
  // fn_printVec3(mma);
  fn_mat4 shadow_proj = state->level.orthomat_sunlight;//fn_ortho3D(-2000,2000,-2000,2000,3000,-3000);
  //fn_mat4 shadow_proj = fn_ortho3D(mmi.x,mma.x,mmi.y,mma.y,3000,-3000);


  fn_mat4 shadow_viewproj = fn_multMat4(shadow_view,shadow_proj);

  if (state->th_pov_sun_shadow)
  {
    proj = shadow_proj;
    view= shadow_view;
    //
    modelViewprojection = shadow_viewproj;
  }

  // proj = shadow_proj;
  // view = shadow_view;
  //  th_startQuery(&state->timer);
  state->invProj = fn_inverse(proj);
  state->invView = fn_inverse(view);
  fn_vec2 invWindow = fn_createVec2(1.0/screenSize.x,1.0/screenSize.y);
  state->lights_new_count = 0;
  //state->lights_new =  malloc(sizeof(th_PointLight)*3);
  if (state->access_new == NULL)
  {
    state->lights_new = malloc(sizeof(th_PointLight)*256);
    state->lights_new_count = 0;


    state->access_new = malloc(sizeof(GLuint)*4*4096);
    memset(state->access_new,0,sizeof(GLuint)*4*4096);

    state->access_cubes = malloc(sizeof(GLuint)*4*4096);
    memset(state->access_cubes,0,sizeof(GLuint)*4*4096);

    state->offsets_new = malloc(sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)));
    memset(state->offsets_new,0,sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)));

    state->offsets_new2 = malloc(sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)));
    memset(state->offsets_new2,0,sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)));


    state->access_decals_new = malloc(sizeof(GLuint)*4*4096);
    memset(state->access_decals_new,0,sizeof(GLuint)*4*4096);
    state->decals_new_count = 0;
    state->decals_new = malloc(sizeof(th_Decal)*800);
    state->offsets_decals_new=malloc(sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)));
    memset(state->offsets_decals_new,0,sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)));
  }
  Uint32 start = SDL_GetTicks();
  uint light_memory = 0;
  if (state->th_dynamiclights)
  {

    th_captureLights(state->rp,&state->level);


    if (state->th_batch_render)
    {
      state->lights_new = state->level.pointlights;
      state->lights_new_count = state->level.pointlightcount;
      //light_memory = th_binLights(planes_frust,view,state->access_new,state->offsets_new,state->lights_new,state->lights_new_count,state->invProj,screenSize,state->zNear,state->zFar);

      //light_memory is access size
      //access_new is access
      //offsets_new is offset

      memset(state->offsets_new,0,sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)));

      const int reserved_dynamic_lights = 2;
      for (int i = 0 ; i <((16*8*CLUSTER_Z_COUNT));i++ )
      {
        state->offsets_new[i*2 + 1] = state->level.lights_reserved - reserved_dynamic_lights;
      }

      light_memory = state->level.lights_reserved - reserved_dynamic_lights;


      for (int i = 0 ; i < state->level.lights_reserved - reserved_dynamic_lights;i++ )
      {
        state->access_new[i] = i + reserved_dynamic_lights;
      }


    }
    else
    {
      state->lights_new = state->level.pointlights;
      state->lights_new_count = state->level.pointlightcount;
      light_memory = th_binLights(planes_frust,view,state->access_new,state->offsets_new,state->lights_new,state->lights_new_count,state->invProj,screenSize,state->zNear,state->zFar);
    }






  }
  else
  {

    for (uint32_t i = 0; i <state->lights_new_count;i++)
    {
      state->lights_new[i].color = fn_createVec4(0,0,0,0);
      state->lights_new[i].pos = fn_createVec4(100000000000,100000000000,100000000000,0);
      state->lights_new[i].pos2 = fn_createVec4(0,0,0,0);
    }
  }

  // printf("EEE");

  uint cube_memory = th_binCubes(view,state->access_cubes,state->offsets_new2,state->level.cube_mins,state->level.cube_maxs,state->level.cube_count,state->invProj,screenSize,state->zNear,state->zFar,debug_key,state->projection,state->pos);
  //   printf("EEE2");
  th_getDecals(state->decals_new,&state->decals_new_count);
  uint decal_memory = th_binDecals(planes_frust,view,state->access_decals_new,state->offsets_decals_new,th_getDecalOrientations(),state->decals_new_count,state->invProj,screenSize,state->zNear,state->zFar,state->pos);



  memcpy(&state->mappedDecals[800*(th_frame() % 3)],state->decals_new,sizeof(th_Decal)*state->decals_new_count);
  memcpy(&state->mapped_accessDecal[4*4096*(th_frame() % 3)],state->access_decals_new,sizeof(GLuint)*decal_memory);//4*4096
  memcpy(&state->mapped_offsetsDecal[((16*8*CLUSTER_Z_COUNT))*(th_frame() % 3)],state->offsets_decals_new,sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)) );
  memcpy(&state->mapped_pointlights[256*(th_frame() % 3)],state->lights_new,sizeof(th_PointLight)*state->lights_new_count);
  memcpy(&state->mapped_acessubo[4*4096*(th_frame() % 3)],state->access_new,sizeof(GLuint)*light_memory);//4*4096
  memcpy(&state->mapped_offsetsubo[((16*8*CLUSTER_Z_COUNT))*(th_frame() % 3)],state->offsets_new,sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)) );
  memcpy(&state->mapped_offsetsubo[((16*8*CLUSTER_Z_COUNT))*(th_frame() % 3) + (16*8*CLUSTER_Z_COUNT)/2],state->offsets_new2,sizeof(GLuint)*2*((16*8*CLUSTER_Z_COUNT)) );
  memcpy(&state->mapped_acessubocubes[4*4096*(th_frame() % 3)],state->access_cubes,sizeof(GLuint)*cube_memory);//4*4096
  //
  // state->lights_new[0].pos = fn_createVec4(-100 + sin(th_frame() / 100.f)*100,0,0,0);
  //


  // printf("EEE3");

  glBindBufferRange(GL_UNIFORM_BUFFER, 7, state->decalsUBO, 800*sizeof(th_Decal)*(th_frame() % 3), 800*sizeof(th_Decal));


  glBindBufferRange(GL_UNIFORM_BUFFER, 8, state->accessDecalUBO, sizeof(GLuint)*4*4096*(th_frame() % 3), sizeof(GLuint)*4*4096);


  glBindBufferRange(GL_UNIFORM_BUFFER, 9, state->offsetsDecalUBO,sizeof(GLuint)*4*((16*8*CLUSTER_Z_COUNT))*(th_frame() % 3), sizeof(GLuint)*4*((16*8*CLUSTER_Z_COUNT)) );




  glBindBufferRange(GL_UNIFORM_BUFFER, 2, state->lightsUBO, 256*sizeof(th_PointLight)*(th_frame() % 3), 256*sizeof(th_PointLight));


  glBindBufferRange(GL_UNIFORM_BUFFER, 3, state->accessUBO, sizeof(GLuint)*4*4096*(th_frame() % 3), sizeof(GLuint)*4*4096);


  glBindBufferRange(GL_UNIFORM_BUFFER, 4, state->offsetsUBO,sizeof(GLuint)*4*((16*8*CLUSTER_Z_COUNT))*(th_frame() % 3), sizeof(GLuint)*4*((16*8*CLUSTER_Z_COUNT)) );


  glBindBufferRange(GL_UNIFORM_BUFFER, 5, state->accesscubesUBO, sizeof(GLuint)*4*4096*(th_frame() % 3), sizeof(GLuint)*4*4096);




  state->occlusion_scale = 0.0;//1.0;



  fn_mat4 projmat = proj;


  int w1,h1;
  w1 = floor((screenSize.x));
  h1 =floor((screenSize.y));


  for (unsigned int i = 0 ; i < state->drawcommands_count;i++)
  {
    state->drawcommands[i].baseInstance = state->final.offsets_instances[i] + state->final.data.instancecount*(th_frame()%3);
  }

  for (unsigned int i = 0 ; i < state->drawcommands_count_tess;i++)
  {
    state->drawcommands_tess[i].baseInstance = state->final_tess.offsets_instances[i] + state->final_tess.data.instancecount*(th_frame()%3);
  }


  for (unsigned int i = 0 ; i < state->animcommands_count;i++)
  {
    state->animcommands[i].baseInstance = state->guy_data.offsets_instances[i] + state->guy_data.data.instancecount*(th_frame()%3);
  }

  int streamcommands_cursor = 0;

  unsigned int frame_offset_streamverts = state->dynamic_geom_offsets.data.vertcount*(th_frame()%3);

  for (int i = 0 ; i < state->level.streamed_meshes;i++)
  {
    th_RenderCommand rc = state->level.render_commands_streamed[i];

    for (int j = 0 ; j < rc.num_models;j++)
    {

      state->streamcommands[streamcommands_cursor].baseVertex = state->dynamic_geom_offsets.offsets_verts[i] + frame_offset_streamverts;

      streamcommands_cursor = streamcommands_cursor + 1;
    }

  }


  for (int i = 0 ; i < state->level.anim_render_commands_count;i++)
  {
    th_RenderCommand r = state->level.anim_render_commands[i];
    th_setInstances(&state->guy_array,r.offset + state->guy_data.offsets_instances[r.model_id],*r.mats,*r.matcount,r.stride);
    //printf("%i %i %i %i\n",i,r.model_id,*r.matcount,r.stride);
  }

  for (int i = 0 ; i < state->level.render_commands_streamed_count;i++)
  {
    th_RenderCommand r = state->level.render_commands_streamed[i];


    //index + array->max_instances*(th_frame()%3)

    memcpy(&state->dynamic_geom_array.mappedVerts[state->dynamic_geom_offsets.offsets_verts[r.model_id] + frame_offset_streamverts],*r.backing_buffer,sizeof(th_Vertex)*r.num_verts);
  }


  if (state->drawcommands_count_tess)
  {
    th_setCommandBuffer(&state->model_data_tess,state->drawcommands_tess,state->drawcommands_count_tess);
  }

  if (state->stream_commands_count > 0)
  {
    th_setCommandBuffer(&state->dynamic_geom_array,state->streamcommands,state->stream_commands_count);
  }

  th_setCommandBuffer(&state->guy_array,state->animcommands,state->animcommands_count);

  if (state->bonesindices_new == NULL)
  {
    state->bonesindices_new = malloc(sizeof(th_uvec2)*2*4096);
    memset(state->bonesindices_new,0,sizeof(th_uvec2)*2*4096);
  }

  GLuint boneindices_count = 0;
  GLuint bone_offset = 0;
  GLuint old_bones_num = 0;
  // if (state->level.animated_models_count != 0)
  // {
  //   old_bones_num = state->level.animated_models[0].bones_count;
  // }
  //printf("Boneindicescount\n");
  for (int i = 0 ; i < state->level.levelstate.animated_models_count;i++)
  {
    //TODO offsets
    // if (old_bones_num != state->level.levelstate.animated_models[i].bones_count)
    // {
      for (GLuint j = 0 ; j < state->level.levelstate.animated_models[i].meshcount;j++)
      {
        state->bonesindices_new[boneindices_count].y = state->level.levelstate.animated_models[i].bones_count;
        state->bonesindices_new[boneindices_count].x = bone_offset;

        //printf("%i %i\n",state->bonesindices_new[boneindices_count].x ,state->bonesindices_new[boneindices_count].y);
        boneindices_count++;
      }
      old_bones_num = state->level.levelstate.animated_models[i].bones_count;
    //}
    for (GLuint j = 0 ; j < state->level.levelstate.animated_models[i].instanceCount;j++)
    {
      memcpy(&state->mapped_AnimUBO[state->max_bones*(th_frame() % 3) + bone_offset],state->level.levelstate.animated_models[i].instances[j].skeleton,sizeof(fn_mat4)*state->level.levelstate.animated_models[i].bones_count);
      bone_offset += state->level.levelstate.animated_models[i].bones_count;

      if (bone_offset > (GLuint)state->max_bones)
      {
        printf("Max bones exceeded\n");
      }
    }



  }





  glBindBufferRange(GL_UNIFORM_BUFFER, 6, state->boneindicesUBO, sizeof(GLuint)*4*4096*(th_frame() % 3), sizeof(GLuint)*4*4096);
  memcpy(&state->mapped_boneindices[4*4096*(th_frame() % 3)],state->bonesindices_new,sizeof(GLuint)*4*4096);



  fn_vec3 point_shadow_pos = fn_createVec3(207.802399,-400.645264,-979.814941);
  static fn_mat4 shadow_viewproj_point[MAX_POINT_SHADOWS];
  if (!state->shadowSampled)
  {
    for (int i = 0 ; i < MAX_POINT_SHADOWS;i++)
    {
      shadow_viewproj_point[i] = fn_identityMat4();
    }
  }




  for (int i = 0 ; i < state->particle_commands_count;i++)
  {
    state->drawcommands_particle[i].baseInstance = state->final_particle.offsets_instances[i] + state->final_particle.data.instancecount*(th_frame()%3);
    state->drawcommands_particle[i].instanceCount = th_particle_count();
  }

  bool isnt_replay = th_captureParticles(state->rp,state->drawcommands_particle,state->final_particle,&state->particle_data,state->particle_commands_count,state->pos,state->look_global);
  if (isnt_replay)
  {
    th_setInstances(&state->particle_data,0,th_particle_matrices(),th_particle_count(),1);

    // printf("%i\n",th_particle_count());
    // for (int i = 0 ; i < th_particle_count();i++)
    // {
    //   fn_printVec3(fn_createVec3(th_particle_matrices()[i].m[4],th_particle_matrices()[i].m[5],th_particle_matrices()[i].m[6]));
    // }
  }


  th_setCommandBuffer(&state->particle_data,state->drawcommands_particle,state->particle_commands_count);

  //do actual rendering
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT );
  glBindFramebuffer(GL_FRAMEBUFFER, state->noshading_fb.framebuffer);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


  if (!state->th_dynamic_objs)
  {
    glDisable(GL_CULL_FACE);
  }

  //pre pass
  glDepthFunc(GL_LESS);
  // r_bindShader(&state->shadowOccluder);

  //#define EARLY_Z 1
  int sub;
  // #ifdef EARLY_Z
  if (state->th_depth_prepass)
  {
    glColorMaski(0,GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    glColorMaski(1,GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

    if (state->th_dynamic_objs)
    {
      if (state->animcommands_count)
      {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->guy_array.cmds.drawCommandBuffer);
        r_bindShader(&state->occluder_anim);
        r_sendmat4(&state->occluder_anim,modelViewprojection,"modelViewprojection");
        r_sendi(&state->occluder_anim,th_frame() %3,"frame_index");
        th_renderArrayCmdBuffer(&state->guy_array,state->animcommands_count);
      }

      //  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    }


    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->model_data.cmds.drawCommandBuffer);
    r_bindShader(&state->shadowOccluder);
    r_sendmat4(&state->shadowOccluder,modelViewprojection,"modelViewprojection");
    sub = (state->th_dynamic_objs) ? 0 : state->drawcommands_dynamic_count_fc;
    th_renderArrayCmdBuffer(&state->model_data,state->drawcommands_count_fc - sub);

    if (state->stream_commands_count > 0 && state->th_dynamic_objs)
    {
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->dynamic_geom_array.cmds.drawCommandBuffer);
      th_renderArrayCmdBuffer(&state->dynamic_geom_array,state->stream_commands_count);
    }


    //rendering pass
    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);
    glColorMaski(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glColorMaski(1,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  }


  if (state->th_dynamic_objs)
  {
    if (state->animcommands_count)
    {
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->guy_array.cmds.drawCommandBuffer);
      r_bindShader(&state->noshading_anim);
      r_sendmat4(&state->noshading_anim,modelViewprojection,"modelViewprojection");
      r_sendi(&state->noshading_anim,th_frame() %3,"frame_index");
      th_renderArrayCmdBuffer(&state->guy_array,state->animcommands_count);
    }

    //  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
  }

  //
  if (state->th_batch_render)
  {
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->model_data.cmds.drawCommandBuffer);
    r_bindShader(&state->noshading);
    r_sendi(&state->noshading,state->drawcommands_count_fc - state->drawcommands_dynamic_count_fc - 1,"dynamic_cutoff");
    int x_dim = (state->batch_atlas_res / 32);
    sub = (state->th_dynamic_objs) ? 0 : state->drawcommands_dynamic_count_fc;
    for (int i = 0 ; i < state->num_batches;i++)
    {
      int x_coord = i % x_dim;
      int y_coord = i / x_dim;

      glViewport(x_coord*32,y_coord*32,resolution,resolution);


      r_sendmat4(&state->noshading,fn_multMat4(state->viewmats[i],state->projection),"modelViewprojection");


      th_renderArrayCmdBuffer(&state->model_data,state->drawcommands_count_fc - sub);
    }
  }
  else
  {
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->model_data.cmds.drawCommandBuffer);
    r_bindShader(&state->noshading);
    r_sendmat4(&state->noshading,modelViewprojection,"modelViewprojection");
    r_sendi(&state->noshading,state->drawcommands_count_fc - state->drawcommands_dynamic_count_fc - 1,"dynamic_cutoff");
    sub = (state->th_dynamic_objs) ? 0 : state->drawcommands_dynamic_count_fc;
    th_renderArrayCmdBuffer(&state->model_data,state->drawcommands_count_fc - sub);

    if (state->stream_commands_count > 0 && state->th_dynamic_objs)
    {
      r_sendi(&state->noshading,-1,"dynamic_cutoff");
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->dynamic_geom_array.cmds.drawCommandBuffer);
      th_renderArrayCmdBuffer(&state->dynamic_geom_array,state->stream_commands_count);
    }
  }

  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  //  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);




  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  // glColorMaski(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  // glColorMaski(1,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glTextureBarrier();
  if (state->th_decal_rendering)
  {

    glColorMaski(2,GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

    r_bindShader(&state->deferredtex);
    r_sendmat4(&state->deferredtex,state->invProj,"projMatrixInv");
    r_sendmat4(&state->deferredtex,state->invView,"viewMatrixInv");
    r_sendmat4(&state->deferredtex,state->postmatrix,"modelViewprojection");
    r_send2f(&state->deferredtex,screenSize,"screenSize");
    r_sendf(&state->deferredtex,state->zNear,"zNear");
    r_sendf(&state->deferredtex,state->zFar,"zFar");
    // glDispatchCompute((state->screen_dims.x)/8,(state->screen_dims.y)/8,1);
    th_renderArray(&state->post_gpu,3,0,0,0);

    // glColorMaski(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    // glColorMaski(1,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glColorMaski(2,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  }


  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  //do shadow buffers
  if (!state->shadowSampled)
  {



    for (GLuint i = 0 ; i < state->drawcommands_count_dynamicshadowcaster;i++)
    {
      state->drawcommands_dynshadow[i].baseInstance = state->final.offsets_instances[state->drawcommands_dynshadow_indices[i]] + state->final.data.instancecount*(th_frame()%3);
    }
    memcpy(&state->dynamic_shadowcaster_commands.mapped[state->dynamic_shadowcaster_commands.commandcount_max*(th_frame()%3)],state->drawcommands_dynshadow,sizeof(r_DrawElementsIndirectCommand)*state->drawcommands_count_dynamicshadowcaster);


    //render static portion of shadows to shadow cache
    if (!state->staticSampled)
    {
      glBindFramebuffer(GL_FRAMEBUFFER, state->shadowBufferCache.framebuffer);
      glViewport(0,0,state->th_shadowmap_resolution*state->level.sun_shadow_scale,state->th_shadowmap_resolution*state->level.sun_shadow_scale);
      glClear(GL_DEPTH_BUFFER_BIT);

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);

      r_bindShader(&state->shadowOccluder);
      r_sendmat4(&state->shadowOccluder,shadow_viewproj,"modelViewprojection");
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->model_data.cmds.drawCommandBuffer);
      th_renderArrayCmdBuffer(&state->model_data,state->drawcommands_count-state->drawcommands_dynamic_count);


      for (int oli = 0 ; oli < state->level.omni_light_count;oli++)
      {
        int idx = (int)(state->level.omni_light_atlascoords[oli].z);
        fn_vec2 resvector = fn_createVec2(state->th_shadowmap_resolution,state->th_shadowmap_resolution);
        th_renderPointShadows(state,state->level.omni_light_positions[oli],&shadow_viewproj_point[idx*6],fn_multVec2(state->level.omni_light_atlascoords[oli].xy,resvector),idx,state->level.omni_light_masks[oli],true);
      }

      state->staticSampled = true;
    }


    glBindFramebuffer(GL_FRAMEBUFFER, state->shadowBuffer.framebuffer);
    // glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(0,0,state->th_shadowmap_resolution*state->level.sun_shadow_scale,state->th_shadowmap_resolution*state->level.sun_shadow_scale);




    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);


    glCopyImageSubData(state->shadowCache_tex,GL_TEXTURE_2D,0,0,0,0,state->shadowBuffer.depthTexture,GL_TEXTURE_2D,0,0,0,0,state->th_shadowmap_resolution*state->level.sun_shadow_scale,state->th_shadowmap_resolution*state->level.sun_shadow_scale,1);





    if (state->th_dynamic_objs && state->th_dynamic_shadows)
    {
      if (state->animcommands_count)
      {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->guy_array.cmds.drawCommandBuffer);
        r_bindShader(&state->occluder_anim);
        r_sendmat4(&state->occluder_anim,shadow_viewproj,"modelViewprojection");
        r_sendi(&state->occluder_anim,th_frame() %3,"frame_index");
        th_renderArrayCmdBuffer(&state->guy_array,state->animcommands_count);
      }


      r_bindShader(&state->shadowOccluder);
      r_sendmat4(&state->shadowOccluder,shadow_viewproj,"modelViewprojection");
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->dynamic_shadowcaster_commands.buffer);
      th_renderArrayCmdBuffer(&state->model_data,state->drawcommands_count_dynamicshadowcaster);


      if (state->stream_commands_count > 0 && state->th_dynamic_objs)
      {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->dynamic_geom_array.cmds.drawCommandBuffer);
        th_renderArrayCmdBuffer(&state->dynamic_geom_array,state->stream_commands_count);
      }
    }




    for (int oli = 0 ; oli < state->level.omni_light_count;oli++)
    {
      int idx = (int)(state->level.omni_light_atlascoords[oli].z);
      fn_vec2 resvector = fn_createVec2(state->th_shadowmap_resolution,state->th_shadowmap_resolution);
      th_renderPointShadows(state,state->level.omni_light_positions[oli],&shadow_viewproj_point[idx*6],fn_multVec2(state->level.omni_light_atlascoords[oli].xy,resvector),idx,state->level.omni_light_masks[oli],false);
    }



    glViewport(0,0,w1,h1);

    if (!state->th_dynamic_shadows)
    {
      // r_bindTextureForce(state->shadowCache_tex,GL_TEXTURE_2D,24);
      state->shadowSampled = true;
    }
    //state->shadowSampled = true;
  }

  /*
   *  CHEAP sphere proxy AO
   */

  // if (state->th_ambient_occlusion)
  // {
  //   glBindFramebuffer(GL_FRAMEBUFFER, state->cheap_ao_fb.framebuffer);
  //   //glViewport(0,0,state->screen_dims.x,state->screen_dims.y);
  //   glClear(GL_COLOR_BUFFER_BIT );
  //
  //
  //   r_bindShader(&state->cheap_ao_shader);
  //
  //
  //
  //   r_sendmat4(&state->cheap_ao_shader,modelViewprojection,"modelViewprojection");
  //   r_sendmat4(&state->cheap_ao_shader,state->invProj,"projMatrixInv");
  //   r_sendmat4(&state->cheap_ao_shader,state->invView,"viewMatrixInv");
  //
  //
  //   r_send2f(&state->cheap_ao_shader,fn_divVec2(fn_createVec2(1,1),screenSize),"invWindow");
  //   r_send2f(&state->cheap_ao_shader,screenSize,"screenSize");
  //   r_sendf(&state->cheap_ao_shader,state->zNear,"zNear");
  //   r_sendf(&state->cheap_ao_shader,state->zFar,"zFar");
  //
  //   r_sendmat4(&state->cheap_ao_shader,view,"viewMat");
  //
  //
  //   int occ_num = 0;
  //   fn_vec4* occluders_new = malloc(sizeof(fn_vec4)*state->cheap_ao_data.instancecount);
  //   for (int i = 0 ; i < state->level.ls.boidgroups->boidscount;i++)
  //   {
  //     occluders_new[i] = fn_createVec4(0,0,0,0);
  //   }
  //
  //
  //   for (int i = 0 ; i < state->level.ls.boidgroups->boidscount;i++)
  //   {
  //     if (occ_num < (int)state->cheap_ao_data.instancecount && state->level.ls.boidgroups->entities[i].alive)
  //     {
  //       occluders_new[occ_num] = fn_createVec4Vec3(state->level.ls.boidgroups->entities[i].aabb.position,70.0);
  //       occ_num++;
  //     }
  //
  //   }
  //
  //
  //   memcpy(&state->cheap_ao_array.mappedInstancesOccluder[state->cheap_ao_data.instancecount*(th_frame() % 3)],occluders_new,sizeof(fn_vec4)*state->cheap_ao_data.instancecount);
  //
  //   free(occluders_new);
  //
  //   glEnable(GL_BLEND);
  //   glBlendFunc(GL_ONE, GL_ONE);
  //   glBlendEquation(GL_FUNC_ADD);
  //   glFrontFace(GL_CCW);
  //
  //   glDepthMask(GL_FALSE);
  //   th_renderArray(&state->cheap_ao_array,state->cheap_ao_data.indicecount,state->cheap_ao_data.instancecount,0,0);
  //
  //
  //
  //
  //   glDisable(GL_BLEND);
  //   glDepthMask(GL_TRUE);
  //
  //   glFrontFace(GL_CW);
  // }

  if (state->th_ambient_occlusion)
  {
      int occ_num = 0;

      // occluders_new[occ_num] = fn_createVec4Vec3(fn_createVec3(-447.395905, 783.310547 ,-153.904251
      // ),70.0);
      // occ_num++;


      // for (int i = 0 ; i < state->level.ls.boidgroups->boidscount;i++)
      // {
      //   occluders_new[i] = fn_createVec4(0,0,0,0);
      //   if (occ_num < (int)2048 && state->level.ls.boidgroups->entities[i].alive)
      //   {
      //     occluders_new[occ_num] = fn_createVec4Vec3(state->level.ls.boidgroups->entities[i].aabb.position,70.0);
      //     occ_num++;
      //   }
      //
      // }

      glBindBufferRange(GL_UNIFORM_BUFFER, 10, state->tiledaoUBO,sizeof(fn_vec4)*2048*(th_frame() % 3),sizeof(fn_vec4)*2048);


      th_getOccluders(&state->mapped_ao_volumes[2048*(th_frame() % 3)],&occ_num);
     // memcpy(&state->mapped_ao_volumes[2048*(th_frame() % 3)],occluders_new,sizeof(fn_vec4)*2048);




      //th_printlnDevConsole("%i",occ_num);

    // GLint64 time_begin_ao,time_end_ao;
    //
    // glGetInteger64v(GL_TIMESTAMP,&time_begin_ao);
    glBindFramebuffer(GL_FRAMEBUFFER, state->cheap_ao_fb.framebuffer);
    //glViewport(0,0,state->screen_dims.x,state->screen_dims.y);
    glClear(GL_COLOR_BUFFER_BIT );

    GLuint zero[4];
    zero[0] = 0;
    zero[1] = 0;
    zero[2] = 0;
    zero[3] = 0;

    glClearNamedBufferData(state->tiled_light_lists,GL_RGBA32UI,GL_RGBA_INTEGER,GL_UNSIGNED_INT,zero);

    r_bindShader(&state->tiled_depth_bound_shader);

    r_sendf(&state->tiled_depth_bound_shader,state->zFar,"zFar");
    r_sendf(&state->tiled_depth_bound_shader,state->zNear,"zNear");
    r_sendmat4(&state->tiled_depth_bound_shader,state->invProj,"invProjection");
    r_send2f(&state->tiled_depth_bound_shader,screenSize,"screen");

    int invocs_x = state->num_tiles_x;//(state->screen_dims.x)/16;
    int invocs_y = state->num_tiles_y;//round up

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glDispatchCompute(invocs_x,invocs_y,1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    if (occ_num > 0)
    {

      glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);

      r_bindShader(&state->tiled_raster_shader);


      r_sendmat4(&state->tiled_raster_shader, state->projection,"Projection");
      r_send2f(&state->tiled_raster_shader, fn_createVec2(state->num_tiles_x,state->num_tiles_y),"resolution");
      r_sendmat4(&state->tiled_raster_shader, view,"view");
      r_sendf(&state->tiled_raster_shader, state->zNear,"zNear");


      glViewport(0,0,state->num_tiles_x,state->num_tiles_y);

      glFrontFace(GL_CCW);
      glBindVertexArray(state->tiled_dummy_vao);
      glDrawArraysInstanced(GL_TRIANGLES,0,6,occ_num);
      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
      glFrontFace(GL_CW);


      glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
      glViewport(0,0,state->screen_dims.x,state->screen_dims.y);

    }

    r_bindShader(&state->tiled_ao_shading);

    r_send2f(&state->tiled_ao_shading,invWindow ,"invWindow");
    r_sendmat4(&state->tiled_ao_shading,state->invProj ,"projMatrixInv");
    r_sendmat4(&state->tiled_ao_shading,state->invView ,"viewMatrixInv");
    r_sendmat4(&state->tiled_ao_shading,state->postmatrix,"modelViewprojection");
    th_renderArray(&state->post_gpu,3,0,0,0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    // glGetInteger64v(GL_TIMESTAMP,&time_end_ao);
    // th_printlnDevConsole("%f %i",(double)(time_end_ao - time_begin_ao)/(double)1000000.0,occ_n);










  }




  // float fog_distances[] = {2000.0,3000.0,4000.0};
  float fog_distances[] = {TH_FOG_MIN_DIST,TH_FOG_MED_DIST,TH_FOG_MAX_DIST};


  if (state->th_particles)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, state->forward_fb.framebuffer);

    glClear(GL_COLOR_BUFFER_BIT );
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,state->particle_data.cmds.drawCommandBuffer);
    glEnable(GL_DEPTH_TEST);
    glCopyImageSubData(state->noshading_fb.depthTexture,GL_TEXTURE_2D,0,0,0,0,state->forward_fb.depthTexture,GL_TEXTURE_2D,0,0,0,0,state->screen_dims.x,state->screen_dims.y,1);

    r_bindShader(&state->forward_particle);
    r_sendmat4(&state->forward_particle,modelViewprojection,"modelViewprojection");
    r_sendmat4(&state->forward_particle,view,"view");
    r_sendmat4(&state->forward_particle,state->projection,"proj");

    r_send3f(&state->forward_particle,state->gridpos,"goffset");
    r_send3f(&state->forward_particle,state->griddims,"gridsize");
    r_sendf(&state->forward_particle,state->gridsize,"griddist");
    r_send2f(&state->forward_particle,state->dims_harmtex,"dims_harmtex");
    r_sendmat4v(&state->forward_particle,shadow_viewproj_point,MAX_POINT_SHADOWS,"lightSpaceMatrices");
    r_sendmat4(&state->forward_particle,shadow_viewproj,"lightSpaceMatrix");
    r_sendmat4(&state->forward_particle,state->invProj,"projMatrixInv");
    r_sendmat4(&state->forward_particle,state->invView,"viewMatrixInv");
    r_send3f(&state->forward_particle,state->pos,"eyePos");
    r_send3f(&state->forward_particle,state->shadowCasterPos,"shadowPos");
    r_send2f(&state->forward_particle,invWindow,"invWindow");
    r_send2f(&state->forward_particle,screenSize,"screenSize");
    r_sendf(&state->forward_particle,state->zNear,"zNear");
    r_sendf(&state->forward_particle,state->zFar,"zFar");
    r_sendmat4(&state->forward_particle,view,"viewMat");
    r_sendf(&state->forward_particle,(float)(state->th_shadowmap_resolution),"th_shadowmap_resolution");
    r_send3f(&state->forward_particle,state->level.sundir_light,"sunlightDir");
    r_send3f(&state->forward_particle,state->level.suncolor_light,"sunlightColor");

    r_send3f(&state->forward_particle,state->level.fog_color,"fog_color");
    r_sendf(&state->forward_particle,state->level.fog_gain,"fog_gain");
    r_sendf(&state->forward_particle,state->level.fog_density,"fog_density");
    r_sendf(&state->forward_particle,state->level.fog_ambient_density,"fog_ambient_density");
    r_sendf(&state->forward_particle,state->level.shadow_radius_fog,"shadow_blur_radius_fog");
    r_sendf(&state->forward_particle,state->level.shadow_radius,"shadow_blur_radius");

    r_sendf(&state->forward_particle,state->level.sun_shadow_scale,"th_shadowmap_sun_scale");
    r_sendf(&state->forward_particle,state->level.omni_shadow_scale,"th_shadowmap_omni_scale");

    r_sendf(&state->forward_particle,state->level.mu,"mu");
    r_sendf(&state->forward_particle,state->level.alpha,"alpha");

    float fog_qualities[] = {2.5,2.0,1.5};
    float fog_step = fog_qualities[fn_clampi(state->config_global->fog_quality,0,2)];
    r_sendf(&state->forward_particle,fog_step,"fog_quality");


    float fog_distance = fog_distances[fn_clampi(state->config_global->fog_quality,0,2)];
    r_sendf(&state->forward_particle,fog_distance,"fog_distance");

    // glEnable(GL_BLEND);
    // glBlendFunc(GL_ONE, GL_SRC1_ALPHA);
    // glBlendEquation(GL_FUNC_ADD);
    //
    // glDepthMask(GL_TRUE);
    // th_renderArrayCmdBuffer(&state->particle_data,state->particle_commands_count);
    // glDisable(GL_BLEND);
    // glDepthMask(GL_TRUE);


    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    // glBlendFunc(GL_ONE, GL_SRC1_ALPHA);
    //glBlendFunc(GL_ONE, GL_SRC1_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glDepthMask(GL_FALSE);
    //glDepthMask(GL_TRUE);
    th_renderArrayCmdBuffer(&state->particle_data,state->particle_commands_count);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

  }



  //insert SH GI here
  if (state->th_reflections )
  {
    //glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT );
    r_bindShader(&state->cubemaps);
    r_send3f(&state->cubemaps,state->pos,"eyePos");
    r_sendmat4(&state->cubemaps,state->postmatrix,"modelViewprojection");
    r_sendmat4(&state->cubemaps,state->invProj,"projMatrixInv");
    r_sendmat4(&state->cubemaps,state->invView,"viewMatrixInv");
    r_sendmat4(&state->cubemaps,proj,"projMat");
    r_sendmat4(&state->cubemaps,view,"viewMat");
    r_send2f(&state->cubemaps,fn_multVec2(screenSize,fn_createVec2(state->reflections_scale,state->reflections_scale)),"screenSize");
    r_sendf(&state->cubemaps,state->zNear,"zNear");
    r_sendf(&state->cubemaps,state->zFar,"zFar");
    r_sendui(&state->cubemaps,th_frame(),"frame");
    r_sendmat4(&state->cubemaps,state->old_mvp,"old_mvp");
    r_sendmat4(&state->cubemaps,state->projMatrixInv_old,"projMatrixInv_old");
    r_sendmat4(&state->cubemaps,state->viewMatrixInv_old,"viewMatrixInv_old");
    r_sendf(&state->cubemaps,state->level.cube_nohit_sky,"cube_nohit_sky");
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT );
    glDispatchCompute((state->screen_dims.x*state->reflections_scale)/8,(state->screen_dims.y*state->reflections_scale)/4,1);


    if (state->th_temporalfilter)
    {
      glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      r_bindShader(&state->temporal_shader);
      r_sendui(&state->temporal_shader,th_frame(),"frame");
      r_sendmat4(&state->temporal_shader,state->postmatrix,"modelViewprojection");
      r_sendmat4(&state->temporal_shader,state->invProj,"projMatrixInv");
      r_sendmat4(&state->temporal_shader,state->invView,"viewMatrixInv");
      r_sendmat4(&state->temporal_shader,proj,"projMat");
      r_sendmat4(&state->temporal_shader,view,"viewMat");
      r_send2f(&state->temporal_shader,fn_multVec2(screenSize,fn_createVec2(1,1)),"screenSize");
      r_sendmat4(&state->temporal_shader,state->old_mvp,"old_mvp");
      r_sendmat4(&state->temporal_shader,state->projMatrixInv_old,"projMatrixInv_old");
      r_sendmat4(&state->temporal_shader,state->viewMatrixInv_old,"viewMatrixInv_old");
      r_send3f(&state->temporal_shader,state->pos,"eyePos");
      r_sendf(&state->temporal_shader,state->zNear,"zNear");
      r_sendf(&state->temporal_shader,state->zFar,"zFar");
      glDispatchCompute((state->screen_dims.x)/8,(state->screen_dims.y)/8,1);
      // th_renderArray(&state->post_gpu,3,0,0,0);

    }

    if (state->th_spatial_filter)
    {
      // glBindFramebuffer(GL_FRAMEBUFFER, state->blurpass_fb.framebuffer);
      //  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT );
      glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      r_bindShader(&state->median_blur);
      r_sendmat4(&state->median_blur,state->postmatrix,"modelViewprojection");
      r_send2f(&state->median_blur,fn_multVec2(screenSize,fn_createVec2(1,1)),"screenSize");
      // th_renderArray(&state->post_gpu,3,0,0,0);
      glDispatchCompute((state->screen_dims.x)/8,(state->screen_dims.y)/8,1);

      glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    }




  }



  glDisable(GL_DEPTH_TEST);
  if (state->th_fog)
  {

    glBindFramebuffer(GL_FRAMEBUFFER, state->fogpass_fb.framebuffer);
    glViewport(0,0,state->screen_dims.x*state->fog_scale,state->screen_dims.y*state->fog_scale);
    glClear(GL_COLOR_BUFFER_BIT );

    r_bindShader(&state->fog_shader);
    r_send3f(&state->fog_shader,state->gridpos,"goffset");
    r_send3f(&state->fog_shader,state->griddims,"gridsize");
    r_sendf(&state->fog_shader,state->gridsize,"griddist");

    r_send3f(&state->fog_shader,state->level.fog_color,"fog_color");
    r_sendf(&state->fog_shader,state->level.fog_gain,"fog_gain");
    r_sendf(&state->fog_shader,state->level.fog_density,"fog_density");
    r_sendf(&state->fog_shader,state->level.fog_ambient_density,"fog_ambient_density");
    r_sendf(&state->fog_shader,state->level.shadow_radius_fog,"shadow_blur_radius_fog");
    r_sendf(&state->fog_shader,state->level.sun_shadow_scale,"th_shadowmap_sun_scale");
    r_sendf(&state->fog_shader,state->level.omni_shadow_scale,"th_shadowmap_omni_scale");

    r_sendf(&state->fog_shader,state->level.mu,"mu");
    r_sendf(&state->fog_shader,state->level.alpha,"alpha");
    // printf("%s\n","AAA" );
    //   printf("%f %f %f\n",gridpos.x,gridpos.y,gridpos.z );
    //   printf("%f %f %f\n",griddims.x,griddims.y,griddims.z );
    //   printf("%f\n",gridsize);

    r_send2f(&state->fog_shader,state->dims_harmtex,"dims_harmtex");

    r_sendmat4v(&state->fog_shader,shadow_viewproj_point,MAX_POINT_SHADOWS,"lightSpaceMatrices");
    r_sendmat4(&state->fog_shader,shadow_viewproj,"lightSpaceMatrix");
    r_sendmat4(&state->fog_shader,state->postmatrix,"modelViewprojection");
    r_sendmat4(&state->fog_shader,state->invProj,"projMatrixInv");
    r_sendmat4(&state->fog_shader,state->invView,"viewMatrixInv");
    r_send3f(&state->fog_shader,state->pos,"eyePos");
    r_send3f(&state->fog_shader,state->shadowCasterPos,"shadowPos");
    r_send2f(&state->fog_shader,fn_divVec2(fn_createVec2(1,1),fn_multVec2(screenSize,fn_createVec2(state->fog_scale,state->fog_scale))),"invWindow");
    r_send2f(&state->fog_shader,fn_multVec2(screenSize,fn_createVec2(state->fog_scale,state->fog_scale)),"screenSize");
    r_sendf(&state->fog_shader,state->zNear,"zNear");
    r_sendf(&state->fog_shader,state->zFar,"zFar");
    r_sendf(&state->fog_shader,state->th_skyRadianceBoost,"sky_boost");
    r_sendmat4(&state->fog_shader,view,"viewMat");
    r_sendf(&state->fog_shader,(float)(state->th_shadowmap_resolution),"th_shadowmap_resolution");

    r_send3f(&state->fog_shader,state->level.sundir_light,"sunlightDir");
    r_send3f(&state->fog_shader,state->level.suncolor_light,"sunlightColor");

    float fog_qualities[] = {2.0,1.0,1.0};
    float fog_step = fog_qualities[fn_clampi(state->config_global->fog_quality,0,2)];
    r_sendf(&state->fog_shader,fog_step,"fog_quality");


    float fog_distance = fog_distances[fn_clampi(state->config_global->fog_quality,0,2)];
    r_sendf(&state->fog_shader,fog_distance,"fog_distance");

    //r_sendf(&state->fog_shader,th_time(),"time");

    //  glViewport(0,0,screenSize.x,screenSize.y);
    //  glDispatchCompute((state->screen_dims.x)/32,(state->screen_dims.y)/30,1);
    th_renderArray(&state->post_gpu,3,0,0,0);
    glViewport(0,0,state->screen_dims.x,state->screen_dims.y);

    glBindFramebuffer(GL_FRAMEBUFFER, state->blurfog_a.framebuffer);
    r_bindShader(&state->fog_blur_shader_a);
    r_send2f(&state->fog_blur_shader_a,screenSize,"screenSize");
    r_send2f(&state->fog_blur_shader_a,fn_multVec2(screenSize,fn_createVec2(state->fog_scale,state->fog_scale)),"scaledres");
    r_sendf(&state->fog_blur_shader_a,state->fog_scale,"fog_scale");
    r_sendmat4(&state->fog_blur_shader_a,state->postmatrix,"modelViewprojection");
    th_renderArray(&state->post_gpu,3,0,0,0);

    glBindFramebuffer(GL_FRAMEBUFFER, state->blurfog_b.framebuffer);
    r_bindShader(&state->fog_blur_shader_b);
    r_send2f(&state->fog_blur_shader_b,screenSize,"screenSize");
    r_sendmat4(&state->fog_blur_shader_b,state->postmatrix,"modelViewprojection");
    th_renderArray(&state->post_gpu,3,0,0,0);
  }












  //final shading

  if (state->th_render_to_cubemap)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, state->cubetarget_fb.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + state->th_render_to_cubemap_face, state->th_render_to_cubemap_texture, 0);
  }
  else if(state->th_render_to_buffer)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, state->cubetarget_fb.framebuffer);
  }
  else
  {
    if (state->config_global->borderless_fullscreen && state->config_global->upscale_mode != TH_RAWOUTPUT)
    {
      glBindFramebuffer(GL_FRAMEBUFFER, state->internal_fb.framebuffer);
    }
    else
    {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

  }

  // glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT );
  if (state->th_batch_render)
  {
    r_bindShader(&state->pbr_deferred);
    r_send3f(&state->pbr_deferred,state->gridpos,"goffset");
    r_send3f(&state->pbr_deferred,state->griddims,"gridsize");
    r_sendf(&state->pbr_deferred,state->gridsize,"griddist");

    r_send3f(&state->pbr_deferred,state->level.occ_gridpos,"goffset_occ");
    r_send3f(&state->pbr_deferred,state->level.occ_griddims,"gridsize_occ");
    r_sendf(&state->pbr_deferred,state->level.occ_gridsize,"griddist_occ");

    r_send2f(&state->pbr_deferred,state->dims_harmtex,"dims_harmtex");

    r_sendmat4v(&state->pbr_deferred,shadow_viewproj_point,MAX_POINT_SHADOWS,"lightSpaceMatrices");
    r_sendmat4(&state->pbr_deferred,shadow_viewproj,"lightSpaceMatrix");
    r_sendmat4(&state->pbr_deferred,state->postmatrix,"modelViewprojection");

    r_send3f(&state->pbr_deferred,state->shadowCasterPos,"shadowPos");

    r_sendf(&state->pbr_deferred,state->zNear,"zNear");
    r_sendf(&state->pbr_deferred,state->zFar,"zFar");
    r_sendf(&state->pbr_deferred,state->th_crosshair_size,"th_crosshair_size");
    r_sendf(&state->pbr_deferred,state->th_skyRadianceBoost,"sky_boost");

    r_sendf(&state->pbr_deferred,(float)(state->th_shadowmap_resolution),"th_shadowmap_resolution");

    r_send3f(&state->pbr_deferred,state->level.sundir_light,"sunlightDir");
    r_send3f(&state->pbr_deferred,state->level.suncolor_light,"sunlightColor");

    r_sendf(&state->pbr_deferred,state->level.shadow_radius,"shadow_blur_radius");

    r_sendf(&state->pbr_deferred,state->level.sun_shadow_scale,"th_shadowmap_sun_scale");
    r_sendf(&state->pbr_deferred,state->level.omni_shadow_scale,"th_shadowmap_omni_scale");
    r_sendf(&state->pbr_deferred,state->level.omni_light_jitter,"omni_light_jitter");
    //r_sendf(&state->pbr_deferred,th_time(),"time");
    fn_vec3 gl_color;
    float gl_amt;
    th_getGameGlow(&gl_color,&gl_amt);

    r_send3f(&state->pbr_deferred,gl_color,"glow_color");
    r_sendf(&state->pbr_deferred,gl_amt,"glow_factor");



    int x_dim = (state->batch_atlas_res / 32);
    sub = (state->th_dynamic_objs) ? 0 : state->drawcommands_dynamic_count_fc;

    state->invProj = fn_inverse(state->projection);
    r_sendmat4(&state->pbr_deferred,state->projection,"projMat");

    r_send2f(&state->pbr_deferred,fn_createVec2(1.0/state->batch_atlas_res,1.0/state->batch_atlas_res),"invWindow");
    r_send2f(&state->pbr_deferred,fn_createVec2(state->batch_atlas_res,state->batch_atlas_res),"screenSize");

    for (int i = 0 ; i < state->num_batches;i++)
    {
      int x_coord = i % x_dim;
      int y_coord = i / x_dim;

      glViewport(x_coord*32,y_coord*32,resolution,resolution);
      r_send3f(&state->pbr_deferred,state->positions[i],"eyePos");

      r_sendmat4(&state->pbr_deferred,state->invProj,"projMatrixInv");
      r_sendmat4(&state->pbr_deferred,fn_inverse(state->viewmats[i]),"viewMatrixInv");
      r_sendmat4(&state->pbr_deferred,state->viewmats[i],"viewMat");


      th_renderArray(&state->post_gpu,3,0,0,0);
    }


  }
  else
  {
    r_bindShader(&state->pbr_deferred);
    r_send3f(&state->pbr_deferred,state->gridpos,"goffset");
    r_send3f(&state->pbr_deferred,state->griddims,"gridsize");
    r_sendf(&state->pbr_deferred,state->gridsize,"griddist");

    r_send3f(&state->pbr_deferred,state->level.occ_gridpos,"goffset_occ");
    r_send3f(&state->pbr_deferred,state->level.occ_griddims,"gridsize_occ");
    r_sendf(&state->pbr_deferred,state->level.occ_gridsize,"griddist_occ");

    r_send2f(&state->pbr_deferred,state->dims_harmtex,"dims_harmtex");

    r_sendmat4v(&state->pbr_deferred,shadow_viewproj_point,MAX_POINT_SHADOWS,"lightSpaceMatrices");
    r_sendmat4(&state->pbr_deferred,shadow_viewproj,"lightSpaceMatrix");
    r_sendmat4(&state->pbr_deferred,state->postmatrix,"modelViewprojection");
    r_sendmat4(&state->pbr_deferred,state->invProj,"projMatrixInv");
    r_sendmat4(&state->pbr_deferred,state->invView,"viewMatrixInv");
    r_send3f(&state->pbr_deferred,state->pos,"eyePos");
    r_send3f(&state->pbr_deferred,state->shadowCasterPos,"shadowPos");
    r_send2f(&state->pbr_deferred,invWindow,"invWindow");
    r_send2f(&state->pbr_deferred,screenSize,"screenSize");
    r_sendf(&state->pbr_deferred,state->zNear,"zNear");
    r_sendf(&state->pbr_deferred,state->zFar,"zFar");

    float cross_size = state->th_crosshair_size;
    if (state->th_inphoto)
    {
      cross_size = -1.0;
    }

    r_sendf(&state->pbr_deferred,cross_size,"th_crosshair_size");
    r_sendf(&state->pbr_deferred,state->th_skyRadianceBoost,"sky_boost");
    r_sendmat4(&state->pbr_deferred,view,"viewMat");
    r_sendmat4(&state->pbr_deferred,state->projection,"projMat");
    r_sendf(&state->pbr_deferred,(float)(state->th_shadowmap_resolution),"th_shadowmap_resolution");

    r_send3f(&state->pbr_deferred,state->level.sundir_light,"sunlightDir");
    r_send3f(&state->pbr_deferred,state->level.suncolor_light,"sunlightColor");
    r_sendf(&state->pbr_deferred,state->level.shadow_radius,"shadow_blur_radius");
    r_sendf(&state->pbr_deferred,state->level.sun_shadow_scale,"th_shadowmap_sun_scale");
    r_sendf(&state->pbr_deferred,state->level.omni_shadow_scale,"th_shadowmap_omni_scale");
    r_sendf(&state->pbr_deferred,state->level.omni_light_jitter,"omni_light_jitter");

    // r_send3f(&state->pbr_deferred,th_computeBlackBody(state->level.ls.barrel_color_interp),"blackbody_barrel_color");

    r_sendf(&state->pbr_deferred,state->level.ls.barrel_color_interp,"blackbody_barrel_tip_temp");

    //0.34 0.41

    float bb_shape = fn_lerp(state->level.ls.barrel_shape_min,state->level.ls.barrel_shape_max,state->level.ls.barrel_color_interp);

    r_sendf(&state->pbr_deferred,bb_shape,"blackbody_barrel_shape");

    r_sendf(&state->pbr_deferred,state->level.ls.barrel_maxtemp,"blackbody_maxtemp");

    r_send3f(&state->pbr_deferred,state->level.ls.blackbody_barrel_pointa,"blackbody_barrel_pointa");

    r_send3f(&state->pbr_deferred,state->level.ls.blackbody_barrel_pointb,"blackbody_barrel_pointb");

    r_sendf(&state->pbr_deferred,state->gamma_display,"displayGamma");
    r_sendf(&state->pbr_deferred,state->exposure_display,"displayExposure");

    // // blackbody_barrel_tip_temp
    // // blackbody_barrel_shape
    // blackbody_barrel_pointa
    // blackbody_barrel_pointb


    //r_sendf(&state->pbr_deferred,th_time(),"time");
    fn_vec3 gl_color;
    float gl_amt;
    th_getGameGlow(&gl_color,&gl_amt);

    if (state->th_inphoto)
    {
      gl_amt = 0.0;
    }

    r_send3f(&state->pbr_deferred,gl_color,"glow_color");
    r_sendf(&state->pbr_deferred,gl_amt,"glow_factor");

    th_renderArray(&state->post_gpu,3,0,0,0);
  }


  //glDrawTextureNV(state->final_image_texture,0,0,0,state->screen_dims.x,state->screen_dims.y,1,0,0,1,1);
  if (state->th_dynamic_objs && state->th_renderui)
  {
    if (state->gameState == STATE_GAMEPLAY && !state->th_no_hud && !state->th_inphoto)
    {
      for (int i = 0 ; i < state->level.text_commands_count;i++)
      {
        th_TextCommand cmd = state->level.text_commands[i];
        th_renderTextGlyph(cmd.string,fn_multVec2(cmd.pos_tx,screenSize),cmd.size,cmd.font,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,character_map,cmd.color);
      }

      //slide indicator
      state->hudLayout.slide_element_indicator->highlighted = true;
      if ( (th_time() > th_get_slide_timer() + 1500 ))
      {
        state->hudLayout.slide_element_indicator->highlighted = false;
      }

      /*
       * TH_HAMMER
       * TH_MACHINEGUN
       * TH_SHOTGUN
       */

      for (int i = 0 ; i < 3;i++)
      {
        state->hudLayout.elements[state->hudLayout.highlighted_element + i*3 + 0 ].alpha = 0.2;
        state->hudLayout.elements[state->hudLayout.highlighted_element + i*3 + 1 ].alpha = 0.2;
        state->hudLayout.elements[state->hudLayout.highlighted_element + i*3 + 2 ].alpha = 0.2;
      }


      state->hudLayout.hammer_level = state->level.levelstate.player->level_weapon[TH_HAMMER];
      state->hudLayout.machinegun_level = state->level.levelstate.player->level_weapon[TH_MACHINEGUN];
      state->hudLayout.shotgun_level = state->level.levelstate.player->level_weapon[TH_SHOTGUN];

      if (state->level.levelstate.weapon != NULL  )
      {
        if (state->level.levelstate.weapon->chosen_weapon == TH_HAMMER)
        {
          int level_offset = (state->hudLayout.hammer_level) - 1;
          state->hudLayout.elements[state->hudLayout.highlighted_element+0 + level_offset].alpha = 1.0;
        }
        else if (state->level.levelstate.weapon->chosen_weapon == TH_MACHINEGUN)
        {
          int level_offset = (state->hudLayout.machinegun_level) - 1;
          state->hudLayout.elements[state->hudLayout.highlighted_element+3 + level_offset].alpha = 1.0;
        }
        else if (state->level.levelstate.weapon->chosen_weapon == TH_SHOTGUN)
        {
          int level_offset = (state->hudLayout.shotgun_level) - 1;
          state->hudLayout.elements[state->hudLayout.highlighted_element+6 + level_offset].alpha = 1.0;
        }
      }

      if (state->level.levelstate.progress_state == TH_BAR_ONE)
      {
        state->hudLayout.elements[state->hudLayout.status_offset + 0].visible = true;
        state->hudLayout.elements[state->hudLayout.status_offset + 1].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 2].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 4].visible = false;

        state->hudLayout.elements[state->hudLayout.status_offset + 3].tint = fn_createVec3s(0.1);


        float alpha = (float)state->level.ls.num_spawners_current / (float) state->level.ls.num_spawners_total;
        alpha = 1.0 - alpha;

        state->hudLayout.elements[state->hudLayout.status_offset + 3].alpha = alpha;

        if (alpha == 0.0)
        {
          if (state->level.ls.num_spawners_killed == state->level.ls.num_spawners_total)
          {
            state->level.levelstate.progress_state = TH_BAR_THREE;
          }
          else
          {
            state->level.levelstate.progress_state = TH_BAR_TWO;
          }
        }
      }
      else if (state->level.levelstate.progress_state == TH_BAR_TWO)
      {
        state->hudLayout.elements[state->hudLayout.status_offset + 0].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 1].visible = true;
        state->hudLayout.elements[state->hudLayout.status_offset + 2].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 4].visible = false;

        state->hudLayout.elements[state->hudLayout.status_offset + 3].tint = fn_createVec3(0.9,0.2,0.0);

        float alpha = (float)state->level.ls.num_spawners_killed / (float) state->level.ls.num_spawners_total;
        alpha = 1.0 - alpha;

        state->hudLayout.elements[state->hudLayout.status_offset + 3].alpha = alpha;

        if (alpha == 0.0)
        {
          state->level.levelstate.progress_state = TH_BAR_THREE;
          state->level.levelstate.num_enemies_highwater = th_getEnemyCount();
        }
      }
      else if (state->level.levelstate.progress_state == TH_BAR_THREE)
      {
        state->hudLayout.elements[state->hudLayout.status_offset + 0].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 1].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 2].visible = true;
        state->hudLayout.elements[state->hudLayout.status_offset + 4].visible = false;

        state->hudLayout.elements[state->hudLayout.status_offset + 3].tint = fn_createVec3(0.2,0.7,0.2);

        float alpha = (float)th_getEnemyCount() / (float) state->level.levelstate.num_enemies_highwater ;
        float alpha_preclamp = fn_clamp(alpha,0.001,1.0);
        alpha = fn_clamp(alpha,0.05,1.0);

        state->hudLayout.elements[state->hudLayout.status_offset + 3].alpha = alpha;




        if (alpha_preclamp < 0.05 || (th_getEnemyCount() < 5 && state->level.levelstate.num_enemies_highwater > 1 ))
        {
          th_timer_t killtime_seconds = (th_getKillTimer() - th_time())/1000.0;
          killtime_seconds = killtime_seconds > 0.0 ? killtime_seconds : 0.0;
          static char killtime_str[16];
          snprintf(killtime_str,16,"%.2f",killtime_seconds);

          float w_op = th_stringDims("00.00",0.333,character_map).x;

          state->hudLayout.elements[state->hudLayout.status_offset + 2].text = killtime_str;
          state->hudLayout.elements[state->hudLayout.status_offset + 2].position.x = 0.5*state->screen_dims.x - w_op*0.5;
        }
        else
        {
          float w_op = th_stringDims("Enemies Remaining",0.333,character_map).x;

          state->hudLayout.elements[state->hudLayout.status_offset + 2].text = "Enemies Remaining";
          state->hudLayout.elements[state->hudLayout.status_offset + 2].position.x = 0.5*state->screen_dims.x - w_op*0.5;
        }



      }
      else if (state->level.levelstate.progress_state == TH_BAR_FOUR)
      {
        state->hudLayout.elements[state->hudLayout.status_offset + 0].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 1].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 2].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 4].visible = true;

        state->hudLayout.elements[state->hudLayout.status_offset + 3].tint = fn_lerpVec3(fn_createVec3(1,0.64,0),fn_createVec3(1,1,1),0.2);


        float alpha = state->level.levelstate.tutorial == NULL ? 0.0 : state->level.levelstate.tutorial->tutorial_progress_pct;//(float)th_getEnemyCount() / (float) state->level.levelstate.num_enemies_highwater ;
        alpha = fn_clamp(alpha,0.05,1.0);

        state->hudLayout.elements[state->hudLayout.status_offset + 3].alpha = alpha;
      }
      else
      {
        state->hudLayout.elements[state->hudLayout.status_offset + 0].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 1].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 2].visible = false;
        state->hudLayout.elements[state->hudLayout.status_offset + 4].visible = false;

        state->hudLayout.elements[state->hudLayout.status_offset + 3].alpha = 0.0;
      }


      // state->hudLayout.elements[state->hudLayout.status_offset + 0].visible = false;
      // state->hudLayout.elements[state->hudLayout.status_offset + 1].visible = false;
      // state->hudLayout.elements[state->hudLayout.status_offset + 2].visible = false;
      //
      // state->hudLayout.elements[state->hudLayout.status_offset + 3].alpha = 0.0;






      //
      //
      //fn_createVec3(0.9,0.2,0.0); erosion tint
      //fn_createVec3(0.2,0.7,0.2); catharsis tint
      //fn_createVec3s(0.1); intrusion tint

      th_processUINoInput(&state->hudLayout,state->screen_dims);
      th_renderUI(&state->hudLayout,state);
    }





    if (state->gameState == STATE_MAINMENU)
    {
      th_renderUI(&state->mainMenuLayout,state);
    }

    if (state->gameState == STATE_OPTIONS_MENU)
    {
      th_renderUI(&state->optionsMenuLayout,state);
    }

    if (state->gameState == STATE_LEVELSELECT)
    {
      th_renderUI(&state->levelSelectLayout,state);
    }

    if (state->gameState == STATE_PAUSED)
    {
      th_renderUI(&state->pauseMenuLayout,state);
    }

    if (state->gameState == STATE_DEATH)
    {
      th_renderUI(&state->deathMenuLayout,state);
    }

    if (state->gameState == STATE_VICTORY)
    {
      th_renderUI(&state->victoryMenuLayout,state);
    }

    th_UiNagInfo ninfo;
    th_getUiNagbar(&ninfo);
    if (ninfo.active)
    {
      th_renderTextGlyph(ninfo.text,ninfo.position,ninfo.size,1.0,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,character_map,ninfo.color);
    }

    bool one_nagbar_active = false;
    fn_vec2 npos_min = fn_createVec2(10000000,10000000);
    fn_vec2 npos_max = fn_createVec2(-10000000,-10000000);

    fn_vec2 str_dims_max = fn_createVec2(-1,-1);
    float chwidth = 0.0;
    for (int i = 0 ; i < N_NAGBARS;i++)
    {
      th_getUiNagbarN(&ninfo,i);
      if (ninfo.active)
      {
        one_nagbar_active = true;
        npos_min.x = npos_min.x > ninfo.position.x ? ninfo.position.x : npos_min.x;
        npos_min.y = npos_min.y > ninfo.position.y ? ninfo.position.y : npos_min.y;

        npos_max.x = npos_max.x < ninfo.position.x ? ninfo.position.x : npos_max.x;
        npos_max.y = npos_max.y < ninfo.position.y ? ninfo.position.y : npos_max.y;

        fn_vec2 str_dims = th_stringDims(ninfo.text,ninfo.size,character_map);

        th_Character ch = character_map[(unsigned char)('A')];
        chwidth = (ch.advance >> 6) * ninfo.size;

        str_dims_max.x = fn_max(str_dims_max.x,str_dims.x);
        str_dims_max.y = fn_max(str_dims_max.y,str_dims.y);
      }
    }

    if (one_nagbar_active)
    {


      fn_vec2 pos_quad = fn_subVec2(npos_min , fn_createVec2(chwidth * 1.0,str_dims_max.y*0.5));//fn_multVec2(state->screen_dims,fn_createVec2(0.5,0.5));
      fn_vec2 size_quad = fn_createVec2(str_dims_max.x,(npos_max.y - npos_min.y) + str_dims_max.y*2.0);//fn_multVec2(state->screen_dims,fn_createVec2(0.1,0.1));
      th_renderShaderQuad(pos_quad,size_quad,&state->gradient_shader,state->postmatrix,&state->text_gpu,&state->text_data,2.0,fn_createVec3(0,0,0));
    }

    for (int i = 0 ; i < N_NAGBARS;i++)
    {
      th_getUiNagbarN(&ninfo,i);
      if (ninfo.active)
      {
        th_renderTextGlyph(ninfo.text,ninfo.position,ninfo.size,1.0,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,character_map,ninfo.color);

      }
    }


    //th_renderTextGlyph("",fn_createVec2(0,0),1,1,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,character_map,fn_createVec3(1,1,1));
    // r_bindShader(&state->glyph_shader);
    // r_sendmat4(&state->glyph_shader,state->postmatrix,"modelViewprojection");


    //glActiveTexture(GL_TEXTURE0 + 35);

    //glEnable(GL_BLEND);



    if (state->th_devconsole)
    {

      int lines = 0;
      char** strings = th_getDevConsole(&lines);
      int index = lines - 1;
      for (float yi = 1; yi <= lines; yi++) {
        index--;

        if (strlen(strings[index + 1]) != 0)
        {
          float y = (1080.0 - yi*64.0*(screenSize.y/1080.0))/1080.0;
          th_renderTextGlyph(strings[index + 1],fn_multVec2(fn_createVec2(0,y),screenSize),0.5,0,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,character_map,fn_createVec3(1,1,1));
        }

      }

    }

    // th_renderTextGlyph("Test",fn_createVec2(100,100),5,0,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,character_map);
  }
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);




  // glDepthRangef(0,1);
  glDepthMask(GL_TRUE);



  glDisable(GL_DEPTH_TEST);

  glColorMaski(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  glColorMaski(1,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
  if (state->th_render_to_cubemap || state->th_render_to_buffer)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  if (state->config_global->borderless_fullscreen && state->config_global->upscale_mode != TH_RAWOUTPUT)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int viewportW = (int)(state->config_global->width * state->config_global->scale_factor);
    int viewportH = (int)(state->config_global->height * state->config_global->scale_factor);

    int viewportX = (state->config_global->d_width - viewportW) / 2;
    int viewportY = (state->config_global->d_height - viewportH) / 2;
    glClear(GL_COLOR_BUFFER_BIT);

    glBlitNamedFramebuffer(state->internal_fb.framebuffer,
                            0,
                            0,
                            0,
                            state->config_global->width,
                            state->config_global->height,
                            viewportX,
                            viewportY,
                            viewportX+viewportW,
                            viewportY+viewportH,
                            GL_COLOR_BUFFER_BIT,
                            GL_LINEAR);
  }



  if (state->th_temporalfilter)
  {


    glCopyImageSubData(state->reflections_fb.textures[0],GL_TEXTURE_2D,0,0,0,0,state->old_reflectiondata,GL_TEXTURE_2D,0,0,0,0,state->screen_dims.x,state->screen_dims.y,1);
    glCopyImageSubData(state->noshading_fb.depthTexture,GL_TEXTURE_2D,0,0,0,0,state->old_depthtexture,GL_TEXTURE_2D,0,0,0,0,state->screen_dims.x,state->screen_dims.y,1);
    glCopyImageSubData(state->noshading_fb.textures[1],GL_TEXTURE_2D,0,0,0,0,state->oldnormalTex,GL_TEXTURE_2D,0,0,0,0,state->screen_dims.x,state->screen_dims.y,1);


  }

  //th_printlnDevConsole("Render %i",SDL_GetTicks() - start_time);



  // th_endQuery(&state->timer);
  // th_printQuery(&state->timer);
  // {
  // unsigned int attachments[4] = { GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2,GL_COLOR_ATTACHMENT3};
  // glDrawBuffers(4,attachments );
  // }

  th_syncTripleSync(state->bonesSyncs);

  //fn_getGLError();


}
void th_updateRenderMatrices(th_RendererState* state)
{
  if (state->th_inphoto)
  {
    state->pos = state->photo_camera_pos;
    state->angles = state->photo_camera_angles;
    state->view = r_camera(state->pos,state->photo_camera_angles);
  }
  else
  {
    state->pos = state->level.levelstate.player_e.aabb.position;

    float screenshake_f = state->level.ls.player->screenshake_f;
    float screenshake_t = state->level.ls.player->screenshake_t;
    float screenshake_amplitude = state->level.ls.player->screenshake_amplitude;


    fn_vec2 noisy_angles = fn_addVec2(state->angles,fn_createVec2(sin( 2.3 + screenshake_t)*screenshake_amplitude,sin(1.37 + screenshake_t*0.9230)*screenshake_amplitude));
    state->view = r_camera(state->pos,noisy_angles);
  }





  if (fn_isIdentity(state->old_mvp))
  {
    state->old_mvp = fn_multMat4(state->view,state->projection);;
    state->projMatrixInv_old = fn_inverse(state->projection);
    state->viewMatrixInv_old = fn_inverse(state->view);
  }
  else
  {
    state->old_mvp = state->modelViewprojection;
    state->projMatrixInv_old = state->invProj;
    state->viewMatrixInv_old = state->invView;
  }


  //  fn_mat4 shadow_proj = fn_ortho3D(-263,230,-269,61,512,-118);
  state->projection = fn_perspective(fn_radians(state->config_global->fov*state->level.ls.player->fov_delta),state->screen_dims.x/state->screen_dims.y,state->zNear,state->zFar);
  state->modelViewprojection = fn_multMat4(state->view,state->projection);
  state->invProj = fn_inverse(state->projection);
  state->invView = fn_inverse(state->view);
}

void th_runUI(th_RendererState* state,fn_RawInput* input)
{
  fn_RawInput input_copy = *input;

  if (state->gameState == STATE_MAINMENU)
  {
    th_processUI(&state->mainMenuLayout,&input_copy,state->screen_dims);
  }

  if (state->gameState == STATE_OPTIONS_MENU)
  {
    th_processUI(&state->optionsMenuLayout,&input_copy,state->screen_dims);
  }

  if (state->gameState == STATE_LEVELSELECT)
  {
    th_processUI(&state->levelSelectLayout,&input_copy,state->screen_dims);
  }

  if (state->gameState == STATE_PAUSED)
  {
    th_processUI(&state->pauseMenuLayout,&input_copy,state->screen_dims);
  }

  if (state->gameState == STATE_DEATH)
  {
    th_processUI(&state->deathMenuLayout,&input_copy,state->screen_dims);
  }

  if (state->gameState == STATE_VICTORY)
  {
    th_processUI(&state->victoryMenuLayout,&input_copy,state->screen_dims);
  }
}





void th_runProgram(th_RendererState* state,fn_RawInput* input,float dt,a_AudioSystem* audiosystem,fn_Config* config)
{


  // if (input->currentKeyStates[SDL_SCANCODE_H] && !input->currentKeyStatesPrev[SDL_SCANCODE_H])
  // {
  //   debug_key = !debug_key;
  // }

  th_runUI(state,input);




  // int a = 0;
  // int b = 1;
  // float tween_value = 0;
  //print_allocated("current");
  th_updateCapturePlayback(state->rp);


  const float sensitivity = 0.01745329;//0.005;
  Uint32 start_time = SDL_GetTicks();//0.01
  fn_vec3 gaze = fn_createVec3(0,0,1);
  if (state->th_inphoto)
  {
    if (!state->photo_camera_backup && !state->photo_camera_locked)
    {
      state->photo_camera_angles.x += input->xrel*sensitivity*config->mouseSens;
      state->photo_camera_angles.y +=-input->yrel*sensitivity*config->mouseSens;
    }

    if (state->photo_camera_follow)
    {
      //find closest eyeball
      fn_vec3 spos = state->photo_camera_pos;
      if(state->level.ls.eyeball != NULL)
      {
        int idx = -1;
        float sq_dist = 10000000000;
        for (int i = 0 ; i < state->level.ls.eyeball->count;i++)
        {
          float newdist = fn_distance2(spos,state->level.ls.eyeball->entities[i].aabb.position);
          if (state->level.ls.eyeball->entities[i].alive &&  newdist < sq_dist)
          {
            sq_dist = newdist;
            idx = i;
          }
        }

        if (idx != -1)
        {
          state->photo_camera_target = state->level.ls.eyeball->entities[idx].aabb.position;
          gaze = state->level.ls.eyeball->data[idx].gaze;
        }
      }
    }

    if (state->photo_camera_track)
    {
      fn_vec3 lookdir = fn_normalizeVec3(fn_subVec3(state->photo_camera_target,state->photo_camera_pos));


       state->photo_camera_angles.x = -atan2(lookdir.x,lookdir.z);

       state->photo_camera_angles.y = asin(-lookdir.y);
    }

    float angle_per_ms_x = state->photo_camera_pan_x/state->photo_camera_time_to_pan;
    float angle_per_ms_y = state->photo_camera_pan_y/state->photo_camera_time_to_pan;
    if (state->photo_camera_backup)
    {


      // float delta_x = last_good_angles.x - state->photo_camera_angles.x;
      // float dx_sign = fn_sign(delta_x);
      //
      //
      // state->photo_camera_angles.x = fabs(delta_x) > angle_per_ms*state->actual_dt ? dx_sign*angle_per_ms*state->actual_dt + state->photo_camera_angles.x : last_good_angles.x;



      float delta_x = last_good_angles.x - state->photo_camera_angles.x;



      // Wrap delta to [-PI, PI] to find shortest arc direction
      if (delta_x > 3.14159265359f)       delta_x -= TWO_PI;
      else if (delta_x < -3.14159265359f) delta_x += TWO_PI;

      float dx_sign = fn_sign(delta_x);
      float step = dx_sign * angle_per_ms_x * state->actual_dt;

      state->photo_camera_angles.x = fabs(delta_x) > angle_per_ms_x * state->actual_dt
      ? state->photo_camera_angles.x + step
      : last_good_angles.x;

      // Keep result in [0, 2PI]
      state->photo_camera_angles.x = fmod(state->photo_camera_angles.x + 2.0*3.14159265359, TWO_PI);

      float delta_y =  last_good_angles.y - state->photo_camera_angles.y ;
      float dy_sign = fn_sign(delta_y);
      state->photo_camera_angles.y = fabs(delta_y) >angle_per_ms_y*state->actual_dt ? dy_sign*angle_per_ms_y*state->actual_dt + state->photo_camera_angles.y : last_good_angles.y;
    }

    state->angles = state->photo_camera_angles;
  }
  else if (state->gameState == STATE_GAMEPLAY)
  {
    state->angles.x += input->xrel*sensitivity*config->mouseSens;

    if (state->optionsMenuLayout.properties->invert_mouse_y.value == 1)
    {
      state->angles.y += input->yrel*sensitivity*config->mouseSens;
    }
    else
    {
      state->angles.y +=-input->yrel*sensitivity*config->mouseSens;
    }

  }

  state->angles.x = fmod(state->angles.x + 2.0*3.14159265359,2.0*3.14159265359);

  state->photo_camera_angles.x = fmod(state->photo_camera_angles.x + 2.0*3.14159265359,2.0*3.14159265359);

  state->angles.y = fn_clamp(state->angles.y,-1.56,1.56);
  fn_vec3 look= r_getLook(state->angles);
  fn_vec3 view_right= r_getLookVector(state->angles,fn_createVec3(1,0,0));
  fn_vec3 view_up= r_getLookVector(state->angles,fn_createVec3(0,1,0));

  fn_vec2 angles2 = state->angles;
  angles2.y = 0;
  fn_vec3 normal = fn_normalizeVec3(r_getLook(angles2));
  fn_vec4 temp = fn_multVec4Mat4(fn_makerotate(1.57,fn_createVec3(0,1,0)),fn_createVec4(normal.x,normal.y,normal.z,0.0));
  fn_vec3 right = fn_normalizeVec3(fn_createVec3(temp.x,temp.y,temp.z));


   fn_vec3 up = fn_normalizeVec3(fn_cross(right,look));
   if (!state->th_inphoto)
   {
     state->photo_camera_backup = false;
   }


   if (input->currentKeyStates[SDL_SCANCODE_Q] && !input->currentKeyStatesPrev[SDL_SCANCODE_Q] && !state->th_inphoto && state->th_photomode_enabled)
   {


     fn_printVec3(state->level.ls.player_e.aabb.position);
   }

   if (state->th_inphoto)
   {
     float dt_move = state->actual_dt;
     float cam_speed = 0.65;
     const float cam_speed_backup = 1.5;

     if (state->photo_camera_backup)
     {
       cam_speed = cam_speed_backup;
     }
     // if (state->time_mult == 0.0)
     // {
     //   dt_move = 1.0;
     // }
     // else
     // {
     //    dt_move = dt_move*(1.0/state->time_mult);
     // }


     if (state->photo_camera_backup)
     {
       fn_vec3 movdelt = fn_multVec3s(fn_normalizeVec3(fn_subVec3(last_good_pos,state->photo_camera_pos)),cam_speed*dt_move);
       if (fn_distance(last_good_pos,state->photo_camera_pos) < cam_speed*dt_move)
       {
        state->photo_camera_pos = last_good_pos;
       }
       else
       {
         state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,movdelt);
       }



     }
     else if (state->photo_camera_follow)
     {
       if (fn_distance(state->photo_camera_target,state->photo_camera_pos) > 0.01)
       {
         fn_vec3 dir_to_look = fn_normalizeVec3(fn_subVec3(state->photo_camera_target,state->photo_camera_pos));
         // fn_vec3 target = fn_addVec3(state->photo_camera_target,fn_multVec3s(dir_to_look,-320.0));
         //
         //
         // fn_vec3 movdelt = fn_multVec3s(fn_normalizeVec3(fn_subVec3(target,state->photo_camera_pos)),cam_speed*dt_move);
         // if (fn_distance(target,state->photo_camera_pos) < cam_speed*dt_move)
         // {
         //   state->photo_camera_pos = target;
         // }
         // else
         // {
         //   state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,movdelt);
         // }
         state->photo_camera_pos = fn_addVec3(state->photo_camera_target,fn_multVec3s(gaze,320.0));;
       }

     }

     if (!state->photo_camera_backup && !state->photo_camera_locked)
     {
       if (input->currentKeyStates[input->binding_forward])
       {
         state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,fn_multVec3s(look,cam_speed*dt_move));
       }

       if (input->currentKeyStates[input->binding_right])
       {
         state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,fn_multVec3s(right,cam_speed*dt_move));
       }
       if (input->currentKeyStates[input->binding_left])
       {
         state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,fn_multVec3s(right,-cam_speed*dt_move));
       }

       if (input->currentKeyStates[input->binding_back])
       {
         state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,fn_multVec3s(look,-cam_speed*dt_move));
       }

       if (input->currentKeyStates[input->binding_jump])
       {
         state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,fn_multVec3s(fn_createVec3(0,1,0),-cam_speed*dt_move));
       }

       if (input->currentKeyStates[input->binding_crouch])
       {
         state->photo_camera_pos = fn_addVec3(state->photo_camera_pos,fn_multVec3s(fn_createVec3(0,1,0),cam_speed*dt_move));
       }
    }

     if (fn_almostequalVec3(state->photo_camera_pos,last_good_pos,0.001) && fn_almostequalVec2(state->photo_camera_angles,last_good_angles,0.01) && state->photo_camera_backup)
     {
        state->photo_camera_done = true;
        state->angles = last_good_angles;
     }




     if (input->currentKeyStates[SDL_SCANCODE_Q] && !input->currentKeyStatesPrev[SDL_SCANCODE_Q] && !state->photo_camera_locked)
     {
        state->photo_camera_target = state->photo_camera_pos;

        fn_printVec3(state->photo_camera_pos);
     }

     if (input->currentKeyStates[SDL_SCANCODE_7] && !input->currentKeyStatesPrev[SDL_SCANCODE_7] && !state->photo_camera_locked )
     {
        state->photo_camera_track = !state->photo_camera_track;
        state->photo_camera_backup = false;
     }

     if (input->currentKeyStates[SDL_SCANCODE_9] && !input->currentKeyStatesPrev[SDL_SCANCODE_9] && !state->photo_camera_locked )
     {
        state->photo_camera_follow = !state->photo_camera_follow;
        state->photo_camera_backup = false;
        state->photo_camera_track = state->photo_camera_follow;

     }


     if (input->currentKeyStates[SDL_SCANCODE_8] && !input->currentKeyStatesPrev[SDL_SCANCODE_8] && !state->photo_camera_locked)
     {
       state->photo_camera_backup= !state->photo_camera_backup;
       state->photo_camera_track = false;


       state->photo_camera_time_to_pan = fn_distance(last_good_pos,state->photo_camera_pos)/cam_speed_backup;


       float delta_x = last_good_angles.x - state->photo_camera_angles.x;



       // Wrap delta to [-PI, PI] to find shortest arc direction
       if (delta_x > 3.14159265359f)       delta_x -= TWO_PI;
       else if (delta_x < -3.14159265359f) delta_x += TWO_PI;

       state->photo_camera_pan_x = fabs(delta_x);
       state->photo_camera_pan_y = fabs(last_good_angles.y - state->photo_camera_angles.y);
     }




   }
   else if (state->gameState == STATE_GAMEPLAY || state->gameState == STATE_DEATH || state->gameState == STATE_VICTORY)
   {
      th_playerPhysicsUpdate(&state->level.levelstate,input,dt,audiosystem,normal,right,look);
   }



  a_setPos(audiosystem,state->level.levelstate.player_e.aabb.position,look,fn_multVec3s(view_up,1));
  a_setVelocity(audiosystem,state->level.levelstate.player_e.velocity);

//fn_printVec3(pos);
  // pos = fn_addVec3(pos,vel);
  //Uint32 atime = SDL_GetTicks();
  bool no_model_playback = th_playbackModels(state->rp,&state->level,dt,state->level.levelstate.world);
  if (no_model_playback)
  {


    for (int i = 0 ; i < state->level.levelstate.animated_models_count;i++)
    {
      th_updateModel(&state->level.levelstate.animated_models[i],0.001*dt,state->level.levelstate.world);
    }



    th_captureModels(state->rp,&state->level);
  }


//  th_printlnDevConsole("Anim u %i",SDL_GetTicks() - atime);
   th_updateRenderMatrices(state);


   if (!state->th_inphoto)
   {
    th_weaponCheckLowering(state->level.ls.weapon,dt,state->pos,look);
   }
   if (!state->rp->th_play_mats_flag)
   {

     fn_vec4 planes_frust[6];
     fn_extractFrustum(planes_frust,state->modelViewprojection);

     th_beginOccluderFrame(state->pos,planes_frust);
     th_beginHitmarkerFrame(state->pos,planes_frust);





   th_updateLights(state->level.light_query);

   if (state->level.ls.centipede != NULL)
   {
     th_centipedeUpdate(state->level.ls.centipede,dt,input);
   }

   if (state->level.ls.centipede_super != NULL)
   {
     th_centipedeUpdate(state->level.ls.centipede_super,dt,input);
   }

   if (state->level.ls.debugger_centi != NULL)
   {
     th_DebuggerUpdate(state->level.ls.debugger_centi);
   }

   if (state->level.ls.debugger_collision != NULL)
   {
     th_DebuggerUpdate(state->level.ls.debugger_collision);
   }


   // uint64_t physics_ns = 0;
   //  uint64_t boids_ns = 0;
   //
   //       PROFILE_SCOPE(physics_ns) {
   //
   //         PROFILE_SCOPE(boids_ns) {
   if (state->level.ls.boidgroups != NULL)
   {
    th_BoidProperties bprop;
    bprop.neightborhood_rad = 6.4*2;
    bprop.speed = 0.09;
    bprop.gotoweight = 1/25.0;
    bprop.attraction = 1/65.0;
    bprop.seperation = 1/45.0;//6.4*1.5*2.2*4;
    bprop.directional = 1.0/100;
    bprop.speedlimit = 0.5*6.4*3;

    th_boidsUpdate(state->level.ls.boidgroups,dt,&bprop);
  }
      //     }





  if (state->level.ls.horse != NULL)
  {
    th_horseUpdate(state->level.ls.horse,dt);
  }
  if (state->level.ls.tricolumn != NULL)
  {
    th_tricolumnUpdate(state->level.ls.tricolumn,dt);
  }

  if (state->level.ls.eyeball != NULL)
  {
    th_eyeballUpdate(state->level.ls.eyeball,dt,input);
  }

  if (state->level.ls.eyeball_super != NULL)
  {
    th_eyeballUpdate(state->level.ls.eyeball_super,dt,input);
  }

  if (state->level.ls.rocket != NULL)
  {
    th_rocketUpdate(state->level.ls.rocket,dt);
  }

  if (state->level.ls.shamblers != NULL)
  {
    th_shamblersUpdate(state->level.ls.shamblers,dt);
  }

  if (state->level.ls.plasma != NULL)
  {
    th_plasmaUpdate(state->level.ls.plasma,dt,input,look,view_right,view_up);
  }

  if (state->level.ls.shotgun != NULL)
  {
    th_shotgunUpdate(state->level.ls.shotgun,dt,input,look,view_right,view_up);
  }

  if(state->level.ls.hammer != NULL)
  {
    th_hammerUpdate(state->level.ls.hammer,dt,input,look,view_right,view_up,state->pos,state->angles);
  }

       }

  if (state->level.ls.brass != NULL)
  {
    th_brassUpdate(state->level.ls.brass,dt);
  }

  if (state->level.ls.shotbrass != NULL)
  {
    th_brassUpdate(state->level.ls.shotbrass,dt);
  }

  if (state->level.ls.gems != NULL)
  {
    th_gemUpdate(state->level.ls.gems,dt);
  }

  if (state->level.ls.weapon != NULL)
  {
    if (state->th_inphoto)
    {
      // state->level.ls.weapon->chosen_weapon = TH_NOWEAPON;
      // state->level.ls.weapon->weapon_switch_request = TH_NOWEAPON;

      th_weaponUpdate(state->level.ls.weapon,input,dt,last_good_pos,last_good_angles,last_good_look,last_good_up);
    }
    else
    {
      th_weaponUpdate(state->level.ls.weapon,input,dt,state->pos,state->angles,look,view_up);

      last_good_pos = state->pos;
      last_good_angles = state->angles;
      last_good_look = look;
      last_good_up = view_up;
    }

  }

  if (state->level.ls.boid_gibs != NULL)
  {
    th_gibUpdate(state->level.ls.boid_gibs,dt);
  }


   //   th_printlnDevConsole("U %d B %d",physics_ns/1000000,boids_ns/1000000);
   //
   //
   // }

  bool player_was_dead = state->level.ls.player->is_dead;
   if (state->level.ls.player != NULL)
   {
     th_playerUpdate(state->level.ls.player,dt,character_map,state->screen_dims);
   }

   if (state->level.ls.tutorial != NULL)
   {
     th_tutorialUpdate(state->level.ls.tutorial,dt,input,state->screen_dims,character_map);
   }

   if (state->level.ls.lifesphere != NULL)
   {
     th_lifeUpdate(state->level.ls.lifesphere,dt);
   }

   if (state->level.ls.question != NULL)
   {
     th_questionUpdate(state->level.ls.question,dt);
   }

   if (!player_was_dead && state->level.ls.player->is_dead && !state->th_inphoto)
   {
      state->gameState = STATE_DEATH;
      a_VirtualSource* track = a_playMusicTrack(music_track_loss,0);
      a_setGainMusicTrack(1.0,0);
      a_setVSPitch(track,1.0/0.3);
      th_incrementViolence();
   }

   //th_printlnDevConsole("%f",th_getViolenceLevel());

   if (state->level.levelstate.progress_state == TH_BAR_THREE && state->gameState == STATE_GAMEPLAY)
   {
     int enemy_count = th_getEnemyCount();

     float alpha = (float)enemy_count / (float) state->level.levelstate.num_enemies_highwater ;
     alpha = fn_clamp(alpha,0.001,1.0);

     if (alpha < 0.05 || (th_getEnemyCount() < 5 && state->level.levelstate.num_enemies_highwater > 1 ))
     {
       float killtime = fn_min(5000.0*(float)enemy_count,30000.0);
        th_runKillTimer(killtime);
     }
   }


   th_VictoryStats vic_stats = th_checkVictory(state->level.ls.player,th_time() - state->level.ls.level_start_time,dt);
   if (state->gameState == STATE_GAMEPLAY && vic_stats.victory)//
   {
      state->gameState = STATE_VICTORY;
      state->level.ls.player->is_dead = true;

      a_VirtualSource* track = a_playMusicTrack(music_track_victory,0);
      a_setGainMusicTrack(1.0,0);
      a_setVSPitch(track,1.0/0.3);
      th_incrementViolence();


      fn_vec3 flt_stats = th_computeVictoryFloats(vic_stats,state->level.brass_standard,state->level.silver_standard,state->level.gold_standard);

      th_registerVictory(state,flt_stats);

      fn_vec3 interps = th_computeVictoryInterps(flt_stats,0.25,0.58,0.86);
      state->victory_alphas = interps;
      // interps.x = 0.86;
      // interps.y = 0.86;
      // interps.z = 0.86;

      // alpha_target
      // alpha_target_timer
      float initial_delay = 200.0;
      float pause_between = 100;
      float scrolltime = 500.0;

      state->victoryMenuLayout.damagetaken_slider->alpha_target = interps.x;
      state->victoryMenuLayout.damagetaken_slider->alpha = 0.0;
      state->victoryMenuLayout.damagetaken_slider->lerptime = state->victoryMenuLayout.damagetaken_slider->alpha_target*scrolltime;
      state->victoryMenuLayout.damagetaken_slider->alpha_target_timer = initial_delay + th_time() + state->victoryMenuLayout.damagetaken_slider->lerptime;
      state->victoryMenuLayout.damagetaken_slider->alpha_target_delay = initial_delay + th_time();
      state->victoryMenuLayout.damagetaken_slider->slider_alpha_slowdown = true;


      state->victoryMenuLayout.cleartime_slider->alpha_target = interps.y;
      state->victoryMenuLayout.cleartime_slider->alpha = 0.0;
      state->victoryMenuLayout.cleartime_slider->lerptime = state->victoryMenuLayout.cleartime_slider->alpha_target*scrolltime;
      state->victoryMenuLayout.cleartime_slider->alpha_target_timer = pause_between + state->victoryMenuLayout.damagetaken_slider->alpha_target_timer + state->victoryMenuLayout.cleartime_slider->lerptime;
      state->victoryMenuLayout.cleartime_slider->alpha_target_delay = pause_between + state->victoryMenuLayout.damagetaken_slider->alpha_target_timer;
      state->victoryMenuLayout.cleartime_slider->slider_alpha_slowdown = true;


      state->victoryMenuLayout.airtime_slider->alpha_target = interps.z;
      state->victoryMenuLayout.airtime_slider->alpha = 0.0;
      state->victoryMenuLayout.airtime_slider->lerptime = state->victoryMenuLayout.airtime_slider->alpha_target*scrolltime;
      state->victoryMenuLayout.airtime_slider->alpha_target_timer = pause_between + state->victoryMenuLayout.cleartime_slider->alpha_target_timer + state->victoryMenuLayout.airtime_slider->lerptime;
      state->victoryMenuLayout.airtime_slider->alpha_target_delay = pause_between + state->victoryMenuLayout.cleartime_slider->alpha_target_timer;
      state->victoryMenuLayout.airtime_slider->slider_alpha_slowdown = true;

      state->victoryMenuLayout.flawless_enabled = 0;

      for (int i = 0 ; i < 9;i++)
      {
        state->victoryMenuLayout.medals_images[i]->scale_central = 1.0;
      }

      {
        state->bardrone_source  = a_playVirtualSource(sound_bardrone,0,fn_createVec3(0,0,0),NULL);
        a_setVSLoop(state->bardrone_source,true);
        a_setVSPos(state->bardrone_source,fn_createVec3s(0));
        a_setVSVel(state->bardrone_source,fn_createVec3s(0));
        a_setVSGain(state->bardrone_source,1);
        a_setVSPitch(state->bardrone_source,3.333);
      }

    }
    else if (state->gameState == STATE_VICTORY)
    {

      //TODO
      //make bar move slower closer to the end DONE
      // 1 - (1 -x)^2
      //make bar sound ramp down in pitch wawawawawawAWAWAWA and volume
      //make the medals expand and then settle down when they are "earned", also make the bar expand and then settle down DONE
      //also give the medals an impact sound
      //if you hit a flawless, make the letters HUGE and drop down when they are finished with gravity and a little bounce and also play a hooray

      //4 gold, 3 silver, 2 bronze, 1 completed 0 uncompleted
      int lv_damage  = state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_damagetaken;
      int lv_speed  = state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_speed;
      int lv_air  = state->levelSelectLayout.victory_state[state->levelSelectLayout.selected_level].level_airtime;

      th_UIElement* sliders[3] = {state->victoryMenuLayout.damagetaken_slider,state->victoryMenuLayout.cleartime_slider,state->victoryMenuLayout.airtime_slider};
      int lvs[3] = {lv_damage,lv_speed,lv_air};
      bool all_flawless = true;
      for (int i = 0 ; i < 3 ;i++)
      {
        float pitch_mul = 1.0 + ((float)i)*0.333;
        th_UIElement* slider = sliders[i];
        slider->tint = fn_createVec3(0.9,0.0,0.2);

        if (slider->alpha > 0.01 && slider->alpha < state->victory_alphas.v[i] && state->bardrone_source != NULL)
        {
          float barpitch = (1.0 - slider->alpha/state->victory_alphas.v[i]) + 1.5;
          a_setVSPitch(state->bardrone_source,3.333*barpitch*2.0*(1.0 - slider->alpha/state->victory_alphas.v[i]));
        }

        if (lvs[i] >= 2 && slider->alpha >= 0.25 )
        {
          slider->tint = fn_createVec3(0.3,0.05,0.0);
          slider->scale_central = state->victoryMenuLayout.medals_images[i]->scale_central;

          if (slider->old_alpha < 0.25)
          {
            state->victoryMenuLayout.medals_images[i]->alpha_target = 1.5;
            state->victoryMenuLayout.medals_images[i]->scale_central = 0.0;
            state->victoryMenuLayout.medals_images[i]->lerptime = 250.0;
            state->victoryMenuLayout.medals_images[i]->alpha_target_timer = th_time() + state->victoryMenuLayout.medals_images[i]->lerptime;
            state->victoryMenuLayout.medals_images[i]->alpha_target_delay = th_time() - 1.0;

            {
              a_VirtualSource* s = a_playVirtualSource(sound_medalbelltoll,0,fn_createVec3(0,0,0),NULL);
              a_setVSLoop(s,false);
              a_setVSPos(s,fn_createVec3s(0));
              a_setVSVel(s,fn_createVec3s(0));
              a_setVSGain(s,1);
              a_setVSPitch(s,3.333*(pitch_mul));
            }
          }
        }

        if (lvs[i] >= 3 && slider->alpha >= 0.58 )
        {
          slider->tint = fn_createVec3(0.7,0.7,0.7);
          slider->scale_central = state->victoryMenuLayout.medals_images[i + 3]->scale_central;

          if (slider->old_alpha < 0.58)
          {
            state->victoryMenuLayout.medals_images[i + 3]->alpha_target = 2.0;
            state->victoryMenuLayout.medals_images[i + 3]->scale_central = 0.0;
            state->victoryMenuLayout.medals_images[i + 3]->lerptime = 250.0;
            state->victoryMenuLayout.medals_images[i + 3]->alpha_target_timer = th_time() + state->victoryMenuLayout.medals_images[i + 3]->lerptime;
            state->victoryMenuLayout.medals_images[i + 3]->alpha_target_delay = th_time() - 1.0;

            {
              a_VirtualSource* s = a_playVirtualSource(sound_medalbelltoll,0,fn_createVec3(0,0,0),NULL);
              a_setVSLoop(s,false);
              a_setVSPos(s,fn_createVec3s(0));
              a_setVSVel(s,fn_createVec3s(0));
              a_setVSGain(s,1);
              a_setVSPitch(s,3.333*1.25*(pitch_mul));
            }
          }
        }

        if (lvs[i] == 4 && slider->alpha >= 0.86 )
        {
          slider->tint = fn_createVec3(1.0,0.8,0.0);
          slider->scale_central = state->victoryMenuLayout.medals_images[i + 6]->scale_central;

          if (slider->old_alpha < 0.86)
          {
            state->victoryMenuLayout.medals_images[i + 6]->alpha_target = 3.0;
            state->victoryMenuLayout.medals_images[i + 6]->scale_central = 0.0;
            state->victoryMenuLayout.medals_images[i + 6]->lerptime = 250.0;
            state->victoryMenuLayout.medals_images[i + 6]->alpha_target_timer = th_time() + state->victoryMenuLayout.medals_images[i + 6]->lerptime;
            state->victoryMenuLayout.medals_images[i + 6]->alpha_target_delay = th_time() - 1.0;

            {
              a_VirtualSource* s = a_playVirtualSource(sound_medalbelltoll,0,fn_createVec3(0,0,0),NULL);
              a_setVSLoop(s,false);
              a_setVSPos(s,fn_createVec3s(0));
              a_setVSVel(s,fn_createVec3s(0));
              a_setVSGain(s,1);
              a_setVSPitch(s,3.333*1.75*(pitch_mul));
            }
          }
        }
        else
        {
          all_flawless = false;
        }
      }

      if (state->victoryMenuLayout.airtime_slider->alpha >= state->victory_alphas.z - 0.01  && state->bardrone_source != NULL)
      {
        //a_stopVS(state->bardrone_source);
        a_setVSLoop(state->bardrone_source,false);
        state->bardrone_source = NULL;
      }

      if (all_flawless)
      {
        state->victoryMenuLayout.flawless_enabled = 1;

        if (state->victoryMenuLayout.airtime_slider->old_alpha < 0.86  )
        {
          //TODO start text falling animation
          state->victoryMenuLayout.flawless_text->alpha_target = 1.0;
          state->victoryMenuLayout.flawless_text->lerptime = 1500.0;
          state->victoryMenuLayout.flawless_text->alpha_target_timer = th_time() + state->victoryMenuLayout.flawless_text->lerptime;
          state->victoryMenuLayout.flawless_text->alpha_target_delay = th_time() - 1.0;

          {
            a_VirtualSource* s = a_playVirtualSource(sound_belltoll,0,fn_createVec3(0,0,0),NULL);
            a_setVSLoop(s,false);
            a_setVSPos(s,fn_createVec3s(0));
            a_setVSVel(s,fn_createVec3s(0));
            a_setVSGain(s,1);
            a_setVSPitch(s,3.333);
          }
        }
      }


    }

  if (state->gameState == STATE_VICTORY || state->gameState == STATE_DEATH)
  {
    a_setGainMusicTrack(1.0,0);
  }

  //th_printlnDevConsole("%f",(float)dt );
  th_spawnBuiltinParticlesAndDecals();
  // static bool demo_light = false;
  // if (!demo_light)
  // {
  //   demo_light = true;
    //  th_spawnLighting(fn_createVec3(-980.424255, -52.557144, -450.230774),fn_createVec3(342.588531, -351.158691, 737.858337),-1,state->level.ls.general_light_query,20,9.0,175);
  //}


  state->look_global = fn_normalizeVec3(look);
  th_simulateParticles(dt,state->pos,fn_normalizeVec3(look),state->level.ls.world);

  if (input->currentKeyStates[SDL_SCANCODE_L] && !input->currentKeyStatesPrev[SDL_SCANCODE_L] )
  {
    printf("%f %f\n",state->angles.x,state->angles.y);
  }


  if (state->th_photomode_enabled && input->currentKeyStates[SDL_SCANCODE_F12] && !input->currentKeyStatesPrev[SDL_SCANCODE_F12] )
  {
    char tempbuffer_fname[1024];
    time_t timestamp = time(NULL);
    static int screenshotcount = 0;
    sprintf(tempbuffer_fname,"Screenshot %s %i.tga", asctime(gmtime(&timestamp)),screenshotcount);
    screenshotcount++;
    save_screenshot(tempbuffer_fname,(int)state->screen_dims.x,(int)state->screen_dims.y);
  }

  th_updateGameBuiltins(dt);


  // th_printlnDevConsole("Frame %i %i %f",state->rp->a,state->rp->b,state->rp->tween_value);
  //
  //th_printlnDevConsole("Frame %f %f",th_time(),state->rp->th_mat_capture_time[state->rp->th_frame_captured - 1]);

  //printf("%i\n",SDL_GetTicks() - start_time );
}

void th_doRendering(th_RendererState* state)
{
  th_render(state->modelViewprojection,state->projection,state->view,state->screen_dims,state);
}

void th_unloadData(th_RendererState* state)
{
  glFinish();


  //glDeleteBuffers(1, &state->EnvBoxesUbo);
  //th_deleteCommandBuffer(&state->dynamic_shadowcaster_commands);
  th_freeVbo(&state->particle_data);
  th_freeVbo(&state->model_data);
  th_freeVbo(&state->guy_array);


  if (state->level.streamed_meshes > 0)
  {
    th_freeVbo(&state->dynamic_geom_array);
  }

  if (state->th_tesselation)
  {
    th_freeVbo(&state->model_data_tess);
  }


  //glDeleteTextures(1,&state->skybox);
  // glDeleteTextures(1,&state->cubemapDepth);
  // glDeleteTextures(1,&state->cubemapColor);
  //glDeleteTextures(1,&state->lightTex);


  //free textures


  //reset decals
  th_resetDecals();

  //lights dont need to be reset

  //particles dont need to be reset

  //free entity edicts
  th_freeEntityGroups();

  //reset the audio system
  a_audioClearFiles();

  //free visiblity data (dependant on level)
  if (state->visibility_data != NULL)
  {
    free(state->visibility_data);
    state->visibility_data = NULL;
  }

  //free the octree
  fn_freeOctree(&state->level.ls.world->octree);

  //reset hitmarkers, which can make reference to memory allocated in the level allocator
  th_resetHitmarkers();

  //finally, free all the memory
  th_free(&state->level.allocator);


  if (!state->th_respawn_flag)
  {
    th_unloadMaterials(); //doesnt free vram, just re-uses it
  }

}

void th_unloadLoadData(th_RendererState* state,const char* levelname,const char* spawnsetname)
{

  th_unloadData(state);
  //load new level
  fn_vec3 scale = fn_createVec3s(1);
  if (strcmp(levelname,"levelskatepark") == 0)
  {
    scale = fn_createVec3s(1.5);
  }
  th_loadLevel(&state->level,false,false,&state->cubemapDepth,&state->cubemapColor,levelname,spawnsetname,scale,state->th_respawn_flag,fn_clampi(state->config_global->fog_quality,0,2));
  th_getMatsPerFrame(state->rp,&state->level);
  th_initRenderingLevel(false, false,state);


  //muntrace();
  //print_allocated("after");
}


