#pragma once
#include "../fn_input.h"
#include "../fn_config.h"
#include "th_audio.h"
#include "th_renderer.h"





void th_bindGlobalTextures(th_RendererState* state);

void th_doRendering(th_RendererState* state);
void th_LoadData(bool sharm,bool gencubemaps,int resolution,fn_Config* config,bool iter,th_RendererState* state,fn_RawInput* input,const char* levelname,const char* spawnsetname);

void th_updateRenderMatrices(th_RendererState* state);
void th_runProgram(th_RendererState* state,fn_RawInput* input,float dt,a_AudioSystem* audiosystem,fn_Config* config);
void th_runUI(th_RendererState* state,fn_RawInput* input);

void th_render(fn_mat4 modelViewprojection,fn_mat4 proj,fn_mat4 view,fn_vec2 screenSize,th_RendererState* state);

void th_unloadLoadData(th_RendererState* state,const char* levelname,const char* spawnsetname);
void th_unloadData(th_RendererState* state);

void th_writeVictory(th_VictoryManifest list_of_victory,th_LevelManifest list_of_levels,char* path);
