#pragma once
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define TH_AUDIO_DWORD unsigned int
#define TH_AUDIO_BYTE unsigned char
#include "../fn_math/fn_math.h"
#include "../../include/AL/al.h"
#include "../../include/AL/alc.h"
#include "../../include/AL/alext.h"
#include "../../include/AL/efx.h"
#include "../../include/AL/efx-presets.h"
#include "../../include/AL/efx-creative.h"
//#define DISABLE_AUDIO

//used to communicate from the audio system to a game object that a virtual source is no longer valid
//takes some data
typedef void (*cleanup_callback_t)(void*);

typedef struct
{
  void* data;
  cleanup_callback_t callback;
}a_CleanupCallback;

typedef struct
{
  ALCdevice *device;                                                          //Create an OpenAL Device
  ALCcontext *context;                                                        //And an OpenAL Context
  ALfloat orientation[6];
}a_AudioSystem;

typedef struct
{
  FILE *fp;
  char type[4];
  TH_AUDIO_DWORD size,chunkSize;
  short formatType,channels;
  TH_AUDIO_DWORD sampleRate,avgBytesPerSec;
  short bytesPerSample,bitsPerSample;
  TH_AUDIO_DWORD dataSize;

  float length_in_seconds;
  ALuint buffer;                                                           //Stores the sound data
  ALuint frequency;                                               //The Sample Rate of the WAVE file
  ALenum format;                                                            //The audio format (bits per sample, number of channels)
  unsigned char* buf ;
}a_AudioFile;



void a_initAudioSys(a_AudioSystem* a,bool hrtf_enable);

void a_closeAudioSys(a_AudioSystem* a);

void a_loadFile(a_AudioFile* file,const char* filename);
void a_deleteFile(a_AudioFile* file);

void a_setPos(a_AudioSystem* a,fn_vec3 location,fn_vec3 forward,fn_vec3 up);
fn_vec3 a_getPos();
void a_setVelocity(a_AudioSystem* a,fn_vec3 velocity);
void a_setGain(float gain);
void a_setGainMusic(float gain);
void a_setGainMaster(float gain);



void a_setUnits(float meters_per_unit);
void a_setPitch(float pitch);

int a_addFile(const char* filename);

int a_addFileMusic(const char* filename);

int a_getRandomMusicTrack();

float a_getPitch();

///2021333
//52102133333
typedef enum
{
  BUMPED_OFF, //0
  RE_PLAYED, //1
  PHYSICALLY_FIRSTPLAY,//2
  STOPPED,//3
  SUPERCEDED,//4
  VIRTUAL_FIRSTPLAY,//5
  STOPPED_A,//6
  STOPPED_B,//7
  STOPPED_C,//8
}th_JournalEntry;


#define TH_MAX_AUDIO_DUCK 6
typedef struct
{
  ALfloat SourcePos[3] ;                                    //Position of the source sound
  ALfloat SourceVel[3] ;                                    //Velocity of the source sound
  float gain ;
  float gain_music;
  float pitch;
  bool mono;
  bool loop;
  bool playing;
  int file_index;
  float offset;
  ALuint* physical_source;

  int priority;
  int vid;
  int physical_id;
  int physical_mix_id;

  int virtual_mix_id;

  th_JournalEntry journal[256];
  int journal_count;

  int memory_location;

  int memory_physical_location;

  bool bumped_off;

  float dist_squared_listener;

  a_CleanupCallback cleanup;
  bool run_cleanup;

  bool relative_to_listener;

  float rolloff;


}a_VirtualSource;

void a_audioClearFiles();

void a_setGainMusicTrack(float gain,int track_num);

a_VirtualSource* a_playMusicTrack(int file_id,int track_num);

a_VirtualSource* a_getMusicTrack(int track_num);

void a_setAudioFileProperties(int file_id, int max_sources,float radius,int within_radius);
a_VirtualSource* a_playVirtualSource(int file_id,int priority,fn_vec3 position,a_VirtualSource* in);
void a_setVSRelativeToListener(a_VirtualSource* source,bool status);
void a_setVSPos(a_VirtualSource* source,fn_vec3 p);
void a_setVSVel(a_VirtualSource* source,fn_vec3 v);
void a_setVSLoop(a_VirtualSource* source,bool loop);
void a_setVSGain(a_VirtualSource* source,float gain);
void a_setVSOffset(a_VirtualSource* source,float offset );
void a_playVS(a_VirtualSource* source);
void a_pauseVS(a_VirtualSource* source);
void a_playVSNoChange(a_VirtualSource* source);

void a_stopVSMusic(int track_num);
void a_stopVS(a_VirtualSource* source);
bool a_VSplaying(a_VirtualSource* source);
void a_setVSPitch(a_VirtualSource* source,float pitch);

//set a reference to null
void a_standardCleanup(void* data);

void a_setVSCleanup(a_VirtualSource* source,void* data,cleanup_callback_t callback);

void a_VirtualMix(float dt);

void a_stopAllSources();

void a_duckVS(a_VirtualSource* source,float duck_duration,float duck_fraction);

void a_setVSRolloff(a_VirtualSource* source,float rolloff);

void a_UpdateGainPitch();
