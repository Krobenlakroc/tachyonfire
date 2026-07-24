#include  "th_builtins.h"
#include  "th_splineutils.h"
#include "../fn_math/fn_grid.h"
#include "../fn_engine/th_system.h"
#include "../fn_engine/th_globals.h"

#define NUM_PARTICLE_CACHE 1024

static th_Particle** particles_list2d;//[th_getNumThreads()][50];
static int* counts2d;//[th_getNumThreads()];


static th_DecalOrientation** decals_list2d;//[th_getNumThreads()][50];
static int* counts2d2;//[th_getNumThreads()];
static fn_vec3 sampleRandomSphere()
{
  float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
  float z =  (float)th_random()/(float)(RAND_MAX/2.0);
  z -= 1;
  float x = cos(theta);
  float y = sin(theta);
  fn_vec3 n = fn_normalizeVec3(fn_createVec3(x,y,z));
  return n;
}


static th_timer_t completiontimer = 0;
static th_timer_t airtimer = 0;
static int num_enemies = 0;
static bool first_victory_check = true;
static bool unwinable = false;


static float violence_p = 0.0;
static float violence_v = 0.0;
static float violence_inst = 0.0;

static pthread_mutex_t violence_mutex;

void th_resetGameplay()
{
  violence_p = 0.0;
  violence_v = 0.0;
  violence_inst = 0.0;
  airtimer = 0;
  num_enemies = 0;
  completiontimer = 0;
  first_victory_check = true;
  unwinable = false;
}

float th_getViolenceLevel()
{
  return violence_p;
}

void th_markEnemyBirth(int count)
{
  num_enemies += count;
}

void th_markEnemyDeath(int count)
{
  num_enemies -= count;
}

int th_getEnemyCount()
{
  return num_enemies;
}

void th_incrementAirtime(float dt)
{
  airtimer = airtimer + dt;
}

void th_incrementCompletionTime(float dt)
{
  completiontimer = completiontimer + dt;
}

//handle damage as part of the player object, dont track 2 damage counters ugh
th_VictoryStats th_checkVictory(th_PlayerObject* player,th_timer_t leveltime,float dt)
{
  float old_violence_p = violence_p;

  float flat_a_violence = 12.0;
  float violence_a = (violence_inst)*4.0*flat_a_violence - 0.25*dt;

 // violence_a = fn_clamp(violence_a,-0.1,1.5);
  violence_v = violence_v + violence_a*0.001;
  violence_p = violence_p + violence_v*dt*0.001;

  violence_v = fn_clamp(violence_v,-0.035,0.5);
  violence_p = fn_clamp(violence_p,0.0,0.74);

  violence_inst = 0.0;

  //set music volume based off of violence
  //violence_p
  if (violence_p > 0.0 && old_violence_p <= 0.0)
  {
    a_VirtualSource* track = a_getMusicTrack(0);
    a_playVSNoChange(track);
    a_setGainMusicTrack(0.0,0);
  }
  else if (violence_p <= 0.0 && old_violence_p > 0.0)
  {
    a_VirtualSource* track = a_getMusicTrack(0);
    a_setGainMusicTrack(0.0,0);
    a_pauseVS(track);
  }
  else
  {
    float gain_remap = fn_remap(violence_p,0.45,0.67,0,1.0);
    a_setGainMusicTrack(gain_remap,0);
  }

  //th_printlnDevConsole("%f",airtimer/leveltime);
  if (first_victory_check)
  {
    first_victory_check = false;
    if (num_enemies == 0)
    {
      unwinable = true;
    }
  }
  th_VictoryStats ret;
  ret.victory = player->hp > 0 && num_enemies == 0 && !unwinable;
  ret.damage_taken = player->hp;
  ret.completiontime = completiontimer;
  ret.airtime = airtimer/leveltime;
  return ret;
}

void th_incrementViolence()
{
  pthread_mutex_lock(&violence_mutex);
  violence_inst = violence_inst + 1;
  pthread_mutex_unlock(&violence_mutex);
}

fn_vec3 th_computeVictoryFloats(th_VictoryStats stats,th_VictoryStats bronze,th_VictoryStats silver,th_VictoryStats gold)
{
  float damage_flt = 0.0;
  float clear_flt = 0.0;
  float air_flt = 0.0;

  if (stats.damage_taken >= gold.damage_taken)
  {
    damage_flt = 3.0;
  }
  else if (stats.damage_taken >= silver.damage_taken)
  {
    damage_flt = fn_lerp(2.0,3.0,fn_clamp((stats.damage_taken - silver.damage_taken)/fn_max(0.0001,gold.damage_taken - silver.damage_taken),0.0,1.0 ) );
  }
  else if (stats.damage_taken >= bronze.damage_taken)
  {
    damage_flt = fn_lerp(1.0,2.0,fn_clamp((stats.damage_taken - bronze.damage_taken)/fn_max(0.0001,silver.damage_taken - bronze.damage_taken),0.0,1.0 ) );
  }
  else if (stats.damage_taken >= 0)
  {
    damage_flt = fn_lerp(0.0,1.0,fn_clamp((stats.damage_taken)/fn_max(0.0001,bronze.damage_taken),0.0,1.0 ) );
  }
  else
  {
    damage_flt = 0;
  }

  if (stats.completiontime <= gold.completiontime)
  {
    clear_flt = 3.0;
  }
  else if (stats.completiontime <= silver.completiontime)
  {
    clear_flt = fn_lerp(2.0,3.0,fn_clamp((silver.completiontime - stats.completiontime)/fn_max(0.0001,silver.completiontime - gold.completiontime ),0.0,1.0 ) );
  }
  else if (stats.completiontime <= bronze.completiontime)
  {
    clear_flt = fn_lerp(1.0,2.0,fn_clamp((bronze.completiontime - stats.completiontime)/fn_max(0.0001,bronze.completiontime - silver.completiontime ),0.0,1.0 ) );
  }
  else if (stats.completiontime >= 0)
  {
    clear_flt = fn_lerp(0.0,1.0,fn_clamp((bronze.completiontime*3.0 - stats.completiontime)/fn_max(0.0001,bronze.completiontime*3.0 - bronze.completiontime ),0.0,1.0 ) );
  }
  else
  {
    clear_flt = 0;
  }

  if (stats.airtime >= gold.airtime)
  {
    air_flt = 3.0;
  }
  else if (stats.airtime >= silver.airtime)
  {
    air_flt = fn_lerp(2.0,3.0,fn_clamp((stats.airtime - silver.airtime)/fn_max(0.0001,gold.airtime - silver.airtime),0.0,1.0 ) );
  }
  else if (stats.airtime >= bronze.airtime)
  {
    air_flt = fn_lerp(1.0,2.0,fn_clamp((stats.airtime - bronze.airtime)/fn_max(0.0001,silver.airtime - bronze.airtime),0.0,1.0 ) );
  }
  else if (stats.airtime >= 0)
  {
    air_flt = fn_lerp(0.0,1.0,fn_clamp((stats.airtime)/fn_max(0.0001,bronze.airtime),0.0,1.0 ) );
  }
  else
  {
    air_flt = 0;
  }

  return fn_createVec3(damage_flt,clear_flt,air_flt);

}

fn_vec3 th_computeVictoryInterps(fn_vec3 floats,float bronze,float silver, float gold)
{
  fn_vec3 output = fn_createVec3s(0);

  for (int i = 0 ; i < 3;i++)
  {
    if (floats.v[i] == 3.0)
    {
      output.v[i] = 1.0;
    }
    else if (floats.v[i] > 2.0)
    {
      output.v[i] = fn_remap(floats.v[i],2.0,3.0,silver,gold);
    }
    else if (floats.v[i] > 1.0)
    {
      output.v[i] = fn_remap(floats.v[i],1.0,2.0,bronze,silver);
    }
    else if (floats.x > 0.0)
    {
      output.v[i] = fn_remap(floats.v[i],0.0,1.0,0.0,bronze);
    }
  }

  return output;


}

fn_vec3 th_sampleRandomSphere()
{
  float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
  float z =  (float)th_random()/(float)(RAND_MAX/2.0);
  z -= 1;
  float x = cos(theta);
  float y = sin(theta);
  fn_vec3 n = fn_normalizeVec3(fn_createVec3(x,y,z));
  return n;
}

void th_initializeBuiltinMemory()
{
  particles_list2d = malloc(sizeof(th_Particle*)*th_getNumThreads());
  for (size_t i = 0; i < (size_t)th_getNumThreads(); i++) {
    particles_list2d[i] = malloc(sizeof(th_Particle)*NUM_PARTICLE_CACHE);
  }

  decals_list2d = malloc(sizeof(th_DecalOrientation*)*th_getNumThreads());
  for (size_t i = 0; i < (size_t)th_getNumThreads(); i++) {
    decals_list2d[i] = malloc(sizeof(th_DecalOrientation)*NUM_PARTICLE_CACHE);
  }

  counts2d = malloc(sizeof(int)*th_getNumThreads());
  counts2d2 = malloc(sizeof(int)*th_getNumThreads());

  memset(counts2d,0,sizeof(int)*th_getNumThreads());
  memset(counts2d2,0,sizeof(int)*th_getNumThreads());

    pthread_mutex_init(&violence_mutex, NULL);
}


void th_spawnMetalNoSound(fn_vec3 pos,int thread_id)
{
  th_Particle p = th_defaultParticle();
  //(float)th_random()/(float)(RAND_MAX/(2*3.14159));
  // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));

  p.alive = true;
  p.life = 1700;
  p.start_time = th_time();
  p.stretch = 1.5;
  p.scale = 20;
  p.overbright = 1;
  p.texture_handle = th_getParticleTexture(TH_METALSPLASH);
  p.makedecal = false;
  p.hasphysics = false;
  p.alphascale = 1.0;
  p.alphascale_base = 1.0;
  p.has_gravity = true;
  p.fade_out = false;
  p.fade_out_sharp = true;
  // th_DecalOrientation blood_decal;
  // blood_decal.scale = fn_createVec3(75*2,75*2,50*2);
  // blood_decal.material_handle = fn_createVec2(th_getParticleTexture(TH_BLOOD_DECAL).x,0);//14
  // p.decal = blood_decal;
  p.use_reflections = 1.0;

  for (int k = 0 ; k < 32;k++)
  {
    float theta = th_randomFloat(0.0,2.0*3.14159);
    p.theta = theta;
    p.velocity = fn_multVec3(fn_createVec3(0,-1,0),fn_createVec3s(0.7));
    p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.4));
    p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(0.7));
    p.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(p.velocity),45));
    if (thread_id == -1)
    {
      th_addParticle(p);
    }
    else
    {
      if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
      {
        particles_list2d[thread_id][counts2d[thread_id]] = p;
        counts2d[thread_id] = counts2d[thread_id] + 1;
      }
      else
      {
        printf("%s\n","Particle Cache Saturation" );
      }
    }



  }
}

void th_spawnBloodNoSound(fn_vec3 pos,int thread_id)
{


  th_Particle p = th_defaultParticle();

 p.alive = true;
 p.life = 10000;
 p.start_time = th_time();
 p.stretch = 4;
 p.scale = 15;
 p.overbright = 1;
 p.texture_handle = th_getParticleTexture(TH_BLOOD);
 p.makedecal = true;
 p.hasphysics = true;
 p.alphascale = 0.75;
 p.alphascale_base = 0.75;
 p.has_gravity = true;
 th_DecalOrientation blood_decal;
 blood_decal.scale = fn_createVec3(75*2,75*2,50*2);
 blood_decal.material_handle = fn_createVec2(th_getParticleTexture(TH_BLOOD_DECAL).x,0);//14
 p.decal = blood_decal;

 for (int k = 0 ; k < 4;k++)
 {
   float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
   // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
   p.theta = theta;

   p.velocity = fn_multVec3(fn_createVec3(0,-1,0),fn_createVec3s(0.7));
   p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.4));
   p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(0.7));
   p.position = fn_addVec3(pos,fn_multVec3s(fn_normalizeVec3(p.velocity),45));
   if (thread_id == -1)
   {
     th_addParticle(p);
   }
   else
   {
     if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
     {
       particles_list2d[thread_id][counts2d[thread_id]] = p;
       counts2d[thread_id] = counts2d[thread_id] + 1;
     }
     else
     {
       printf("%s\n","Particle Cache Saturation" );
     }
   }



 }
}

void th_spawnBlood(fn_vec3 pos,int thread_id)
{

  {
    int offset = th_frame() % 3;

    a_VirtualSource* s = a_playVirtualSource(sound_impact1 + offset,-1,pos,NULL );
    a_setVSLoop(s,false);
    a_setVSPos(s,pos);
    a_setVSVel(s,fn_createVec3s(0));
    a_setVSGain(s,0.5);
    a_setVSPitch(s,th_randomFloat(0.85,1.15));
  }
   th_spawnBloodNoSound( pos, thread_id);

}

void th_spawnExplosion(fn_vec3 pos,int thread_id)
{
  th_Particle p = th_defaultParticle();
 float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
 // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
 p.theta = theta;
 p.alive = true;
 p.life = 750;
 p.start_time = th_time();
 p.stretch = 0;
 p.scale = 45;
 p.scale_end = 57;
 p.overbright = 1.01;
 p.texture_handle = th_getParticleTexture(TH_EXPLOSION);
 p.makedecal = false;
 p.alphascale = 1.0;
 p.hasphysics = false;
 p.has_gravity = false;
 p.animated = true;

 p.frame_id = 0 ;
 p.last_frame_start = th_time();
 p.time_per_frame = 100;
 //p.offset_per_frame = 3;
 p.num_frames = 8;
 p.blend = 0;
 p.use_add_blending = 1.0;
 // th_DecalOrientation blood_decal;
 // blood_decal.scale = fn_createVec3(75,75,50);
 // blood_decal.material_handle = fn_createVec2(0,30);//14
 // p.decal = blood_decal;
 //
 for (int k = 0 ; k < 1;k++)
 {
   p.velocity = fn_createVec3s(0);
   p.position = pos;
   if (thread_id == -1)
   {
     th_addParticle(p);
   }
   else
   {
     if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
     {
       particles_list2d[thread_id][counts2d[thread_id]] = p;
       counts2d[thread_id] = counts2d[thread_id] + 1;
     }
   }



 }
}

void th_spawnImpactRing(fn_vec3 pos,int thread_id)
{
  th_Particle p = th_defaultParticle();
 float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
 // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
 p.theta = theta;
 p.alive = true;
 p.life = 800.0;
 p.start_time = th_time();
 p.stretch = 0;
 p.scale = 70;
 //p.overbright = 1.01;
 p.texture_handle = th_getParticleTexture(TH_IMPACTRING);
 p.makedecal = false;
 p.alphascale = 1.0;
 p.alphascale_base = 4.0;
 p.hasphysics = false;
 p.has_gravity = false;
 p.animated = true;


 p.frame_id = 0 ;
 p.last_frame_start = th_time();
 p.time_per_frame = 250;
 // p.offset_per_frame = 3;
 p.num_frames = 4;
 p.blend = 0;
 p.use_geom_normal = 0.0;
 // th_DecalOrientation blood_decal;
 // blood_decal.scale = fn_createVec3(75,75,50);
 // blood_decal.material_handle = fn_createVec2(0,30);//14
 // p.decal = blood_decal;
 //
 for (int k = 0 ; k < 1;k++)
 {
   p.velocity = fn_createVec3s(0);
   p.position = pos;
   if (thread_id == -1)
   {
     th_addParticle(p);
   }
   else
   {
     if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
     {
       particles_list2d[thread_id][counts2d[thread_id]] = p;
       counts2d[thread_id] = counts2d[thread_id] + 1;
     }
   }



 }
}

void th_spawnSmokeRocket(fn_vec3 pos,int thread_id)
{
  th_Particle p = th_defaultParticle();
 float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
 // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
 p.theta = theta;
 p.alive = true;
 p.life = 500;
 p.start_time = th_time();
 p.stretch = 0;
 p.scale = 25;
 //p.overbright = 1.01;
 p.texture_handle = th_getParticleTexture(TH_SMOKEB);
 p.makedecal = false;
 p.alphascale = 0.6;
 p.alphascale_base = 0.6;
 p.hasphysics = false;
 p.has_gravity = false;
 p.use_geom_normal = 0.0;
 // th_DecalOrientation blood_decal;
 // blood_decal.scale = fn_createVec3(75,75,50);
 // blood_decal.material_handle = fn_createVec2(0,30);//14
 // p.decal = blood_decal;
 //
 for (int k = 0 ; k < 1;k++)
 {
   p.velocity = fn_createVec3s(0);
   p.position = pos;
   if (thread_id == -1)
   {
     th_addParticle(p);
   }
   else
   {
     if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
     {
       particles_list2d[thread_id][counts2d[thread_id]] = p;
       counts2d[thread_id] = counts2d[thread_id] + 1;
     }
   }



 }
}

void th_spawnSmokePuffs(fn_vec3 pos,int thread_id)
{

  // {
  //   a_VirtualSource* s = a_playFile(0 );
  //   a_setVSLoop(s,false);
  //   a_setVSPos(s,pos);
  //   a_setVSVel(s,fn_createVec3s(0));
  //   a_setVSGain(s,60.0);
  // }

  th_Particle p = th_defaultParticle();;
 float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
 // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
 p.theta = theta;
 p.alive = true;
 p.life = 1000;
 p.start_time = th_time();
 p.stretch = 0;
 p.scale = 50;
 p.overbright = 1.0;//1.01;
 p.texture_handle = th_getParticleTexture(TH_SMOKEB);
 p.makedecal = false;
 p.alphascale = 0.25;
 p.alphascale_base = 0.7;
 p.hasphysics = false;
 p.has_gravity = true;
 p.use_geom_normal = 0.0;
 // th_DecalOrientation blood_decal;
 // blood_decal.scale = fn_createVec3(75,75,50);
 // blood_decal.material_handle = fn_createVec2(0,30);//14
 // p.decal = blood_decal;
 //
 for (int k = 0 ; k < 1;k++)
 {
   p.velocity = fn_multVec3(fn_createVec3(0,-1,0),fn_createVec3s(0.4));
   p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.7));
   p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(0.7));
   p.position = pos;
   if (thread_id == -1)
   {
     th_addParticle(p);
   }
   else
   {
     if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
     {
       particles_list2d[thread_id][counts2d[thread_id]] = p;
       counts2d[thread_id] = counts2d[thread_id] + 1;
     }
   }



 }
}

void th_spawnBloodSpurt(fn_vec3 collision_normal,fn_vec3 collision_position,int thread_id)
{
  th_Particle p = th_defaultParticle();;
  float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
  // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
  p.theta = 0;
  p.alive = true;
  p.life = 5000;
  p.start_time = th_time();
  p.stretch = 7;
  p.scale = 4;
  p.overbright = 1;
  p.texture_handle = th_getParticleTexture(TH_BLOOD_SPURT);
  p.makedecal = false;
  p.alphascale = 0.75;
   p.alphascale_base = 0.75;
  p.hasphysics = false;
  p.has_gravity = true;
  // p.haslight = true;
  // p.lightcolor = fn_createVec3(60*10,45*10,0);
  // p.lightq = lightq;

  for (int k = 0 ; k < 4;k++)
  {
    p.velocity = fn_multVec3(collision_normal,fn_createVec3s(0.7));
    p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.3));
    p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(1.5));
    p.position = collision_position;
    if (thread_id == -1)
    {
      th_addParticle(p);
    }
    else
    {
      if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
      {
        particles_list2d[thread_id][counts2d[thread_id]] = p;
        counts2d[thread_id] = counts2d[thread_id] + 1;
      }
    }

  }
}

void th_spawnSparksFewLight(fn_vec3 collision_normal,fn_vec3 collision_position,int thread_id,th_LightQuery* lightq,int modulus)
{
  th_Particle p = th_defaultParticle();
/*
  // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
  p.theta = 0;
  p.alive = true;
  p.life = 500;
  p.start_time = th_time();
  p.stretch = 2;
  p.scale = 1;
  p.overbright = 1.1;
  p.texture_handle = th_getParticleTexture(TH_SPARKS);
  p.makedecal = false;
  p.alphascale = 1.0;
  p.hasphysics = false;
  p.has_gravity = true;*/



  for (int k = 0 ; k < 7;k++)
  {
    p = th_defaultParticle();
    p.theta = 0;
    p.alive = true;
    p.life = 500;
    p.start_time = th_time();
    //SIX SEVEN!
    p.stretch = 6.7;//4.5;
    p.scale = 1.35;
    p.overbright = 1.1;
    p.texture_handle = th_getParticleTexture(TH_SPARKS);
    p.use_premultiplied = 1.0;
    p.makedecal = false;
    p.alphascale = 1.0;
    p.hasphysics = false;
    p.has_gravity = true;
    if (modulus >= 0 && k % modulus == 0)
    {
      p.haslight = true;
      p.lightcolor = fn_createVec3(60*3,55*3,55*3);
      p.lightq = lightq;
      p.tubelight = false;
      p.overbright = 1.0;

      p.use_geom_normal = 0.0;
      p.alphascale = 0.25;
      p.alphascale_base = 0.7;
      //p.lightthresh = 0.00001;
    }


    p.velocity = fn_multVec3(collision_normal,fn_createVec3s(0.7));
    p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.3));
    p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(1.5));
    p.position = collision_position;
    if (thread_id == -1)
    {
      th_addParticle(p);
    }
    else
    {
      if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
      {
        particles_list2d[thread_id][counts2d[thread_id]] = p;
        counts2d[thread_id] = counts2d[thread_id] + 1;
      }
    }

  }
}

void th_spawnSparks(fn_vec3 collision_normal,fn_vec3 collision_position,int thread_id,th_LightQuery* lightq)
{
  th_spawnSparksFewLight( collision_normal, collision_position, thread_id,lightq,3);
}

void th_spawnBuiltinParticlesAndDecals()
{
  for (int i = 0 ; i < th_getNumThreads();i++)
  {
    for (int j = 0 ; j < counts2d2[i];j++)
    {
      th_addDecal(decals_list2d[i][j]);
    }
  }

  for (int i = 0 ; i < th_getNumThreads();i++)
  {
    for (int j = 0 ; j < counts2d[i];j++)
    {
      th_addParticle(particles_list2d[i][j]);
    }
  }

  memset(counts2d,0,sizeof(int)*th_getNumThreads());
  memset(counts2d2,0,sizeof(int)*th_getNumThreads());
}

static fn_vec3 gameplay_timescale = {{1.0,0.0,0.0}};
void th_setGameplayTimeScale(fn_vec3 scale)
{
  gameplay_timescale = scale;
}

fn_vec3 th_getGameplayTimeScale()
{
  return gameplay_timescale;
}

static fn_vec3 gameplay_glow_color = {{0.0,0.0,0.0}};
static float gameplay_glow_factor = 0;

void th_setGameGlow(fn_vec3 color,float amount)
{
    if (amount > gameplay_glow_factor)
    {
      gameplay_glow_color = color;
      gameplay_glow_factor = amount;
    }

}

void th_getGameGlow(fn_vec3* color,float* amount)
{


  *color = gameplay_glow_color;
  *amount = gameplay_glow_factor;
}

void th_updateGameBuiltins(float dt)
{
  gameplay_glow_factor -= dt*0.0003;
  if (gameplay_glow_factor < 0)
  {
    gameplay_glow_factor = 0;
  }


}

void th_spawnTracer(fn_vec3 pos,fn_vec3 target,int thread_id)
{
    const float size_particle = 7.0;


    fn_vec3 center = fn_multVec3s(fn_addVec3(pos,target),0.5);
    float dist = fn_distance(pos,center);
    float amp = 100.0;
    th_Particle p = th_defaultParticle();;
    // float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
    // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
    p.theta = 0;
    p.alive = true;
    p.life = 20.0;
    p.fade_out = true;
    p.start_time = th_time();
    p.stretch = (dist*0.725)/(size_particle*2.0);
    p.scale = size_particle;
    p.overbright = 10;
    p.texture_handle = th_getParticleTexture(TH_TRACER_BULLET);
    p.makedecal = false;
    p.alphascale = 1.0;
    p.hasphysics = false;
    p.has_gravity = false;
    p.has_velocity = false;
    p.constrained_billboard = true;
    p.use_add_blending = 0.0;
    p.self_destruct = true;
    p.use_premultiplied = 1.0;

    p.velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(target,center)),1.0);

    p.position = center;
    if (thread_id == -1)
    {
      th_addParticle(p);
    }
    else
    {
      if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
      {
        particles_list2d[thread_id][counts2d[thread_id]] = p;
        counts2d[thread_id] = counts2d[thread_id] + 1;
      }
    }

}

void th_spawnLighting(fn_vec3 pos,fn_vec3 target,int thread_id,th_LightQuery* lightq,int point_count,int* randstate,float size_particle,float amplitude)
{
  //int point_count = 20;
  float dist_global = fn_distance(target,pos);
  fn_vec3 direction = fn_normalizeVec3(fn_subVec3(target,pos));

  fn_vec3* points = malloc(sizeof(fn_vec3)*point_count);
  points[0] = pos;
  points[point_count - 1] = target;
  for (int i = 1; i < point_count - 1; i++) {
    float factor = 1.0/((float)point_count - 1.0 );
    factor *= dist_global;
    points[i] = fn_addVec3(points[i - 1],fn_multVec3s(direction,factor));
  }


  fn_vec3 tangent = th_getDecalTangentVector(direction);
  int rand_idx = 0;
  for (int i = 1; i < point_count - 1; i++) {

      float delta = (float)randstate[rand_idx]/(float)(RAND_MAX/(amplitude));
      float gamma = (float)randstate[rand_idx + 1]/(float)(RAND_MAX/(2*3.14159));
      rand_idx = rand_idx + 2;

      fn_vec3 rotated_tangent = fn_transformNormal(tangent,fn_maketranslaterotate(fn_createVec3s(0),gamma,direction));
      points[i] = fn_addVec3(points[i],fn_multVec3s(rotated_tangent,delta));
  }


  for (int i = 1; i < point_count; i++) {
    fn_vec3 center = fn_multVec3s(fn_addVec3(points[i],points[i - 1]),0.5);
    float dist = fn_distance(points[i],center);
    float amp = 100.0;
    th_Particle p = th_defaultParticle();;
    // float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
    // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
    p.theta = 0;
    p.alive = true;
    p.life = 20.0;
    p.fade_out = false;
    p.start_time = th_time();
    p.stretch = (dist*0.725)/(size_particle*2.0);
    p.scale = size_particle;
    p.overbright = 10;
    p.texture_handle = th_getParticleTexture(TH_LIGHTNING);
    p.makedecal = false;
    p.alphascale = 1.0;
    p.hasphysics = false;
    p.has_gravity = false;
    p.has_velocity = false;
    p.constrained_billboard = true;
    p.use_add_blending = 0.0;
    p.self_destruct = true;
    // if (i == 1)
    // {
      p.haslight = true;
      p.lightcolor = fn_createVec3(100*amp,100*amp,150*amp);
      p.lightq = lightq;
      p.tubelight = true;
  //  }


    p.velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(points[i],center)),1.0);
    // p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.3));
    // p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(1.5));
    p.position = center;
    if (thread_id == -1)
    {
      th_addParticle(p);
    }
    else
    {
      if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
      {
        particles_list2d[thread_id][counts2d[thread_id]] = p;
        counts2d[thread_id] = counts2d[thread_id] + 1;
      }
    }
  }

  free(points);

}


void th_spawnLaserBeam(fn_vec3 pos,fn_vec3 target,int thread_id,th_LightQuery* lightq,int point_count,int* randstate,float size_particle,float amplitude)
{
  //int point_count = 20;
  float dist_global = fn_distance(target,pos);
  fn_vec3 direction = fn_normalizeVec3(fn_subVec3(target,pos));

  fn_vec3* points = malloc(sizeof(fn_vec3)*point_count);
  points[0] = pos;
  points[point_count - 1] = target;
  for (int i = 1; i < point_count - 1; i++) {
    float factor = 1.0/((float)point_count - 1.0 );
    factor *= dist_global;
    points[i] = fn_addVec3(points[i - 1],fn_multVec3s(direction,factor));
  }


  // fn_vec3 tangent = th_getDecalTangentVector(direction);
  // int rand_idx = 0;
  // for (int i = 1; i < point_count - 1; i++) {
  //
  //   float delta = (float)randstate[rand_idx]/(float)(RAND_MAX/(amplitude));
  //   float gamma = (float)randstate[rand_idx + 1]/(float)(RAND_MAX/(2*3.14159));
  //   rand_idx = rand_idx + 2;
  //
  //   fn_vec3 rotated_tangent = fn_transformNormal(tangent,fn_maketranslaterotate(fn_createVec3s(0),gamma,direction));
  //   points[i] = fn_addVec3(points[i],fn_multVec3s(rotated_tangent,delta));
  // }


  for (int i = 1; i < point_count; i++) {
    fn_vec3 center = fn_multVec3s(fn_addVec3(points[i],points[i - 1]),0.5);
    float dist = fn_distance(points[i],center);
    float amp = 100.0;
    th_Particle p = th_defaultParticle();;
    // float theta = (float)th_random()/(float)(RAND_MAX/(2*3.14159));
    // p.rotation = fn_makeQuaternion(theta,fn_createVec3(0,0,1));
    p.theta = 0;
    p.alive = true;
    p.life = 20.0;
    p.fade_out = false;
    p.start_time = th_time();
    p.stretch = (dist*0.625)/(size_particle*2.0);
    p.scale = size_particle;
    p.overbright = 10;
    p.texture_handle = th_getParticleTexture(TH_LASERBEAM);
    p.makedecal = false;
    p.alphascale = 1.0;
    p.hasphysics = false;
    p.has_gravity = false;
    p.has_velocity = false;
    p.constrained_billboard = true;
    p.use_add_blending = 0.0;
    p.self_destruct = true;
    // if (i == 1)
    // {
    p.haslight = true;
    p.lightcolor = fn_createVec3(100*amp,0*amp,150*amp);
    p.lightq = lightq;
    p.tubelight = true;
    //  }


    p.velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(points[i],center)),1.0);
    // p.velocity = fn_addVec3(p.velocity,fn_multVec3s(sampleRandomSphere(),0.3));
    // p.velocity = fn_multVec3(fn_normalizeVec3(p.velocity),fn_createVec3s(1.5));
    p.position = center;
    if (thread_id == -1)
    {
      th_addParticle(p);
    }
    else
    {
      if (counts2d[thread_id]< NUM_PARTICLE_CACHE)
      {
        particles_list2d[thread_id][counts2d[thread_id]] = p;
        counts2d[thread_id] = counts2d[thread_id] + 1;
      }
    }
  }

  free(points);



}

void th_playSoundTerminated(a_VirtualSource** s,th_Entity* e,int sound_index,float gain)
{
  if ( *s == NULL)
  {
    *s = a_playVirtualSource(sound_index,0, e->aabb.position,NULL);
    a_setVSLoop(*s,false);
    a_setVSPos(*s,e->aabb.position);
    a_setVSVel(*s,e->velocity);
    a_setVSGain(*s,gain);
    a_setVSCleanup(*s,s,a_standardCleanup);
  }
  else
  {
    a_setVSLoop(*s,false);
    a_setVSPos(*s,e->aabb.position);
    a_setVSVel(*s,e->velocity);
    a_setVSGain(*s,gain);
    a_setVSCleanup(*s,s,a_standardCleanup);
    a_setVSOffset(*s,0.0);
    a_playVS(*s);
  }
}

void th_playSoundIfNotPlaying(a_VirtualSource** s,fn_vec3 position,int sound_index,float gain)
{
  if ( *s == NULL)
  {
    *s = a_playVirtualSource(sound_index,0, position,NULL);
    a_setVSLoop(*s,false);
    a_setVSPos(*s,position);
    a_setVSVel(*s,fn_createVec3s(0));
    a_setVSGain(*s,gain);
    a_setVSCleanup(*s,s,a_standardCleanup);
  }
}

void th_updateSound(a_VirtualSource** s,th_Entity* e)
{
  if (*s != NULL)
  {
    a_setVSPos(*s,e->aabb.position);
    a_setVSVel(*s,e->velocity);
  }
}

void th_updateSoundPosVel(a_VirtualSource** s,fn_vec3 pos,fn_vec3 vel)
{
  if (*s != NULL)
  {
    a_setVSPos(*s,pos);
    a_setVSVel(*s,vel);
  }
}

fn_mat4 th_fadeoutMatrix(float* value,float decay)
{
  *value = *value - decay;
  if (*value < 0 )
  {
    *value = 0;
  }
  return fn_makescale(fn_createVec3s(fn_clamp(*value,0.0,1.0)));
}

fn_mat4 th_fadeinMatrix(float* value,float decay)
{
  *value = *value - decay;
  if (*value < 0 )
  {
    *value = 0;
  }
  return fn_makescale(fn_createVec3s(1.0 - fn_clamp(*value,0.0,1.0)));
}

fn_quat th_update_orient(fn_vec3* from_vec,fn_vec3 target_vec,float ang_spd_deg,float dt,fn_vec3* old_ups)
{
  fn_vec3 from_vector = *from_vec;
  fn_vec3 target_vector = target_vec;
  // fn_printVec3(from_vector);
  // fn_printVec3(target_vector);

  fn_vec3 axis = fn_normalizeVec3(fn_cross(from_vector,target_vector));
  //fn_printVec3(axis);

  float angle_to_rot = -acosf(fn_clamp(fn_dot(from_vector,target_vector),-1,1));
  float delta_angle = -dt*fn_radians(ang_spd_deg);
  bool snap = false;
  //printf("%f %f\n",fabs(delta_angle) , fabs(angle_to_rot));
  if (fabs(delta_angle) > fabs(angle_to_rot))
  {
    delta_angle = angle_to_rot;
    snap = true;
  }


  fn_quat rot = fn_makeQuaternion(delta_angle, axis);
  fn_vec3 dir_rotated = fn_normalizeVec3( fn_rotatePointQuat( from_vector,rot) );
  if (snap)
  {
    dir_rotated = target_vector;
  }


  *from_vec = dir_rotated;


  fn_vec3 bone_to = dir_rotated;

  //c->data[i].old_ups[j] = fn_createVec3(0,1,0);
  fn_mat4 cam = th_6dofCamera(NULL,NULL,old_ups,fn_multVec3(bone_to,fn_createVec3(1,1,1)),fn_createVec3(0,0,0));

  fn_mat4 m = fn_inverse(cam);

  return fn_mat4toquat(m);
}




void th_enactExplosion(fn_vec3 position,th_PlayerObject* playerstate,float dt,th_World* world,th_Entity* player,int thread_id,th_LightQuery* lq,th_Entity* dmg_entity,th_ImpactBuffered** impact_list_2d,int* impact_counts_2d)
{


  if (fn_length2(fn_subVec3(position,player->aabb.position)) < 200*200)
  {
    fn_vec3 n;
    bool can_hit = false;
    float t;
    th_trace(world,position,player->aabb.position,&n,&can_hit,th_getPhysicsMemory(world,thread_id),&t);

    if (!can_hit)
    {
      fn_vec3 d = fn_subVec3(position,player->aabb.position);
      fn_vec3 av = fn_multVec3s(fn_normalizeVec3(d),-1.75f);
      player->velocity = fn_addVec3(av,player->velocity);

      th_setGameGlow(fn_createVec3(1,0,0),0.25);

      th_PlayerObject* po = playerstate;
      th_decrementPlayerHealth(po,9);
      th_decrementPlayerGem(po,9);

      playerstate->screenshake_f = 0.13*0.4;
      playerstate->screenshake_t = 0;
      playerstate->screenshake_amplitude = 0.1;
    }

  }

  int forcecount = 0;
  th_Entity** eptr = th_getEntityPointers(world,thread_id);
  th_getEntitiesInRadius(TH_ENEMY,position,150,thread_id,dt,&forcecount,eptr);

  for (int k = 0; k < forcecount; k++) {
    th_Entity* e = eptr[k];
    //fn_addVec3(e->velocity,
    // e->velocity = fn_multVec3s(fn_normalizeVec3(fn_subVec3(e->aabb.position,position)),20);//);

    int hits_to_give = 1;
    for (int l = 0 ; l < hits_to_give;l++)
    {
      //fn_addVec3(e->velocity,
      // e->impact = true;
      // th_Impact impact = {(void*)(dmg_entity),position};
      // e->impacts[e->impact_count] = impact;
      // e->impact_count++;
      // if(e->impact_count >= MAX_IMPACTS)
      // {
      //   printf("%s\n","MAX IMPACT" );
      //   e->impact_count = 0;
      // }

      if (impact_counts_2d[thread_id] < 1024)
      {
        th_ImpactBuffered buffered = {(void*)e,{(void*)(dmg_entity),position}};
        impact_list_2d[thread_id][impact_counts_2d[thread_id]] = buffered;
        impact_counts_2d[thread_id] = impact_counts_2d[thread_id] + 1;
      }
    }

  }



  {
    a_VirtualSource* s = a_playVirtualSource(32,-1,position,NULL );
    a_setVSLoop(s,false);
    a_setVSPos(s,position);
    a_setVSVel(s,fn_createVec3s(0));
    a_setVSGain(s,0.6);
  }

  th_spawnExplosion(position,thread_id);



  th_LightProperties lnew = th_getDefaultLight();
  lnew.life = 700;
  lnew.fadeout = true;
  float sc = 100;
  lnew.base_color = fn_createVec3(1000*sc,400*sc,0);
  th_makeLight(lq,lnew,fn_createVec3(1000*sc,400*sc,0),position);
}
