#include "th_audio.h"
#include "th_system.h"
#include "th_time.h"
#include "../th_fopen.h"
#include <pthread.h>

// #define AUDIO_ERRCHECK
//Audio System
static fn_vec3 playerPos;
static LPALCGETSTRINGISOFT alcGetStringiSOFT;
static LPALCRESETDEVICESOFT alcResetDeviceSOFT;

static pthread_mutex_t audio_lock;

static ALuint* physical_sources = NULL;
static int physical_source_count = 0;
static int physical_source_index = 0;

static a_AudioFile* files = NULL;
static int filecount = 0;

static int* music_files = NULL;
static int music_filecount = 0;

#define MAX_AUDIO_FILES 1024

static const float TH_DEFAULT_ROLLOFF = 0.75;
static int* file_instance_count = NULL; //[MAX_AUDIO_FILES];
static int* max_sources_table = NULL; //[MAX_AUDIO_FILES];

static float* radius_table = NULL; //[MAX_AUDIO_FILES];
static int* within_radius_table = NULL; //[MAX_AUDIO_FILES];

static a_VirtualSource* virtual_sources = NULL;
static const int virtual_sources_count = 4096;
static int virtual_sources_index = 0;

static a_VirtualSource** virtualMix = NULL;
static int virtualMix_count = 0;

static a_VirtualSource** physicalMix = NULL;
static int physicalMix_count = 0;

static float globalPitch = 1.0;

static float master_gain = 1.0;

static float internal_master_gain = 1.0;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define ERROR_MSG(msg) __FILE__ ":" TOSTRING(__LINE__) ": " msg

static void doALerrcheck(const char* warning)
{
  int err = alGetError();
  if(err != AL_NO_ERROR)
    printf("%s %i\n",warning,err);
}

typedef struct
{
  a_VirtualSource* source;
  th_timer_t duck_time;
  float duck_duration;
  float duck_fraction;
}th_AudioDuck;

static th_AudioDuck duckers[TH_MAX_AUDIO_DUCK];
static int ducking_count = 0;
//
struct QueueNode
{
  int val;
  struct QueueNode* next;
};

typedef struct
{
  struct QueueNode* bottom;
  struct QueueNode* top;
}th_Queue;


static th_Queue virtual_queue;
static th_Queue physical_queue;

//use the top 2 physical sources for music
static float music_volume_gain = 1.0;
static a_VirtualSource* music_sources = NULL;
static const int th_music_max_sources = 2.0;

void a_setGainMusicTrack(float gain,int track_num)
{
  if (music_sources[track_num].physical_source != NULL)
  {
    music_sources[track_num].gain_music = gain;
    #ifdef DISABLE_AUDIO
    return;
    #endif
    if (gain > 1)
    {
      gain = 1;
    }
    music_sources[track_num].gain = gain;
    if (music_sources[track_num].physical_source != NULL)
    {
      alSourcef (*music_sources[track_num].physical_source, AL_GAIN,   music_volume_gain*music_sources[track_num].gain_music    );

      #ifdef AUDIO_ERRCHECK
      doALerrcheck(ERROR_MSG("Audio Error"));
      #endif
    }
  }
}

void a_setGainMusic(float gain)
{
  music_volume_gain = gain;

  for (int i = 0 ; i < 2;i++)
  {
    if (music_sources[i].physical_source != NULL)
    {
      //a_setVSGain(&music_sources[i],music_volume_gain*music_sources[i].gain_music);

      #ifdef DISABLE_AUDIO
      continue;
      #endif
      if (gain > 1)
      {
        gain = 1;
      }
      music_sources[i].gain = gain;
      if (music_sources[i].physical_source != NULL)
      {
        alSourcef (*music_sources[i].physical_source, AL_GAIN,   music_volume_gain*music_sources[i].gain_music    );

        #ifdef AUDIO_ERRCHECK
        doALerrcheck(ERROR_MSG("Audio Error"));
        #endif
      }
    }
  }
}



void th_initQueue(th_Queue* q)
{
  q->bottom = NULL;
  q->top = NULL;
}

void th_queueAddTop(th_Queue* q,int i)
{
  if (q->bottom == NULL && q->top == NULL)
  {
  //  printf("%s\n","A" );
    q->bottom = malloc(sizeof(struct QueueNode));
    q->top = NULL;
    q->bottom->next = NULL;

    q->bottom->val = i;
  }
  else if (q->bottom == q->top)
  {
  //  printf("%s\n","B" );
    q->top = malloc(sizeof(struct QueueNode));
    q->bottom->next = q->top;
    q->top->val = i;
    q->top->next = NULL;
  }
  else if (q->bottom != NULL && q->top == NULL)
  {
  //  printf("%s\n","C" );
    q->top = malloc(sizeof(struct QueueNode));
    q->bottom->next = q->top;
    q->top->val = i;
    q->top->next = NULL;
  }
  else
  {
  //  printf("%s\n","D" );
    struct QueueNode* newnode = malloc(sizeof(struct QueueNode));
    newnode->val = i;
    newnode->next = NULL;

    // q->top->val = i;
    q->top->next = newnode;

    q->top = newnode;
  }
}

bool th_queueRemoveBottom(th_Queue* q,int* i)
{
  if (q->bottom == NULL && q->top == NULL)
  {
    return false;
  }
  else if (q->bottom == q->top)
  {
    *i = q->bottom->val;
    free(q->bottom);
    q->bottom = NULL;
    q->top = NULL;
  }
  else if (q->bottom != NULL && q->top == NULL)
  {
    *i = q->bottom->val;
    free(q->bottom);
    q->bottom = NULL;
  }
  else if (q->bottom != NULL)
  {
    *i = q->bottom->val;
    struct QueueNode* oldnode = q->bottom;
    q->bottom = q->bottom->next;
    free(oldnode);
  }
  else
  {
    return false;
  }
  return true;

}

static void a_journalAdd(a_VirtualSource* s,th_JournalEntry e)
{
  s->journal[s->journal_count] = e;
  s->journal_count++;
  if (s->journal_count >= 256)
  {
    s->journal_count = 0;
  }
}


static void a_lockAudioMutex()
{
  pthread_mutex_lock(&audio_lock);
}

static void a_unlockAudioMutex()
{
  pthread_mutex_unlock(&audio_lock);
}


static bool physicalIsPlaying(ALuint s)
{
  ALint sourceState;
  alGetSourcei(s, AL_SOURCE_STATE, &sourceState);

  ALint loopstate;
  alGetSourcei(s, AL_LOOPING, &loopstate);

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif


  return sourceState == AL_PLAYING || loopstate == AL_TRUE;
}

static bool physicalIsStopped(ALuint s)
{
  ALint sourceState;
  alGetSourcei(s, AL_SOURCE_STATE, &sourceState);

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif

  return sourceState == AL_STOPPED ;
}

static void linkSource(a_VirtualSource* source,ALuint buffer,bool first_time)
{
  if (source->physical_source != NULL)
  {

      alSourcei (*source->physical_source, AL_BUFFER,   buffer);                                 //Link the buffer to the source
  }

  ALint format;
  alGetBufferi(buffer,AL_CHANNELS,&format);
  if (format == 1)
  {
    source->mono = true;
  }
  else
  {
    source->mono = false;
  }

  if (first_time)
  {
    if (source->physical_source != NULL)
    {
      alSourcef (*source->physical_source, AL_PITCH,    1.0f     );                                 //Set the pitch of the source
      alSourcef (*source->physical_source, AL_GAIN,     1.0f     );                                 //Set the gain of the source
      alSourcefv(*source->physical_source, AL_POSITION, fn_createVec3(0,0,0).v);                                 //Set the position of the source
      alSourcefv(*source->physical_source, AL_VELOCITY, fn_createVec3(0,0,0).v);                                 //Set the velocity of the source
      alSourcei (*source->physical_source, AL_LOOPING,  AL_FALSE );                                 //Set if source is looping sound
    }

      source->gain  = 1.0;
      source->pitch = 1.0;
      source->loop = false;
      source->offset = 0;
      source->playing = true;
  }




    if (source->physical_source != NULL)
    {
          alSourcePlay(*source->physical_source);
  if (!source->mono)
  {
    alSourcei(*source->physical_source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
  }
  }
}


static ALCint nummono;
static ALCint numstereo;
void a_initAudioSys(a_AudioSystem* a,bool hrtf_enable)
{

  file_instance_count = malloc(sizeof(int)*MAX_AUDIO_FILES); //[MAX_AUDIO_FILES];
  max_sources_table =  malloc(sizeof(int)*MAX_AUDIO_FILES); //[MAX_AUDIO_FILES];

  radius_table =  malloc(sizeof(float)*MAX_AUDIO_FILES); //[MAX_AUDIO_FILES];
   within_radius_table = malloc(sizeof(int)*MAX_AUDIO_FILES);

  #ifdef DISABLE_AUDIO
  virtual_sources = malloc(sizeof(a_VirtualSource));

  virtual_sources[0].playing = false;
  virtual_sources[0].physical_source = NULL;
  virtual_sources[0].file_index = 0;
  virtual_sources[0].journal_count = 0;
  virtual_sources[0].memory_location = 0;
  virtual_sources[0].bumped_off = false;
  virtual_sources[0].physical_mix_id = 0;
  virtual_sources[0].run_cleanup = false;
  virtual_sources[0].rolloff = TH_DEFAULT_ROLLOFF;

  return;
  #endif

  for (int i = 0; i < MAX_AUDIO_FILES; i++) {
    max_sources_table[i] = 10;
    file_instance_count[i] = 0;
    radius_table[i] = 200;
    within_radius_table[i] = 3;
  }

  th_initQueue(&virtual_queue);
  th_initQueue(&physical_queue);

  pthread_mutex_init(&audio_lock, NULL);

  ALint attribs[7] = { 0 };
  ALCint iSends = 0;

  attribs[0] = ALC_MAX_AUXILIARY_SENDS;
  attribs[1] = 4;
  attribs[2] = ALC_STEREO_SOURCES;
  attribs[3] = 2;
  attribs[4] = ALC_MONO_SOURCES ;
  attribs[5] = 100;
  attribs[6] = 0;

  a->device = alcOpenDevice(NULL);                                               //Open the device
  if(!a->device) printf("no sound device");                         //Error during device oening
  a->context = alcCreateContext(a->device, attribs);                                   //Give the device a context
  alcMakeContextCurrent(a->context);                                             //Make the context the current
  if(!a->context) printf("no sound context");                       //Error during context handeling

  alcGetIntegerv(a->device, ALC_MAX_AUXILIARY_SENDS, 1, &iSends);
  printf("Device supports %d Aux Sends per Source\n", iSends);

//   alGenEffects=(LPALGENEFFECTS)
//  alGetProcAddress("alGenEffects");
// alDeleteEffects=(LPALDELETEEFFECTS)
//  alGetProcAddress("alDeleteEffects");
// alIsEffect=(LPALISEFFECT)
//  alGetProcAddress("alIsEffect");

  alSpeedOfSound(343.3*100);
  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
  alDopplerFactor(1500.5);

alcGetIntegerv(a->device, ALC_MONO_SOURCES, 1, &nummono);
alcGetIntegerv(a->device, ALC_STEREO_SOURCES, 1, &numstereo);
virtualMix = malloc(sizeof(a_VirtualSource*)*virtual_sources_count);

virtual_sources = malloc(sizeof(a_VirtualSource)*virtual_sources_count);
for (int i = 0 ; i < virtual_sources_count;i++)
{
  virtualMix[i] = NULL;
  virtual_sources[i].playing = false;
  virtual_sources[i].physical_source = NULL;
  virtual_sources[i].file_index = 0;
  virtual_sources[i].journal_count = 0;
  virtual_sources[i].memory_location = i;
  virtual_sources[i].bumped_off = false;
  virtual_sources[i].physical_mix_id = 0;
  virtual_sources[i].run_cleanup = false;
  virtual_sources[i].gain_music = 1.0;
  virtual_sources[i].rolloff = TH_DEFAULT_ROLLOFF;
  th_queueAddTop(&virtual_queue,i);
}

music_sources = malloc(sizeof(a_VirtualSource)*th_music_max_sources);
for (int i = 0 ; i < th_music_max_sources;i++)
{
  music_sources[i].playing = false;
  music_sources[i].physical_source = NULL;
  music_sources[i].file_index = 0;
  music_sources[i].journal_count = 0;
  music_sources[i].memory_location = i;
  music_sources[i].bumped_off = false;
  music_sources[i].physical_mix_id = 0;
  music_sources[i].run_cleanup = false;
  music_sources[i].gain_music = 1.0;
  music_sources[i].rolloff = 0.0;
}

physicalMix = malloc(sizeof(a_VirtualSource*)*(nummono + numstereo));
physical_sources = malloc(sizeof(ALuint)*(nummono + numstereo));
physical_source_count = nummono - 2;
for (int i = 0 ; i < nummono + numstereo;i++)
{
  physicalMix[i]  = NULL;
  alGenSources(1, &physical_sources[i]);
  // printf("%i\n",physical_sources[i] );
}

for (int i = 0; i < physical_source_count; i++) {
  th_queueAddTop(&physical_queue,i);
}

printf("Maximum Audio Sources MONO: %i STEREO: %i\n",nummono,numstereo );
alcGetStringiSOFT = (LPALCGETSTRINGISOFT)alcGetProcAddress(a->device,"alcGetStringiSOFT");
alcResetDeviceSOFT = (LPALCRESETDEVICESOFT)alcGetProcAddress(a->device,"alcResetDeviceSOFT");

if(!alcIsExtensionPresent(a->device, "ALC_SOFT_HRTF"))
{
    printf("Error: ALC_SOFT_HRTF not supported\n");

}

if (hrtf_enable)
{
  ALCint num_hrtf;
  const char *hrtfname = "44100";
  alcGetIntegerv(a->device, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &num_hrtf);
      ALCint* attr = malloc(sizeof(ALCint)*5);
      ALCint index = -1;
      ALCint i;

      printf("Available HRTFs:\n");
      for(i = 0;i < num_hrtf;i++)
      {
          const ALCchar *name = alcGetStringiSOFT(a->device, ALC_HRTF_SPECIFIER_SOFT, i);
          printf("    %d: %s\n", i, name);

          /* Check if this is the HRTF the user requested. */
          if(hrtfname && strcmp(name, hrtfname) == 0)
              index = i;
      }

      i = 0;
      attr[i++] = ALC_HRTF_SOFT;
      attr[i++] = ALC_TRUE;
      if(index == -1)
      {
          if(hrtfname)
              printf("HRTF \"%s\" not found\n", hrtfname);
          printf("Using default HRTF...\n");
      }
      else
      {
          printf("Selecting HRTF %d...\n", index);
          attr[i++] = ALC_HRTF_ID_SOFT;
          attr[i++] = index;
      }
      attr[i] = 0;

      if(!alcResetDeviceSOFT(a->device, attr))
          printf("Failed to reset a->device: %s\n", alcGetString(a->device, alcGetError(a->device)));

  free(attr);
  }
  ALCint hrtfenabled;
  alcGetIntegerv(a->device, ALC_HRTF_SOFT, 1, &hrtfenabled);
  if(hrtfenabled)
    printf("%s\n","HRTF Enabled!" );

  if (alcIsExtensionPresent(a->device, "ALC_EXT_EFX") == AL_FALSE)
  {
    printf("EFX Extension NOT found!\n");
  }
  else
  {
    printf("EFX Extension found!\n");
  }

  playerPos = fn_createVec3(0,0,0);


  int err = alGetError();
  if(err != AL_NO_ERROR)
    printf("Error initializing AL %i\n",err);
}

void a_closeAudioSys(a_AudioSystem* a)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  pthread_mutex_destroy(&audio_lock);
  alcMakeContextCurrent(NULL);
  alcDestroyContext(a->context);
  alcCloseDevice(a->device);
}


void a_loadFile(a_AudioFile* file,const char* filename)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
                                                            //Create FILE pointer for the WAVE file
 file->fp=th_fopen(filename,"rb");                                            //Open the WAVE file
 if (!file->fp) {printf("%s\n","File error" ); perror("File error");}                        //Could not open file

 //Variables to store info about the WAVE file (all of them is not needed for OpenAL)


 //Check that the WAVE file is OK
 fread(file->type,sizeof(char),4,file->fp);                                              //Reads the first bytes in the file
 if(file->type[0]!='R' || file->type[1]!='I' || file->type[2]!='F' || file->type[3]!='F')            //Should be "RIFF"
 printf("No RIFF");                                            //Not RIFF

 fread(&file->size, sizeof(TH_AUDIO_DWORD),1,file->fp);                                           //Continue to read the file
 fread(file->type, sizeof(char),4,file->fp);                                             //Continue to read the file
 if (file->type[0]!='W' || file->type[1]!='A' || file->type[2]!='V' || file->type[3]!='E')           //This part should be "WAVE"
 printf("not WAVE");                                            //Not WAVE

 fread(file->type,sizeof(char),4,file->fp);                                              //Continue to read the file
 if (file->type[0]!='f' || file->type[1]!='m' || file->type[2]!='t' || file->type[3]!=' ')           //This part should be "fmt "
 printf("not fmt ");                                            //Not fmt

 //Now we know that the file is a acceptable WAVE file
 //Info about the WAVE data is now read and stored
 fread(&file->chunkSize,sizeof(TH_AUDIO_DWORD),1,file->fp);
 fread(&file->formatType,sizeof(short),1,file->fp);
 fread(&file->channels,sizeof(short),1,file->fp);
 fread(&file->sampleRate,sizeof(TH_AUDIO_DWORD),1,file->fp);
 fread(&file->avgBytesPerSec,sizeof(TH_AUDIO_DWORD),1,file->fp);
 fread(&file->bytesPerSample,sizeof(short),1,file->fp);
 fread(&file->bitsPerSample,sizeof(short),1,file->fp);

 fread(file->type,sizeof(char),4,file->fp);
 // if (type[0]!='d' || type[1]!='a' || type[2]!='t' || type[3]!='a')           //This part should be "data"
 // return endWithError("Missing DATA");                                        //not data

 fread(&file->dataSize,sizeof(TH_AUDIO_DWORD),1,file->fp);                                        //The size of the sound data is read


 file->length_in_seconds = (float)file->dataSize / ((float)file->sampleRate * (float)file->channels * (file->bitsPerSample/8.0));
 printf("%s %f %i\n", filename,file->length_in_seconds,file->sampleRate);
 //Display the info about the WAVE file
 // cout << "Chunk Size: " << chunkSize << "\n";
 // cout << "Format Type: " << formatType << "\n";
 // cout << "Channels: " << channels << "\n";
 // cout << "Sample Rate: " << sampleRate << "\n";
 // cout << "Average Bytes Per Second: " << avgBytesPerSec << "\n";
 // cout << "Bytes Per Sample: " << bytesPerSample << "\n";
 // cout << "Bits Per Sample: " << bitsPerSample << "\n";
 // cout << "Data Size: " << dataSize << "\n";

 file->buf= malloc( sizeof(unsigned char)*file->dataSize);                            //Allocate memory for the sound data
 fread(file->buf,sizeof(TH_AUDIO_BYTE),file->dataSize,file->fp);           //Read the sound data and display the
                                                                             //number of bytes loaded.
                                                                             //Should be the same as the Data Size if OK
 file->frequency=file->sampleRate;
 alGenBuffers(1, &file->buffer);

 //Figure out the format of the WAVE file
 file->format = 0;
 if(file->bitsPerSample == 8)
 {
     if(file->channels == 1)
         file->format = AL_FORMAT_MONO8;
     else if(file->channels == 2)
         file->format = AL_FORMAT_STEREO8;
 }
 else if(file->bitsPerSample == 16)
 {
     if(file->channels == 1)
         file->format = AL_FORMAT_MONO16;
     else if(file->channels == 2)
         file->format = AL_FORMAT_STEREO16;
 }
  if(!file->format) printf("Wrong BitPerSample");                      //Not valid format

 alBufferData(file->buffer, file->format, file->buf, file->dataSize, file->frequency);                    //Store the sound data in the OpenAL Buffer
 if(alGetError() != AL_NO_ERROR)
 printf("Error loading ALBuffer %i\n",alGetError());
//  printf("%i\n%i\n%d\n%u\n%i\n",(file->buffer),file->format,file->buf,file->dataSize,file->frequency);
  free(file->buf);
  fclose(file->fp);
}

void a_deleteFile(a_AudioFile* file)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  alDeleteBuffers(1,&file->buffer);
}

void a_setPos(a_AudioSystem* a,fn_vec3 location,fn_vec3 forward,fn_vec3 up)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  a->orientation[0]=forward.x;
  a->orientation[1]=forward.y;
  a->orientation[2]=forward.z;
  a->orientation[3]=up.x;
  a->orientation[4]=up.y;
  a->orientation[5]=up.z;
  playerPos = location;
  alListener3f(AL_POSITION,location.x,location.y,location.z);                                  //Set position of the listener
  alListener3f(AL_VELOCITY,0,0,0);                                  //Set velocity of the listener
  alListenerfv(AL_ORIENTATION, a->orientation);                                  //Set orientation of the listener

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

fn_vec3 a_getPos()
{
  #ifdef DISABLE_AUDIO
  return fn_createVec3s(0);
  #endif
  return playerPos;
}

void a_setVelocity(a_AudioSystem* a,fn_vec3 velocity)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  alListener3f(AL_VELOCITY,velocity.x,velocity.y,velocity.z);

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

void a_setUnits(float meters_per_unit)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
    alListenerf(AL_METERS_PER_UNIT, meters_per_unit);

    #ifdef AUDIO_ERRCHECK
    doALerrcheck(ERROR_MSG("Audio Error"));
    #endif
}

void a_setGain(float gain)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  master_gain = gain;
    alListenerf(AL_GAIN, 1.0);

    #ifdef AUDIO_ERRCHECK
    doALerrcheck(ERROR_MSG("Audio Error"));
    #endif
}

void a_setGainMaster(float gain)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  internal_master_gain = gain;
}



void a_setPitch(float pitch)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  globalPitch = pitch;
}

float a_getPitch()
{
  #ifdef DISABLE_AUDIO
  return 1.0;
  #endif
  return fn_clamp(globalPitch,0.01,10.0);
}

int a_addFile(const char* filename)
{
  #ifdef DISABLE_AUDIO
  return 0;
  #endif
  files = realloc(files,sizeof(a_AudioFile)*(filecount + 1));
  a_loadFile(&files[filecount],filename);
  filecount++;
  return filecount - 1;
}

int a_addFileMusic(const char* filename)
{
  #ifdef DISABLE_AUDIO
  return 0;
  #endif
  int fid = a_addFile(filename);
  music_files = realloc(music_files,sizeof(int)*(music_filecount + 1));
  music_files[music_filecount] = fid;
  music_filecount++;

  return fid;
}

int a_getRandomMusicTrack()
{
  if (music_filecount == 0)
  {
    return 0;
  }
  int idx = th_frame() % (music_filecount - 3);//last 3 are victory and loss themes, and title themes
  return music_files[idx];
}

//set a reference to null
void a_standardCleanup(void* data)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  a_VirtualSource** vptr = (a_VirtualSource**)data;
  *vptr = NULL;
}

void a_setVSCleanup(a_VirtualSource* source,void* data,cleanup_callback_t callback)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->run_cleanup = true;
  source->cleanup.callback = callback;
  source->cleanup.data = data;
}


//Virtual sources
a_VirtualSource* a_playVirtualSource(int file_id,int priority,fn_vec3 position,a_VirtualSource* in)
{
  #ifdef DISABLE_AUDIO
  return &virtual_sources[0];
  #endif
 a_lockAudioMutex();

 int id = 0;
 bool fail = th_queueRemoveBottom(&virtual_queue,&id );
 if (!fail)
 {
   printf("%s\n","NO VIRTUAL SOURCES LEFT!!!" );
 }


 a_VirtualSource* r = &virtual_sources[id];
 r->priority = priority;
 r->bumped_off = false;

 virtualMix[virtualMix_count] = r;
 virtualMix_count++;
 r->physical_source = NULL;
 r->offset = 0;
 r->playing = true;
 r->file_index = file_id;
 r->vid = id;
 r->physical_id = 0;
 r->physical_mix_id = 0;
 r->run_cleanup = false;

 r->gain  = 1.0;
 r->pitch = 1.0;
 r->loop = false;
 r->offset = 0;
 r->playing = true;
 r->relative_to_listener = false;
 r->rolloff = TH_DEFAULT_ROLLOFF;



  a_unlockAudioMutex();

  return r;
}


a_VirtualSource* a_playMusicTrack(int file_id,int track_num)
{
  if (track_num > 1)
  {
    printf("Only 2 music tracks\n");
    return NULL;
  }

  a_lockAudioMutex();

  bool looped = track_num == 0;



  a_VirtualSource* r = &music_sources[track_num];


  if (a_VSplaying(r) && r->physical_source != NULL)
  {
    a_stopVSMusic(track_num);
  }


  r->priority = 10;
  r->bumped_off = false;

  r->physical_source = NULL;
  r->offset = 0;
  r->playing = true;
  r->file_index = file_id;
  r->vid = -1; //DONT RELY ON THIS AHHHHHHHH
  r->physical_id = 0;
  r->physical_mix_id = 0;
  r->run_cleanup = false;

  r->gain  = music_volume_gain;
  r->pitch = 1.0;
  r->loop = looped;
  r->offset = 0;
  r->playing = true;
  r->relative_to_listener = false;


  //pick a physical source
  int pid = nummono + track_num;

  //play new



  ALuint* phys = &physical_sources[pid];
  r->physical_source = phys;
  linkSource(r,files[r->file_index].buffer,false);
  r->physical_source = phys;
  r->physical_id = pid;
  a_setVSLoop(r,r->loop);
  a_setVSGain(r,r->gain);
  a_playVS(r);
  a_setVSOffset(r,0);

  a_unlockAudioMutex();

  return r;
}

a_VirtualSource* a_getMusicTrack(int track_num)
{
  return &music_sources[track_num];
}

void a_setVSRelativeToListener(a_VirtualSource* source,bool status)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->relative_to_listener = status;
  if (source->physical_source != NULL)
  {
    if (status)
    {
      alSourcei(*source->physical_source, AL_SOURCE_RELATIVE, AL_TRUE);
    }
    else
    {
      alSourcei(*source->physical_source, AL_SOURCE_RELATIVE, AL_FALSE);
    }

  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

void a_setVSPos(a_VirtualSource* source ,fn_vec3 p)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->SourcePos[0] = p.x;source->SourcePos[1] = p.y;source->SourcePos[2] = p.z;
  if (source->physical_source != NULL)
  {
    alSourcefv(*source->physical_source, AL_POSITION, source->SourcePos);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

void a_setVSPitch(a_VirtualSource* source,float pitch)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->pitch = pitch;
}

void a_setVSVel(a_VirtualSource* source,fn_vec3 v)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->SourceVel[0] = v.x;source->SourceVel[1] = v.y;source->SourceVel[2] = v.z;
  if (source->physical_source != NULL)
  {
    alSourcefv(*source->physical_source, AL_VELOCITY, source->SourceVel);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

void a_setVSLoop(a_VirtualSource* source,bool loop)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->loop = loop;

  if (source->physical_source != NULL)
  {
    if(!loop)
    alSourcei (*source->physical_source, AL_LOOPING,  AL_FALSE );
    if(loop)
    alSourcei (*source->physical_source, AL_LOOPING,  AL_TRUE);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif

}

void a_setVSGain(a_VirtualSource* source,float gain)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  if (gain > 1)
  {
    gain = 1;
  }
  source->gain = gain;
  if (source->physical_source != NULL)
  {
    //alSourcef (*source->physical_source, AL_GAIN,   gain    );
    alSourcef (*source->physical_source, AL_GAIN,    source->gain*master_gain*internal_master_gain    );
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

void a_setVSOffset(a_VirtualSource* source,float offset )
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  //printf("%s\n","set offset" );
  source->offset = offset;
  if (source->physical_source != NULL)
  {
    alSourcef(*source->physical_source, AL_SEC_OFFSET, offset);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}


void a_setVSRolloff(a_VirtualSource* source,float rolloff)
{
  source->rolloff = rolloff;
  if (source->physical_source != NULL)
  {
    alSourcef(*source->physical_source,AL_ROLLOFF_FACTOR,source->rolloff);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

void a_playVS(a_VirtualSource* source)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->playing = true;
  if (source->physical_source != NULL)
  {
    alSourcef(*source->physical_source,AL_MIN_GAIN,0.0);
    alSourcef(*source->physical_source,AL_MAX_GAIN,1.0);
    alSourcef(*source->physical_source,AL_MAX_DISTANCE ,2500.0);
    alSourcef(*source->physical_source,AL_ROLLOFF_FACTOR,source->rolloff);
    alSourcef(*source->physical_source,AL_REFERENCE_DISTANCE,500.0);

    alSourcePlay(*source->physical_source);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif

}


void a_playVSNoChange(a_VirtualSource* source)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  source->playing = true;
  if (source->physical_source != NULL)
  {
    alSourcePlay(*source->physical_source);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif

}

void a_pauseVS(a_VirtualSource* source)
{
  if (source->physical_source != NULL)
  {
    alSourcePause(*source->physical_source);
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif

}

void a_stopVSMusic(int track_num)
{
  a_VirtualSource* source = &music_sources[track_num];
  source->playing = false;
  alSourceStop(*source->physical_source);

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif
}

void a_stopVS(a_VirtualSource* source)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
   a_lockAudioMutex();
  //printf("%s\n","Stopping" );
  if (source->playing)
  {
    source->playing = false;
    if (source->physical_source != NULL)
    {
      alSourceStop(*source->physical_source);

      #ifdef AUDIO_ERRCHECK
      doALerrcheck(ERROR_MSG("Audio Error"));
      #endif

      th_queueAddTop(&physical_queue,source->physical_id);
      source->physical_source = NULL;

      int i = source->physical_mix_id;
      if (i == physicalMix_count - 1)
      {
        physicalMix_count--;
      }
      else
      {
        physicalMix[physicalMix_count - 1]->physical_mix_id = i;
        physicalMix[i] = physicalMix[physicalMix_count - 1];
        physicalMix_count--;
      }
    }

    //th_queueAddTop(&virtual_queue,source->vid);

    // if (i == virtualMix_count - 1)
    // {
    //   virtualMix_count--;
    // }
    // else
    // {
    //   virtualMix[i] = virtualMix[virtualMix_count - 1];
    //   virtualMix_count--;
    // }

    for (int i = virtualMix_count - 1; i >= 0; i--) {
      if (!virtualMix[i]->playing  )
      {
        if (virtualMix[i]->run_cleanup)
        {
          virtualMix[i]->cleanup.callback(virtualMix[i]->cleanup.data);
        }

        th_queueAddTop(&virtual_queue,virtualMix[i]->vid);
        if (i == virtualMix_count - 1)
        {
          virtualMix_count--;
        }
        else
        {
          virtualMix[i] = virtualMix[virtualMix_count - 1];
          virtualMix_count--;
        }
      }
    }
  }

  #ifdef AUDIO_ERRCHECK
  doALerrcheck(ERROR_MSG("Audio Error"));
  #endif


  a_unlockAudioMutex();
}

bool a_VSplaying(a_VirtualSource* source)
{
  #ifdef DISABLE_AUDIO
  return false;
  #endif
  if (source->physical_source != NULL)
  {
    return physicalIsPlaying(*source->physical_source);
  }

  return source->playing;
  // ALint sourceState;
  // alGetSourcei(source->source, AL_SOURCE_STATE, &sourceState);
  // return sourceState == AL_PLAYING;
}



int cmpfunc_priority (const void * a, const void * b) {
   // const a_VirtualSource** a_data = (const a_VirtualSource**)a;
   // const a_VirtualSource** b_data = (const a_VirtualSource**)b;
  const a_VirtualSource* const *a_data = (const a_VirtualSource* const *)a;
  const a_VirtualSource* const *b_data = (const a_VirtualSource* const *)b;

   if ((*b_data)->priority == (*a_data)->priority)
   {
    float bdist =  (*b_data)->dist_squared_listener;
    float adist =  (*a_data)->dist_squared_listener;
     return adist > bdist ? 1 : -1;
   }
   else
   {
     return ( (*b_data)->priority - (*a_data)->priority );
   }
}

void a_setAudioFileProperties(int file_id, int max_sources,float radius,int within_radius)
{
  max_sources_table[file_id] = max_sources;
  radius_table[file_id] = radius;
  within_radius_table[file_id] = within_radius;
}


void a_stopAllSources()
{

  #ifdef DISABLE_AUDIO
  return;
#endif

  int err = alGetError();
  if(err != AL_NO_ERROR)
    printf("Error pre stopall sources %i\n",err);
  //stop playing all sources
  for (int i = virtualMix_count - 1; i >= 0; i--)
  {
    a_stopVS(virtualMix[i]);
  }
  a_VirtualMix(1.0);

  for (int i = 0 ; i < nummono + numstereo;i++)
  {
    alSourceStop(physical_sources[i]);
    alSourcei(physical_sources[i], AL_BUFFER, 0);

  }

  for (int i = 0 ; i < 2;i++)
  {
    music_sources[i].physical_source = NULL;
  }

  err = alGetError();
  if(err != AL_NO_ERROR)
    printf("Error post stopall sources %i\n",err);
}

void a_audioClearFiles()
{
  #ifdef DISABLE_AUDIO
  return;
  #endif
  printf("AL_INVALID_NAME %i\n",AL_INVALID_NAME);
  printf("AL_INVALID_ENUM %i\n",AL_INVALID_ENUM);
  printf("AL_INVALID_VALUE %i\n",AL_INVALID_VALUE);
  printf("AL_INVALID_OPERATION %i\n",AL_INVALID_OPERATION);
  printf("AL_OUT_OF_MEMORY %i\n",AL_OUT_OF_MEMORY);

  int err = alGetError();
  if(err != AL_NO_ERROR)
    printf("Error pre clearing sources %i\n",err);

  //stop playing all sources
  for (int i = virtualMix_count - 1; i >= 0; i--)
  {
    a_stopVS(virtualMix[i]);
  }
  a_VirtualMix(1.0);
  err = alGetError();
  if(err != AL_NO_ERROR)
    printf("Error pre stopping sources %i\n",err);

  for (int i = 0 ; i < nummono + numstereo;i++)
  {
    alSourceStop(physical_sources[i]);
    alSourcei(physical_sources[i], AL_BUFFER, 0);

  }
  err = alGetError();
  if(err != AL_NO_ERROR)
    printf("Error stopping sources %i\n",err);


  if (files != NULL)
  {
    for (int i = 0 ; i < filecount;i++)
    {
      a_deleteFile(&files[i]);
    }
    free(files);
    files = NULL;
    filecount = 0;
  }

  free(music_files);
  music_files = NULL;
  music_filecount = 0;

  for (int i = 0 ; i < th_music_max_sources;i++)
  {
    music_sources[i].playing = false;
    music_sources[i].physical_source = NULL;
    music_sources[i].file_index = 0;
    music_sources[i].journal_count = 0;
    music_sources[i].memory_location = i;
    music_sources[i].bumped_off = false;
    music_sources[i].physical_mix_id = 0;
    music_sources[i].run_cleanup = false;
    music_sources[i].gain_music = 1.0;
    music_sources[i].rolloff = 0.0;
  }

  err = alGetError();
  if(err != AL_NO_ERROR)
    printf("Error deleteing sources %i\n",err);

}

void a_UpdateGainPitch()
{
  for (int i = 0; i <physicalMix_count;i++)
  {
    if (physicalMix[i]->physical_source != NULL)
    {
      float local_gain = 1.0;
      bool ducked = true;
      for (int j = 0; j < ducking_count;j++ )
      {
        if (physicalMix[i] == duckers[j].source)
        {
          ducked = false;
          break;
        }
        else
        {
          float gain_i = (duckers[j].duck_time - th_time())/duckers[j].duck_duration;
          local_gain = local_gain*fn_clamp(1.0 - gain_i,duckers[j].duck_fraction,1.0);
        }
      }

      if (!ducked)
      {
        local_gain = 1.0;
      }
      if (globalPitch > 0.01)
      {
        alSourcef (*physicalMix[i]->physical_source, AL_PITCH,    physicalMix[i]->pitch*globalPitch    );
        alSourcef (*physicalMix[i]->physical_source, AL_GAIN,    physicalMix[i]->gain*master_gain*local_gain*internal_master_gain     );
      }
      else
      {
        alSourcef (*physicalMix[i]->physical_source, AL_PITCH,    1.0   );
        alSourcef (*physicalMix[i]->physical_source, AL_GAIN,    0.0    );
      }

    }

    #ifdef AUDIO_ERRCHECK
    doALerrcheck(ERROR_MSG("Audio Error"));
    #endif

  }
}

//Virtual Mixing
void a_VirtualMix(float dt)
{
  #ifdef DISABLE_AUDIO
  return;
  #endif

  for (int i = physicalMix_count - 1; i >= 0; i--) {

    if (physicalMix[i]->physical_mix_id != i)
    {
      printf("%s\n", "PMIX ID MISMATCH");
    }
    //physicalMix[i]->physical_source != NULL &&
    if (!physicalMix[i]->playing || ( physicalMix[i]->playing && !physicalMix[i]->loop && !physicalIsPlaying(*physicalMix[i]->physical_source)))
    {

      if (physicalMix[i]->playing && (!physicalMix[i]->loop && !physicalIsPlaying(*physicalMix[i]->physical_source)))
      {

        alSourceStop(*physicalMix[i]->physical_source);
        th_queueAddTop(&physical_queue,physicalMix[i]->physical_id);
        // th_queueAddTop(&virtual_queue,physicalMix[i]->vid);
        physicalMix[i]->playing = false;
      }
      physicalMix[i]->playing = false;

      physicalMix[i]->physical_source = NULL;


      if (i == physicalMix_count - 1)
      {
        physicalMix_count--;
      }
      else
      {
        physicalMix[physicalMix_count - 1]->physical_mix_id = i;
        physicalMix[i] = physicalMix[physicalMix_count - 1];
        physicalMix_count--;
      }
    }
  }

  for (int i = virtualMix_count - 1; i >= 0; i--) {
    if (virtualMix[i]->relative_to_listener)
    {
      virtualMix[i]->dist_squared_listener = fn_distance2(fn_createVec3v(virtualMix[i]->SourcePos),fn_createVec3(0,0,0));
    }
    else
    {
      virtualMix[i]->dist_squared_listener = fn_distance2(fn_createVec3v(virtualMix[i]->SourcePos),playerPos);
    }

    if (!virtualMix[i]->playing || (virtualMix[i]->bumped_off && virtualMix[i]->playing && !virtualMix[i]->loop && virtualMix[i]->offset >= files[virtualMix[i]->file_index].length_in_seconds - dt*0.001) )
    {
      if (virtualMix[i]->run_cleanup)
      {
        virtualMix[i]->cleanup.callback(virtualMix[i]->cleanup.data);
      }
      virtualMix[i]->playing = false;
      th_queueAddTop(&virtual_queue,virtualMix[i]->vid);
      if (i == virtualMix_count - 1)
      {
        virtualMix_count--;
      }
      else
      {
        virtualMix[i] = virtualMix[virtualMix_count - 1];
        virtualMix_count--;
      }


    }
  }

  if (virtualMix_count == 0)
  {
    return;
  }

  qsort(virtualMix,virtualMix_count,sizeof(a_VirtualSource*),cmpfunc_priority);


  for (int i = 0; i < filecount; i++) {
    // max_sources_table[i] = 10;
    file_instance_count[i] = 0;
    // radius_table[i] = 200;
    // within_radius_table[i] = 3;
  }


  int allocated_count = 0;



  for (int i = 0; i < virtualMix_count; i++) {
    if (virtualMix[i]->playing)
    {
        file_instance_count[virtualMix[i]->file_index] = file_instance_count[virtualMix[i]->file_index] + 1;
    }

    bool unplayable = false;
    if (file_instance_count[virtualMix[i]->file_index] > max_sources_table[virtualMix[i]->file_index])
    {
      //cant play this source
      unplayable = true;

    }


    if (!unplayable && allocated_count < physical_source_count)
    {
      //check for already playing sources that might be close
      int to_check_count = i;
      // if (to_check_count >= physical_source_count)
      // {
      //   to_check_count = physical_source_count;
      // }
      int hits = 0;
      for (int j = 0; j < to_check_count; j++) {
        if (j != i && virtualMix[i]->file_index == virtualMix[j]->file_index && virtualMix[j]->playing && virtualMix[j]->physical_source != NULL)
        {
          float dist = fn_distance2(fn_createVec3v(virtualMix[i]->SourcePos),fn_createVec3v(virtualMix[j]->SourcePos));
          if (dist < radius_table[virtualMix[j]->file_index])
          {
            hits++;
            if (hits > within_radius_table[virtualMix[i]->file_index])
            {
              file_instance_count[virtualMix[i]->file_index] = file_instance_count[virtualMix[i]->file_index] - 1;
              unplayable = true;
            }
          }
        }
      }
    }

    if (unplayable && virtualMix[i]->physical_source != NULL)
    {
      //remove from physical mix
      int pid = virtualMix[i]->physical_mix_id;

      alGetSourcef(*physicalMix[pid]->physical_source, AL_SEC_OFFSET, &physicalMix[pid]->offset);
      alSourceStop(*physicalMix[pid]->physical_source);

      #ifdef AUDIO_ERRCHECK
      doALerrcheck(ERROR_MSG("Audio Error"));
      #endif

      physicalMix[pid]->physical_source = NULL;
      physicalMix[pid]->bumped_off = true;
      th_queueAddTop(&physical_queue,virtualMix[i]->physical_id);

      if (pid == physicalMix_count - 1)
      {
        physicalMix_count--;
      }
      else
      {
        physicalMix[physicalMix_count - 1]->physical_mix_id = pid;
        physicalMix[pid] = physicalMix[physicalMix_count - 1];
        physicalMix_count--;
      }


    }

    if (unplayable)
    {
        virtualMix[i]->bumped_off = true;
    }

    if (unplayable && !virtualMix[i]->loop)
    {
      virtualMix[i]->playing = false;
    }

    if (allocated_count < physical_source_count && !unplayable)
    {
      allocated_count++;
      //physically play the virtual source
      if (virtualMix[i]->physical_source == NULL)
      {
        int pid = 0;
        bool success = th_queueRemoveBottom(&physical_queue,&pid);

        if (success)
        {
          ALuint* phys = &physical_sources[pid];
          virtualMix[i]->physical_source = phys;
          linkSource(virtualMix[i],files[virtualMix[i]->file_index].buffer,false);
          virtualMix[i]->physical_source = phys;
          virtualMix[i]->physical_id = pid;

          a_setVSRelativeToListener(virtualMix[i],virtualMix[i]->relative_to_listener);
          a_setVSPos(virtualMix[i],fn_createVec3v(virtualMix[i]->SourcePos));
          a_setVSVel(virtualMix[i],fn_createVec3v(virtualMix[i]->SourceVel));
          a_setVSLoop(virtualMix[i],virtualMix[i]->loop);
          a_setVSGain(virtualMix[i],virtualMix[i]->gain);
          a_playVS(virtualMix[i]);
          //a_setVSOffset(virtualMix[i],virtualMix[i]->offset);
          a_setVSOffset(virtualMix[i],fmod(virtualMix[i]->offset,files[virtualMix[i]->file_index].length_in_seconds));

          virtualMix[i]->physical_mix_id = physicalMix_count;
          physicalMix[physicalMix_count] = virtualMix[i];
          physicalMix_count++;
        }
        else
        {
          int lowest_priority_id = -1;
          int lowest_priority = 100000;
          for (int j = 0; j < physicalMix_count; j++) {
            if (physicalMix[j]->priority < lowest_priority)
            {
              lowest_priority_id = j;
              lowest_priority = physicalMix[j]->priority;
            }
          }

          if (lowest_priority_id != -1 && lowest_priority < virtualMix[i]->priority)
          {

            //alienate old
            pid = physicalMix[lowest_priority_id]->physical_id;

            alGetSourcef(*physicalMix[lowest_priority_id]->physical_source, AL_SEC_OFFSET, &physicalMix[lowest_priority_id]->offset);
            alSourceStop(*physicalMix[lowest_priority_id]->physical_source);
            physicalMix[lowest_priority_id]->physical_source = NULL;
            physicalMix[lowest_priority_id]->bumped_off = true;

            //play new
            ALuint* phys = &physical_sources[pid];
            virtualMix[i]->physical_source = phys;
            linkSource(virtualMix[i],files[virtualMix[i]->file_index].buffer,false);
            virtualMix[i]->physical_source = phys;
            virtualMix[i]->physical_id = pid;
            a_setVSRelativeToListener(virtualMix[i],virtualMix[i]->relative_to_listener);
            a_setVSPos(virtualMix[i],fn_createVec3v(virtualMix[i]->SourcePos));
            a_setVSVel(virtualMix[i],fn_createVec3v(virtualMix[i]->SourceVel));
            a_setVSLoop(virtualMix[i],virtualMix[i]->loop);
            a_setVSGain(virtualMix[i],virtualMix[i]->gain);
            a_playVS(virtualMix[i]);
            a_setVSOffset(virtualMix[i],0);

            virtualMix[i]->physical_mix_id = lowest_priority_id;
            physicalMix[lowest_priority_id] = virtualMix[i];
          }
        }

      }

      if ( virtualMix[i]->physical_source != NULL && virtualMix[i]->bumped_off)
      {

        virtualMix[i]->bumped_off = false;
        a_VirtualSource*s = virtualMix[i];
        a_setVSRelativeToListener(s,s->relative_to_listener);
        a_setVSPos(s,fn_createVec3v(s->SourcePos));
        a_setVSVel(s,fn_createVec3v(s->SourceVel));
        a_setVSLoop(s,s->loop);
        a_setVSGain(s,s->gain);
        a_playVS(s);
        a_setVSOffset(s,fmod(s->offset,files[virtualMix[i]->file_index].length_in_seconds));
      }
    }
    else
    {
      //do virtual playing
      virtualMix[i]->offset += dt*0.001;
    }

  }


  for (int i = 0; i < ducking_count; )
  {
    if (duckers[i].duck_time < th_time())
    {
      duckers[i] = duckers[--ducking_count];
    }
    else
    {
      i++;
    }
  }

  a_UpdateGainPitch();



  // for (size_t i = 0; i < physicalMix_count; i++) {
  //   printf("%i ",physicalMix[i]->file_index );
  // }
  //  printf("\n");
}

void a_duckVS(a_VirtualSource* source,float duck_duration,float duck_fraction)
{
  if (ducking_count < TH_MAX_AUDIO_DUCK)
  {
    duckers[ducking_count].source = source;
    if (duck_duration != 0.0)
    {
      duckers[ducking_count].duck_time = th_time() + duck_duration;
      duckers[ducking_count].duck_duration = duck_duration;
    }
    else
    {
      duckers[ducking_count].duck_time = th_time() + files[source->file_index].length_in_seconds*1000.0;
      duckers[ducking_count].duck_duration = files[source->file_index].length_in_seconds*1000.0;
    }

    if (duck_fraction == 0.0)
    {
      duckers[ducking_count].duck_fraction = 0.5;
    }
    else
    {
      duckers[ducking_count].duck_fraction = duck_fraction;
    }

    ducking_count++;
  }
}
