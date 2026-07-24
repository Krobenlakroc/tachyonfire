#pragma once

#include "../fn_engine/th_time.h"
#include "../fn_engine/th_decal.h"
#include "../fn_engine/th_particle.h"
#include "../fn_engine/th_audio.h"
#include "../fn_engine/th_threads.h"
#include "th_player.h"

/*
 * victory conditions
 */

typedef struct
{
    bool victory;
    th_timer_t airtime;//express as a fraction
    int damage_taken; //reported as damage, displayed as health?
    th_timer_t completiontime;
}th_VictoryStats;

#define TH_DEFAULT_VICTORY_STATS (th_VictoryStats){.victory = true,.airtime = 0.3,.damage_taken = 0,.completiontime = 60000}

void th_resetGameplay();

void th_incrementViolence();

float th_getViolenceLevel();

void th_markEnemyBirth(int count);
void th_markEnemyDeath(int count);

int th_getEnemyCount();

void th_incrementAirtime(float dt);

void th_incrementCompletionTime(float dt);

//handle damage as part of the player object, dont track 2 damage counters ugh
th_VictoryStats th_checkVictory(th_PlayerObject* player,th_timer_t leveltime,float dt);

//damage cleartime airtime
fn_vec3 th_computeVictoryFloats(th_VictoryStats stats,th_VictoryStats bronze,th_VictoryStats silver,th_VictoryStats gold);
fn_vec3 th_computeVictoryInterps(fn_vec3 floats,float bronze,float silver, float gold);

void th_spawnBuiltinParticlesAndDecals();

void th_spawnBloodSpurt(fn_vec3 collision_normal,fn_vec3 collision_position,int thread_id);

void th_spawnSparks(fn_vec3 collision_normal,fn_vec3 collision_position,int thread_id,th_LightQuery* lightq);

void th_spawnSparksFewLight(fn_vec3 collision_normal,fn_vec3 collision_position,int thread_id,th_LightQuery* lightq,int modulus);


//NOTE!!!!! tracers are spawned near the end, so they are the "first among losers" in the particle pecking order, compared to sparks and other long lived particles
//they are also respawned every frame, like lightning so they will almost always dominate other particles
void th_spawnTracer(fn_vec3 pos,fn_vec3 target,int thread_id);

void th_spawnLighting(fn_vec3 pos,fn_vec3 target,int thread_id,th_LightQuery* lightq,int point_count,int* randstate,float size_particle,float amplitude);

void th_spawnLaserBeam(fn_vec3 pos,fn_vec3 target,int thread_id,th_LightQuery* lightq,int point_count,int* randstate,float size_particle,float amplitude);

void th_spawnBlood(fn_vec3 pos,int thread_id);

void th_spawnBloodNoSound(fn_vec3 pos,int thread_id);

void th_spawnMetalNoSound(fn_vec3 pos,int thread_id);

void th_spawnSmokePuffs(fn_vec3 pos,int thread_id);

void th_spawnSmokeRocket(fn_vec3 pos,int thread_id);

void th_spawnExplosion(fn_vec3 pos,int thread_id);

void th_spawnImpactRing(fn_vec3 pos,int thread_id);

void th_initializeBuiltinMemory();

void th_setGameplayTimeScale(fn_vec3 scale);

fn_vec3 th_getGameplayTimeScale();

fn_vec3 th_sampleRandomSphere();

void th_setGameGlow(fn_vec3 color,float amount);

void th_getGameGlow(fn_vec3* color,float* amount);

void th_updateGameBuiltins(float dt);

void th_playSoundTerminated(a_VirtualSource** s,th_Entity* e,int sound_index,float gain);

void th_playSoundIfNotPlaying(a_VirtualSource** s,fn_vec3 position,int sound_index,float gain);

void th_updateSound(a_VirtualSource** s,th_Entity* e);

void th_updateSoundPosVel(a_VirtualSource** s,fn_vec3 pos,fn_vec3 vel);

fn_mat4 th_fadeoutMatrix(float* value,float decay);

fn_mat4 th_fadeinMatrix(float* value,float decay);

fn_quat th_update_orient(fn_vec3* from_vec,fn_vec3 target_vec,float ang_spd_deg,float dt,fn_vec3* old_ups);

void th_enactExplosion(fn_vec3 position,th_PlayerObject* playerstate,float dt,th_World* world,th_Entity* player,int thread_id,th_LightQuery* lq,th_Entity* dmg_entity,th_ImpactBuffered** impact_list_2d,int* impact_counts_2d);
