#pragma once
#include "th_gpu.h"
#include "th_time.h"
#include "th_light.h"
#include "th_decal.h"
#include "th_level.h"
#include "th_allocator.h"
#define MAX_PARTICLES_C 256
typedef struct
{

    unsigned int a;
    unsigned int b;
    float tween_value;

    bool th_play_mats_flag;
    bool th_disable_frustum_culling;

    bool th_capture_mats_flag;
    unsigned int th_mat_capture_maxframe;
    fn_mat4** th_mat_capture;
    th_timer_t* th_mat_capture_time;
    int** th_mat_capture_count;

    fn_mat4** th_particle_mat_capture;
    uint64_t** th_particle_ids_capture;
    int* th_particle_mat_capture_count;

    th_Decal** th_decal_capture;
    int* th_decal_capture_count;

    unsigned int th_frame_captured;
    unsigned int th_mat_per_frame;
    unsigned int th_rendercommands_capture;
    th_timer_t playback_old_time;
    unsigned int th_playback_old_frame;

    fn_mat4** th_anim_mat_capture;
    int** th_anim_mat_capture_count;
    unsigned int th_anim_mat_per_frame;
    unsigned int th_anim_rendercommands_capture;

    float** th_anim_time_capture;
    uint32_t** th_anim_framenum_capture;
    uint32_t** th_anim_nextframe_capture;
    char** th_anim_is_ragdoll;
    fn_vec3** th_anim_ragdoll_velcenter;
    fn_vec3** th_anim_ragdoll_vel;
    unsigned int th_anim_captured_model_instances;

    th_PointLight** th_pointlight_capture;
    int* th_pointlight_capture_count;

    unsigned int frame_idx;
    th_timer_t frame_last_time;
    bool capture_this_frame;
    bool needs_cleanup;

    th_Allocator* alloc;

}th_ReplayData;
#define DEFAULT_REPLAY_STATE (th_ReplayData){0,1,0,false,false,false,18000,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,0,0,0,-1.0,0,NULL,NULL,0,0,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,NULL,0,0,false,false,NULL}

void th_getMatsPerFrame(th_ReplayData* rp,th_LevelDescriptor* level);

void th_tickReplayFrameCapture(th_ReplayData* rp);

void th_startCapture(th_ReplayData* rp);
void th_saveCapture(th_ReplayData* rp,const char* filename,bool compression);
void th_loadCapture(th_ReplayData* rp,const char* filename,bool compression);

void th_captureLights(th_ReplayData* rp,th_LevelDescriptor* level);
void th_performCapture(th_ReplayData* rp,th_LevelDescriptor* level);
bool th_captureParticles(th_ReplayData* rp,r_DrawElementsIndirectCommand* drawcommands_particle,th_GpuDataOffsets final_particle,th_ArrayObject* particle_data,int particle_commands_count,fn_vec3 pos,fn_vec3 look_global );// returns true if not replay
void th_updateCapturePlayback(th_ReplayData* rp);
void th_captureModels(th_ReplayData* rp,th_LevelDescriptor* level);
bool th_playbackModels(th_ReplayData* rp,th_LevelDescriptor* level,float delta_time,th_World* world);

void th_cleanupReplay(th_ReplayData* rp);
