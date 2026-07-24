#pragma once

typedef enum
{
  TH_PILLARBOX,
  TH_LETTERBOX,
  TH_SCALEBOX,
  TH_RAWOUTPUT,
}th_UpscaleBoxMode;

typedef struct
{
  int width;
  int height;
  bool fullscreen;
  float viewportscale;
  int gl_major;
  int gl_minor;
  bool vsync;
  bool invMouse;
  float fov;
  float mouseSens;
  float mastervolume;
  float musicvolume;


  int dynamic_shadows;
  int shadow_quality;
  int reflection_quality;
  int fog_quality;
  int reflection_scale;

  int particle_lighting;

  int maxFPS;

  bool borderless_fullscreen;
  bool window_hidden;

  //only used for borderless fullscreen pillarboxing
  //destination resolution
  int d_width;
  int d_height;
  float scale_factor;

  th_UpscaleBoxMode upscale_mode;

  bool dpi_nonsense;

  float exposure;
  float gamma;
}fn_Config;

#define TH_FOG_MIN_DIST 3000.0

#define TH_FOG_MED_DIST 4000.0

#define TH_FOG_MAX_DIST 7000.0
