#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

#include "fn_window.h"
#include "fn_input.h"
#include "fn_config.h"
#include "fn_math/fn_vec4.h"
#include "fn_math/fn_mat4.h"
#include "fn_math/fn_common.h"
#include "fn_engine/th_audio.h"
#include <time.h>
#include "fn_engine/th_threads.h"
// #include "watermark.h"
#include "fn_engine/r_texture.h"
#include "fn_engine/th_program.h"
#include "fn_engine/th_baking.h"
#include "fn_engine/th_time.h"
#include "fn_engine/th_gpu.h"
#include "fn_engine/th_ui.h"
#include "fn_engine/th_system.h"
#include "fn_game/th_builtins.h"
#include "fn_engine/th_occlusion.h"
#include "fn_engine/th_hitmarker.h"
#include "fn_engine/th_globals.h"
#include "th_fopen.h"
#include <malloc.h>
// #include <mcheck.h>

Uint32 oldtime;
Uint32 newtime;


#define RTIME 30
float refreshtimes[RTIME];
int refreshtimeCount = 0;
float curDt = 16.666f;
bool first = true;

double accumulator = 0.0;
double dt = 1;

static double maxFPS = 200.0;

typedef struct
{
  int range;
  int start;
}thread_test;

typedef struct
{
  fn_RawInput input;
  Uint32 frameTime;
}th_ReplayFrame;

int main(int argc, char **argv) {
  printf("Num scancodes: %d %d\n",TH_NUM_SCANCODES,sizeof(SDL_Scancode));
  //SDL
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
  {
    printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
  }


  printf("Desktop mode index %i\n",th_getDesktopDisplayModeIndex());
  //enumerate resolutions
  int displayIndex = 0; // 0 = primary monitor
  int numModes = SDL_GetNumDisplayModes(displayIndex);

  int x_modes[TH_MAX_MODES] = {1920};
  int y_modes[TH_MAX_MODES] = {1080};
  int found_modes = 0;
  th_getWindowModes(x_modes,y_modes,&found_modes);



  char** modestrings = malloc(sizeof(char*)*TH_MAX_MODES);
  for (int i = 0 ; i < TH_MAX_MODES;i++)
  {
    modestrings[i] = malloc(sizeof(char)*100);
    snprintf(modestrings[i], 100, "%ix%i",x_modes[i],y_modes[i]);
  }

  th_setModeStrings(modestrings,found_modes);
  // int displayIndex = 0; // 0 = primary monitor
  // int numModes = SDL_GetNumDisplayModes(displayIndex);
  // printf("DISPLAY MODES %i\n",numModes);
  // printf("%s\n",SDL_GetError());
  // for (int i = 0; i < numModes; i++) {
  //   SDL_DisplayMode mode;
  //   if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0) {
  //     printf("%dx%d @ %dHz (format: %s)\n",
  //            mode.w, mode.h, mode.refresh_rate,
  //            SDL_GetPixelFormatName(mode.format));
  //   }
  // }

  //check for levels
  th_LevelManifest level_manifest = th_getLevelManifest("levels.txt");

  //populate the config files

  char* settings_path = th_getPathConfig("settings.txt",NULL);
  char* victory_path = th_getPathConfig("victory.txt",NULL);

  th_setPathSettings(settings_path);
  th_setPathVictory(victory_path);

  bool write_new_settings = !th_fileExists(settings_path);
  bool write_new_victory = !th_fileExists(victory_path);

  if (write_new_settings)
  {
    th_UIlayout temp_layout;
    th_liftPropertiesConfig(&temp_layout,"settings.txt");

    temp_layout.properties->resolution.value = th_getDesktopDisplayModeIndex();

    th_dumpPropertiesConfig(&temp_layout,settings_path);
  }

  if (write_new_victory)
  {
    th_VictoryManifest vicmanifest = th_getVictoryManifest("victory.txt",level_manifest.count);

    th_writeVictory(vicmanifest,level_manifest,victory_path);
  }


  //mtrace();
  printf("%i\n",(int)sizeof(unsigned long) );

  int cpus_num = SDL_GetCPUCount();

  printf("Hardware Concurrency: %i\n",cpus_num);

  int hardware_threads = cpus_num;

  if (hardware_threads > 16)
  {
    hardware_threads = 16;
  }

  if (hardware_threads > 4)
  {
    hardware_threads = hardware_threads - 2;
  }
  th_initUINagbar();
  r_allocateTextureHandles();

  //hardware_threads = 1;
  th_createThreads(hardware_threads);
  th_srandom((unsigned int)1503541399);


  bool paused = false;

  bool record = false;
  bool playback = false;
  //"replay.trp"
  // const char* replayname = "demodebug.trp";
  // const char* replayname = "demoanijump.trp";
  const char* replayname = "demolightissue.trp";
  th_ReplayFrame* replayBuffer = malloc(sizeof(th_ReplayFrame)*42000);
  int replayFrames = 0;
  int replayFrameCounter = 0;
  if (playback)
  {
    FILE* fp = th_fopen(replayname,"rb");
    fread(&replayFrames,sizeof(int),1,fp);
    fread(replayBuffer,sizeof(th_ReplayFrame),replayFrames,fp);
    fclose(fp);
  }

  int resolution = 32;
  bool gensharmonics = false;
  bool gencubemaps = false;
  bool iter = false;
  bool exportwad = false;
  char* wadlocation = NULL;
  bool loadmap_direct = false;
  char* map_name_arg = NULL;
  char* spawnset_name_arg = NULL;
  bool savecap = false;
  bool noclip = false;
  bool nogui = false;
  bool photomode = false;
  bool recachemode = false;
  for (int i = 1;i < argc;i++)
  {
    if (strcmp(argv[i],"-gensharmonics") == 0)
    {
      gensharmonics = true;
    }
    if (strcmp(argv[i],"-gencubemaps")== 0)
    {
      gencubemaps = true;
    }
    if(strcmp(argv[i],"-res")== 0)
    {
      sscanf(argv[i + 1], "%d", &resolution);
    }
    if (strcmp(argv[i],"-iter")== 0)
    {
      iter = true;
    }
    if (strcmp(argv[i],"-exportwad")== 0)
    {
      exportwad = true;
      wadlocation = argv[i + 1];
    }
    if (strcmp(argv[i],"-loadmap")== 0)
    {
      loadmap_direct = true;
      map_name_arg = argv[i + 1];
    }

    if (strcmp(argv[i],"-loadspawnset")== 0)
    {
      spawnset_name_arg = argv[i + 1];
    }

    if (strcmp(argv[i],"-savecap")== 0)
    {
      savecap = true;
    }

    if (strcmp(argv[i],"-noclip")== 0)
    {
      noclip = true;
    }

    if (strcmp(argv[i],"-nogui")== 0)
    {
      nogui = true;
    }

    if (strcmp(argv[i],"-photo") == 0)
    {
      photomode = true;
    }

    if (strcmp(argv[i],"-recache") == 0)
    {
      recachemode = true;
      printf("RECACHE ENABLED\n");
    }
  }

  if ((gencubemaps || gensharmonics) && !loadmap_direct)
  {
    printf("Specify Map with -loadmap\n");
    return 0;
  }

  if ((gencubemaps || gensharmonics) && !exportwad)
  {

    wadlocation = map_name_arg;
    exportwad = true;
    //printf("Specify wad name with -exportwad\n");
    //return 0;
  }

  // test();
  // return 0;
  // printf("%i\n",fn_frame() );
  unsigned int d1;//,index,index2;
  // index = 0;
  // index2 = 0;
  //decode watermark
  // GLubyte* decoded = malloc(sizeof(GLubyte)*3*watermark_width*watermark_height);
  // for (d1 =0;d1 < watermark_width*watermark_height;d1++)
  // {
  //   int data[4] = {header_data[index],header_data[index+1],header_data[index+2],header_data[index+3]};
  //   int pixel[3];
  //   index += 4;
  //   HEADER_PIXEL(data,pixel);
  //   decoded[index2] = (GLubyte)pixel[0];
  //   decoded[index2+1] = (GLubyte)pixel[1];
  //   decoded[index2+2] = (GLubyte)pixel[2];
  //   index2 += 3;
  // }

  th_Allocator allocator;
  th_createAllocator(&allocator);

  int num_keyvals = 0;
  th_KeyValuePair* keyvals = th_keyValueLoad(&allocator,settings_path,&num_keyvals);
  int resolution_index =  (int)th_keyValueGetFloat(keyvals,num_keyvals,"resolution");

  if (resolution_index >= found_modes)
  {
    resolution_index = found_modes - 1;
  }
  if (resolution_index < 0)
  {
    resolution_index = 0;
  }




  // int x_resolutions[] = {1280,1366,1600,1920,2560,2560,3440,3840};
  // int y_resolutions[] = {720,768,900,1080,1080,1440,1440,2160};

  //int i;
  float aspect_scale_a = (float)x_modes[resolution_index] / 1920.0;
  float aspect_scale_b = (float)y_modes[resolution_index] / 1080.0;
  th_setAspectScale(fmin(aspect_scale_a,aspect_scale_b));

  fn_Config config = {0};
  config.width = x_modes[resolution_index];
  config.height = y_modes[resolution_index];
  config.viewportscale = 1.0;
  config.fullscreen = (int)th_keyValueGetFloat(keyvals,num_keyvals,"fullscreen");
  config.borderless_fullscreen = ((int)th_keyValueGetFloat(keyvals,num_keyvals,"fullscreen") == 2);
  config.window_hidden = false;

  config.gl_major = 4;
  config.gl_minor = 6;
  config.vsync = (int)th_keyValueGetFloat(keyvals,num_keyvals,"vsync");
  config.mouseSens =  th_keyValueGetFloat(keyvals,num_keyvals,"mouse_sensitivity");//(1.5);//0.009*0.75;//0.009*0.75;
  config.fov = (int)th_keyValueGetFloat(keyvals,num_keyvals,"fov");
  config.mastervolume = th_keyValueGetFloat(keyvals,num_keyvals,"sfx_volume");
  config.musicvolume = th_keyValueGetFloat(keyvals,num_keyvals,"music_volume");

  config.dynamic_shadows = (int)th_keyValueGetFloat(keyvals,num_keyvals,"dynamic_shadows");
  config.shadow_quality = (int)th_keyValueGetFloat(keyvals,num_keyvals,"shadow_quality");
  config.reflection_quality = (int)th_keyValueGetFloat(keyvals,num_keyvals,"reflection_quality");
  config.fog_quality = (int)th_keyValueGetFloat(keyvals,num_keyvals,"fog_quality");
  config.reflection_scale = (int)th_keyValueGetFloat(keyvals,num_keyvals,"reflection_scale");
  config.particle_lighting = (int)th_keyValueGetFloatDefault(keyvals,num_keyvals,"particle_lighting",1.0);
  config.maxFPS = (int)th_keyValueGetFloatDefault(keyvals,num_keyvals,"max_fps",200.0);
  config.exposure = th_keyValueGetFloatDefault(keyvals,num_keyvals,"exposure",0.0);
  config.gamma = th_keyValueGetFloatDefault(keyvals,num_keyvals,"gamma",2.2);

  config.d_width = config.width;
  config.d_height = config.height;
  config.scale_factor = 1.0;
  config.upscale_mode = TH_RAWOUTPUT;

  config.dpi_nonsense = false;

  if (config.borderless_fullscreen)
  {
    int displayIndex = 0; // 0 = primary monitor

    SDL_DisplayMode desktop_mode;

    int err = SDL_GetDesktopDisplayMode(displayIndex, &desktop_mode );

    config.d_width = desktop_mode.w;
    config.d_height = desktop_mode.h;

    if (config.d_width != config.width || config.d_height != config.height)
    {
      float scale_x = (float)config.d_width / (float)config.width;
      float scale_y = (float)config.d_height / (float)config.height;

      config.scale_factor = fmin(scale_x,scale_y);
      if (fabs(scale_x - scale_y) < 0.0001)
      {
        config.upscale_mode = TH_SCALEBOX;
      }
      else if (scale_x > scale_y)
      {
        config.upscale_mode = TH_PILLARBOX;
      }
      else if (scale_y > scale_x)
      {
        config.upscale_mode = TH_LETTERBOX;
      }
    }


  }

  if (gensharmonics || gencubemaps)
  {
    config.fullscreen = false;
    config.fov = 90;
    config.vsync = false;
    config.width = 1280;
    config.height = 720;
    config.window_hidden = true;
    config.borderless_fullscreen = false;
  }




  maxFPS = (double)config.maxFPS;

  fn_Window window;
  fn_initWindow(&window,&config);

  //getdesktopdisplaymode gives the wrong mode because of dpi reasons
  //fix it for when we are doing borderless_fullscreen
  if (config.dpi_nonsense)
  {
    if (config.d_width != config.width || config.d_height != config.height)
    {
      float scale_x = (float)config.d_width / (float)config.width;
      float scale_y = (float)config.d_height / (float)config.height;

      config.scale_factor = fmin(scale_x,scale_y);
      if (fabs(scale_x - scale_y) < 0.0001)
      {
        config.upscale_mode = TH_SCALEBOX;
      }
      else if (scale_x > scale_y)
      {
        config.upscale_mode = TH_PILLARBOX;
      }
      else if (scale_y > scale_x)
      {
        config.upscale_mode = TH_LETTERBOX;
      }
    }
    else
    {
      config.scale_factor = 1.0;
      config.upscale_mode = TH_RAWOUTPUT;
    }
  }


  fn_initGL(&config);
  printf("Hello world!\n");


  //GLuint watermarkTex = fn_loadWatermark(watermark_width,watermark_height,decoded);


  fn_moveMouse(&window,config.width/2.f,config.height/2.f);
  SDL_SetRelativeMouseMode(SDL_TRUE);
  bool running = true;
  SDL_Event event;

  fn_RawInput input;
  input.leftPressedT = 1;
  input.leftReleasedT = 0;
  input.rightPressedT = 3;
  input.rightReleasedT = 2;
  input.leftHoldDuration = 0.f;
  input.rightHoldDuration = 0.f;
  input.reset = false;
  input.quit = false;
  input.inFocus = true;
  input.xpos_win = config.width/2.f;
  input.ypos_win = config.height/2.f;
  input.changemap = false;
  unsigned char fillval = 0xFF;
  memset(input.keyPresstime,fillval,sizeof(Uint32)*TH_NUM_SCANCODES);
  memset(input.keyReleasetime,fillval,sizeof(Uint32)*TH_NUM_SCANCODES);



  unsigned char fillval2 = 0x00;
  memset(input.currentKeyStates,fillval2,sizeof(Uint8)*TH_NUM_SCANCODES);
  memset(input.currentKeyStatesPrev,fillval2,sizeof(Uint8)*TH_NUM_SCANCODES);
  // printf("%u\n",(Uint32)((1<<32)-1) );

  input.binding_forward = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"forward_key");
  input.binding_back = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"backward_key");
  input.binding_left = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"left_key");
  input.binding_right = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"right_key");
  input.binding_crouch = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"crouch_key");
  input.binding_jump = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"jump_key");

  input.binding_weapon1 = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"machinegun_key");
  input.binding_weapon2 = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"shotgun_key");
  input.binding_weapon3 = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"hammer_key");


  th_free(&allocator);

  a_AudioSystem audioSystem;


  a_initAudioSys(&audioSystem,true);
  a_setGain(config.mastervolume);
  a_setGainMusic(config.musicvolume);


  a_setUnits(0.75);
  Uint64 start = SDL_GetPerformanceCounter();
  Uint64 end = SDL_GetPerformanceCounter();
  Uint64 freq = SDL_GetPerformanceFrequency();


  if (gensharmonics || gencubemaps)
  {
    config.width = resolution;
    config.height = resolution;
    maxFPS = 1000;
  }


  th_RendererState* renderState = malloc(sizeof(th_RendererState));
  memset(renderState,0,sizeof(th_RendererState));
  th_initRendererState(renderState);
  renderState->th_crosshair_size = -1.0;
  //renderState->th_no_hud = true;
  if (!gencubemaps && !gensharmonics && loadmap_direct)
  {
      renderState->th_crosshair_size = 1.0;
  }

  if (gencubemaps || gensharmonics)
  {
    renderState->th_ambient_occlusion = false;
  }

  if (nogui)
  {
    renderState->th_no_hud = true;
    renderState->th_dynamic_objs = false;
    renderState->th_crosshair_size = -1.0;
  }

  if (photomode)
  {
    renderState->th_photomode_enabled = true;
  }

  //tiled configuration
  renderState->num_tiles_x = (int)ceil((float)config.width / 16.0);
  renderState->num_tiles_y = (int)ceil((float)config.height / 16.0);
  renderState->num_tiled_div4 = (renderState->num_tiles_x*renderState->num_tiles_y)/4;
  renderState->num_uints_div4 = (renderState->num_tiles_x*renderState->num_tiles_y*64)/4;

  printf("%i %i %i %i\n",renderState->num_tiles_x,renderState->num_tiles_y,renderState->num_tiled_div4,renderState->num_uints_div4);


  th_ReplayData* rp = malloc(sizeof(th_ReplayData));
  *rp = DEFAULT_REPLAY_STATE;
  renderState->rp = rp;

  renderState->list_of_levels = level_manifest;
  renderState->list_of_victory = th_getVictoryManifest(victory_path,level_manifest.count);

  //batch rendering
  renderState->batch_atlas_res = 8064;
  renderState->th_batch_render = gensharmonics;
  if (gensharmonics)
  {
    config.dynamic_shadows = 0;
  }

  th_initOcclusion();
  th_initHitmarkers();

  th_setrecacheMode(recachemode);

  th_LoadData(gensharmonics,gencubemaps,resolution,&config,iter,renderState,&input,map_name_arg,spawnset_name_arg);

  if (!loadmap_direct)
  {
    //th_loadCapture(rp,"intro_compressed.cap",true);
    renderState->photo_camera_done = false;
    renderState->th_inphoto = true;

    renderState->photo_camera_pos = renderState->level.spawnpoint;
    renderState->photo_camera_angles = renderState->level.spawnangles;

    renderState->photo_camera_locked = true;

    //make enemies target a point offscreen
    renderState->level.ls.player_e.aabb.position = renderState->level.player_pos_titlescreen;
    renderState->level.ls.player->is_dead = true;

    a_setGainMaster(0.0);
  }
  else
  {
    renderState->gameState = STATE_GAMEPLAY;
  }

  if (savecap)
  {
    th_startCapture(rp);
  }

  if (noclip && renderState->level.ls.player != NULL)
  {
    renderState->level.ls.player->noclip = true;
  }


  printf("Pinned Memory: %i Bytes\n",(int)th_getPinnedMemory());
  if (gensharmonics)
  {
     renderState->th_depth_prepass = false;
     renderState->th_decal_rendering = false;
     renderState->th_particles = false;
     renderState->th_temporalfilter = false;
     renderState->th_frustumcull_enabled = false;

    th_generateHarmonics(resolution,window.gWindow,wadlocation,renderState);
    return 0;
  }
  if (gencubemaps)
  {
    // th_generateCubemaps(resolution,window.gWindow);
    th_CubemapGenData cubedat = th_generateCubemaps(resolution,window.gWindow,renderState);
    int layers = cubedat.cube_count;
    if (exportwad)
    {
      GLfloat* wad_data = cubedat.wad_data;
      GLfloat** color_data = cubedat.color_data;
      GLfloat* minmaxdata = cubedat.minmaxdata;

      char filename[128];
      sprintf(filename,"th1/wad/%s/%i.cwad",wadlocation,0);
      FILE* mwad = th_fopen(filename,"wb");
      if (mwad == NULL)
      {
        printf("%s\n","CANNOT OPEN" );
      }
      fwrite(minmaxdata,sizeof(GLfloat),6*layers,mwad);
      fwrite(wad_data,sizeof(GLfloat),6*resolution*resolution*layers,mwad);
      for (int i = 0 ; i < 5;i++)
      {
        unsigned int mipWidth = resolution * powf(0.5, i);
        unsigned int mipHeight = resolution * powf(0.5, i);
        fwrite(color_data[i],sizeof(GLfloat),6*mipWidth*mipHeight*layers*3,mwad);
      }
      fclose(mwad);

    }
    return 0;
  }


  float avgframe = 0.f;
  int frame_num = 0;
  //  float avgframe_sto = 0.f;
  // float mult = 1.0;
  renderState->time_mult = 1.0;

  bool paused_gameplay = false;
  bool queued_focus = false;

  int framepass = 0;
  int frame_id = 0;

  double min_ms = 1000.0/maxFPS;


  bool first_boot = !loadmap_direct;
  bool set_boot_music = loadmap_direct;

  while (running)
  {




    end = start;
    start = SDL_GetPerformanceCounter();

    if (input.currentKeyStates[SDL_SCANCODE_ESCAPE] && renderState->gameState == STATE_MAINMENU)
    {
      input.quit = true;
    }
    running = !input.quit && renderState->gameState != STATE_QUIT;


    SDL_PumpEvents();
    fn_getInput(&event,&input,1);

    //transform to iresolution
    if (config.borderless_fullscreen && config.upscale_mode != TH_RAWOUTPUT)
    {
      int viewportW = (int)(config.width * config.scale_factor);
      int viewportH = (int)(config.height * config.scale_factor);

      int viewportX = (config.d_width - viewportW) / 2;
      int viewportY = (config.d_height - viewportH) / 2;

      input.xpos_win = (input.xpos_win - viewportX) * ((float)config.width / viewportW);
      input.ypos_win = (input.ypos_win - viewportY) * ((float)config.height / viewportH);
    }

    if (renderState->gameState != STATE_GAMEPLAY)
    {
      SDL_SetRelativeMouseMode(SDL_FALSE);
    }

    if ((input.gainedFocus || queued_focus ) && renderState->gameState == STATE_GAMEPLAY)
    {
      SDL_SetRelativeMouseMode(SDL_TRUE);

      fn_moveMouse(&window,config.width/2.f,config.height/2.f);
      queued_focus = false;
    }
    else if (input.gainedFocus && renderState->gameState != STATE_GAMEPLAY )
    {
      queued_focus = true;
    }
    else if (input.lostFocus && renderState->gameState != STATE_GAMEPLAY)
    {
      queued_focus = false;
    }

    if (input.currentKeyStates[SDL_SCANCODE_6] && !input.currentKeyStatesPrev[SDL_SCANCODE_6] && renderState->th_photomode_enabled)
    {
      renderState->photo_camera_done = false;
      renderState->th_inphoto = !renderState->th_inphoto;

      renderState->photo_camera_pos = renderState->pos;
      renderState->photo_camera_angles = renderState->angles;

      renderState->time_mult = 0.01;
    }

    if (renderState->photo_camera_done)
    {
      renderState->photo_camera_done = false;
      renderState->th_inphoto = false;
    }

    if (renderState->gameState != STATE_GAMEPLAY && renderState->gameState != STATE_VICTORY && renderState->gameState != STATE_DEATH)
    {
      a_setGainMaster(0.0);
    }
    else
    {
      a_setGainMaster(1.0);
    }

    // if (!renderState->th_photomode_enabled)
    // {
    //   //go into locked photomode if its a normal game session and you are in the menu, otherwise you are not in photomode (and thus no need to lock it)
    //   renderState->photo_camera_locked = renderState->gameState == STATE_MAINMENU || renderState->gameState == STATE_OPTIONS_MENU || renderState->gameState == STATE_LEVELSELECT;
    //   renderState->th_inphoto = renderState->photo_camera_locked;
    // }



    if (frame_num < 32)
    {
      avgframe +=((start - end))/32.f;
      frame_num++;
    }
    else
    {
      //avgframe_sto = avgframe;
      avgframe = 0.f;
      frame_num = 0;
    }
    double frameTime = (double)(start - end) / (double)freq;
    frameTime = frameTime * 1000.0;
    //accumulator += frameTime;
    // if (playback)
    // {
    //   if (input.quit)
    //   {
    //     break;
    //   }
    //   if (input.currentKeyStates[SDL_SCANCODE_P])
    //   {
    //     paused = true;
    //   }
    //   if (input.currentKeyStates[SDL_SCANCODE_I])
    //   {
    //     paused = false;
    //   }
    //
    //   static bool step = false;
    //   bool step_prev = step;
    //   step = input.currentKeyStates[SDL_SCANCODE_U];
    //
    //   if (paused && !(step && !step_prev) )
    //   {
    //     continue;
    //   }
    //
    //
    //
    //   input = replayBuffer[replayFrameCounter].input;
    //   frameTime = replayBuffer[replayFrameCounter].frameTime;
    //   if (replayFrameCounter >= replayFrames)
    //   {
    //     printf("%s\n","END OF REPLAY" );
    //   }
    //   else
    //   {
    //     replayFrameCounter++;
    //   }
    //
    //
    //
    // }
    // else if (record)
    // {
    //   if (replayFrames < 42000)
    //   {
    //     replayBuffer[replayFrames].input = input;
    //     replayBuffer[replayFrames].frameTime = frameTime;
    //     replayFrames++;
    //   }
    // }

    //singlestepping
    // if (input.currentKeyStates[SDL_SCANCODE_Y] && renderState->gameState == STATE_GAMEPLAY )
    // {
    //   if (!(input.currentKeyStates[SDL_SCANCODE_U] && !input.currentKeyStatesPrev[SDL_SCANCODE_U] ) )
    //   {
    //
    //     continue;
    //   }
    // }


    //do game stuff
    //  printf("%f\n",1000.f/avgframe_sto );
    // while ( accumulator >= dt )
    // {
    if(input.wheel > 0)
    {
      renderState->time_mult += 0.1;
    }
    else if (input.wheel < 0)
    {
      renderState->time_mult -= 0.1;
    }

    fn_vec3 gscale = th_getGameplayTimeScale();
    if (renderState->gameState == STATE_GAMEPLAY)
    {
      gscale.x += gscale.y*frameTime;
      gscale.y += gscale.z*frameTime;
      if (gscale.x > 1.0)
      {
        gscale.x = 1.0;
        gscale.y = 0.0;
        gscale.z = 0.0;
      }
      th_setGameplayTimeScale(gscale);
    }
    //  printf("%f\n",mult );
    if (renderState->gameState == STATE_GAMEPLAY && ! renderState->th_inphoto)
    {
      renderState->time_mult = gscale.x;
    }

    if ((renderState->gameState == STATE_MAINMENU || renderState->gameState == STATE_DEATH || renderState->gameState == STATE_VICTORY)&& th_frame() > 2)
    {
      renderState->time_mult = 0.3;
      renderState->th_crosshair_size = -1.0;
    }
    renderState->time_mult = fn_clamp(renderState->time_mult,0,1);
    a_setPitch(renderState->time_mult);
    #define MAX_MS 33



    // if (fn_clampi(frameTime,0,MAX_MS)*mult > 0.0)
    // {
    //  Uint32 start_gamecode = SDL_GetTicks();
    renderState->actual_dt = fn_clamp(frameTime,1,MAX_MS);

    if (!paused_gameplay)
    {
      th_runProgram(renderState,&input,fn_clamp(frameTime,1,MAX_MS)*renderState->time_mult,&audioSystem,&config);
      //  th_printlnDevConsole("%i",SDL_GetTicks() - start_gamecode);
      //  }
      a_VirtualMix(fn_clamp(frameTime,1,MAX_MS)*renderState->time_mult);
    }
    else
    {
      th_updateRenderMatrices(renderState);

      th_runUI(renderState,&input);

      a_UpdateGainPitch();
    }

    if (first_boot)
    {
      if (!set_boot_music)
      {
        a_VirtualSource* track = a_playMusicTrack(music_track_title,0);
        set_boot_music = true;
      }
      a_VirtualSource* track = a_getMusicTrack(0);

      a_setGainMusicTrack(1.0,0);
      a_setVSPitch(track,1.0/0.3);
      th_incrementViolence();
    }



    // accumulator -= dt;
    // }

    th_doRendering(renderState);
    fn_updateWindow(&window);
    //fn_moveMouse(&window,config.width/2.f,config.height/2.f);

    th_tickFrame();
    th_addTimeGlobal(fn_clamp(frameTime,1,MAX_MS));
    if (!paused_gameplay)
    {
      th_addTime(fn_clamp(frameTime,1,MAX_MS)*renderState->time_mult);
      th_tickReplayFrameCapture(rp);
    }

    Uint64 time_end = SDL_GetPerformanceCounter();

    double framedurr = (double)(time_end - start) / (double)freq;
    framedurr = framedurr * 1000.0;

    if (min_ms > framedurr)
    {
      double durr = (min_ms/1000.0)*freq;
      Uint64 target = start + (Uint64)durr ;
      while (target > SDL_GetPerformanceCounter())
      {
        if ((double)(target - SDL_GetPerformanceCounter()) >= ((double)freq/1000.0))
        {
          SDL_Delay(1);
        }
        else
        {
          #ifdef _WIN32
          Sleep(0);
          #else
          sched_yield();
          #endif
        }


      };
    }



    if (renderState->th_load_new_level)
    {
      first_boot = false;
      th_cleanupReplay(rp);
      int level_index = renderState->levelSelectLayout.selected_level;
      printf("Loading level %i %s\n",level_index,renderState->list_of_levels.levelfiles[level_index].name);

      *rp = DEFAULT_REPLAY_STATE;
      th_unloadLoadData(renderState,renderState->list_of_levels.levelfiles[level_index].name,renderState->list_of_levels.levelfiles[level_index].spawnset_name);

      renderState->gameState = STATE_GAMEPLAY;
      renderState->th_load_new_level = false;
      renderState->th_respawn_flag = false;

      SDL_SetRelativeMouseMode(SDL_TRUE);
      fn_moveMouse(&window,config.width/2.f,config.height/2.f);

      renderState->th_crosshair_size = 1.0;
      paused_gameplay = false;

      renderState->th_inphoto = false;
      renderState->photo_camera_locked = false;
    }

    // if (input.currentKeyStates[SDL_SCANCODE_U] && !input.currentKeyStatesPrev[SDL_SCANCODE_U])
    // {
    //   paused_gameplay = !paused_gameplay;
    // }
    if (input.currentKeyStates[SDL_SCANCODE_ESCAPE] && !input.currentKeyStatesPrev[SDL_SCANCODE_ESCAPE] && renderState->gameState == STATE_GAMEPLAY)
    {
      paused_gameplay = true;
      renderState->gameState = STATE_PAUSED;
      renderState->gameState_return = STATE_PAUSED;
    }
    else if ((renderState->th_resume_flag || (input.currentKeyStates[SDL_SCANCODE_ESCAPE] && !input.currentKeyStatesPrev[SDL_SCANCODE_ESCAPE] ) ) && renderState->gameState == STATE_PAUSED)
    {
      renderState->th_resume_flag = false;
      paused_gameplay = false;
      renderState->gameState = STATE_GAMEPLAY;

      SDL_SetRelativeMouseMode(SDL_TRUE);
      fn_moveMouse(&window,config.width/2.f,config.height/2.f);
      // SDL_SetRelativeMouseMode(SDL_TRUE);
      //SDL_SetWindowGrab(window.gWindow, SDL_TRUE); // optional
    }

    //SDL_Delay(40.0);

  }

  printf("Shutting Down\n");

  if (savecap)
  {
      th_saveCapture(rp,"intro_compressed.cap",true);
  }



  //go
  if (record)
  {
    FILE* fp = th_fopen(replayname,"wb");
    fwrite(&replayFrames,sizeof(int),1,fp);
    fwrite(replayBuffer,sizeof(th_ReplayFrame),replayFrames,fp);
    fclose(fp);
  }


  a_closeAudioSys(&audioSystem);
  fn_closeWindow(&window);
  free(renderState);
  free(rp);

  //muntrace();
  return 0;
}
