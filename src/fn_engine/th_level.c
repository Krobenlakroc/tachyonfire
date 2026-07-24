#include "r_shader.h"
#include "th_gpu.h"
#include "r_mesh.h"
#include <stdlib.h>
#include "../fn_window.h"
#include "r_texture.h"
#include "../fn_input.h"
#include "../fn_config.h"
#include "th_time.h"
#include "th_harmonics.h"
#include "th_cubemap.h"

#include <math.h>
#include <pthread.h>
#include "th_iqm.h"
#include "th_physics.h"


#include <limits.h>


#include "th_clusters.h"
#include "th_level.h"
#include "th_audio.h"
#include "th_threads.h"
#include "th_particle.h"
#include "../fn_game/th_builtins.h"

#include "th_system.h"
#include "md5.h"
#define TH_PI 3.1415926535897932384626433
#include "th_globals.h"
#include "th_spawnset.h"
#include "th_splinegen.h"

#include "th_renderer.h"
//#define FASTTEX
//#define LOGO_SHOW
#define TH_MAX_MATERIALS 1024

#include <malloc.h>
#include "../th_fopen.h"

#define TH_NO_RECACHE

#ifdef TH_NO_RECACHE
bool NORECACHE_MODE = true;
#else
bool NORECACHE_MODE = false;
#endif

void getShadowBounds(th_LevelDescriptor* level,fn_vec3* max,fn_vec3* min)
{

  fn_vec3 shadowCasterPos = level->center_sunlight;
  //-56.692692 ,-224.180786 ,355.918701
  //shadowCasterDir = fn_createVec3(0.068358 ,0.488178 ,-0.870063);
  fn_vec3 shadowCasterDir = level->sundir_shadow;
  //
  // course[0]=fn_createVec3(827.902039,-3330.415283,1546.699463);
  // course[1]=fn_createVec3(-434.229034,-136.181198,-1146.720215);

  // fn_mat4 shadow_proj = fn_ortho3D(-1280,1280,-1500,1500,3150,-3150);
    fn_mat4 shadow_view = fn_scale(fn_lookat(fn_multVec3(shadowCasterPos,fn_createVec3(-1,1,1)),fn_addVec3(fn_multVec3(shadowCasterPos,fn_createVec3(-1,1,1)),fn_multVec3(shadowCasterDir,fn_createVec3(-1,1,1))),fn_createVec3(0,-1,0)),fn_createVec3(-1,1,1));

    // fn_vec3 gma =fn_createVec3(-1531.168579,-2393.254395,879.690613);
    // fn_vec3 gmi =fn_createVec3(1391.097656,10.417622,-818.210022);

    fn_vec3 gmi = level->ls.world->octree.min_position ;//fn_createVec3(-2195.829834,-2513.986328,-1302.32);
    fn_vec3 gma = level->ls.world->octree.max_position ;//fn_createVec3(1265.751099,-91.827774,1975.616333);


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
  *max = mma;
  *min = mmi;
}



typedef struct
{
  unsigned int* indices;
  int icount;
  int vcount;
  fn_vec3* verts;
  th_CollidableVolume volume;
}th_CollisionMeshData;


void th_exportCVOLWAD(th_CollisionMeshData* data,const char* directory,const char* prefix,uint8_t* hash)
{
    //printf("%s\n",prefix );
    char filename[1024];
    sprintf(filename,"%s%s%i.cvolwad",directory,prefix,0);
  //   printf("%i\n",data->icount );
    FILE* mwad = th_fopen(filename,"wb");
    if (mwad == NULL)
    {
      printf("%s\n","CANNOT OPEN" );
    }
    fwrite(hash,sizeof(uint8_t),16,mwad);
    fwrite(&data->icount,sizeof(int),1,mwad);
    fwrite(&data->vcount,sizeof(int),1,mwad);
    if (data->icount > 0)
      fwrite(data->indices,sizeof(unsigned int),data->icount,mwad);

    if (data->vcount > 0)
      fwrite(data->verts,sizeof(fn_vec3),data->vcount,mwad);

    // printf("%i\n", data->volume.planeCount);
    fwrite(&data->volume.planeCount,sizeof(int),1,mwad);
    // // fwrite(&data->volume.usablePlaneCount,sizeof(int),1,mwad);
    // // fwrite(&data->volume.aabb,sizeof(th_Collider),1,mwad);


    //
    if (data->volume.planeCount > 0)
      fwrite(data->volume.planes,sizeof(th_Plane),data->volume.planeCount,mwad);




    fclose(mwad);

}

void th_importCVOLWAD(th_CollisionMeshData* data,const char* directory,const char* prefix)
{
    uint8_t garbage[16];
    char filename[1024];
    sprintf(filename,"%s%s%i.cvolwad",directory,prefix,0);

    FILE* mwad = th_fopen(filename,"rb");
    if (mwad == NULL)
    {
      printf("%s\n","CANNOT OPEN" );
    }
    fread(garbage,sizeof(uint8_t),16,mwad);
    fread(&data->icount,sizeof(int),1,mwad);
    fread(&data->vcount,sizeof(int),1,mwad);


    data->indices = data->icount > 0 ? malloc(sizeof(unsigned int)*data->icount) : NULL;
    data->verts = data->vcount > 0 ? malloc(sizeof(fn_vec3)*data->vcount) : NULL;

    if (data->icount > 0)
      fread(data->indices,sizeof(unsigned int),data->icount,mwad);
    if (data->vcount > 0)
      fread(data->verts,sizeof(fn_vec3),data->vcount,mwad);

    fread(&data->volume.planeCount,sizeof(int),1,mwad);
    // fread(&data->volume.usablePlaneCount,sizeof(int),1,mwad);
    // fread(&data->volume.aabb,sizeof(th_Collider),1,mwad);


    data->volume.planes = data->volume.planeCount > 0 ? malloc(sizeof(th_Plane)*data->volume.planeCount) : NULL;



    if (data->volume.planeCount > 0)
      fread(data->volume.planes,sizeof(th_Plane),data->volume.planeCount,mwad);

    fclose(mwad);

}

th_CollisionMeshData th_getCollisionMeshData(char* name,bool normalize,bool center_mesh)
{
  th_CollisionMeshData out;
  out.verts = th_loadMeshVerts(name,&out.indices,&out.icount,normalize,&out.vcount,center_mesh);
  out.volume.planes = th_getVolumeFromTris(out.verts,out.indices,out.icount,&out.volume.planeCount);
  return out;
}

th_CollisionMeshData th_cacheCollisionMeshData(th_Allocator* alloc,const char* waddir,char* filename,bool normalize,bool center_mesh)
{


  //check hashes
  char prefix[1024];
  int pc = 0;
  for (size_t i = 0; i < strlen(filename) + 1; i++) {
    if (filename[i] != '/')
    {
      prefix[pc] = filename[i];
      pc++;
    }
  }

  char fullname[2048];
  sprintf(fullname,"%s%s%i.cvolwad",waddir,prefix,0);




  uint8_t result_filename[16];

  if (NORECACHE_MODE)
  {
    for (int i = 0 ; i < 16;i++)
    {
      result_filename[i] = 0;
    }
  }
  else
  {
    FILE* hashfile = th_fopen(filename,"rb");
    md5File(hashfile, result_filename);
    fclose(hashfile);
  }

  th_CollisionMeshData out;
  bool exists_cache = th_fileExists(fullname);
  #define TH_FORCE_RECACHE false



  bool recache = false;
  if ((exists_cache && !NORECACHE_MODE  &&(th_getFileModTime(fullname) <  th_getFileModTime(filename) )))
  {
    //chech hashes
    uint8_t result_fromcache[16];
    FILE* cachefile = th_fopen(fullname,"rb");
    fread(result_fromcache, sizeof(uint8_t), 16, cachefile);
    fclose(cachefile);

    for (size_t i = 0; i < 16; i++) {
      if (result_filename[i] != result_fromcache[i])
      {
        //printf("Recaching %s\n",fullname );
        recache = true;
        break;
      }
    }
  }



  if (!exists_cache || recache || TH_FORCE_RECACHE)
  {
    //recache file
    printf("Recaching Collision %s\n",filename );
    // out.verts = th_loadMeshVerts(filename,&out.indices,&out.icount,normalize,&out.vcount,center_mesh);
    // out.volume.planes = th_getVolumeFromTris(out.verts,out.indices,out.icount,&out.volume.planeCount);
    out = th_getCollisionMeshData(filename,normalize,center_mesh);
    if (out.vcount > 0)
    {
      out.verts = th_arenaManage(alloc,out.verts,sizeof(fn_vec3)*out.vcount);
      out.indices = th_arenaManage(alloc,out.indices,sizeof(unsigned int)*out.icount);
      out.volume.planes = th_arenaManage(alloc,out.volume.planes,sizeof(th_Plane)*out.volume.planeCount);
    }

    th_exportCVOLWAD(&out,waddir,prefix,result_filename);

  }
  else {
    th_importCVOLWAD(&out,waddir,prefix);

    if (out.vcount > 0)
    {
     // printf("EEEE %s\n",filename);
      out.verts = th_arenaManage(alloc,out.verts,sizeof(fn_vec3)*out.vcount);
     // printf("%i\n",out.indices[0]);
      //printf("%zu %zu\n",malloc_usable_size(out.indices),sizeof(unsigned int)*out.icount);
      out.indices = th_arenaManage(alloc,out.indices,sizeof(unsigned int)*out.icount);
     // printf("%i\n",out.indices[0]);
      out.volume.planes = th_arenaManage(alloc,out.volume.planes,sizeof(th_Plane)*out.volume.planeCount);
     // printf("EEEE2\n");
    }

  }
  // out.volume.planeEdges = NULL;
  // out.volume.planeEdgesCount = NULL;

  return out;
}



void th_createMeshBrushes(th_Allocator* alloc,th_CollisionMeshData data,th_CollidableVolume** volumes,th_AABB** aabbs,int* index,fn_mat4 mat,const char* tag)
{
  for (int i = 0; i < data.icount/3; i += 1) {
    int a = data.indices[i*3 + 0];
    int b = data.indices[i*3 + 1];
    int c = data.indices[i*3 + 2];

    fn_vec3 a_v = fn_transformVec3(data.verts[a],mat);
    fn_vec3 b_v = fn_transformVec3(data.verts[b],mat);
    fn_vec3 c_v = fn_transformVec3(data.verts[c],mat);

    // th_Plane* planes = malloc(sizeof(th_Plane)*5);

     fn_vec3 points[3];
     points[0] = a_v;
     points[1] = b_v;
     points[2] = c_v;
     fn_vec3 n = th_calculateSurfaceNormal(points);
     fn_vec3 avg = fn_addVec3(fn_addVec3(points[0],points[2]),points[1]);
     avg = fn_multVec3s(avg,1.0/3.0);


     th_CollidableVolume volume;
     volume.planes= NULL;
     volume.planeCount = 0;
     volume.usablePlaneCount = 0;
     volume.points = th_alloc(alloc,sizeof(fn_vec3)*3);
     volume.points[0] = a_v;
     volume.points[1] = b_v;
     volume.points[2] = c_v;
     volume.pointCount = 3;

     *index = *index + 1;
     *volumes = realloc(*volumes,sizeof(th_CollidableVolume)*(*index));
     *aabbs = realloc(*aabbs,sizeof(th_AABB)*(*index));
     volume.id = *index -1;
     volume.tag = tag;
     (*volumes)[*index -1] = volume;
     (*aabbs)[*index -1] = th_getBoundingBox(volume.points,3,fn_identityMat4());
  }
}




typedef enum
{
  TH_STATIC = 1,
  TH_DYNAMC = 2,
  TH_TESSELATED = 4,
  TH_SHADOWCASTING =8,
  TH_RENDERCOMMAND = 16,
  TH_ANIMATEDBRUSH = 32,
  TH_NORMALIZEBRUSH = 64,
  TH_NOCENTER = 128,
  TH_LIGHTSOURCE = 256,
  TH_PSEUDOSTATIC = 512,
  TH_NOCULLING = 1024,
  TH_PRECACHE_SCALE = 2048,
  TH_PRECACHE_SCALEUV = 4096,
  TH_PRECACHE_TR =8192,
  TH_NON_CONVEX = 16384,
  TH_STREAM_DRAW = 32768,

}th_BrushFlags;

typedef struct
{
  th_BrushFlags flags;
  int id;
  int culldata_offset;
}th_BrushTuple;

th_BrushTuple th_createBrush(th_LevelDescriptor* l ,const char* filename,fn_vec2* handles,fn_mat4* matrices,int matcount,th_CollisionMeshData* colmesh,th_BrushFlags flags,th_RenderCommand* command,fn_vec3 iqmscale,th_GpuData* importedData)
{

  th_BrushTuple out;
  out.flags = flags;
  th_World* world = l->levelstate.world;
  if ((flags & (TH_STATIC) || flags & TH_TESSELATED ) && colmesh != NULL)
  {

    char* fn = th_alloc(&l->allocator,sizeof(char)*(strlen(filename) + 1));
    strcpy(fn,filename);
    for (int i = 0; i < matcount; i++) {

      th_createMeshBrushes(&l->allocator,*colmesh,&world->volumes,&world->aabbs,&world->volumecount,matrices[i],fn);
    }

  }
  int meshindex = 0;
  int staticdynamic_index = 0;
  if (!(flags & TH_ANIMATEDBRUSH) && !(flags & TH_STREAM_DRAW))
  {

    meshindex = l->meshes_count;
    out.id = meshindex;
    l->meshes_count++;


    if (importedData == NULL)
    {
      l->meshes = realloc(l->meshes,sizeof(th_GpuData)*l->meshes_count);
      l->meshes[meshindex] = r_loadThorMesh(filename,flags & TH_NORMALIZEBRUSH,false,!(flags & TH_NOCENTER));
      l->meshes[meshindex].verts = th_arenaManage(&l->allocator,l->meshes[meshindex].verts,sizeof(th_Vertex)*(l->meshes[meshindex].vertcount));
      l->meshes[meshindex].indices = th_arenaManage(&l->allocator,l->meshes[meshindex].indices,sizeof(GLuint)*(l->meshes[meshindex].indicecount));
      l->meshes[meshindex].ids = handles;
      l->meshes[meshindex].instances = matrices;
      l->meshes[meshindex].instancecount = matcount;
    }
    else
    {
      l->meshes = realloc(l->meshes,sizeof(th_GpuData)*l->meshes_count);
      l->meshes[meshindex] = *importedData;
      if (matrices != NULL)
      {
        l->meshes[meshindex].instances = matrices;
      }
      if (handles != NULL)
      {
        l->meshes[meshindex].ids = handles;
      }
      l->meshes[meshindex].instancecount = matcount;
    }

    int start_offset = l->culldata->aabbCount;
    out.culldata_offset = start_offset;
    l->culldata->aabbCount += matcount;
    l->culldata->min_x = realloc(l->culldata->min_x,sizeof(float)*(l->culldata->aabbCount));
    l->culldata->max_x = realloc(l->culldata->max_x,sizeof(float)*(l->culldata->aabbCount));
    l->culldata->min_y = realloc(l->culldata->min_y,sizeof(float)*(l->culldata->aabbCount));
    l->culldata->max_y = realloc(l->culldata->max_y,sizeof(float)*(l->culldata->aabbCount));
    l->culldata->min_z = realloc(l->culldata->min_z,sizeof(float)*(l->culldata->aabbCount));
    l->culldata->max_z = realloc(l->culldata->max_z,sizeof(float)*(l->culldata->aabbCount));
    l->culldata->skip_culling_flag = realloc(l->culldata->skip_culling_flag,sizeof(bool)*(l->culldata->aabbCount));
    l->culldata->oc = realloc(l->culldata->oc,sizeof(th_ObjectClass)*(l->culldata->aabbCount));
    for (int m = 0 ; m < matcount;m++)
    {
      fn_vec3 min = fn_createVec3s(0);
      fn_vec3 max = fn_createVec3s(0);
      for (GLuint i = 0 ; i < l->meshes[meshindex].vertcount;i++)
      {
        fn_vec3 v = l->meshes[meshindex].verts[i].position;
        fn_vec3 transformed_vert = fn_multVec4Mat4(matrices[m],fn_createVec4(v.x,v.y,v.z,1.0)).xyz;
        if (i == 0)
        {
          min = transformed_vert;
          max = transformed_vert;
        }
        else
        {
          max.x = transformed_vert.x > max.x ? transformed_vert.x : max.x;
          max.y = transformed_vert.y > max.y ? transformed_vert.y : max.y;
          max.z = transformed_vert.z > max.z ? transformed_vert.z : max.z;

          min.x = transformed_vert.x < min.x ? transformed_vert.x : min.x;
          min.y = transformed_vert.y < min.y ? transformed_vert.y : min.y;
          min.z = transformed_vert.z < min.z ? transformed_vert.z : min.z;
        }
      }

      l->culldata->min_x[m+start_offset] = min.x;
      l->culldata->max_x[m+start_offset] = max.x;
      l->culldata->min_y[m+start_offset] = min.y;
      l->culldata->max_y[m+start_offset] = max.y;
      l->culldata->min_z[m+start_offset] = min.z;
      l->culldata->max_z[m+start_offset] = max.z;

      l->culldata->skip_culling_flag[m + start_offset] = false;

      if (flags & TH_STATIC)
      {
        l->culldata->oc[m+start_offset] = TH_STATIC_OBJECT;
      }
      else
      {
        l->culldata->oc[m+start_offset] = TH_DYNAMIC_OBJECT;
      }
    }

    l->cull_commands = realloc(l->cull_commands,sizeof(th_FrustumCullCommand)*(l->cull_commands_count + 1));
    l->cull_commands[l->cull_commands_count].obj_class = flags & TH_STATIC ? TH_STATIC_OBJECT : TH_DYNAMIC_OBJECT;
    if (flags & TH_NOCULLING)
    {
      l->cull_commands[l->cull_commands_count].obj_class = TH_UNCULLED_OBJECT;
    }
    l->cull_commands[l->cull_commands_count].aabbs_start = start_offset;
    l->cull_commands[l->cull_commands_count].aabbs_end= start_offset + matcount;
    l->cull_commands[l->cull_commands_count].cpu_render_command_id = l->render_commands_count;
    l->cull_commands[l->cull_commands_count].gpu_render_command_id = l->staticdynamic_count;


    l->cull_commands_count  = l->cull_commands_count + 1;

    if (flags & TH_LIGHTSOURCE)
    {
      for (GLuint bn = 0; bn < l->meshes[meshindex].vertcount;bn++)
      {
        l->meshes[meshindex].verts[bn].bitangent = fn_createVec3(10,10,10);
      }
    }

    if (!(flags & TH_TESSELATED) && !(flags & TH_LIGHTSOURCE))
    {
      staticdynamic_index = l->staticdynamic_count;
      l->staticdynamic_count++;
      l->staticdynamic = realloc(l->staticdynamic,sizeof(int)*l->staticdynamic_count);
      l->staticdynamic[staticdynamic_index] = meshindex;
    }


    if (flags & TH_DYNAMC)
    {

      l->meshes_dynamic_count++;
      if (flags & TH_SHADOWCASTING)
      {
        int dynamic_shadowcaster_index = l->dynamic_shadowcaster_count;
        l->dynamic_shadowcaster_count++;

        l->dynamic_shadowcaster = realloc(l->dynamic_shadowcaster,sizeof(int)*l->dynamic_shadowcaster_count);
        l->dynamic_shadowcaster[dynamic_shadowcaster_index] = staticdynamic_index;
      }

    }

    if (flags & TH_TESSELATED)
    {
      int tesselated_index = l->tesselated_count;
      l->tesselated_count++;
      l->tesselated = realloc(l->tesselated ,sizeof(int)*l->tesselated_count);
      l->tesselated[tesselated_index] = meshindex;
    }

    if (flags & TH_LIGHTSOURCE)
    {
      int light_index = l->light_count;
      l->light_count++;
      l->lights = realloc(l->lights ,sizeof(int)*l->light_count);
      l->lights[light_index] = meshindex;
    }
  }

  int anim_index = 0;
  int anim_index_mesh = 0;
  if (flags & TH_ANIMATEDBRUSH)
  {
    anim_index_mesh = l->levelstate.animated_meshes;
    anim_index = l->levelstate.animated_models_count ;
    out.id = anim_index;
    l->levelstate.animated_models_count++;
    l->levelstate.animated_models = realloc(l->levelstate.animated_models,sizeof(th_Model)*l->levelstate.animated_models_count);
    l->levelstate.animated_models[anim_index] = th_loadIQM(&l->allocator,filename,0,matcount,iqmscale);
    l->levelstate.animated_meshes += l->levelstate.animated_models[anim_index].meshcount;
    for (int i = 0 ; i < matcount;i++)
    {
      th_setAnim(&l->levelstate.animated_models[anim_index],l->levelstate.animated_models[anim_index].anims[0].name,i);
    }


    for (GLuint i = 0 ; i < l->levelstate.animated_models[anim_index].meshcount;i++)
    {
      l->levelstate.animated_models[anim_index].meshes[i].instances = matrices;
      l->levelstate.animated_models[anim_index].meshes[i].ids = handles;

      l->levelstate.animated_models[anim_index].meshes[i].instancecount = matcount;
    }

  }

  int stream_index = 0;


  if ((flags & TH_STREAM_DRAW) && importedData != NULL)
  {
    stream_index = l->streamed_meshes;

    out.id = stream_index;
    l->streamed_meshes++;
    l->streamed = realloc(l->streamed,sizeof(th_GpuData)*l->streamed_meshes);
    l->streamed[stream_index] = *importedData;

    if (matrices != NULL)
    {
      l->streamed[stream_index].instances = matrices;
    }
    if (handles != NULL)
    {
      l->streamed[stream_index].ids = handles;
    }
    l->streamed[stream_index].instancecount = matcount;
  }


  if (flags & TH_RENDERCOMMAND)
  {
    if (flags & TH_ANIMATEDBRUSH)
    {
      int anim_render_index = l->anim_render_commands_count;
      l->anim_render_commands_count++;
      l->anim_render_commands = realloc(l->anim_render_commands,sizeof(th_RenderCommand)*l->anim_render_commands_count);
      l->anim_render_commands[anim_render_index] = *command;
      l->anim_render_commands[anim_render_index].model_id = anim_index_mesh;
      l->anim_render_commands[anim_render_index].stride = l->levelstate.animated_models[anim_index].meshcount;
    }
    else if (flags & TH_STREAM_DRAW)
    {
      int render_index = l->render_commands_streamed_count;
      l->render_commands_streamed_count++;
      l->render_commands_streamed = realloc(l->render_commands_streamed,sizeof(th_RenderCommand)*l->render_commands_streamed_count);
      l->render_commands_streamed[render_index] = *command;
      l->render_commands_streamed[render_index].model_id = stream_index;
    }
    else
    {
      int render_index = l->render_commands_count;
      l->render_commands_count++;
      l->render_commands = realloc(l->render_commands,sizeof(th_RenderCommand)*l->render_commands_count);
      l->render_commands[render_index] = *command;
      l->render_commands[render_index].model_id = staticdynamic_index;
    }
  }

  return out;



}

th_BrushTuple th_cacheBrush(const char* waddir,th_LevelDescriptor* l ,const char* filename,fn_vec2* handles,fn_mat4* matrices,int matcount,th_CollisionMeshData* colmesh,th_BrushFlags flags,th_RenderCommand* command,fn_vec3 iqmscale,fn_vec3 modelscale,fn_vec2 modeluvscale,fn_vec3 modeltr)
{

  char prefix[1024];
  int pc = 0;
  for (size_t i = 0; i < strlen(filename) + 1; i++) {
    if (filename[i] != '/')
    {
      prefix[pc] = filename[i];
      pc++;
    }
  }

  char fullname[2048];
  sprintf(fullname,"%s%s%i.mwad",waddir,prefix,0);

  uint8_t result_filename[16];

  if (NORECACHE_MODE)
  {
    for (int i = 0 ; i < 16;i++)
    {
      result_filename[i] = 0;
    }
  }
  else
  {
    FILE* hashfile = th_fopen(filename,"rb");
    if (hashfile == NULL)
    {
      printf("File %s does not exist\n", filename);
    }
    md5File(hashfile, result_filename);
    fclose(hashfile);
  }






  th_BrushTuple shot_brush;
  bool exists_cache = th_fileExists(fullname);
  bool recache = false;
  if (exists_cache && !NORECACHE_MODE && (th_getFileModTime(fullname) <  th_getFileModTime(filename) ))
  {
    //check hashes
    uint8_t result_fromcache[16];
    FILE* cachefile = th_fopen(fullname,"rb");
    fread(result_fromcache, sizeof(uint8_t), 16, cachefile);
    fclose(cachefile);

    for (size_t i = 0; i < 16; i++) {
      if (result_filename[i] != result_fromcache[i])
      {
        printf("Recaching %s\n",fullname );
        recache = true;
        break;
      }
    }

    // md5File(hashfile, result_filename);
    // fclose(hashfile);
  }


  if (!exists_cache || recache)
  {
    printf("Recaching Mesh %s\n",filename );
    //recache file
    shot_brush = th_createBrush(l,filename,handles,matrices,matcount,colmesh,flags,command,iqmscale,NULL);

    if (flags & TH_PRECACHE_SCALEUV)
      r_scaleMeshUVs(&l->meshes[shot_brush.id],modeluvscale);
    if (flags & TH_PRECACHE_SCALE)
      r_scaleThorMesh(&l->meshes[shot_brush.id],modelscale);
    if (flags & TH_PRECACHE_TR)
      r_translateThorMesh(&l->meshes[shot_brush.id],modeltr);

    th_exportMWADs(&l->meshes[shot_brush.id],1,waddir,prefix,result_filename);

  }
  else {

    // for(unsigned int i = 0; i < 16; ++i){
    //     printf("%02x", result[i]);
    // }
    // printf("\n");

    //load from cache
    // printf("%s\n","Loading from cache" );
    th_GpuData* d_temp = malloc(sizeof(th_GpuData));
    th_importMWADs(&d_temp[0],1,waddir,prefix);

    d_temp[0].verts = th_arenaManage(&l->allocator, d_temp[0].verts,sizeof(th_Vertex)*d_temp[0].vertcount);
    d_temp[0].indices = th_arenaManage(&l->allocator,d_temp[0].indices,sizeof(GLuint)*d_temp[0].indicecount);
    if (d_temp[0].instancecount > 0)
    {
      d_temp[0].instances = th_arenaManage(&l->allocator, d_temp[0].instances ,sizeof(fn_mat4)*d_temp[0].instancecount);
      d_temp[0].ids = th_arenaManage(&l->allocator,d_temp[0].ids,sizeof(fn_vec2)*d_temp[0].instancecount);
    }


    shot_brush = th_createBrush(l,filename,handles,matrices,matcount,colmesh,flags,command,iqmscale,&d_temp[0]);
    free(d_temp);
  }

  return shot_brush;
}

th_BrushTuple th_cacheBrushDefault(const char* waddir,th_LevelDescriptor* l ,const char* filename,fn_vec2* handles,fn_mat4* matrices,int matcount,th_CollisionMeshData* colmesh,th_BrushFlags flags,th_RenderCommand* command,fn_vec3 iqmscale)
{
  return th_cacheBrush(waddir,l,filename,handles,matrices,matcount,colmesh,flags,command,iqmscale,fn_createVec3(1,1,1),fn_createVec2(1,1),fn_createVec3(0,0,0));
}

th_BrushTuple th_cacheBrushUVScale(const char* waddir,th_LevelDescriptor* l ,const char* filename,fn_vec2* handles,fn_mat4* matrices,int matcount,th_CollisionMeshData* colmesh,th_BrushFlags flags,th_RenderCommand* command,fn_vec3 iqmscale,fn_vec2 uv)
{
  return th_cacheBrush(waddir,l,filename,handles,matrices,matcount,colmesh,flags | TH_PRECACHE_SCALEUV,command,iqmscale,fn_createVec3(1,1,1),uv,fn_createVec3(0,0,0));
}

th_BrushTuple th_cacheBrushScale(const char* waddir,th_LevelDescriptor* l ,const char* filename,fn_vec2* handles,fn_mat4* matrices,int matcount,th_CollisionMeshData* colmesh,th_BrushFlags flags,th_RenderCommand* command,fn_vec3 iqmscale,fn_vec3 scale)
{
  return th_cacheBrush(waddir,l,filename,handles,matrices,matcount,colmesh,flags | TH_PRECACHE_SCALE,command,iqmscale,scale,fn_createVec2(1,1),fn_createVec3(0,0,0));
}


char* str_append(const char* begin,const char* end)
{
  char* out = malloc(sizeof(char)*(strlen(begin) +  strlen(end)  + 1));
  sprintf(out,"%s%s",begin,end);
  return out;
}

typedef struct
{
  const char* string;
  int id;
}th_TextureFolderHandle;

static th_TextureFolderHandle* texture_folder_ids = NULL;
static int texture_folder_count = 0;

int th_createMaterialDuplicate(th_LevelDescriptor* l ,const char* folder,bool height,bool allow_duplicate)
{

  if (texture_folder_ids == NULL)
  {
    texture_folder_ids = malloc(sizeof(th_TextureFolderHandle)*TH_MAX_MATERIALS);
  }

  if (!allow_duplicate)
  {
    for (int i = 0 ; i < texture_folder_count;i++)
    {
      if (strcmp(texture_folder_ids[i].string,folder) == 0)
      {
        return texture_folder_ids[i].id;
      }
    }
  }


  l->handles_count++;
  l->handles = realloc(l->handles,sizeof(fn_vec2)*l->handles_count);
  l->all_materials = realloc(l->all_materials,sizeof(char*)*l->handles_count*TEX_PER_MATERIAL);
  int index = l->handles_count - 1;
//  printf("%s\n",str_append(folder,"albedo.png") );
  l->all_materials[0 + TEX_PER_MATERIAL*index] = str_append(folder,"albedometal.ktx");
  l->all_materials[1 + TEX_PER_MATERIAL*index] = str_append(folder,"normal.ktx");
  l->all_materials[2 + TEX_PER_MATERIAL*index] = str_append(folder,"roughness.ktx");
  if (height)
  {
    l->all_materials[3 + TEX_PER_MATERIAL*index] = str_append(folder,"displacement.ktx");
  }
  else
  {
    l->all_materials[3 + TEX_PER_MATERIAL*index] = th_strdup("");
  }

  texture_folder_ids[texture_folder_count].id = index;
  texture_folder_ids[texture_folder_count].string = folder;
  texture_folder_count++;
  if (texture_folder_count > TH_MAX_MATERIALS)
  {
    printf("%s\n","Too many textures" );
  }
  return index;
}

int th_createMaterial(th_LevelDescriptor* l ,const char* folder,bool height)
{

  return th_createMaterialDuplicate(l ,folder,height,false);
}

int th_createMaterialAmbientOcclusion(th_LevelDescriptor* l ,const char* folder,bool height)
{

  if (texture_folder_ids == NULL)
  {
    texture_folder_ids = malloc(sizeof(th_TextureFolderHandle)*TH_MAX_MATERIALS);
  }

  for (int i = 0 ; i < texture_folder_count;i++)
  {
    if (strcmp(texture_folder_ids[i].string,folder) == 0)
    {
      return texture_folder_ids[i].id;
    }
  }

  l->handles_count++;
  l->handles = realloc(l->handles,sizeof(fn_vec2)*l->handles_count);
  l->all_materials = realloc(l->all_materials,sizeof(char*)*l->handles_count*TEX_PER_MATERIAL);
  int index = l->handles_count - 1;
//  printf("%s\n",str_append(folder,"albedo.png") );
  l->all_materials[0 + TEX_PER_MATERIAL*index] = str_append(folder,"albedometal.ktx");
  l->all_materials[1 + TEX_PER_MATERIAL*index] = str_append(folder,"normal.ktx");
  l->all_materials[2 + TEX_PER_MATERIAL*index] = str_append(folder,"roughness.ktx");
  if (height)
  {
    l->all_materials[3 + TEX_PER_MATERIAL*index] = str_append(folder,"ambient.ktx");
  }
  else
  {
    l->all_materials[3 + TEX_PER_MATERIAL*index] = th_strdup("");
  }

  texture_folder_ids[texture_folder_count].id = index;
  texture_folder_ids[texture_folder_count].string = folder;
  texture_folder_count++;
  if (texture_folder_count > TH_MAX_MATERIALS)
  {
    printf("%s\n","Too many textures" );
  }
  return index;
}

int th_createMaterialBatch(th_LevelDescriptor* l ,const char* folder,bool height,int start,int end)
{

  if (texture_folder_ids == NULL)
  {
    texture_folder_ids = malloc(sizeof(th_TextureFolderHandle)*TH_MAX_MATERIALS);
  }

  for (int i = 0 ; i < texture_folder_count;i++)
  {
    if (strcmp(texture_folder_ids[i].string,folder) == 0)
    {
      return texture_folder_ids[i].id;
    }
  }

  int first = 0;

  for (int i = start; i <= end; i++) {
    l->handles_count++;
    l->handles = realloc(l->handles,sizeof(fn_vec2)*l->handles_count);
    l->all_materials = realloc(l->all_materials,sizeof(char*)*l->handles_count*TEX_PER_MATERIAL);
    int index = l->handles_count - 1;
    if ( i == start)
    {
      first = index;
    }

    char albed_str[128];
    char disp_str[128];
    sprintf(albed_str,"%i.ktx",i);
    sprintf(disp_str,"%i_d.ktx",i);
  //  printf("%s\n",str_append(folder,"albedo.png") );
    l->all_materials[0 + TEX_PER_MATERIAL*index] = str_append(folder,albed_str);
    l->all_materials[1 + TEX_PER_MATERIAL*index] = str_append(folder,"normal.ktx");
    l->all_materials[2 + TEX_PER_MATERIAL*index] = str_append(folder,"roughness.ktx");
    if (height)
    {
      l->all_materials[3 + TEX_PER_MATERIAL*index] = str_append(folder,disp_str);
    }
    else
    {
      l->all_materials[3 + TEX_PER_MATERIAL*index] = th_strdup("");
    }

    texture_folder_ids[texture_folder_count].id = index;
    texture_folder_ids[texture_folder_count].string = folder;
    texture_folder_count++;
    if (texture_folder_count > TH_MAX_MATERIALS)
    {
      printf("%s\n","Too many textures" );
    }
  }

  return first;
}

fn_mat4* th_make_matrices(fn_mat4 m,int count)
{
  fn_mat4* r = malloc(sizeof(fn_mat4)*count);
  for (int i = 0 ; i < count;i++)
  {
    r[i] = m;
  }
  return r;
}

fn_vec2* th_make_handles(fn_vec2  m,int count)
{
  fn_vec2* r = malloc(sizeof(fn_vec2)*count);
  for (int i = 0 ; i < count;i++)
  {
    r[i] = m;
  }
  return r;
}

fn_mat4* th_make_matrices_arena(th_Allocator* alloc,fn_mat4 m,int count)
{
  fn_mat4* r = th_make_matrices(m,count);
  return th_arenaManage(alloc,r,sizeof(fn_mat4)*count);
}

fn_vec2* th_make_handles_arena(th_Allocator* alloc,fn_vec2  m,int count)
{
  fn_vec2* r = th_make_handles(m,count);
  return th_arenaManage(alloc,r,sizeof(fn_vec2)*count);
}

// fn_mat4 cyl_mat;// = fn_translaterotatescale(fn_createVec3(0,-180,0),fn_radians(0),fn_createVec3(1,0,0),fn_createVec3s(10));
// fn_mat4 wall_mat;// = fn_translaterotatescale(fn_createVec3(0,0,-500),fn_radians(90),fn_createVec3(1,0,0),fn_createVec3s(10));
// fn_mat4 floor_mat;// = fn_makescale(fn_createVec3(10,10,10));
//
// static fn_vec2* centipede_handles;
// static fn_mat4* centipede_matrices;


int th_createMaterialManifest(th_LevelDescriptor* l ,const char* folder)
{
  bool has_height = false;
  FILE* fptr = NULL;
  char* tmp_check = str_append(folder,"displacement.png");
  fptr = th_fopen(tmp_check,"r");
  if (fptr)
  {
    has_height = true;
    fclose(fptr);

  }
  free(tmp_check);
  return th_createMaterial(l,folder,has_height);
}

typedef struct
{
  char** filenames;
  int num_objects;
  int num_noncolliding;
  int* noncolliding;
  int num_cubemapmesh;
  int* cubemapmesh;
  int num_colliders;
  int* colliders;
  int num_concave;
  int* concave;
}th_SceneManifest;

th_SceneManifest th_getSceneManifest(th_Allocator* allocator,const char* folder)
{
  th_SceneManifest ret;
  ret.noncolliding = NULL;

//  int ret = 0;
  FILE* fptr = NULL;
  char* mani_string = str_append(folder,"manifest.txt");
  fptr = th_fopen(mani_string,"r");
  free(mani_string);
  if (fptr)
  {
    fscanf(fptr, "%i", &ret.num_objects);
    int c;
    while ((c = fgetc(fptr)) != '\n' && c != EOF);//skip to next line
    ret.filenames = th_alloc(allocator,sizeof(char*)*ret.num_objects);
    for (int i = 0; i < ret.num_objects; i++) {
      char temp_buf[1024];
      //fscanf(fptr, "%s", temp_buf);
      fgets(temp_buf,1024,fptr);
      temp_buf[strcspn(temp_buf, "\n")] = 0;

      ret.filenames[i] = th_alloc(allocator,sizeof(char)*(1 + strlen(temp_buf)));
      strcpy(ret.filenames[i],temp_buf);
    }
    fscanf(fptr, "%i", &ret.num_noncolliding);
    ret.noncolliding = th_alloc(allocator,sizeof(int)*ret.num_noncolliding);
    for (int i = 0 ; i < ret.num_noncolliding;i++)
    {
      fscanf(fptr, "%i", &ret.noncolliding[i]);
    }
    fscanf(fptr, "%i", &ret.num_cubemapmesh);
    ret.cubemapmesh = th_alloc(allocator,sizeof(int)*ret.num_cubemapmesh);
    for (int i = 0 ; i < ret.num_cubemapmesh;i++)
    {
      fscanf(fptr, "%i", &ret.cubemapmesh[i]);
    }

    fscanf(fptr, "%i", &ret.num_colliders);
    ret.colliders = th_alloc(allocator,sizeof(int)*ret.num_colliders);
    for (int i = 0 ; i < ret.num_colliders;i++)
    {
      fscanf(fptr, "%i", &ret.colliders[i]);
    }

    fscanf(fptr, "%i", &ret.num_concave);
    ret.concave = th_alloc(allocator,sizeof(int)*ret.num_concave);
    for (int i = 0 ; i < ret.num_concave;i++)
    {
      fscanf(fptr, "%i", &ret.concave[i]);
    }
    fclose(fptr);
  }
  return ret;
}


void th_loadLevelAudio()
{
  a_addFile("th1/sound/flesh/impact1.wav");//0
  a_addFile("th1/sound/gunshot2.wav");//1
  a_addFile("th1/sound/gunshot2.wav");//2
  a_addFile("th1/sound/gunshot2.wav");//3
  a_addFile("th1/sound/gunshot2.wav");//4
  a_addFile("th1/sound/brass1.wav");//5
  a_addFile("th1/sound/brass2.wav");//6
  a_addFile("th1/sound/brass3.wav");//7
  a_addFile("th1/sound/brass4.wav");//8
  a_addFile("th1/sound/brass4.wav");//9
  a_addFile("th1/sound/rics/ric1.wav");//10
  a_addFile("th1/sound/rics/ric2.wav");//11
  a_addFile("th1/sound/rics/ric3.wav");//12
  a_addFile("th1/sound/rics/ric3.wav");//13
  a_addFile("th1/sound/shotgun1.wav");//14
  a_addFile("th1/sound/metal/clang1.wav");//15
  a_addFile("th1/sound/metal/clang2.wav");//16
  a_addFile("th1/sound/metal/clang3.wav");//17
  a_addFile("th1/sound/growling_worm.wav");//18
  a_addFile("th1/sound/crash.wav");//19
  a_addFile("th1/sound/chatter.wav");//20
  a_addFile("th1/sound/metal/ding_metal2.wav");//21
  a_addFile("th1/sound/metal/ding_metal2.wav");//22
  a_addFile("th1/sound/screamstereo.wav");//23
  a_addFile("th1/sound/step/boot1.wav");//24
  a_addFile("th1/sound/step/boot2.wav");//25
  a_addFile("th1/sound/step/boot3.wav");//26
  a_addFile("th1/sound/step/boot4.wav");//27
  a_addFile("th1/sound/step/land1.wav");//28
  a_addFile("th1/sound/step/jump1.wav");//29

  a_addFile("th1/sound/rocketloop.wav");//30
  a_addFile("th1/sound/rocketlaunch.wav");//31
  a_addFile("th1/sound/explosion.wav");//32

  a_addFile("th1/sound/angel/fall1.wav");//33 unused
  a_addFile("th1/sound/angel/fall1.wav");//34 DONE
  a_addFile("th1/sound/angel/falling1.wav");//35 DONE
  a_addFile("th1/sound/angel/gasp.wav");//36 DONE
  a_addFile("th1/sound/angel/jump1.wav");//37 DONE
  a_addFile("th1/sound/angel/pain50_1.wav");//38 unused
  a_addFile("th1/sound/angel/pain50_1.wav");//39 DONE
  a_addFile("th1/sound/angel/pain75_1.wav");//40 DONE
  a_addFile("th1/sound/angel/pain100_1.wav");//41 DONE
  a_addFile("th1/sound/hammerfly.wav");//42
  a_addFile("th1/sound/boom2.wav");//43
  a_addFile("th1/sound/hammergrab.wav");//44

  a_addFile("th1/sound/tileslide.wav");//45

  // a_addFile("th1/sound/pain/pain25_1.wav");//46
  // a_addFile("th1/sound/pain/pain50_1.wav");//47
  // a_addFile("th1/sound/pain/pain75_1.wav");//48
  // a_addFile("th1/sound/pain/pain100_1.wav");//49

  a_addFile("th1/sound/step/pain1.wav");//46
  a_addFile("th1/sound/step/pain2.wav");//47
  a_addFile("th1/sound/step/pain3.wav");//48
  a_addFile("th1/sound/step/pain4.wav");//49

  a_addFile("th1/sound/gem/pickup1.wav");//50
  a_addFile("th1/sound/gem/pickup2.wav");//51
  a_addFile("th1/sound/gem/pickup3.wav");//52
  a_addFile("th1/sound/gem/pickup4.wav");//53

  a_addFile("th1/sound/metal/debris1.wav");//54
  a_addFile("th1/sound/ringping.wav");//55
  a_addFile("th1/sound/gem/midair.wav");//56

  sound_shambler_growl = a_addFile("th1/sound/shambler/growl.wav");//57
  sound_shambler_growlstep = a_addFile("th1/sound/shambler/growlstep.wav");//58

  sound_shambler_lightning_charge = a_addFile("th1/sound/shambler/electric_charge.wav");//59
  sound_shambler_lightning_strike = a_addFile("th1/sound/shambler/electric_strike.wav");//60

  sound_horse_mech1 = a_addFile("th1/sound/walker/mechanical1.wav");//61
  sound_horse_mech2 = a_addFile("th1/sound/walker/mechanical2.wav");//62
  sound_horse_impact1 = a_addFile("th1/sound/walker/step1.wav");//63
  sound_horse_impact2 = a_addFile("th1/sound/walker/step2.wav");//64
  sound_horse_impact3 = a_addFile("th1/sound/walker/step3.wav");//65
  sound_horse_givebirth = a_addFile("th1/sound/walker/liquid_spawn.wav");//66


  sound_laser_tricol = a_addFile("th1/sound/tricol/lazer.wav");//67
  sound_tricol_groan1 = a_addFile("th1/sound/tricol/groan1.wav");//68
  sound_tricol_groan2 = a_addFile("th1/sound/tricol/groan2.wav");//69
  sound_tricol_groan3 = a_addFile("th1/sound/tricol/groan3.wav");//70

  sound_boid_break1 = a_addFile("th1/sound/glass/break1.wav");//71
  sound_boid_break2 = a_addFile("th1/sound/glass/break2.wav");//72
  sound_boid_break3 = a_addFile("th1/sound/glass/break3.wav");//73

  sound_shambler_death = a_addFile("th1/sound/shambler/death.wav");//74

  music_track_1999 = a_addFileMusic("th1/sound/music/1999.wav");//75
  music_track_nowhere = a_addFileMusic("th1/sound/music/nowhere.wav");//76
  music_track_outmyface = a_addFileMusic("th1/sound/music/outmyface.wav");//77
  music_track_printer = a_addFileMusic("th1/sound/music/printer.wav");//78
  music_track_punch = a_addFileMusic("th1/sound/music/punch.wav");//79
  music_track_thehum = a_addFileMusic("th1/sound/music/thehum.wav");//80

  sound_gem_impact = a_addFile("th1/sound/gem/impact2.wav");//81
  sound_gem_breakfree = a_addFile("th1/sound/gem/extract2.wav");//82

  sound_landinghard = a_addFile("th1/sound/landinghard.wav");//83
  sound_slideloop = a_addFile("th1/sound/tileslide.wav");//84

  sound_impact1 = a_addFile("th1/sound/flesh/impact1.wav");//85
  sound_impact2 = a_addFile("th1/sound/flesh/impact2.wav");//86
  sound_impact3 = a_addFile("th1/sound/flesh/impact3.wav");//87

  sound_healthpickup = a_addFile("th1/sound/health.wav");//88

  sound_spawn_creature = a_addFile("th1/sound/creaturespawn.wav");//89

  sound_tricol_dooropen = a_addFile("th1/sound/tricol/dooropen.wav");//90

  sound_spawn_creatureii = a_addFile("th1/sound/tricol/spawnin.wav");//91

  sound_level_up1 = a_addFile("th1/sound/levelup1stereo.wav");//92
  sound_level_up2 = a_addFile("th1/sound/levelup2stereo.wav");//93

  sound_tricol_givebirth = a_addFile("th1/sound/walker/spawn_children_extended.wav");//94

  sound_bardrone = a_addFile("th1/sound/victory/bardrone.wav");//95
  sound_belltoll = a_addFile("th1/sound/victory/belltoll.wav");//96
  sound_medalbelltoll = a_addFile("th1/sound/victory/medalhitbell.wav");//97

  music_track_victory = a_addFileMusic("th1/sound/music/victory_theme.wav");//98
  music_track_loss = a_addFileMusic("th1/sound/music/loss.wav");//99
  music_track_title = a_addFileMusic("th1/sound/music/titletheme.wav");//100

  sound_mouseover = a_addFile("th1/sound/menus/click.wav");//101
  sound_clicked = a_addFile("th1/sound/menus/switch.wav");//102


  a_setAudioFileProperties(20, 10, 200, 3);
  a_setAudioFileProperties(18, 5, 300, 3);
  a_setAudioFileProperties(56, 10, 200, 3);

  a_setAudioFileProperties(10, 2, 200, 2);
  a_setAudioFileProperties(11, 2, 200, 2);
  a_setAudioFileProperties(12, 2, 200, 2);
  a_setAudioFileProperties(13, 2, 200, 2);

  a_setAudioFileProperties(15, 2, 200, 2);
  a_setAudioFileProperties(16, 2, 200, 2);
  a_setAudioFileProperties(17, 2, 200, 2);

  a_setAudioFileProperties(1, 100, 200, 100);
  a_setAudioFileProperties(2, 100, 200, 100);
  a_setAudioFileProperties(3, 100, 200, 100);
  a_setAudioFileProperties(4, 100, 200, 100);
  a_setAudioFileProperties(14, 100, 200, 100);

  a_setAudioFileProperties(sound_boid_break1, 8, 200, 4);
  a_setAudioFileProperties(sound_boid_break2, 8, 200, 4);
  a_setAudioFileProperties(sound_boid_break3, 8, 200, 4);
}

static int mat_iron = 0;
static int mat_tile_green = 0;
static int mat_plastic = 0;//th_createMaterial(&l,"th1/compressed/plastic/",false);
static int mat_bricks = 0;//th_createMaterial(&l,"th1/compressed/bricks/",true);
static int mat_gridmetal = 0;//th_createMaterial(&l,"th1/compressed/gridmetal/",false);
static int mat_plasma = 0;//th_createMaterial(&l,"th1/compressed/plasma/",false);
static int mat_marble = 0;//th_createMaterial(&l,"th1/compressed/marble/",false);
static int mat_splotch = 0;//th_createMaterial(&l,"th1/compressed/splotch/",false);
static int mat_qskull = 0;//th_createMaterial(&l,"th1/compressed/qskull/",false);
static int mat_paintmetal = 0;//th_createMaterial(&l,"th1/compressed/chipped_pastatic int/",false);
static int mat_plasma_impact = 0;//th_createMaterial(&l,"th1/compressed/spark/",true);
static int mat_lightning = 0;
static int mat_brass = 0;//th_createMaterial(&l,"th1/compressed/brass/",false);
static int mat_bullethole = 0;
static int mat_blood = 0;
static int mat_ribbed = 0;
static int mat_brushed = 0;
//static int mat_smoke1 = 0;
static int mat_smoke2 = 0;
static int mat_explosion = 0;
static int mat_eye = 0;
static int mat_eye_blue = 0;
static int mat_chrome = 0;
static int mat_gold = 0;
static int mat_impactring = 0;
static int mat_mirror = 0;
static int mat_splotchdark = 0;
static int mat_eyeball_dead = 0;
static int mat_blood_decal = 0;
static int mat_chrome_red = 0;
static int mat_laserbeam = 0;

static int mat_blood_spurt = 0;
static int mat_chippedpaint = 0;
static int mat_bronze = 0;
static int mat_muscles = 0;
static int mat_black = 0;
static int mat_scuffcopper = 0;
static int mat_health = 0;
static int mat_gunblood = 0;
static int mat_machineflash = 0;
static int mat_shotgunflash = 0;
static int mat_jester = 0;
static int mat_rubberseal = 0;
static int mat_circutry = 0;
static int mat_peelingred = 0;
static int mat_wood = 0;

static int mat_blue_wire = 0;
static int mat_green_wire = 0;
static int mat_pink_wire = 0;
static int mat_black_wire = 0;
static int mat_plugblock = 0;

static int mat_hotbarel = 0;
static int mat_chrome_red_hotbarel = 0;
static int mat_brass_bullet = 0;

static int mat_tracer = 0;

static int mat_gold_glow = 0;

static int mat_chrome_glow = 0;

static int mat_metalparticle = 0;

void th_loadLevelTextures(th_LevelDescriptor* l)
{

  #ifdef FASTTEX
  mat_iron = th_createMaterial(l,"th1/compressed/iron/",false);
  mat_tile_green = 0;
  mat_plastic = th_createMaterial(l,"th1/compressed/plastic/",false);//th_createMaterial(&l,"th1/compressed/plastic/",false);
  mat_bricks = 0;//th_createMaterial(&l,"th1/compressed/bricks/",true);
  mat_gridmetal = 0;//th_createMaterial(&l,"th1/compressed/gridmetal/",false);
  mat_plasma = 0;//th_createMaterial(&l,"th1/compressed/plasma/",false);
  mat_marble = 0;//th_createMaterial(&l,"th1/compressed/marble/",false);
  mat_splotch = 0;//th_createMaterial(&l,"th1/compressed/splotch/",false);
  mat_qskull = 0;//th_createMaterial(&l,"th1/compressed/qskull/",false);
  mat_paintmetal = 0;//th_createMaterial(&l,"th1/compressed/chipped_pa/",false);
  mat_plasma_impact = 0;//th_createMaterial(&l,"th1/compressed/spark/",true);
  mat_brass = 0;//th_createMaterial(&l,"th1/compressed/brass/",false);
  mat_bullethole = 0;//th_createMaterial(&l,"th1/compressed/decal_bullethole/",true);
  mat_blood = 0;//th_createMaterial(&l,"th1/compressed/decal_impact/",true);
  mat_ribbed = 0;//th_createMaterial(&l,"th1/compressed/ribbed/",false);
  mat_brushed = 0;
  mat_smoke1 = 0;
  mat_smoke2 = 0;
  mat_explosion = 0;
  mat_eye = 0;
  mat_chrome = 0;
  mat_gold = 0;
  mat_impactring = 0;
  mat_mirror = 0;
  mat_splotchdark = 0;
  mat_eyeball_dead = 0;

  mat_blood_decal = 0;
  mat_lightning = 0;
  mat_chippedpaint = 0;
  mat_bronze = 0;
  mat_black = 0;
  mat_scuffcopper = 0;
  mat_health = 0;
  #else
  /*
   *  I M P O R T A N T
   *  LOAD DECALS FIRST SO THEIR ALPHA MASKS APPEAR IN REGULAR ORDER
   */
  mat_blood_decal = th_createMaterial(l,"th1/compressed/blood_decalnew/",true); //replaced
  mat_bullethole = th_createMaterial(l,"th1/compressed/decal_bullethole2/",true); //replaced


  // mat_tile_green = th_createMaterial(l,"th1/compressed/tile_green/",true);
  mat_iron = th_createMaterial(l,"th1/compressed/iron/",false);
  mat_plastic = th_createMaterial(l,"th1/compressed/plastic/",false);
  // mat_bricks = th_createMaterialAmbientOcclusion(l,"th1/compressed/bricks/",true);
  // mat_gridmetal = th_createMaterial(l,"th1/compressed/gridmetal/",false);
  mat_plasma = th_createMaterial(l,"th1/compressed/plasma/",false);
  // mat_marble = th_createMaterial(l,"th1/compressed/marble/",false);
  mat_splotch = th_createMaterial(l,"th1/compressed/splotch/",false);
  // mat_qskull = th_createMaterial(l,"th1/compressed/qskull/",false);
  // mat_paintmetal = th_createMaterial(l,"th1/compressed/chipped_paint/",false);
  mat_plasma_impact = th_createMaterial(l,"th1/compressed/spark/",true);
  mat_lightning = th_createMaterial(l,"th1/compressed/lightning/",true);
  mat_laserbeam = th_createMaterial(l,"th1/compressed/laserbeam/",true);

  mat_tracer = th_createMaterial(l,"th1/compressed/tracer/",true);

  mat_brass = th_createMaterial(l,"th1/compressed/brass/",false);

  mat_metalparticle = th_createMaterial(l,"th1/compressed/metalparticle/",true); //replaced

  mat_blood = th_createMaterial(l,"th1/compressed/blood_particlenew/",true); //replaced
  mat_ribbed = th_createMaterial(l,"th1/compressed/ribbed/",false);
  mat_brushed = th_createMaterial(l,"th1/compressed/brushed/",false);
  //mat_smoke1 = th_createMaterial(l,"th1/compressed/smokepuff3/",true);
  mat_smoke2 = th_createMaterial(l,"th1/compressed/newsmoke/",true); //replaced
  mat_eye = th_createMaterial(l,"th1/compressed/eyeball/",false);
  mat_eye_blue = th_createMaterial(l,"th1/compressed/eyeball_blue/",false);
  mat_eyeball_dead = th_createMaterial(l,"th1/compressed/eyeball_dead/",false);
  mat_chrome = th_createMaterial(l,"th1/compressed/chrome/",false);
  mat_gold = th_createMaterial(l,"th1/compressed/gold/",false);
  mat_mirror = th_createMaterial(l,"th1/compressed/mirror/",false);
//  mat_splotchdark = th_createMaterial(l,"th1/compressed/chrome/",false);

  mat_explosion = th_createMaterialBatch(l,"th1/compressed/explosion_redo/",true,0,7); //replaced

  mat_impactring = th_createMaterialBatch(l,"th1/compressed/newimpact/",true,0,3); //replaced

  mat_blood_spurt = th_createMaterial(l,"th1/compressed/blood/",true);

  mat_chrome_red = th_createMaterial(l,"th1/compressed/chrome_red/",false);

  mat_chippedpaint = th_createMaterial(l,"th1/compressed/flakingmetal/",false);

  mat_bronze = th_createMaterial(l,"th1/compressed/copper/",false);

  mat_muscles = th_createMaterial(l,"th1/compressed/muscles/",false);

  mat_black = th_createMaterial(l,"th1/compressed/black/",false);

  mat_scuffcopper = th_createMaterial(l,"th1/compressed/scuffcopper/",false);

  mat_health = th_createMaterial(l,"th1/compressed/health/",false);

  mat_gunblood = th_createMaterial(l,"th1/compressed/gunblood/",false);

  mat_machineflash = th_createMaterial(l,"th1/compressed/flash2/",true);

  mat_shotgunflash = th_createMaterial(l,"th1/compressed/shotgunflash/",true);

   mat_jester = th_createMaterial(l,"th1/compressed/jester/",false);

   mat_rubberseal = th_createMaterial(l,"th1/compressed/rubberseal/",false);

   mat_circutry = th_createMaterial(l,"th1/compressed/circutry/",false);

   mat_peelingred = th_createMaterial(l,"th1/compressed/scuffedred/",false);

   mat_wood = th_createMaterial(l,"th1/compressed/glosswood/",false);

   mat_bricks = th_createMaterial(l,"th1/compressed/bricks/",false);

   mat_blue_wire = th_createMaterial(l,"th1/compressed/blue_wire/",false);
   mat_green_wire = th_createMaterial(l,"th1/compressed/green_wire/",false);
   mat_pink_wire = th_createMaterial(l,"th1/compressed/pink_wire/",false);
   mat_black_wire = mat_rubberseal;
   mat_plugblock = th_createMaterialDuplicate(l,"th1/compressed/rubberseal/",false,true);

   mat_hotbarel = th_createMaterialDuplicate(l,"th1/compressed/brushed/",false,true);

   mat_chrome_red_hotbarel = th_createMaterialDuplicate(l,"th1/compressed/chrome_red/",false,true);

   mat_brass_bullet = th_createMaterialDuplicate(l,"th1/compressed/brass/",false,true);

   mat_gold_glow = th_createMaterialDuplicate(l,"th1/compressed/gold/",false,true);
   mat_chrome_glow = th_createMaterialDuplicate(l,"th1/compressed/chrome/",false,true);


  #endif


}

void th_loadLevelParticles(th_LevelDescriptor* l)
{
  //th_registerParticle(l->handles[mat_smoke1],TH_SMOKEA);
  th_registerParticle(l->handles[mat_smoke2],TH_SMOKEB);
  th_registerParticle(l->handles[mat_blood],TH_BLOOD);
  th_registerParticle(l->handles[mat_blood_decal],TH_BLOOD_DECAL);
  th_registerParticle(l->handles[mat_plasma_impact],TH_SPARKS);
  th_registerParticle(l->handles[mat_lightning],TH_LIGHTNING);
  th_registerParticle(l->handles[mat_laserbeam],TH_LASERBEAM);
  th_registerParticle(l->handles[mat_bullethole],TH_BULLETHOLE);
  th_registerParticle(l->handles[mat_explosion],TH_EXPLOSION);
  th_registerParticle(l->handles[mat_impactring],TH_IMPACTRING);
  th_registerParticle(l->handles[mat_blood_spurt],TH_BLOOD_SPURT);
  th_registerParticle(l->handles[mat_machineflash],TH_MACHINEGUN_FLASH);
  th_registerParticle(l->handles[mat_shotgunflash],TH_SHOTGUN_FLASH);

  th_registerParticle(l->handles[mat_tracer],TH_TRACER_BULLET);

  th_registerParticle(l->handles[mat_metalparticle],TH_METALSPLASH);




}

static void th_buildCentipedeCourses(
  th_LevelDescriptor* l,
  const char* spawn_name,
  th_Allocator* alloc,
  th_CentipedeCourse** out_courses,
  int* out_count,
  float** out_times
) {
  int num_spawns = 0;
  th_SpawnsetPair* spawns = th_spawnSetFind(l->spawnset, l->spawnset_entries, spawn_name, &num_spawns);

  th_CentipedeCourse* courses = th_alloc(alloc, sizeof(th_CentipedeCourse) * num_spawns);

  float* times = th_alloc(alloc, sizeof(float) * num_spawns);

  th_markEnemyBirth(num_spawns);
  for (int i = 0; i < num_spawns; i++) {
    times[i] = spawns[i].duration;
    if (spawns[i].duration == 0.0)
    {
      times[i] = 12000.0;
    }

    int num_course_spawn = 0;
    th_SpawnsetPair* course_spawn = th_spawnSetFind(
      l->spawnset, l->spawnset_entries, spawns[i].courseName, &num_course_spawn
    );

    bool predicate_spiral = (num_course_spawn == 3);
    if (predicate_spiral) {
      for (size_t j = 0; j < 3; j++) {
        if (strcmp(course_spawn[j].courseName, "spiral") != 0) {
          predicate_spiral = false;
          break;
        }
      }
    }

    if (!predicate_spiral) continue;

    th_CentipedeCourse* course = &courses[i];
    course->spawn_when = spawns[i].time;
    course->course_count = (int)course_spawn[0].time;
    printf("%s %i\n", spawns[i].courseName, course->course_count);

    course->course = th_alloc(alloc, sizeof(fn_vec3) * MAX_COURSE_SIZE);
    course->angles = th_alloc(alloc, sizeof(float) * MAX_COURSE_SIZE);
    course->ups = th_alloc(alloc, sizeof(fn_vec3) * (MAX_COURSE_SIZE * 2 - 1));

    for (int j = 0; j < MAX_COURSE_SIZE; j++) {
      course->angles[j] = 0;
      course->course[j] = fn_createVec3(0, 0, 0);
    }

    float denum = (float)(course->course_count - 1.0);
  //   for (int j = 0; j < course->course_count; j++) {
  //     float t = (float)j / denum;
  //     float theta = t * 2.0f * 3.14159265359f * course_spawn[1].time;
  //
  //     fn_vec3 p = fn_multVec3s(course_spawn[0].position, cosf(theta));
  //     p = fn_addVec3(p, fn_multVec3s(course_spawn[1].position, sinf(theta)));
  //     p = fn_addVec3(p, fn_multVec3s(course_spawn[2].position, t));
  //     p = fn_addVec3(p, spawns[i].position);
  //
  //     course->course[j] = p;
  //   }
  // }

    const int DENSE = course->course_count * 64;
    fn_vec3* dense = th_alloc(alloc, sizeof(fn_vec3) * DENSE);
    for (int j = 0; j < DENSE; j++) {
      float t     = (float)j / (float)(DENSE - 1);
      float theta = t * 2.0f * 3.14159265359f * course_spawn[1].time;
      fn_vec3 p   = fn_multVec3s(course_spawn[0].position, cosf(theta));
      p = fn_addVec3(p, fn_multVec3s(course_spawn[1].position, sinf(theta)));
      p = fn_addVec3(p, fn_multVec3s(course_spawn[2].position, t));
      p = fn_addVec3(p, spawns[i].position);
      dense[j] = p;
    }

    const float SPACING = 200.0f;
    int   placed       = 0;
    float accumulated  = 0.0f;
    course->course[placed++] = dense[0];          // always include the start

    for (int j = 1; j < DENSE && placed < MAX_COURSE_SIZE; j++) {
      fn_vec3 delta = fn_subVec3(dense[j], dense[j - 1]);
      float   seg   = sqrtf(
        delta.x * delta.x + delta.y * delta.y + delta.z * delta.z
      );
      accumulated += seg;

      while (accumulated >= SPACING && placed < MAX_COURSE_SIZE) {
        // interpolate back to find the exact 200-unit spot
        float  over = accumulated - SPACING;
        float  frac = (seg > 0.0f) ? (1.0f - over / seg) : 1.0f;
        fn_vec3 pt  = fn_lerpVec3(dense[j - 1], dense[j], frac);
        course->course[placed++] = pt;
        accumulated -= SPACING;
      }
    }

    course->course_count = placed;

  }

  *out_courses = courses;
  *out_count = num_spawns;
  *out_times = times;
}

void th_levelLoadDynamicObjects(th_LevelDescriptor* l,const char* wadname)
{
  int num_completiontime;
  th_SpawnsetPair* completiontime_vector = th_spawnSetFind(l->spawnset,l->spawnset_entries,"completiontime",&num_completiontime);

  if (completiontime_vector != NULL)
  {
    l->brass_standard.completiontime = completiontime_vector->position.x;
    l->silver_standard.completiontime = completiontime_vector->position.y;
    l->gold_standard.completiontime = completiontime_vector->position.z;
  }
  th_SpawnsetPair* airtime_vector = th_spawnSetFind(l->spawnset,l->spawnset_entries,"airtime",&num_completiontime);

  if (airtime_vector != NULL)
  {
    l->brass_standard.airtime = airtime_vector->position.x;
    l->silver_standard.airtime = airtime_vector->position.y;
    l->gold_standard.airtime = airtime_vector->position.z;
  }

  int num_spawnlocation;
  th_SpawnsetPair* spawnlocation_vector = th_spawnSetFind(l->spawnset,l->spawnset_entries,"spawnlocation",&num_completiontime);

  if (spawnlocation_vector != NULL)
  {
    l->spawnpoint = spawnlocation_vector->position;
  }

  int num_spawnangles;
  th_SpawnsetPair* spawnangles_vector = th_spawnSetFind(l->spawnset,l->spawnset_entries,"spawnangles",&num_spawnangles);

  if (spawnangles_vector != NULL)
  {
    l->spawnangles = fn_createVec2(spawnangles_vector->position.x,spawnangles_vector->position.y);
  }




  th_Allocator* alloc = &l->allocator;

  l->ls.lifesphere = NULL;
  int num_lifesphere;
  th_SpawnsetPair* life_vector = th_spawnSetFind(l->spawnset,l->spawnset_entries,"life",&num_lifesphere);
  if (life_vector != NULL)
  {
    l->ls.lifesphere = th_alloc(alloc,sizeof(th_LifeSphere));
    th_lifeInit(alloc,l->ls.lifesphere,num_lifesphere,&l->ls);

    for (int i = 0 ; i < num_lifesphere;i++ )
    {
      th_lifeSpawn(l->ls.lifesphere,life_vector[i].position);
    }
  }

  l->ls.question = NULL;
  int num_question;
  life_vector = th_spawnSetFind(l->spawnset,l->spawnset_entries,"question",&num_question);
  if (life_vector != NULL)
  {
    l->ls.question = th_alloc(alloc,sizeof(th_Question));
    th_questionInit(alloc,l->ls.question,num_question,&l->ls);

    for (int i = 0 ; i < num_question;i++ )
    {
      th_questionSpawn(l->ls.question,life_vector[i].position);
    }
  }


  l->ls.tutorial = NULL;
  int num_tutorial;
  th_SpawnsetPair* tutorial_vector = th_spawnSetFind(l->spawnset,l->spawnset_entries,"tutorial_sequence",&num_tutorial);
  if (tutorial_vector != NULL)
  {
    th_TutorialType type = TH_TUT_MOVEMENT;
    if (tutorial_vector[0].time == 1.0)
    {
      type = TH_TUT_KILL_BEETLE;
    }
    else if (tutorial_vector[0].time == 2.0)
    {
      type = TH_TUT_KILL_CENTI;
    }
    l->ls.tutorial = th_alloc(alloc,sizeof(th_TutorialObject));
    th_tutorialInitialize(alloc,l->ls.tutorial,&l->ls,type);
  }

  // fn_vec3 skoffset = fn_createVec3(963.608459 ,-1182.538940, 70.984810);
  fn_vec3 skoffset = fn_createVec3(  -1354.420410, -850.906921, 17.274073);


  int tricol_count = 0;
  th_SpawnsetPair* tricol_spawns = th_spawnSetFind(l->spawnset,l->spawnset_entries,"spawner_ii",&tricol_count);

  fn_vec3* tricol_positions = th_alloc(alloc,sizeof(fn_vec3)*tricol_count);
  float* tricol_times = th_alloc(alloc,sizeof(float)*tricol_count);
  fn_vec3** tricol_courses = th_alloc(alloc,sizeof(fn_vec3*)*tricol_count);
  int* tricol_courses_count = th_alloc(alloc,sizeof(int)*tricol_count);

  th_markEnemyBirth(tricol_count);

  for (int i = 0; i < tricol_count; i++) {
    tricol_positions[i] = tricol_spawns[i].position;
    tricol_times[i] = tricol_spawns[i].time;

    int num_tricol_course_spawn = 0;
    th_SpawnsetPair* tricol_course_spawn = th_spawnSetFind(l->spawnset,l->spawnset_entries,tricol_spawns[i].courseName ,&num_tricol_course_spawn);

    fn_vec3* tricol_course = th_alloc(alloc,sizeof(fn_vec3)*num_tricol_course_spawn);//{target,target4,target5};
    for (int j = 0; j < num_tricol_course_spawn; j++) {
      tricol_course[j] = tricol_course_spawn[j].position;
    }

    tricol_courses[i] = tricol_course;
    tricol_courses_count[i] = num_tricol_course_spawn;

    l->ls.num_spawners_total = l->ls.num_spawners_total + 1;
  }


 int num_horse_spawns_hard = 0;
 th_SpawnsetPair* horse_spawns_hard = th_spawnSetFind(l->spawnset,l->spawnset_entries,"horse",&num_horse_spawns_hard);

 int num_horse_spawns_easy = 0;
 th_SpawnsetPair* horse_spawns_easy = th_spawnSetFind(l->spawnset,l->spawnset_entries,"horse_easy",&num_horse_spawns_easy);

 //union
 int num_horse_spawns = num_horse_spawns_hard + num_horse_spawns_easy;
 th_SpawnsetPair* horse_spawns = NULL;
 if (horse_spawns_easy == NULL && horse_spawns_hard != NULL)
 {
    horse_spawns = horse_spawns_hard;
 }
 else if (horse_spawns_hard == NULL && horse_spawns_easy != NULL)
 {
    horse_spawns = horse_spawns_easy;
 }
 else
 {
    horse_spawns = horse_spawns_hard < horse_spawns_easy ? horse_spawns_hard : horse_spawns_easy;
 }



 fn_vec3* horse_positions = th_alloc(alloc,sizeof(fn_vec3)*num_horse_spawns);
 float* horse_times = th_alloc(alloc,sizeof(float)*num_horse_spawns);
 fn_vec3** horse_courses = th_alloc(alloc,sizeof(fn_vec3*)*num_horse_spawns);
 int* horse_courses_count = th_alloc(alloc,sizeof(int)*num_horse_spawns);
 bool* horse_easymode = th_alloc(alloc,sizeof(bool)*num_horse_spawns);


 th_markEnemyBirth(num_horse_spawns);

 for (int i = 0; i < num_horse_spawns; i++) {
    l->ls.num_spawners_total = l->ls.num_spawners_total + 1;
   int num_horse_course_spawn = 0;

   horse_positions[i] = horse_spawns[i].position;
   horse_times[i] = horse_spawns[i].time;
   horse_easymode[i] = false;

   th_SpawnsetPair* horse_course_spawn = th_spawnSetFind(l->spawnset,l->spawnset_entries,horse_spawns[i].courseName ,&num_horse_course_spawn);

   fn_vec3* horse_course = th_alloc(alloc,sizeof(fn_vec3)*num_horse_course_spawn);//{target,target4,target5};
   for (int j = 0; j < num_horse_course_spawn; j++) {
     horse_course[j] = horse_course_spawn[j].position;
   }

   horse_courses[i] = horse_course;
   horse_courses_count[i] = num_horse_course_spawn;

   if (strcmp(horse_spawns[i].name,"horse_easy") == 0)
   {
     horse_easymode[i] = true;
   }
 }




 int num_shambler_spawns = 0;
 th_SpawnsetPair* shambler_spawns = th_spawnSetFind(l->spawnset,l->spawnset_entries,"shambler",&num_shambler_spawns);

 fn_vec3* shambler_positions = th_alloc(alloc,sizeof(fn_vec3)*num_shambler_spawns);
 float* shambler_times = th_alloc(alloc,sizeof(float)*num_shambler_spawns);


 th_markEnemyBirth(num_shambler_spawns);
 for (int i = 0; i < num_shambler_spawns; i++) {

   shambler_positions[i] = shambler_spawns[i].position;
   shambler_times[i] = shambler_spawns[i].time;
 }


 float* centi_spawn_times = NULL;
 th_CentipedeCourse* centi_courses = NULL;
 int num_centi_spawns = 0;
 th_buildCentipedeCourses(l, "centipede", alloc, &centi_courses, &num_centi_spawns,&centi_spawn_times);


 float* centi_spawn_times_super = NULL;
 th_CentipedeCourse* centi_courses_super = NULL;
 int num_centi_spawns_super = 0;
 th_buildCentipedeCourses(l, "centipedeii", alloc, &centi_courses_super, &num_centi_spawns_super,&centi_spawn_times_super);


 int centi_segments = 20;
 int centi_count = num_centi_spawns*centi_segments;

 int centi_segments_super = 5;
 int centi_count_super = num_centi_spawns_super*centi_segments_super;

 int boidscount = 1024;
 int dim = 0;
 int eyeball_count = 25;
 int eyeballs_spawn = 0;
 int shamcount = 20;
 int demon_count = 0;
 bool spawn_shams = false;
  int horse_count = num_horse_spawns;
  // int centi_count = 40;
  // int dim = 5;
  // int eyeball_count = 2;
  // int shamcount = 2;

  /*
  *PLAYER INITIALIZATION
  */
  th_LevelState* ls = &l->levelstate;

  float fraction_charsize_x = 128.0/1920.0;
  //float spc_op = th_stringDims(" ",1,cmap).x;

  l->text_commands_count = 4;
  l->text_commands = th_alloc(alloc,sizeof(th_TextCommand)*l->text_commands_count);
  //health% number
  l->text_commands[0].string = th_alloc(alloc,sizeof(char)*100);
  l->text_commands[0].pos_tx = fn_createVec2(fraction_charsize_x*0.5,fraction_charsize_x*0.25);
  l->text_commands[0].size = 1;
  l->text_commands[0].font = 0;
  l->text_commands[0].color = fn_createVec3(1,1,1);

  //weapon name
  l->text_commands[1].string = th_alloc(alloc,sizeof(char)*100);
  l->text_commands[1].pos_tx = fn_createVec2(fraction_charsize_x*0.5 + fraction_charsize_x*7,fraction_charsize_x*0.85);
  l->text_commands[1].size = 0.5;
  l->text_commands[1].font = 0;
  l->text_commands[1].color = fn_lerpVec3(fn_createVec3(1,0.64,0),fn_createVec3(1,1,1),0.2);

  //level % number
  //10.85
  l->text_commands[2].string = th_alloc(alloc,sizeof(char)*100);
  l->text_commands[2].pos_tx = fn_createVec2(fraction_charsize_x*0.5 + fraction_charsize_x*10.0,fraction_charsize_x*0.25);
  l->text_commands[2].size = 1;
  l->text_commands[2].font = 1;
  l->text_commands[2].color = fn_createVec3(1,0,0);

  l->text_commands[3].string = th_alloc(alloc,sizeof(char)*100);
  l->text_commands[3].pos_tx = fn_createVec2(fraction_charsize_x*0.5 + fraction_charsize_x*11.6,fraction_charsize_x*0.25);
  l->text_commands[3].size = 0.33333;
  l->text_commands[3].font = 1;
  l->text_commands[3].color = fn_createVec3(1,1,1);

  // l->text_commands[3].string = malloc(sizeof(char)*100);
  // l->text_commands[3].pos_tx = fn_createVec2(0,0);
  // l->text_commands[3].size = 1;
  // l->text_commands[3].font = 1;

  l->ls.horse = NULL;

  l->ls.player = th_alloc(alloc,sizeof(th_PlayerObject));
  th_playerInitialize(l->ls.player,l->text_commands[2].string,l->text_commands[0].string,l->text_commands[2].string,l->text_commands[1].string,&l->text_commands[2].color,l->text_commands[3].string,&l->ls);
  l->ls.player->level_pct_size = &l->text_commands[2].size;
  l->ls.player->health_pct_size = &l->text_commands[0].size;
  l->ls.player->health_pct_color = &l->text_commands[0].color;
  l->ls.player->health_pos = &l->text_commands[0].pos_tx;
  l->ls.player->level_pos = &l->text_commands[2].pos_tx;


  ls->player_e = TH_DEFAULT_ENTITY;
  ls->player_e.velocity = fn_createVec3s(0);
  ls->player_e.aabb.position = l->spawnpoint;
  ls->player_e.grounded = false;
  ls->player_e.mode = TH_SLIDE_MODE;




    /*
    *SHAMBLER INITIALIZATION
    */

    ls->shamblers = NULL;
    if (shamcount != 0)
    {
      ls->shamblers = th_alloc(alloc,sizeof(th_ShamblerGroup));
      th_shamblerInitialize(alloc,ls->shamblers,shamcount,&l->ls,shambler_positions,shambler_times,num_shambler_spawns);

      if (spawn_shams)
      {
        th_shamblersSpawn(ls->shamblers,fn_createVec3(0,-500,200));
      }

      // ls->shamblers->entities[0].aabb.position = fn_createVec3(0,-500,200);
      //
      // if(shamcount >= 2)
      //   ls->shamblers->entities[1].aabb.position = fn_createVec3(200,-500,0);
      fn_createGrid(alloc,&ls->shamblers->grid,ls->shamblers->entities,ls->shamblers->entity_count,fn_createVec3s(6.4*1.7*2*6),200);

      th_RenderCommand shambler_command;
      shambler_command.offset = 0;
      shambler_command.model_id = 0;
      shambler_command.matcount = &ls->shamblers->entity_count;
      shambler_command.mats = &ls->shamblers->transforms;
      shambler_command.stride = 0;
      fn_vec2* shambler_handles = th_make_handles_arena(alloc,l->handles[mat_muscles],shamcount);
      // shambler_handles[0] = l->handles[mat_chrome_red];
      // if(shamcount >= 2)
      //   shambler_handles[1] = l->handles[mat_chrome_red];
      fn_mat4* shambler_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),shamcount);
      // shambler_matrices[0] = fn_identityMat4();
      // if(shamcount >= 2)
      //   shambler_matrices[1] = fn_identityMat4();

      th_BrushTuple shambler_brush = th_createBrush(l,"th1/models/muscle.iqm",shambler_handles,shambler_matrices,shamcount,NULL,TH_ANIMATEDBRUSH | TH_RENDERCOMMAND,&shambler_command,fn_createVec3s(250),NULL);
      //th_transformModel(&l->animated_models[shambler_brush.id],fn_makescale(fn_createVec3s(1.5)));
      for (int i = 0; i < shamcount; i++) {
        ((th_ShamblerData*)ls->shamblers->data)[i].model_id = shambler_brush.id;
      }

      th_EntityCollisionEdict shambler_edict;
      shambler_edict.entities = ls->shamblers->entities;
      shambler_edict.entityCount= &ls->shamblers->entity_count;
      shambler_edict.grid = NULL;//&ls->boidgroups[0].grid;
      shambler_edict.flags = TH_ENEMY;
      th_registerEntityGroup(shambler_edict);

      // for (int i = 0 ; i < num_shambler_spawns;i++)
      // {
      //   printf("Shambler spawn \n");
      //   th_shamblersSpawn(ls->shamblers,shambler_positions[i]);
      // }

    }


   // ((th_ShamblerData*)ls->shamblers->data)[0].model_id = shambler_brush.id;
   // ((th_ShamblerData*)ls->shamblers->data)[1].model_id = shambler_brush.id;


   /*
   *DEMON INITIALIZATION
   */



  demon_count = 0;


   fn_vec2* demon_handles = th_alloc(alloc,sizeof(fn_vec2)*demon_count);
   if (demon_count >= 1)
   {
     demon_handles[0] = l->handles[mat_muscles];
   }

   if (demon_count >= 2)
   {
     demon_handles[1] = l->handles[mat_muscles];
   }

   if (demon_count >= 3)
   {
     demon_handles[2] = l->handles[mat_muscles];
   }



   fn_mat4* demon_matrices = th_alloc(alloc,sizeof(fn_mat4)*demon_count);

   if (demon_count > 0)
   {
     fn_quat rot_quat = fn_getRotationQuaternion(fn_createVec3(0,0,-1),fn_createVec3(0,-1,0));
     demon_matrices[0] = fn_translaterotatescaleq(fn_createVec3(60.937172, -217.144196, -255.763474),rot_quat,fn_createVec3s(1.0));//275

     // rot_quat = fn_getRotationQuaternion(fn_createVec3(0,0,-1),fn_createVec3(0,-1,0));
     // rot_quat = fn_multquat(rot_quat,fn_makeQuaternion(fn_radians(-90),fn_createVec3(0,-1,0)));
     // demon_matrices[1] = fn_translaterotatescaleq(fn_createVec3(777.149292, -596.312988 + 35, -386.491058 + 50),rot_quat,fn_createVec3s(4));
     //
     // rot_quat = fn_getRotationQuaternion(fn_createVec3(0,0,-1),fn_createVec3(0,-1,0));
     // rot_quat = fn_multquat(rot_quat,fn_makeQuaternion(fn_radians(90),fn_createVec3(0,-1,0)));
     // demon_matrices[2] = fn_translaterotatescaleq(fn_createVec3(777.149292, -596.312988 + 35, 487.543884 - 50),rot_quat,fn_createVec3s(4));
   }

   th_BrushTuple demon_brush;
   if (demon_count > 0)
   {
     demon_brush = th_createBrush(l,"th1/models/muscleman.iqm",demon_handles,demon_matrices,demon_count,NULL,TH_ANIMATEDBRUSH,NULL,fn_createVec3s(275.0),NULL);
   }

   // th_setAnim(&l->levelstate.animated_models[demon_brush.id],"demon/idle1",0);
   // th_setAnim(&l->levelstate.animated_models[demon_brush.id],"demon/idle1",1);
   // th_setAnim(&l->levelstate.animated_models[demon_brush.id],"demon/idle1",2);
   for (int i = 0; i < demon_count; i++) {
     th_setAnim(&l->levelstate.animated_models[demon_brush.id],"walk",i);
     //th_setTimeScale(&l->levelstate.animated_models[demon_brush.id],0.75,i);
   }





   /*
   *VIEWMODEL WEAPON INITIALIZATION
   */

   fn_vec2* hammer_handles = th_make_handles_arena(alloc,l->handles[mat_gold_glow],2);
   hammer_handles[1] = l->handles[mat_chrome_glow];
   fn_mat4* hammer_mats = th_make_matrices_arena(alloc,fn_identityMat4(),2);

   fn_vec2* gun_handles = th_make_handles_arena(alloc,l->handles[mat_brushed],1);//mat_chrome_red
   fn_vec2* machinegun_handles = th_make_handles_arena(alloc,l->handles[mat_gunblood],1);//

   fn_vec2* sledge_handles = th_make_handles_arena(alloc,l->handles[mat_gold_glow],1);//mat_chrome_red

   fn_vec2* gun_handles_level2 = th_make_handles_arena(alloc,l->handles[mat_chrome_red_hotbarel],1);//
   fn_vec2* machinegun_handles_level2 = th_make_handles_arena(alloc,l->handles[mat_hotbarel],2);//mat_iron

   fn_mat4* gun_mats_akimbo = th_make_matrices_arena(alloc,fn_identityMat4(),2);

   fn_mat4* gun_mats = th_make_matrices_arena(alloc,fn_identityMat4(),1);
   gun_mats[0] = fn_translaterotatescale(fn_createVec3(200,-180,0),fn_radians(0),fn_createVec3(1,0,0),fn_createVec3s(10));

   ls->weapon = th_alloc(alloc,sizeof(th_Weapon));
   th_weaponInitialize(alloc,ls->weapon,&l->ls);





   th_RenderCommand shot_command;
   shot_command.offset = 0;
   shot_command.model_id = 0;
   shot_command.matcount = &ls->weapon->weapon_transform_count_shotgun;
   shot_command.mats = &ls->weapon->weapon_transforms_shotgun;
   shot_command.stride = 1;


   th_BrushTuple shot_brush = th_cacheBrush(wadname,l,"th1/models/shotgun2.obj",gun_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING,&shot_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));


   th_RenderCommand gun_command;
   gun_command.offset = 0;
   gun_command.model_id = 0;
   gun_command.matcount = &ls->weapon->weapon_transform_count;
   gun_command.mats = &ls->weapon->weapon_transforms;
   gun_command.stride = 1;

   th_RenderCommand gun_bolt_command;
   gun_bolt_command.offset = 0;
   gun_bolt_command.model_id = 0;
   gun_bolt_command.matcount = &ls->weapon->weapon_transform_count;
   gun_bolt_command.mats = &ls->weapon->weapon_transforms_bolt;
   gun_bolt_command.stride = 1;


   // th_BrushTuple gun_brush = th_cacheBrush(wadname,l,"th1/models/machinegun2.obj",machinegun_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING,&gun_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));
    machinegun_handles = th_make_handles_arena(alloc,l->handles[mat_wood],1);

    th_BrushTuple gun_brush = th_cacheBrush(wadname,l,"th1/models/sten/sten_stock.obj",machinegun_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER,&gun_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

    machinegun_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],1);

     gun_brush = th_cacheBrush(wadname,l,"th1/models/sten/sten_bolt.obj",machinegun_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER,&gun_bolt_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

     machinegun_handles = th_make_handles_arena(alloc,l->handles[mat_rubberseal],1);

     gun_brush = th_cacheBrush(wadname,l,"th1/models/sten/sten_mag.obj",machinegun_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER,&gun_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

     machinegun_handles = th_make_handles_arena(alloc,l->handles[mat_brushed],1);

     gun_brush = th_cacheBrush(wadname,l,"th1/models/sten/sten_metallic.obj",machinegun_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER,&gun_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

     machinegun_handles = th_make_handles_arena(alloc,l->handles[mat_hotbarel],1);

     gun_brush = th_cacheBrush(wadname,l,"th1/models/sten/sten_barrel.obj",machinegun_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER,&gun_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

     fn_vec3 barrel_center = th_getCenterVerts(&l->meshes[gun_brush.id]);
     fn_vec3 barrel_max,barrel_min;
     th_getMinMaxVerts(&l->meshes[gun_brush.id],&barrel_min,&barrel_max);

     fn_vec3 center_axis_a = fn_createVec3(barrel_center.x,barrel_center.y,barrel_min.z);
     fn_vec3 center_axis_b = fn_createVec3(barrel_center.x,barrel_center.y,barrel_max.z);

     ls->weapon->sten_axis_a = center_axis_a;
     ls->weapon->sten_axis_b = center_axis_b;



   th_RenderCommand shot_level2_command;
   shot_level2_command.offset = 0;
   shot_level2_command.model_id = 0;
   shot_level2_command.matcount = &ls->weapon->weapon_transform_count_shotgun_level2;
   shot_level2_command.mats = &ls->weapon->weapon_transforms_shotgun_level2;
   shot_level2_command.stride = 1;


   th_BrushTuple shot_level2_brush = th_cacheBrush(wadname,l,"th1/models/spas12.obj",gun_handles_level2,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING,&shot_level2_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

   barrel_center = th_getCenterVerts(&l->meshes[shot_level2_brush.id]);
   th_getMinMaxVerts(&l->meshes[shot_level2_brush.id],&barrel_min,&barrel_max);

   center_axis_a = fn_createVec3(barrel_center.x,barrel_center.y,barrel_min.z);
   center_axis_b = fn_createVec3(barrel_center.x,barrel_center.y,barrel_max.z);

   ls->weapon->spas12_axis_a = center_axis_a;
   ls->weapon->spas12_axis_b = center_axis_b;


   th_RenderCommand gun_level2_command;
   gun_level2_command.offset = 0;
   gun_level2_command.model_id = 0;
   gun_level2_command.matcount = &ls->weapon->weapon_transform_count_level2;
   gun_level2_command.mats = &ls->weapon->weapon_transforms_level2;
   gun_level2_command.stride = 1;


   th_BrushTuple gun_brush_level2 = th_cacheBrush(wadname,l,"th1/models/chaingun.obj",machinegun_handles_level2,gun_mats_akimbo,2,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING,&gun_level2_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

   barrel_center = th_getCenterVerts(&l->meshes[gun_brush_level2.id]);
   th_getMinMaxVerts(&l->meshes[gun_brush_level2.id],&barrel_min,&barrel_max);

   center_axis_a = fn_createVec3(barrel_center.x,barrel_center.y,barrel_min.z);
   center_axis_b = fn_createVec3(barrel_center.x,barrel_center.y,barrel_max.z);

   ls->weapon->chaingun_axis_a = center_axis_a;
   ls->weapon->chaingun_axis_b = center_axis_b;


   fn_vec2* flak_handles = th_make_handles_arena(alloc,l->handles[mat_chippedpaint],1);
   fn_vec2* flak_pole_handles = th_make_handles_arena(alloc,l->handles[mat_iron],1);
   fn_mat4* flak_mats = th_make_matrices_arena(alloc,fn_identityMat4(),1);

   th_RenderCommand flak_cannon_front_command;
   flak_cannon_front_command.offset = 0;
   flak_cannon_front_command.model_id = 0;
   flak_cannon_front_command.matcount = &ls->weapon->weapon_transform_count_flak_cannon_front;
   flak_cannon_front_command.mats = &ls->weapon->weapon_transforms_flak_cannon_front;
   flak_cannon_front_command.stride = 1;


   th_BrushTuple flak_front_brush = th_cacheBrush(wadname,l,"th1/models/flak_cannon/front.obj",flak_handles,flak_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER | TH_NOCULLING,&flak_cannon_front_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));

   th_RenderCommand flak_cannon_pole_command;
   flak_cannon_pole_command.offset = 0;
   flak_cannon_pole_command.model_id = 0;
   flak_cannon_pole_command.matcount = &ls->weapon->weapon_transform_count_flak_cannon_front;
   flak_cannon_pole_command.mats = &ls->weapon->weapon_transforms_flak_cannon_front;
   flak_cannon_pole_command.stride = 1;


   th_BrushTuple flak_pole_brush = th_cacheBrush(wadname,l,"th1/models/flak_cannon/columns.obj",flak_pole_handles,flak_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER | TH_NOCULLING,&flak_cannon_pole_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));


   th_RenderCommand flak_cannon_back_command;
   flak_cannon_back_command.offset = 0;
   flak_cannon_back_command.model_id = 0;
   flak_cannon_back_command.matcount = &ls->weapon->weapon_transform_count_flak_cannon_back;
   flak_cannon_back_command.mats = &ls->weapon->weapon_transforms_flak_cannon_back;
   flak_cannon_back_command.stride = 1;


   th_BrushTuple flak_back_brush = th_cacheBrush(wadname,l,"th1/models/flak_cannon/back.obj",flak_handles,flak_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER | TH_NOCULLING,&flak_cannon_back_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1,1),fn_createVec3(0,0,0));


   th_RenderCommand sledge_command;
   sledge_command.offset = 0;
   sledge_command.model_id = 0;
   sledge_command.matcount = &ls->weapon->weapon_transform_count_sledge;
   sledge_command.mats = &ls->weapon->weapon_transforms_sledge;
   sledge_command.stride = 1;

   th_BrushTuple sledge_brush = th_cacheBrush(wadname,l,"th1/models/sledge_head.obj",sledge_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_PRECACHE_SCALEUV | TH_NOCENTER,&sledge_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.2,0.2),fn_createVec3(0,0,0));


   sledge_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],1);

    th_cacheBrush(wadname,l,"th1/models/sledge_handle.obj",sledge_handles,gun_mats,1,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_PRECACHE_SCALEUV | TH_NOCENTER,&sledge_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.2,0.2),fn_createVec3(0,0,0));

   // r_scaleMeshUVs(&l->meshes[gun_brush.id],fn_createVec2(0.2,0.2));

   /*
   * HAMMER INITIALIZATION
   */

   ls->hammer = th_alloc(alloc,sizeof(th_HammerObject));
   th_hammerInitialize(alloc,ls->hammer,ls->weapon->weapon_transforms_hammer,ls->weapon->weapon_transforms_sledge,&l->ls);

   th_RenderCommand hammer_command;
   hammer_command.offset = 0;
   hammer_command.model_id = 0;
   hammer_command.matcount = &ls->weapon->weapon_transform_count_hammer;
   hammer_command.mats = &ls->weapon->weapon_transforms_hammer;
   hammer_command.stride = 1;

   th_BrushTuple hammer_brush = th_cacheBrush(wadname,l,"th1/models/hammer_head.obj",hammer_handles,hammer_mats,2,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER
   | TH_PRECACHE_SCALEUV,&hammer_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1.0,1.0),fn_createVec3(0,0,0));


   fn_vec2* hammer_handles_rod = th_make_handles_arena(alloc,l->handles[mat_wood],2);

  hammer_brush = th_cacheBrush(wadname,l,"th1/models/hammer_rod.obj",hammer_handles_rod,hammer_mats,2,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER
   | TH_PRECACHE_SCALEUV,&hammer_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1.0,1.0),fn_createVec3(0,0,0));
   // r_scaleMeshUVs(&l->meshes[hammer_brush.id],fn_createVec2(0.2,0.2));

   /*
   *COLLISION DEBUGGER INITIALIZATION
   */

   int new_debug_points = 200;
   ls->debugger_collision = th_alloc(alloc,sizeof(th_Debugger));
   ls->debugger_collision->points_default = th_alloc(alloc,sizeof(fn_vec3)*new_debug_points);
   for (int i = 0; i < new_debug_points; i++) {
    ls->debugger_collision->points_default[i] = fn_createVec3(0,0,0);
   }

   ls->debugger_collision->point_count_default = new_debug_points;
   th_DebuggerInit(alloc,ls->debugger_collision,new_debug_points,&ls->debugger_collision->points_default,&ls->debugger_collision->point_count_default);
   th_setDefaultDebugger(ls->debugger_collision);
   th_RenderCommand debug_command_col;
   debug_command_col.offset = 0;
   debug_command_col.model_id = 0;
   debug_command_col.matcount = &ls->debugger_collision->count_allocated;
   debug_command_col.mats = &ls->debugger_collision->transforms;
   debug_command_col.stride = 1;

   int debug_count_col = new_debug_points;
   fn_vec2* debug_handles_col = th_make_handles_arena(alloc,l->handles[mat_plastic],debug_count_col);
   fn_mat4* debug_matrices_col = th_make_matrices_arena(alloc,fn_identityMat4(),debug_count_col);


   th_cacheBrushDefault(wadname,l,"th1/models/sphere.obj",debug_handles_col,debug_matrices_col,debug_count_col,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_NOCULLING | TH_RENDERCOMMAND,&debug_command_col,fn_createVec3s(0));


   /*
    * CENTIPEDE SUPER INITIALIZATION
    */
   th_CentiConfig centi_config;
   centi_config.scale = 1.5;
   centi_config.speed = 0.5;
   centi_config.spawn_speed = 0.5;
   centi_config.turn_rate = 3.14159*0.04;
   centi_config.gem_health = 100;

   ls->centipede_super = th_alloc(alloc,sizeof(th_CentipedeGroup));
   th_centipedeInitialize(alloc,ls->centipede_super,centi_count_super,centi_segments_super,centi_count_super/centi_segments_super,centi_spawn_times_super,centi_courses_super,&l->ls,centi_config);


   th_EntityCollisionEdict centipede_edict;
   centipede_edict.entities = ls->centipede_super->entities_gems;
   centipede_edict.entityCount= &ls->centipede_super->count;
   centipede_edict.grid = NULL;//&l.boidgroups[0].grid;
   centipede_edict.flags = TH_ENEMY;
   th_registerEntityGroup(centipede_edict);

   //mat_ribbed
   fn_vec2* centipede_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],centi_count_super);

   fn_mat4* centipede_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),centi_count_super);

   th_RenderCommand centipede_command;
   centipede_command.offset = 0;
   centipede_command.model_id = 0;
   centipede_command.matcount = &ls->centipede_super->count;
   centipede_command.mats = &ls->centipede_super->transforms_body;
   centipede_command.stride = 1;

   //th_BrushTuple segment = th_createBrush(&l,"th1/models/segment3.obj",centipede_handles,centipede_matrices,centi_count_super,NULL,TH_DYNAMC | TH_SHADOWCASTING ,NULL,fn_createVec3s(0));
   th_BrushTuple segment = th_cacheBrush(wadname,l,"th1/models/segment4.obj",centipede_handles,centipede_matrices,centi_count_super,NULL,TH_DYNAMC | TH_SHADOWCASTING  | TH_RENDERCOMMAND | TH_PRECACHE_TR | TH_PRECACHE_SCALEUV | TH_NOCENTER ,&centipede_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1.5,1.5),fn_createVec3(0,1,0));
   // r_scaleMeshUVs(&l->meshes[segment.id],fn_createVec2(1.5,1.5));
   // r_translateThorMesh(&l->meshes[segment.id],fn_createVec3(0,1,0));
   ls->centipede_super->frustum_data = l->culldata;
   ls->centipede_super->frustum_offset = segment.culldata_offset;

   th_RenderCommand gem_command;
   gem_command.offset = 0;
   gem_command.model_id = 0;
   gem_command.matcount = &ls->centipede_super->count;
   gem_command.mats = &ls->centipede_super->transforms_gems;
   gem_command.stride = 1;


   fn_vec2* gem_handles = th_make_handles_arena(alloc,l->handles[mat_plasma],centi_count_super);
   //th_BrushTuple gem = th_createBrush(&l,"th1/models/gem.obj",gem_handles,centipede_matrices,centi_count_super,NULL,TH_DYNAMC | TH_SHADOWCASTING ,NULL,fn_createVec3s(0));
   th_BrushTuple gem = th_cacheBrush(wadname,l,"th1/models/gem.obj",gem_handles,centipede_matrices,centi_count_super,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_PRECACHE_SCALEUV,&gem_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.5,0.5),fn_createVec3(0,1,0));
   // r_scaleMeshUVs(&l->meshes[gem.id],fn_createVec2(0.5,0.5));
   ls->centipede_super->frustum_offset_gem = gem.culldata_offset;


   fn_vec2* centi_legsa_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],centi_count_super*2);
   fn_mat4* centi_legsa_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),centi_count_super*2);
   th_RenderCommand centi_leg_a_command;
   centi_leg_a_command.offset = 0;
   centi_leg_a_command.model_id = 0;
   centi_leg_a_command.matcount = &l->ls.centipede_super->leg_count;
   centi_leg_a_command.mats = &l->ls.centipede_super->transforms_legs_a;
   centi_leg_a_command.stride = 1;

   th_BrushTuple centi_lega_brush = th_cacheBrushDefault(wadname,l,"th1/models/centilegs/leg_1.obj",centi_legsa_handles,centi_legsa_matrices,centi_count_super*2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&centi_leg_a_command,fn_createVec3s(0));


   th_RenderCommand centi_leg_b_command;
   centi_leg_b_command.offset = 0;
   centi_leg_b_command.model_id = 0;
   centi_leg_b_command.matcount = &l->ls.centipede_super->leg_count;
   centi_leg_b_command.mats = &l->ls.centipede_super->transforms_legs_b;
   centi_leg_b_command.stride = 1;

   th_BrushTuple centi_legb_brush = th_cacheBrushDefault(wadname,l,"th1/models/centilegs/leg_2.obj",centi_legsa_handles,centi_legsa_matrices,centi_count_super*2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&centi_leg_b_command,fn_createVec3s(0));

   /*
   * CENTIPEDE INITIALIZATION
   */
  centi_config.scale = 1.0;
  centi_config.speed = 0.8;
  centi_config.spawn_speed = 0.7;
  centi_config.turn_rate = 0.0;
  centi_config.gem_health = 12.0;

   ls->centipede = th_alloc(alloc,sizeof(th_CentipedeGroup));
   th_centipedeInitialize(alloc,ls->centipede,centi_count,centi_segments,centi_count/centi_segments,centi_spawn_times,centi_courses,&l->ls,centi_config);


   centipede_edict.entities = ls->centipede->entities_gems;
   centipede_edict.entityCount= &ls->centipede->count;
   centipede_edict.grid = NULL;//&l.boidgroups[0].grid;
   centipede_edict.flags = TH_ENEMY;
   th_registerEntityGroup(centipede_edict);

   //mat_ribbed
   centipede_handles = th_make_handles_arena(alloc,l->handles[mat_ribbed],centi_count);

   centipede_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),centi_count);


   centipede_command.offset = 0;
   centipede_command.model_id = 0;
   centipede_command.matcount = &ls->centipede->count;
   centipede_command.mats = &ls->centipede->transforms_body;
   centipede_command.stride = 1;

   //th_BrushTuple segment = th_createBrush(&l,"th1/models/segment3.obj",centipede_handles,centipede_matrices,centi_count,NULL,TH_DYNAMC | TH_SHADOWCASTING ,NULL,fn_createVec3s(0));
  segment = th_cacheBrush(wadname,l,"th1/models/segment6.obj",centipede_handles,centipede_matrices,centi_count,NULL,TH_DYNAMC | TH_SHADOWCASTING  | TH_RENDERCOMMAND | TH_PRECACHE_TR | TH_PRECACHE_SCALEUV | TH_NOCENTER ,&centipede_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(1.5,1.5),fn_createVec3(0,1,0));
   // r_scaleMeshUVs(&l->meshes[segment.id],fn_createVec2(1.5,1.5));
   // r_translateThorMesh(&l->meshes[segment.id],fn_createVec3(0,1,0));
   ls->centipede->frustum_data = l->culldata;
   ls->centipede->frustum_offset = segment.culldata_offset;


   gem_command.offset = 0;
   gem_command.model_id = 0;
   gem_command.matcount = &ls->centipede->count;
   gem_command.mats = &ls->centipede->transforms_gems;
   gem_command.stride = 1;


   gem_handles = th_make_handles_arena(alloc,l->handles[mat_plasma],centi_count);
   //th_BrushTuple gem = th_createBrush(&l,"th1/models/gem.obj",gem_handles,centipede_matrices,centi_count,NULL,TH_DYNAMC | TH_SHADOWCASTING ,NULL,fn_createVec3s(0));
   gem = th_cacheBrush(wadname,l,"th1/models/gem.obj",gem_handles,centipede_matrices,centi_count,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_PRECACHE_SCALEUV,&gem_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.5,0.5),fn_createVec3(0,1,0));
   // r_scaleMeshUVs(&l->meshes[gem.id],fn_createVec2(0.5,0.5));
   ls->centipede->frustum_offset_gem = gem.culldata_offset;



   centi_legsa_handles = th_make_handles_arena(alloc,l->handles[mat_scuffcopper],centi_count*2);
   centi_legsa_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),centi_count*2);

   centi_leg_a_command.offset = 0;
   centi_leg_a_command.model_id = 0;
   centi_leg_a_command.matcount = &l->ls.centipede->leg_count;
   centi_leg_a_command.mats = &l->ls.centipede->transforms_legs_a;
   centi_leg_a_command.stride = 1;

    centi_lega_brush = th_cacheBrushDefault(wadname,l,"th1/models/centilegs/leg_1.obj",centi_legsa_handles,centi_legsa_matrices,centi_count*2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&centi_leg_a_command,fn_createVec3s(0));



   centi_leg_b_command.offset = 0;
   centi_leg_b_command.model_id = 0;
   centi_leg_b_command.matcount = &l->ls.centipede->leg_count;
   centi_leg_b_command.mats = &l->ls.centipede->transforms_legs_b;
   centi_leg_b_command.stride = 1;

    centi_legb_brush = th_cacheBrushDefault(wadname,l,"th1/models/centilegs/leg_2.obj",centi_legsa_handles,centi_legsa_matrices,centi_count*2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&centi_leg_b_command,fn_createVec3s(0));


   /*
   *SKULL BOID INITIALIZATION
   */


   ls->boidgroups = th_alloc(alloc,sizeof(th_BoidGroup)*1);
   th_boidsInitialize(alloc,&ls->boidgroups[0],fn_createVec3s(6.4*5),200,boidscount,fn_createVec3s(35*1.25),&l->ls);
   //dim*dim*dim


//  fn_vec3 skoffset =  fn_createVec3(-415.155914, -918.689636, -62.155334);//fn_createVec3(-197.598053,-1216.638062,-37.804482);


   fn_vec2* skull_handles = th_make_handles_arena(alloc,l->handles[mat_jester],boidscount);
   fn_mat4* skull_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),boidscount);
   for (int i = 0; i < dim;i++)
   {
     for (int j = 0 ; j < dim;j++)
     {
       for (int k = 0 ; k < dim;k++)
       {
         fn_mat4 rotmat = fn_makerotate(fn_radians(90),fn_createVec3(1,0,0));
         skull_matrices[i + j*dim + k*dim*dim] = fn_multMat4(rotmat,fn_translatescale(fn_createVec3(i*60,-j*60,k*60),fn_createVec3s(10)));
         skull_handles[i + j*dim + k*dim*dim] = l->handles[mat_chrome];

         fn_vec3 p_boid = fn_addVec3(fn_multVec3s(fn_createVec3(i*60,-j*60,k*60),0.01),skoffset);
         int index = th_boidsSpawn(&ls->boidgroups[0],p_boid);
         ls->boidgroups[0].entities[index].aabb.position = p_boid;
         ls->boidgroups[0].entities[index].velocity = fn_normalizeVec3(fn_createVec3(i*60,-j*60,k*60));
         ls->boidgroups[0].entities[index].alive = true;
         ls->boidgroups[0].entities[index].impact = false;
         ls->boidgroups[0].transforms[index] = skull_matrices[index];
       }
     }
   }

   th_RenderCommand skull_command;
   skull_command.offset = 0;
   skull_command.model_id = 0;
   skull_command.matcount = &ls->boidgroups[0].boidscount;
   skull_command.mats = &ls->boidgroups[0].transforms;
   skull_command.stride = 1;

   th_BrushTuple skull_tuple = th_cacheBrushDefault(wadname,l,"th1/models/facetest1.obj",skull_handles,skull_matrices,boidscount,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&skull_command,fn_createVec3s(0));
   ls->boidgroups[0].frustum_data = l->culldata;
   ls->boidgroups[0].frustum_offset = skull_tuple.culldata_offset;


   fn_vec2* sheild_handles = th_make_handles_arena(alloc,l->handles[mat_gold],boidscount);
   fn_mat4* sheild_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),boidscount);

   th_RenderCommand sheild_command;
   sheild_command.offset = 0;
   sheild_command.model_id = 0;
   sheild_command.matcount = &ls->boidgroups[0].boidscount;
   sheild_command.mats = &ls->boidgroups[0].transforms_sheild;
   sheild_command.stride = 1;

   th_BrushTuple sheild_tuple = th_cacheBrushDefault(wadname,l,"th1/models/sheild2.obj",sheild_handles,sheild_matrices,boidscount,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER,&sheild_command,fn_createVec3s(0));
   ls->boidgroups[0].frustum_offset_sheild = sheild_tuple.culldata_offset;

   for (int k = 0 ; k < ls->boidgroups[0].boidscount;k++)
   {
     ls->boidgroups[0].frustum_data->skip_culling_flag[ls->boidgroups[0].frustum_offset_sheild + k] = true;
   }


   // fn_createGrid(alloc,&ls->boidgroups[0].grid,ls->boidgroups[0].entities,ls->boidgroups[0].boidscount,fn_createVec3s(130.56),200);
   fn_createGrid(alloc,&ls->boidgroups[0].grid,ls->boidgroups[0].entities,ls->boidgroups[0].boidscount,fn_createVec3s(130.56),200);

   th_EntityCollisionEdict skull_edict;
   skull_edict.entities = ls->boidgroups[0].entities;
   skull_edict.entityCount= &ls->boidgroups[0].boidscount;
   skull_edict.grid = &ls->boidgroups[0].grid;
   skull_edict.flags = TH_ENEMY | TH_BOID;
   th_registerEntityGroup(skull_edict);




   /*
   * PLASMA INITIALIZATION
   */
   l->ls.plasma = th_alloc(alloc,sizeof(th_PlasmaObject));
   th_plasmaInitialize(alloc,l->ls.plasma,2048,&l->ls);

   th_RenderCommand plasma_command;
   plasma_command.offset = 0;
   plasma_command.model_id = 0;
   plasma_command.matcount = &l->ls.plasma->entity_count;
   plasma_command.mats = &l->ls.plasma->transforms;
   plasma_command.stride = 1;
   fn_vec2* plasma_handles = th_make_handles_arena(alloc,l->handles[mat_brass_bullet],l->ls.plasma->entity_count);
   fn_mat4* plasma_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),l->ls.plasma->entity_count);
   th_BrushTuple plasma_brush = th_cacheBrushScale(wadname,l,"th1/models/bullet.obj",plasma_handles,plasma_matrices,l->ls.plasma->entity_count,NULL,TH_NOCULLING | TH_DYNAMC | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&plasma_command,fn_createVec3s(0),fn_createVec3(5,5,5));
   // r_scaleThorMesh(&l->meshes[plasma_brush.id],fn_createVec3(5,5,5));

   /*
   * SHOTGUN INITIALIZATION
   */

   l->ls.shotgun = th_alloc(alloc,sizeof(th_ShotgunObject));
   th_shotgunInitialize(alloc,l->ls.shotgun,512,&l->ls);

   th_RenderCommand shell_command;
   shell_command.offset = 0;
   shell_command.model_id = 0;
   shell_command.matcount = &l->ls.shotgun->entity_count;
   shell_command.mats = &l->ls.shotgun->transforms;
   shell_command.stride = 1;
   fn_vec2* shell_handles = th_make_handles_arena(alloc,l->handles[mat_brass_bullet],l->ls.shotgun->entity_count);
   fn_mat4* shell_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),l->ls.shotgun->entity_count);
   th_BrushTuple shell_brush = th_cacheBrushScale(wadname,l,"th1/models/bullet.obj",shell_handles,shell_matrices,l->ls.shotgun->entity_count,NULL,TH_NOCULLING | TH_DYNAMC | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&shell_command,fn_createVec3s(0),fn_createVec3(5,5,5));
   //r_scaleThorMesh(&l->meshes[shell_brush.id],fn_createVec3(5,5,5));


   /*
   *BRASS INITIALIZATION
   */
   l->ls.brass = th_alloc(alloc,sizeof(th_BrassObject));
   th_brassInit(alloc,l->ls.brass,40,&l->ls);

   l->ls.shotbrass = th_alloc(alloc,sizeof(th_BrassObject));
   th_brassInit(alloc,l->ls.shotbrass,40,&l->ls);

   th_RenderCommand brass_command;
   brass_command.offset = 0;
   brass_command.model_id = 0;
   brass_command.matcount = &l->ls.brass->entity_count;
   brass_command.mats = &l->ls.brass->transforms;
   brass_command.stride = 1;
   fn_vec2* brass_handles = th_make_handles_arena(alloc,l->handles[mat_brass],40);
   fn_mat4* brass_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),40);
   th_BrushTuple brass_brush = th_cacheBrushScale(wadname,l,"th1/models/casing_smooth.obj",brass_handles,brass_matrices,40,NULL,TH_NOCULLING | TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&brass_command,fn_createVec3s(0),fn_createVec3(2.5,2.5,2.5));
   //r_scaleThorMesh(&l->meshes[brass_brush.id],fn_createVec3(2.5,2.5,2.5));

   th_RenderCommand shotbrass_command;
   shotbrass_command.offset = 0;
   shotbrass_command.model_id = 0;
   shotbrass_command.matcount = &l->ls.shotbrass->entity_count;
   shotbrass_command.mats = &l->ls.shotbrass->transforms;
   shotbrass_command.stride = 1;
   fn_vec2* shotbrass_handles = th_make_handles_arena(alloc,l->handles[mat_brass],40);
   fn_mat4* shotbrass_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),40);
   th_BrushTuple shotbrass_brush = th_cacheBrushScale(wadname,l,"th1/models/shotshell2.obj",shotbrass_handles,shotbrass_matrices,40,NULL,TH_NOCULLING | TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&shotbrass_command,fn_createVec3s(0),fn_createVec3(3.4,3.4,3.4));
   // r_scaleThorMesh(&l->meshes[shotbrass_brush.id],fn_createVec3(3.4,3.4,3.4));


   /*
   *DYNAMIC GEM INITIALIZATION
   */

   int dynamic_gems_alloc = 500;
   l->ls.gems = th_alloc(alloc,sizeof(th_GemObject));
   th_gemInit(alloc,l->ls.gems,dynamic_gems_alloc,&l->ls);

   th_RenderCommand dynamic_gem_command;
   dynamic_gem_command.offset = 0;
   dynamic_gem_command.model_id = 0;
   dynamic_gem_command.matcount = &l->ls.gems->entity_count;
   dynamic_gem_command.mats = &l->ls.gems->transforms;
   dynamic_gem_command.stride = 1;

   fn_mat4* dynamic_gem_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),dynamic_gems_alloc);
   fn_vec2* dynamic_gem_handles = th_make_handles_arena(alloc,l->handles[mat_plasma],dynamic_gems_alloc);
   th_BrushTuple dynamic_gem = th_cacheBrushUVScale(wadname,l,"th1/models/gem.obj",dynamic_gem_handles,dynamic_gem_matrices,dynamic_gems_alloc,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND,&dynamic_gem_command,fn_createVec3s(0),fn_createVec2(0.5,0.5));
   // r_scaleMeshUVs(&l->meshes[dynamic_gem.id],fn_createVec2(0.5,0.5));
   l->ls.gems->frustum_data = l->culldata;
   l->ls.gems->frustum_offset = dynamic_gem.culldata_offset;


   /*
    * BOID GIB INITIALIZATION
    */

   ls->boid_gibs = th_alloc(alloc,sizeof(th_GibCollection));
   th_gibCreateInstances(alloc,ls->boid_gibs,12);

   int boidgib_count = 128;
   th_gibInit(alloc,ls->boid_gibs,boidgib_count,&l->ls);
   fn_vec2* boidgib_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],boidgib_count);
   fn_mat4* boidgib_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),boidgib_count);
   const char* meshpath_boidgib[] = {
     "th1/models/boidgibs/Sphere_cell.obj",
     "th1/models/boidgibs/Sphere_cell_001.obj",
     "th1/models/boidgibs/Sphere_cell_002.obj",
     "th1/models/boidgibs/Sphere_cell_003.obj",
     "th1/models/boidgibs/Sphere_cell_004.obj",
     "th1/models/boidgibs/Sphere_cell_005.obj",
     "th1/models/boidgibs/Sphere_cell_006.obj",
     "th1/models/boidgibs/Sphere_cell_007.obj",
     "th1/models/boidgibs/Sphere_cell_008.obj",
     "th1/models/boidgibs/Sphere_cell_009.obj",
     "th1/models/boidgibs/Sphere_cell_010.obj",
     "th1/models/boidgibs/Sphere_cell_011.obj"
   };

   for (int k = 0 ; k < 12;k++)
   {
     th_RenderCommand boidgib_command;
     boidgib_command.offset = 0;
     boidgib_command.model_id = 0;
     boidgib_command.matcount = &ls->boid_gibs->instances[k].entity_count;
     boidgib_command.mats = &ls->boid_gibs->instances[k].transforms;
     boidgib_command.stride = 1;


     th_BrushTuple boidgib_tuple = th_cacheBrushDefault(wadname,l,meshpath_boidgib[k],boidgib_handles,boidgib_matrices,ls->boid_gibs->instances[k].entity_count,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_NOCENTER | TH_RENDERCOMMAND | TH_NOCULLING,&boidgib_command,fn_createVec3s(0));
   }


   /*
   *ROCKET INITIALIZATION
   */
   int rocket_count = 128;
   ls->rocket = th_alloc(alloc,sizeof(th_RocketObject));
   th_rocketInit(alloc,ls->rocket,rocket_count,l->light_query,&l->ls);
   th_EntityCollisionEdict rocket_edict;
   rocket_edict.entities = ls->rocket->entities;
   rocket_edict.entityCount= &ls->rocket->entity_count;
   rocket_edict.grid = NULL;//&ls->boidgroups[0].grid;
   rocket_edict.flags = TH_ENEMY_ROCKET;
   th_registerEntityGroup(rocket_edict);

   th_RenderCommand rocket_command;
   rocket_command.offset = 0;
   rocket_command.model_id = 0;
   rocket_command.matcount = &ls->rocket->entity_count;
   rocket_command.mats = &ls->rocket->transforms;
   rocket_command.stride = 1;
   fn_vec2* rocket_handles = th_make_handles_arena(alloc,l->handles[mat_gold],rocket_count);
   fn_mat4* rocket_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),rocket_count);
   th_BrushTuple rocket_brush = th_cacheBrushScale(wadname,l,"th1/models/rocket.obj",rocket_handles,rocket_matrices,rocket_count,NULL,  TH_DYNAMC | TH_NOCULLING | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&rocket_command,fn_createVec3s(0),fn_createVec3s(25));
   // r_scaleThorMesh(&l->meshes[rocket_brush.id],fn_createVec3s(25));

   /*
   *EYEBALL SUPER INITIALIZATION
   */


    fn_vec3* eyeball_pos_super = th_alloc(alloc,sizeof(fn_vec3)*eyeball_count);
    for (int p = 0; p < eyeball_count; p++) {
      eyeball_pos_super[p] = fn_createVec3(0,0,0);
    }

   ls->eyeball_super = th_alloc(alloc,sizeof(th_EyeballGroup));
   th_eyeballInitialize(alloc,ls->eyeball_super,eyeball_count,eyeball_pos_super,&l->ls,true);




   th_EntityCollisionEdict eyeball_edict;
   eyeball_edict.entities = ls->eyeball_super->entities;
   eyeball_edict.entityCount= &ls->eyeball_super->count;
   eyeball_edict.grid = NULL;//&ls->boidgroups[0].grid;
   eyeball_edict.flags = TH_ENEMY | TH_EYEBALL;
   th_registerEntityGroup(eyeball_edict);

   th_RenderCommand eye_gib_a_command;
   eye_gib_a_command.offset = 0;
   eye_gib_a_command.model_id = 0;
   eye_gib_a_command.matcount = &ls->eyeball_super->count;
   eye_gib_a_command.mats = &ls->eyeball_super->transforms_eye_gib_a;
   eye_gib_a_command.stride = 1;
   fn_vec2* eye_gib_a_handles = th_make_handles_arena(alloc,l->handles[mat_eyeball_dead],eyeball_count);
   fn_mat4* eye_gib_a_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
   th_BrushTuple eye_gib_a_brush = th_cacheBrushScale(wadname,l,"th1/models/eye_part_a_2.obj",eye_gib_a_handles,eye_gib_a_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER,&eye_gib_a_command,fn_createVec3s(0),fn_createVec3s(25));
   // r_scaleThorMesh(&l->meshes[eye_gib_a_brush.id],fn_createVec3s(25));

   th_RenderCommand eye_gib_b_command;
   eye_gib_b_command.offset = 0;
   eye_gib_b_command.model_id = 0;
   eye_gib_b_command.matcount = &ls->eyeball_super->count;
   eye_gib_b_command.mats = &ls->eyeball_super->transforms_eye_gib_b;
   eye_gib_b_command.stride = 1;
   fn_vec2* eye_gib_b_handles = th_make_handles_arena(alloc,l->handles[mat_eyeball_dead],eyeball_count);
   fn_mat4* eye_gib_b_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
   th_BrushTuple eye_gib_b_brush = th_cacheBrushScale(wadname,l,"th1/models/epe_part_b_2.obj",eye_gib_b_handles,eye_gib_b_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER,&eye_gib_b_command,fn_createVec3s(0),fn_createVec3s(25));
   // r_scaleThorMesh(&l->meshes[eye_gib_b_brush.id],fn_createVec3s(25));

   th_RenderCommand eyeball_command;
   eyeball_command.offset = 0;
   eyeball_command.model_id = 0;
   eyeball_command.matcount = &ls->eyeball_super->count;
   eyeball_command.mats = &ls->eyeball_super->transforms;
   eyeball_command.stride = 1;
   fn_vec2* eyeball_handles = th_make_handles_arena(alloc,l->handles[mat_eye_blue],eyeball_count);
   fn_mat4* eyeball_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
   th_BrushTuple eyeball_brush = th_cacheBrushScale(wadname,l,"th1/models/eyeballcorrected.obj",eyeball_handles,eyeball_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND,&eyeball_command,fn_createVec3s(0),fn_createVec3s(25));
   // r_scaleThorMesh(&l->meshes[eyeball_brush.id],fn_createVec3s(25));

   int mat_eyering = mat_chrome_red;
   th_RenderCommand ring_command;
   ring_command.offset = 0;
   ring_command.model_id = 0;
   ring_command.matcount = &ls->eyeball_super->count;
   ring_command.mats = &ls->eyeball_super->transforms_ring_a;
   ring_command.stride = 1;
   fn_vec2* ring_handles = th_make_handles_arena(alloc,l->handles[mat_eyering],eyeball_count);
   fn_mat4* ring_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
   th_BrushTuple ring_brush = th_cacheBrushScale(wadname,l,"th1/models/ring_deformed.obj",ring_handles,ring_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&ring_command,fn_createVec3s(0),fn_createVec3s(50));
   // r_scaleThorMesh(&l->meshes[ring_brush.id],fn_createVec3s(50));

   th_RenderCommand ringb_command;
   ringb_command.offset = 0;
   ringb_command.model_id = 0;
   ringb_command.matcount = &ls->eyeball_super->count;
   ringb_command.mats = &ls->eyeball_super->transforms_ring_b;
   ringb_command.stride = 1;
   fn_vec2* ringb_handles = th_make_handles_arena(alloc,l->handles[mat_eyering],eyeball_count);
   fn_mat4* ringb_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
   th_BrushTuple ringb_brush = th_cacheBrushScale(wadname,l,"th1/models/ring_deformed_duplicate.obj",ringb_handles,ringb_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&ringb_command,fn_createVec3s(0),fn_createVec3s(60));
   // r_scaleThorMesh(&l->meshes[ringb_brush.id],fn_createVec3s(60));

   th_RenderCommand pupil_command;
   pupil_command.offset = 0;
   pupil_command.model_id = 0;
   pupil_command.matcount = &ls->eyeball_super->count;
   pupil_command.mats = &ls->eyeball_super->transforms_pupil;
   pupil_command.stride = 1;
   fn_vec2* pupil_handles = th_make_handles_arena(alloc,l->handles[mat_black],eyeball_count);
   fn_mat4* pupil_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
   th_BrushTuple pupil_brush = th_cacheBrushScale(wadname,l,"th1/models/pupil3.obj",pupil_handles,pupil_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_NOCENTER | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND,&pupil_command,fn_createVec3s(0),fn_createVec3s(1));


   fn_vec3 pupil_center_super = th_getCenterVerts(&l->meshes[pupil_brush.id]);


   ls->eyeball_super->pupil_center = pupil_center_super;

   ls->eyeball_super->frustum_data = l->culldata;
   ls->eyeball_super->frustum_offset = eyeball_brush.culldata_offset;
   ls->eyeball_super->frustum_offset_ring_a = ring_brush.culldata_offset;
   ls->eyeball_super->frustum_offset_ring_b = ringb_brush.culldata_offset;
   ls->eyeball_super->frustum_offset_eye_gib_a = eye_gib_a_brush.culldata_offset;
   ls->eyeball_super->frustum_offset_eye_gib_b = eye_gib_b_brush.culldata_offset;


   /*
    * EYEBALL INITIALIZATION
    */


   fn_vec3* eyeball_pos = th_alloc(alloc,sizeof(fn_vec3)*eyeball_count);
   for (int p = 0; p < eyeball_count; p++) {
     eyeball_pos[p] = fn_createVec3(0,0,0);
   }

   ls->eyeball = th_alloc(alloc,sizeof(th_EyeballGroup));
   th_eyeballInitialize(alloc,ls->eyeball,eyeball_count,eyeball_pos,&l->ls,false);





   eyeball_edict.entities = ls->eyeball->entities;
   eyeball_edict.entityCount= &ls->eyeball->count;
   eyeball_edict.grid = NULL;//&ls->boidgroups[0].grid;
   eyeball_edict.flags = TH_ENEMY | TH_EYEBALL;
   th_registerEntityGroup(eyeball_edict);


   eye_gib_a_command.offset = 0;
   eye_gib_a_command.model_id = 0;
   eye_gib_a_command.matcount = &ls->eyeball->count;
   eye_gib_a_command.mats = &ls->eyeball->transforms_eye_gib_a;
   eye_gib_a_command.stride = 1;
   eye_gib_a_handles = th_make_handles_arena(alloc,l->handles[mat_eyeball_dead],eyeball_count);
   eye_gib_a_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
    eye_gib_a_brush = th_cacheBrushScale(wadname,l,"th1/models/eye_part_a_2.obj",eye_gib_a_handles,eye_gib_a_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER,&eye_gib_a_command,fn_createVec3s(0),fn_createVec3s(25));
   // r_scaleThorMesh(&l->meshes[eye_gib_a_brush.id],fn_createVec3s(25));


   eye_gib_b_command.offset = 0;
   eye_gib_b_command.model_id = 0;
   eye_gib_b_command.matcount = &ls->eyeball->count;
   eye_gib_b_command.mats = &ls->eyeball->transforms_eye_gib_b;
   eye_gib_b_command.stride = 1;
   eye_gib_b_handles = th_make_handles_arena(alloc,l->handles[mat_eyeball_dead],eyeball_count);
   eye_gib_b_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
    eye_gib_b_brush = th_cacheBrushScale(wadname,l,"th1/models/epe_part_b_2.obj",eye_gib_b_handles,eye_gib_b_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCENTER,&eye_gib_b_command,fn_createVec3s(0),fn_createVec3s(25));
   // r_scaleThorMesh(&l->meshes[eye_gib_b_brush.id],fn_createVec3s(25));


   eyeball_command.offset = 0;
   eyeball_command.model_id = 0;
   eyeball_command.matcount = &ls->eyeball->count;
   eyeball_command.mats = &ls->eyeball->transforms;
   eyeball_command.stride = 1;
   eyeball_handles = th_make_handles_arena(alloc,l->handles[mat_eye],eyeball_count);
   eyeball_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
    eyeball_brush = th_cacheBrushScale(wadname,l,"th1/models/eyeballcorrected.obj",eyeball_handles,eyeball_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCENTER | TH_RENDERCOMMAND,&eyeball_command,fn_createVec3s(0),fn_createVec3s(25));
   // r_scaleThorMesh(&l->meshes[eyeball_brush.id],fn_createVec3s(25));

   mat_eyering = mat_gold;

   ring_command.offset = 0;
   ring_command.model_id = 0;
   ring_command.matcount = &ls->eyeball->count;
   ring_command.mats = &ls->eyeball->transforms_ring_a;
   ring_command.stride = 1;
   ring_handles = th_make_handles_arena(alloc,l->handles[mat_eyering],eyeball_count);
   ring_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
    ring_brush = th_cacheBrushScale(wadname,l,"th1/models/ring.obj",ring_handles,ring_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&ring_command,fn_createVec3s(0),fn_createVec3s(50));
   // r_scaleThorMesh(&l->meshes[ring_brush.id],fn_createVec3s(50));

  //ringb needs a different name for cache purposes
   ringb_command.offset = 0;
   ringb_command.model_id = 0;
   ringb_command.matcount = &ls->eyeball->count;
   ringb_command.mats = &ls->eyeball->transforms_ring_b;
   ringb_command.stride = 1;
   ringb_handles = th_make_handles_arena(alloc,l->handles[mat_eyering],eyeball_count);
   ringb_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
    ringb_brush = th_cacheBrushScale(wadname,l,"th1/models/ring_duplicate.obj",ringb_handles,ringb_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NORMALIZEBRUSH | TH_RENDERCOMMAND,&ringb_command,fn_createVec3s(0),fn_createVec3s(60));




    pupil_command.offset = 0;
    pupil_command.model_id = 0;
    pupil_command.matcount = &ls->eyeball->count;
    pupil_command.mats = &ls->eyeball->transforms_pupil;
    pupil_command.stride = 1;
     pupil_handles = th_make_handles_arena(alloc,l->handles[mat_black],eyeball_count);
    pupil_matrices = th_make_matrices_arena(alloc,fn_makescale(fn_createVec3s(0)),eyeball_count);
     pupil_brush = th_cacheBrushScale(wadname,l,"th1/models/pupil3.obj",pupil_handles,pupil_matrices,eyeball_count,NULL,  TH_DYNAMC | TH_NOCENTER | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND,&pupil_command,fn_createVec3s(0),fn_createVec3s(1));


    fn_vec3 pupil_center = th_getCenterVerts(&l->meshes[pupil_brush.id]);


   ls->eyeball->pupil_center = pupil_center;
   ls->eyeball->frustum_data = l->culldata;
   ls->eyeball->frustum_offset = eyeball_brush.culldata_offset;
   ls->eyeball->frustum_offset_ring_a = ring_brush.culldata_offset;
   ls->eyeball->frustum_offset_ring_b = ringb_brush.culldata_offset;
   ls->eyeball->frustum_offset_eye_gib_a = eye_gib_a_brush.culldata_offset;
   ls->eyeball->frustum_offset_eye_gib_b = eye_gib_b_brush.culldata_offset;

   l->ls.shotgun->lights = l->pointlights;
   l->ls.plasma->lights = l->pointlights;

   /*
    HORSE INITIALIZATION
   */


   int legs_per_count = 12;
   int legs_per_count_div2 = legs_per_count/2;


  if (horse_count == 0)
  {
    l->ls.horse = NULL;
  }
  else
  {
    l->ls.horse = th_alloc(alloc,sizeof(th_HorseGroup));



    th_horseInitialize(alloc,l->ls.horse,horse_count,horse_positions,horse_times,&l->ls);

    for (int i = 0; i < horse_count; i++) {
      th_horseSetCourse(l->ls.horse,i,horse_courses[i],horse_courses_count[i]);
      if (horse_easymode[i])
      {
        th_horseSetEasyMode(l->ls.horse,i);
      }
    }

    th_horsePositionPlugs(l->ls.horse);





    th_EntityCollisionEdict horse_edict;
    horse_edict.entities = ls->horse->entities_gems;
    horse_edict.entityCount= &l->ls.horse->gem_count;
    horse_edict.grid = NULL;//&l.boidgroups[0].grid;
    horse_edict.flags = TH_ENEMY | TH_CAN_KILL_PLAYER;
    th_registerEntityGroup(horse_edict);

    th_EntityCollisionEdict horse_leg_edict;
    horse_leg_edict.entities = ls->horse->entities_legs;
    horse_leg_edict.entityCount= &l->ls.horse->leg_count;
    horse_leg_edict.grid = NULL;
    horse_leg_edict.flags = TH_ENEMY;
    th_registerEntityGroup(horse_leg_edict);

    // fn_vec3 horse_pos = ;//-TH_PI/2.0
    // fn_mat4 horsemat = fn_translaterotatescale(horse_pos,0,fn_createVec3(0,1,0),fn_createVec3s(10));
    fn_vec2* horse_handles = th_make_handles_arena(alloc,l->handles[mat_gold],horse_count);
    fn_mat4* horse_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),horse_count);

    fn_vec2* horse_handles_gear = th_make_handles_arena(alloc,l->handles[mat_chrome],horse_count);

    fn_vec2* horse_handles_tubing = th_make_handles_arena(alloc,l->handles[mat_scuffcopper],horse_count);

    for (int i = 0; i < horse_count; i++) {
      if (horse_easymode[i])
      {
        horse_handles[i] = l->handles[mat_bronze];
        horse_handles_gear[i] = l->handles[mat_gold];
        horse_handles_tubing[i] = l->handles[mat_chrome];
      }
    }


    th_RenderCommand horse_command;
    horse_command.offset = 0;
    horse_command.model_id = 0;
    horse_command.matcount = &l->ls.horse->count;
    horse_command.mats = &l->ls.horse->transforms;
    horse_command.stride = 1;

    th_BrushTuple horse_brush = th_cacheBrushDefault(wadname,l,"th1/models/horse/body.obj",horse_handles,horse_matrices,horse_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_command,fn_createVec3s(0));



    th_BrushTuple horse_brush_tubing = th_cacheBrushDefault(wadname,l,"th1/models/horse/bodytubing.obj",horse_handles_tubing,horse_matrices,horse_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_command,fn_createVec3s(0));


    horse_handles = th_make_handles_arena(alloc,l->handles[mat_peelingred],horse_count);

    th_BrushTuple horse_brush_tanks = th_cacheBrushDefault(wadname,l,"th1/models/horse/body_tanks.obj",horse_handles,horse_matrices,horse_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_command,fn_createVec3s(0));

    horse_handles = th_make_handles_arena(alloc,l->handles[mat_peelingred],horse_count);


    th_RenderCommand horse_hatch_command;
    horse_hatch_command.offset = 0;
    horse_hatch_command.model_id = 0;
    horse_hatch_command.matcount = &l->ls.horse->count;
    horse_hatch_command.mats = &l->ls.horse->hatch_transforms;
    horse_hatch_command.stride = 1;

    th_BrushTuple horse_brush_hatch = th_cacheBrushDefault(wadname,l,"th1/models/horse/hatch.obj",horse_handles,horse_matrices,horse_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_hatch_command,fn_createVec3s(0));


    horse_handles = th_make_handles_arena(alloc,l->handles[mat_rubberseal],horse_count);

    th_BrushTuple horse_brush_hatch_trim = th_cacheBrushDefault(wadname,l,"th1/models/horse/hatch_trim.obj",horse_handles,horse_matrices,horse_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_hatch_command,fn_createVec3s(0));


    fn_vec3 horsedoor_center = th_getCenterVerts(&l->meshes[horse_brush_hatch.id]);
    fn_vec3 horsedoor_max,horsedoor_min;
    th_getMinMaxVerts(&l->meshes[horse_brush_hatch.id],&horsedoor_min,&horsedoor_max);

    l->ls.horse->door_hinge = fn_createVec3(horsedoor_center.x,horsedoor_max.y,horsedoor_min.z);




    th_BrushTuple horse_brush_hatch_gears = th_cacheBrushDefault(wadname,l,"th1/models/horse/gears.obj",horse_handles_gear,horse_matrices,horse_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_hatch_command,fn_createVec3s(0));


    fn_vec2* horse_legs_handles = th_make_handles_arena(alloc,l->handles[mat_gold],horse_count*legs_per_count_div2);
    fn_mat4* horse_legs_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),horse_count*legs_per_count_div2);

    for (int i = 0; i < horse_count; i++) {
      if (horse_easymode[i])
      {
        for (int j = 0; j < legs_per_count_div2; j++) {
          horse_legs_handles[i*legs_per_count_div2 + j] = l->handles[mat_bronze];
        }
      }
    }







    th_RenderCommand horse_legs_command_lower;
    horse_legs_command_lower.offset = 0;
    horse_legs_command_lower.model_id = 0;
    horse_legs_command_lower.matcount = &l->ls.horse->leg_count_div2;
    horse_legs_command_lower.mats = &l->ls.horse->transforms_legs_lower;
    horse_legs_command_lower.stride = 1;

    th_BrushTuple horse_legs_brush_lower = th_cacheBrushDefault(wadname,l,"th1/models/horse/lowerleg.obj",horse_legs_handles,horse_legs_matrices,horse_count*legs_per_count_div2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_legs_command_lower,fn_createVec3s(0));


    horse_legs_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],horse_count*legs_per_count_div2);

    th_RenderCommand horse_legs_command;
    horse_legs_command.offset = 0;
    horse_legs_command.model_id = 0;
    horse_legs_command.matcount = &l->ls.horse->leg_count_div2;
    horse_legs_command.mats = &l->ls.horse->transforms_legs_upper;
    horse_legs_command.stride = 1;

    th_BrushTuple horse_legs_brush = th_cacheBrushDefault(wadname,l,"th1/models/horse/upperleg.obj",horse_legs_handles,horse_legs_matrices,horse_count*legs_per_count_div2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_legs_command,fn_createVec3s(0));



    horse_legs_handles = th_make_handles_arena(alloc,l->handles[mat_rubberseal],horse_count*legs_per_count_div2);
    th_BrushTuple horse_legs_brush_lower_contact = th_cacheBrushDefault(wadname,l,"th1/models/horse/lowerleg_contact.obj",horse_legs_handles,horse_legs_matrices,horse_count*legs_per_count_div2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_legs_command_lower,fn_createVec3s(0));

    horse_legs_handles = th_make_handles_arena(alloc,l->handles[mat_peelingred],horse_count*legs_per_count_div2);
    th_BrushTuple horse_legs_brush_lower_tubes = th_cacheBrushDefault(wadname,l,"th1/models/horse/lowerleg_tubes.obj",horse_legs_handles,horse_legs_matrices,horse_count*legs_per_count_div2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_legs_command_lower,fn_createVec3s(0));


    //horse_legs_handles = th_make_handles_arena(alloc,l->handles[mat_peelingred],horse_count*legs_per_count_div2);
    th_BrushTuple horse_legs_brush_spring = th_cacheBrushDefault(wadname,l,"th1/models/horse/upperleg_spring.obj",horse_legs_handles,horse_legs_matrices,horse_count*legs_per_count_div2,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&horse_legs_command,fn_createVec3s(0));


    th_RenderCommand gem_command_horse;
    gem_command_horse.offset = 0;
    gem_command_horse.model_id = 0;
    gem_command_horse.matcount = &l->ls.horse->gem_count;
    gem_command_horse.mats = &l->ls.horse->transforms_gems;
    gem_command_horse.stride = 1;


    fn_vec2* gem_handles_horse = th_make_handles_arena(alloc,l->handles[mat_plasma],horse_count*6);
    fn_mat4* horse_gems_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),horse_count*6);

    th_BrushTuple gem_horse = th_cacheBrush(wadname,l,"th1/models/gem.obj",gem_handles_horse,horse_gems_matrices,horse_count*6,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_PRECACHE_SCALEUV | TH_NOCULLING,&gem_command_horse,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.5,0.5),fn_createVec3(0,1,0));


    /*
     * HORSE WIRES INIT
     */



    int wire_bundle = l->ls.horse->num_wire_bundles*4;

    fn_vec3 starts[4];
    fn_vec3 mids[4];
    fn_vec3 mid2s[4];
    fn_vec3 ends[4];
    starts[0] = fn_createVec3(0,100,0);
    starts[1] = fn_createVec3(0,100,14*2);
    starts[2] = fn_createVec3(0,100,14*4);
    starts[3] = fn_createVec3(0,100,14*6);

    mids[0] = fn_createVec3(0,-200,0);
    mids[1] = fn_createVec3(0,-200,14*2);
    mids[2] = fn_createVec3(0,-200,14*4);
    mids[3] = fn_createVec3(0,-200,14*6);

    mid2s[0] = fn_createVec3(-200,-400,0);
    mid2s[1] = fn_createVec3(-200,-400,14*2);
    mid2s[2] = fn_createVec3(-200,-400,14*4);
    mid2s[3] = fn_createVec3(-200,-400,14*6);

    ends[0] = fn_createVec3(-300,-400,0);
    ends[1] = fn_createVec3(-300,-400,14*2);
    ends[2] = fn_createVec3(-300,-400,14*4);
    ends[3] = fn_createVec3(-300,-400,14*6);

    th_GpuData* wire_datas = th_alloc(alloc,sizeof(th_GpuData)*l->ls.horse->num_wire_bundles*4);

    for (int i = 0 ; i < l->ls.horse->num_wire_bundles;i++ )
    {
      th_GpuData* wire_datas_i = th_generateWireBundle(alloc,starts,mids,mid2s,ends,4,fn_createVec3(0,-1,0),fn_createVec3(-1,0,0),fn_createVec3(1,0,0),1.0,NULL);
      memcpy(&wire_datas[i*4],wire_datas_i,sizeof(th_GpuData)*4);
    }


    th_GpuDataOffsets wire_offsets = th_mergeGpuData(alloc,wire_datas,l->ls.horse->num_wire_bundles*4,TH_INCREMENT_INDICES);
    wire_offsets.data.instancecount = 1;




    fn_vec2* wire_handles = th_make_handles_arena(alloc,l->handles[mat_blue_wire],wire_bundle);

    for (int i = 0;i < wire_bundle;i += 4 )
    {
      wire_handles[i + 1] = l->handles[mat_black_wire];
      wire_handles[i + 2] = l->handles[mat_pink_wire];
      wire_handles[i + 3] = l->handles[mat_green_wire];
    }



    fn_mat4* wire_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),wire_bundle);
    // wire_matrices[1] = fn_maketranslate(fn_createVec3(0,0,14*2));
    // wire_matrices[2] = fn_maketranslate(fn_createVec3(0,0,14*4));
    // wire_matrices[3] = fn_maketranslate(fn_createVec3(0,0,14*6));

    int* mcount  = th_alloc(alloc,sizeof(int));
    *mcount = 1;

    fn_mat4** wire_matptr = th_alloc(alloc,sizeof(fn_mat4*));
    *wire_matptr = wire_matrices;

    th_Vertex** wire_backing = th_alloc(alloc,sizeof(th_Vertex*));
    *wire_backing = wire_offsets.data.verts;


    th_RenderCommand wire_command;
    wire_command.offset = 0;
    wire_command.model_id = 0;
    wire_command.matcount = mcount;
    wire_command.mats = wire_matptr;
    wire_command.stride = 1;

    l->ls.horse->wire_verts_count = wire_offsets.data.vertcount;
    l->ls.horse->wire_verts = th_alloc(alloc,sizeof(th_Vertex)*l->ls.horse->wire_verts_count);

    wire_command.backing_buffer = &l->ls.horse->wire_verts;
    wire_command.num_verts = l->ls.horse->wire_verts_count;
    wire_command.elements_model = wire_datas[0].indicecount;
    wire_command.num_models = wire_bundle;

    th_BrushTuple wire_brush = th_createBrush(l ,"wire",wire_handles,wire_matrices,wire_bundle,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_STREAM_DRAW ,&wire_command,fn_createVec3s(1),&wire_offsets.data);

    fn_vec2* backplug_handles = th_make_handles_arena(alloc,l->handles[mat_plugblock],l->ls.horse->num_wire_bundles);
    fn_mat4* backplug_mats = th_make_matrices_arena(alloc,fn_identityMat4(),l->ls.horse->num_wire_bundles);

    //backplug_mats[0] = fn_translaterotatescale(fn_createVec3(-300 - 40,-400,40),fn_radians(90.0),fn_createVec3(0,1,0),fn_createVec3s(10));

    // mcount  = th_alloc(alloc,sizeof(int));
    // *mcount = 1;
    //
    // wire_matptr = th_alloc(alloc,sizeof(fn_mat4*));
    // *wire_matptr = backplug_mats;

    th_RenderCommand backplug_command;
    backplug_command.offset = 0;
    backplug_command.model_id = 0;
    backplug_command.matcount = &l->ls.horse->connector_count;//mcount;
    backplug_command.mats = &l->ls.horse->connector_mats;
    backplug_command.stride = 1;

    th_BrushTuple backplug_base = th_cacheBrush(wadname,l,"th1/models/horse/backplug_base.obj",backplug_handles,backplug_mats,l->ls.horse->num_wire_bundles,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER,&backplug_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.5,0.5),fn_createVec3(0,1,0));


    backplug_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],l->ls.horse->num_wire_bundles);

    th_BrushTuple backplug_pins = th_cacheBrush(wadname,l,"th1/models/horse/backplug_pins.obj",backplug_handles,backplug_mats,l->ls.horse->num_wire_bundles,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING | TH_NOCENTER,&backplug_command,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.5,0.5),fn_createVec3(0,1,0));


    /*HORSE FLUID INIT*/

    th_GpuData* lmeshes = th_alloc(alloc,sizeof(th_GpuData)*horse_count);

    for (int i = 0 ; i < horse_count;i++)
    {
      lmeshes[i] = th_generateLiquidMesh(alloc,&l->ls.horse->fsim[i],fn_identityMat4());
    }

    th_GpuDataOffsets liquid_offsets = th_mergeGpuData(alloc,lmeshes,horse_count,TH_INCREMENT_INDICES);
    liquid_offsets.data.instancecount = 1;



    fn_vec2* liquid_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],horse_count);




    fn_mat4* liquid_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),horse_count);


    mcount  = th_alloc(alloc,sizeof(int));
    *mcount = horse_count;

    fn_mat4** liquid_mat4 = th_alloc(alloc,sizeof(fn_mat4*));
    *liquid_mat4 = liquid_matrices;

    // th_Vertex** wire_backing = th_alloc(alloc,sizeof(th_Vertex*));
    // *wire_backing = lmesh.verts;


    th_RenderCommand liquid_command;
    liquid_command.offset = 0;
    liquid_command.model_id = 0;
    liquid_command.matcount = mcount;
    liquid_command.mats = liquid_mat4;
    liquid_command.stride = 1;

    l->ls.horse->liquid_vert_count = liquid_offsets.data.vertcount;
    l->ls.horse->liquid_verts = th_alloc(alloc,sizeof(th_Vertex)*l->ls.horse->liquid_vert_count);
    memcpy(l->ls.horse->liquid_verts,liquid_offsets.data.verts,sizeof(th_Vertex)*liquid_offsets.data.vertcount);

    liquid_command.backing_buffer = &l->ls.horse->liquid_verts;
    liquid_command.num_verts = l->ls.horse->liquid_vert_count;
    liquid_command.elements_model = lmeshes[0].indicecount;
    liquid_command.num_models = horse_count;

    th_BrushTuple liquid_brush = th_createBrush(l ,"liquid",liquid_handles,liquid_matrices,horse_count,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_STREAM_DRAW ,&liquid_command,fn_createVec3s(1),&liquid_offsets.data);

  }




  /*
   * TRICOLUMN INITIALIZATION
   */


  int colunn_per_count = 3;


  if (tricol_count == 0)
  {
    l->ls.tricolumn = NULL;
  }
  else
  {
    l->ls.tricolumn = th_alloc(alloc,sizeof(th_TricolumnGroup));



    th_tricolumnInitialize(alloc,l->ls.tricolumn,tricol_count,tricol_positions,tricol_times,&l->ls);

    for (int i = 0; i < tricol_count; i++) {
      th_tricolumnSetCourse(l->ls.tricolumn,i,tricol_courses[i],tricol_courses_count[i]);
    }



    th_EntityCollisionEdict tricol_edict;
    tricol_edict.entities = ls->tricolumn->entities_gems;
    tricol_edict.entityCount= &l->ls.tricolumn->gem_count;
    tricol_edict.grid = NULL;//&l.boidgroups[0].grid;
    tricol_edict.flags = TH_ENEMY | TH_CAN_KILL_PLAYER;
    th_registerEntityGroup(tricol_edict);

    fn_vec2* tricol_handles_eye = th_make_handles_arena(alloc,l->handles[mat_eye_blue],tricol_count);
    fn_mat4* tricol_matrices_eye = th_make_matrices_arena(alloc,fn_identityMat4(),tricol_count);


    th_RenderCommand tricol_eye_command;
    tricol_eye_command.offset = 0;
    tricol_eye_command.model_id = 0;
    tricol_eye_command.matcount = &l->ls.tricolumn->count;
    tricol_eye_command.mats = &l->ls.tricolumn->transforms_eye;
    tricol_eye_command.stride = 1;

    th_BrushTuple tricol_eye_brush = th_cacheBrushDefault(wadname,l,"th1/models/eyeballcorrected.obj",tricol_handles_eye,tricol_matrices_eye,tricol_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_eye_command,fn_createVec3s(25));

    fn_vec2* tricol_handles = th_make_handles_arena(alloc,l->handles[mat_circutry],tricol_count);
    fn_mat4* tricol_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),tricol_count);


    th_RenderCommand tricol_baseplate_command;
    tricol_baseplate_command.offset = 0;
    tricol_baseplate_command.model_id = 0;
    tricol_baseplate_command.matcount = &l->ls.tricolumn->count;
    tricol_baseplate_command.mats = &l->ls.tricolumn->transforms;
    tricol_baseplate_command.stride = 1;

    th_BrushTuple tricol_brush = th_cacheBrushDefault(wadname,l,"th1/models/spawnerii/baseplate.obj",tricol_handles,tricol_matrices,tricol_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_baseplate_command,fn_createVec3s(0));


    // fn_vec2* tricol_col_handles = th_make_handles_arena(alloc,l->handles[mat_splotch],tricol_count*colunn_per_count);
    // fn_mat4* tricol_col_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),tricol_count*colunn_per_count);


    // th_RenderCommand tricol_col_command;
    // tricol_col_command.offset = 0;
    // tricol_col_command.model_id = 0;
    // tricol_col_command.matcount = &l->ls.tricolumn->col_count;
    // tricol_col_command.mats = &l->ls.tricolumn->transforms_cols;
    // tricol_col_command.stride = 1;
    //
    // th_BrushTuple tricol_col_brush = th_cacheBrushDefault(wadname,l,"th1/models/spawnerii/actuator.obj",tricol_col_handles,tricol_col_matrices,tricol_count*colunn_per_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_col_command,fn_createVec3s(0));


    fn_vec2* tricol_col_handles = th_make_handles_arena(alloc,l->handles[mat_chrome],tricol_count*colunn_per_count);
    fn_mat4* tricol_col_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),tricol_count*colunn_per_count);

    th_RenderCommand tricol_col_command;
    tricol_col_command.offset = 0;
    tricol_col_command.model_id = 0;
    tricol_col_command.matcount = &l->ls.tricolumn->col_count;
    tricol_col_command.mats = &l->ls.tricolumn->transforms_cols;
    tricol_col_command.stride = 1;

    th_BrushTuple tricol_col_brush = th_cacheBrushDefault(wadname,l,"th1/models/spawnerii/actuator/actuator_base.obj",tricol_col_handles,tricol_col_matrices,tricol_count*colunn_per_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_col_command,fn_createVec3s(0));


    tricol_col_command.mats = &l->ls.tricolumn->transforms_foot;
    tricol_col_brush = th_cacheBrushDefault(wadname,l,"th1/models/spawnerii/actuator/actuator_foot.obj",tricol_col_handles,tricol_col_matrices,tricol_count*colunn_per_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_col_command,fn_createVec3s(0));


    tricol_col_handles = th_make_handles_arena(alloc,l->handles[mat_rubberseal],tricol_count*colunn_per_count);


    tricol_col_command.mats = &l->ls.tricolumn->transforms_cols;
    tricol_col_brush = th_cacheBrushDefault(wadname,l,"th1/models/spawnerii/actuator/actuator_seal.obj",tricol_col_handles,tricol_col_matrices,tricol_count*colunn_per_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_col_command,fn_createVec3s(0));

    tricol_col_handles = th_make_handles_arena(alloc,l->handles[mat_splotch],tricol_count*colunn_per_count);

    tricol_col_command.mats = &l->ls.tricolumn->transforms_leg;
    tricol_col_brush = th_cacheBrushDefault(wadname,l,"th1/models/spawnerii/actuator/actuator_arm.obj",tricol_col_handles,tricol_col_matrices,tricol_count*colunn_per_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_col_command,fn_createVec3s(0));


    tricol_col_handles = th_make_handles_arena(alloc,l->handles[mat_peelingred],tricol_count*colunn_per_count);

    tricol_col_command.mats = &l->ls.tricolumn->transforms_trapdoors;
    tricol_col_brush = th_cacheBrushDefault(wadname,l,"th1/models/spawnerii/actuator/actuator_trapdoor.obj",tricol_col_handles,tricol_col_matrices,tricol_count*colunn_per_count,NULL,  TH_DYNAMC | TH_SHADOWCASTING | TH_NOCULLING | TH_RENDERCOMMAND | TH_NOCENTER ,&tricol_col_command,fn_createVec3s(0));

    fn_vec3 trapdoor_center = th_getCenterVerts(&l->meshes[tricol_col_brush.id]);
    fn_vec3 trapdoor_max,trapdoor_min;
    th_getMinMaxVerts(&l->meshes[tricol_col_brush.id],&trapdoor_min,&trapdoor_max);

    l->ls.tricolumn->trapdoor_hinge = fn_createVec3(trapdoor_center.x,trapdoor_max.y,trapdoor_max.z);


    th_GpuData pivot_mesh = r_loadThorMesh("th1/models/spawnerii/actuator/actuator_balljoint_origin.obj",false,false,false);

    fn_vec3 pivot_center = th_getCenterVerts(&pivot_mesh);

    free(pivot_mesh.verts);
    free(pivot_mesh.indices);

    l->ls.tricolumn->pivot_center = pivot_center;




    th_RenderCommand gem_command_tricol;
    gem_command_tricol.offset = 0;
    gem_command_tricol.model_id = 0;
    gem_command_tricol.matcount = &l->ls.tricolumn->gem_count;
    gem_command_tricol.mats = &l->ls.tricolumn->transforms_gems;
    gem_command_tricol.stride = 1;


    fn_vec2* gem_handles_tricol = th_make_handles_arena(alloc,l->handles[mat_plasma],tricol_count*7);
    fn_mat4* tricol_gems_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),tricol_count*7);

    th_BrushTuple gem_tricol = th_cacheBrush(wadname,l,"th1/models/gem.obj",gem_handles_tricol,tricol_gems_matrices,tricol_count*7,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_PRECACHE_SCALEUV | TH_NOCULLING,&gem_command_tricol,fn_createVec3s(0),fn_createVec3s(1),fn_createVec2(0.5,0.5),fn_createVec3(0,1,0));

  }


  /*
   * LIFESPHERE INIT
   */
  if (num_lifesphere != 0)
  {
    th_RenderCommand lifesphere_command;
    lifesphere_command.offset = 0;
    lifesphere_command.model_id = 0;
    lifesphere_command.matcount = &l->ls.lifesphere->entity_count;
    lifesphere_command.mats = &l->ls.lifesphere->transforms;
    lifesphere_command.stride = 1;

    fn_mat4* lifesphere_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),num_lifesphere);
    fn_vec2* lifesphere_handles = th_make_handles_arena(alloc,l->handles[mat_health],num_lifesphere);

    th_BrushTuple healthcross = th_cacheBrushDefault(wadname,l,"th1/models/healthcross.obj",lifesphere_handles,lifesphere_matrices,num_lifesphere,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING,&lifesphere_command,fn_createVec3s(0));

  }

  if (num_question != 0)
  {
    th_RenderCommand lifesphere_command;
    lifesphere_command.offset = 0;
    lifesphere_command.model_id = 0;
    lifesphere_command.matcount = &l->ls.question->entity_count;
    lifesphere_command.mats = &l->ls.question->transforms;
    lifesphere_command.stride = 1;

    fn_mat4* lifesphere_matrices = th_make_matrices_arena(alloc,fn_identityMat4(),num_question);
    fn_vec2* lifesphere_handles = th_make_handles_arena(alloc,l->handles[mat_gold_glow],num_question);

    th_BrushTuple healthcross = th_cacheBrushDefault(wadname,l,"th1/models/fragenpunkt.obj",lifesphere_handles,lifesphere_matrices,num_question,NULL,TH_DYNAMC | TH_SHADOWCASTING | TH_RENDERCOMMAND | TH_NOCULLING,&lifesphere_command,fn_createVec3s(0));

  }


    printf("PLASMA HANDLES\n");
    fn_printVec2(l->handles[mat_plasma]);

}

fn_vec3 quantify_occ(fn_vec3 position,fn_vec3 goffset,fn_vec3 gridsize,float griddist)
{
  return fn_subVec3(fn_floorVec3(fn_multVec3s(fn_addVec3(fn_subVec3(position , goffset) , fn_multVec3s(gridsize,0.5*griddist)),1.0/griddist)),fn_createVec3(1,1,1));
}
fn_vec3 getPos_occ(fn_vec3 indices,fn_vec3 goffset,fn_vec3 gridsize,float griddist)
{
  float x = indices.x +1;
  float y = indices.y +1;
  float z = indices.z +1;
  return fn_createVec3((x-(gridsize.x*0.5))*griddist + goffset.x,(y-(gridsize.y*0.5))*griddist + goffset.y,(z-(gridsize.z*0.5))*griddist + goffset.z);
}




void th_loadLevelNamed(th_LevelDescriptor* l,const char* wadname,const char* wadname_dynamic,const char* levelname, bool gencubemaps,bool sharm,GLuint* cubemapDepth,GLuint* cubemapColor,fn_mat4 global_mat,bool respawn)
{
  th_resetGameplay();
  th_Allocator* allocator = &l->allocator;
  int SCENE_OFFSET = 0;
  th_loadLevelAudio();

  l->levelstate.boidgroups = NULL;
  l->levelstate.shamblers = NULL;
  l->levelstate.plasma = NULL;
  l->levelstate.brass = NULL;
  l->levelstate.shotbrass = NULL;
  l->levelstate.centipede = NULL;
  l->levelstate.debugger_centi = NULL;
  l->levelstate.shotgun = NULL;
  l->levelstate.player = NULL;
  l->levelstate.eyeball = NULL;
  l->levelstate.rocket = NULL;
  l->levelstate.horse = NULL;
  l->levelstate.hammer = NULL;
  l->levelstate.gems = NULL;
  l->levelstate.weapon = NULL;
  l->levelstate.debugger_collision = NULL;
  l->levelstate.tutorial = NULL;
  l->levelstate.world = th_alloc(allocator,sizeof(th_World));


  l->culldata = th_alloc(allocator,sizeof(th_FrustumCullData));
  l->culldata->aabbCount = 0;
  l->culldata->min_x = NULL;
  l->culldata->max_x = NULL;
  l->culldata->min_y = NULL;
  l->culldata->max_y = NULL;
  l->culldata->min_z = NULL;
  l->culldata->max_z = NULL;
  l->culldata->skip_culling_flag = NULL;
  l->culldata->oc = NULL;//malloc(sizeof(th_ObjectClass)* )

  l->cull_commands = NULL;
  l->cull_commands_count = 0;

  l->levelstate.animated_meshes = 0;

  l->handles_count = 0;
  l->handles = NULL;

  l->all_materials = NULL;

  l->levelstate.num_spawners_total = 0;
  l->levelstate.num_spawners_current = 0;
  l->levelstate.num_enemies_highwater = 0;
  l->levelstate.num_enemies_current = 0;
  l->levelstate.num_spawners_killed = 0;
  l->levelstate.progress_state = TH_TUTORIAL_BAR;

  th_loadLevelTextures(l);



  int scene_objs_count = 0;
  //int scene_objs_no_collision_count = 0;
  th_SceneManifest manifest = th_getSceneManifest(&l->allocator,levelname);
  scene_objs_count = manifest.num_objects;

  int* level_imported_texture = malloc(sizeof(int)*scene_objs_count);

  /*
  ***********************
  * LOAD SCENE TEXTURES *
  ***********************
  */

  for (int i = SCENE_OFFSET ; i < scene_objs_count;i++)
  {
    //level_imported_texture[i] = mat_tile_green;
    char file_location[2048];

    sprintf(file_location,"%s%s.mtl",levelname,manifest.filenames[i]);
    #ifdef FASTTEX
    char* texture_id = th_strdup("splotch/");//th_getTextureFromMtl(file_location);
    #else
    char* texture_id = th_getTextureFromMtl(file_location);
    #endif
    // char* texture_id = "wood/";
    char* lookupname = str_append("th1/compressed/",texture_id);
    lookupname = th_arenaManage(allocator,lookupname,strlen(lookupname) + 1);//what if strlen + 1 ISNT the size of the memory
    //printf("%i %s\n",i,lookupname );
     free(texture_id);
     level_imported_texture[i] = th_createMaterialManifest(l,lookupname);
     //free(lookupname);
  }

  //only load textures if its not a respawn (reloading same level)
  if (!respawn)
  {
    th_loadMaterials(l->all_materials,l->handles_count*TEX_PER_MATERIAL);
  }



  for (int i = 0; i < l->handles_count*TEX_PER_MATERIAL;i++)
  {
    free(l->all_materials[i]);
  }

  free(l->all_materials);
  l->all_materials = NULL;

  printf("LOAding Data2\n");

  l->handles = th_arenaManage(allocator,l->handles,sizeof(fn_vec2)*l->handles_count);

  for (int i = 0 ; i < l->handles_count;i++)
  {
    l->handles[i] = th_getMaterialHandle(i);
  }
    printf("BCISK tex %f\n",l->handles[mat_bricks].y );
  th_loadLevelParticles(l);











  th_World* world = l->levelstate.world;

  float global_scale = 1;
  th_initWorld(world);
   world->aabbs = NULL; //malloc(sizeof(th_AABB)*3);
  // th_AABB* aabbs = world->aabbs;
   world->volumes = NULL;//malloc(sizeof(th_CollidableVolume)*3);
   world->volumecount = 0;

// th_CollisionMeshData floor = th_getCollisionMeshData("th1/models/floor.obj",false,true);
// th_CollisionMeshData cylinder = th_getCollisionMeshData("th1/models/cylinder.obj",false,true);
// th_CollisionMeshData cube = th_getCollisionMeshData("th1/models/cube.obj",false,true);




  //l->spawnpoint = fn_createVec3(-438.795898, -1900.612061, -406.033173);//fn_createVec3(0,-500,0);
//  l->spawnpoint = fn_createVec3(-802.294617 ,-134.916412, 336.047974);
  // l->spawnpoint = fn_createVec3(-105.010605 ,-300.707367, 34.144585);



  l->harmonics_file = th_alloc(allocator,sizeof(char)*2048);//"th1/wad/leveldream/harmonics.hwad";
  sprintf(l->harmonics_file,"%sharmonics.hwad",wadname);

  // fn_vec3 gridmax = fn_createVec3(1052.274170,-94.513741,767.79);
  // fn_vec3 gridmin = fn_createVec3(-977.709839,-2128.320801,-664.190186);


  l->render_commands_streamed_count = 0;
  l->render_commands_streamed = NULL;

  l->streamed_meshes = 0;
  l->streamed = NULL;

  l->levelstate.animated_models = NULL;
  l->levelstate.animated_models_count = 0;

  l->anim_render_commands_count = 0;
  l->anim_render_commands = NULL;//malloc(sizeof(th_RenderCommand)*l->anim_render_commands_count);




  l->staticdynamic_count = 0;
  l->staticdynamic = NULL;//malloc(sizeof(int)*l->staticdynamic_count);

  l->render_commands_count = 0;
  l->render_commands = NULL;//malloc(sizeof(th_RenderCommand)*l->render_commands_count);

  l->tesselated_count = 0;
  l->tesselated = NULL;//malloc(sizeof(int)*l->tesselated_count);


  l->meshes_dynamic_count = 0;

  l->dynamic_shadowcaster_count = 0;
  l->dynamic_shadowcaster = NULL;//malloc(sizeof(int)*l->dynamic_shadowcaster_count);
  //l->dynamic_shadowcaster[0] = 2;

  l->meshes_count = 0;
  l->meshes = NULL;

  /*
  *
  *STATIC MESHES
  *
  */
  th_ObjProperties* obj_properties = malloc(sizeof(th_ObjProperties)*scene_objs_count);
  memset(obj_properties,0,sizeof(th_ObjProperties)*scene_objs_count);
  for (int i = 0 ; i < manifest.num_noncolliding;i++)
  {
    //printf("DBG %i\n",manifest.noncolliding[i] );
    obj_properties[manifest.noncolliding[i]] |= TH_NONCOLLIDING;
  }
  for (int i = 0 ; i < manifest.num_cubemapmesh;i++)
  {
    obj_properties[manifest.cubemapmesh[i]] |= TH_CUBEMAPMESH;
    obj_properties[manifest.cubemapmesh[i]] |= TH_NONCOLLIDING;
  }
  for (int i = 0 ; i < manifest.num_colliders;i++)
  {
    obj_properties[manifest.colliders[i]] |= TH_COLLIDER;
  }
  for (int i = 0 ; i < manifest.num_concave;i++)
  {
    obj_properties[manifest.concave[i]] |= TH_USE_NONCONVEX;
  }

  int start_mindex = 0;
  int end_mindex = 0;

  l->cube_count = manifest.num_cubemapmesh;
  l->cube_positions = th_alloc(allocator,sizeof(fn_vec3)*l->cube_count);
  l->cube_mins = th_alloc(allocator,sizeof(fn_vec3)*l->cube_count);
  l->cube_maxs = th_alloc(allocator,sizeof(fn_vec3)*l->cube_count);
  int cube_count = 0;





  for (int i = SCENE_OFFSET ; i < scene_objs_count;i++)
  {

    // if ((obj_properties[i] & TH_NONCOLLIDING))
    // {
    //   printf("DBG 2 %i\n",i );
    // }

    char file_location[2048];

    sprintf(file_location,"%s%s.obj",levelname,manifest.filenames[i]);
    th_CollisionMeshData* colmesh = th_alloc(allocator,sizeof(th_CollisionMeshData));

    if (!(obj_properties[i] & TH_NONCOLLIDING))
    {
      *colmesh = th_cacheCollisionMeshData(allocator,wadname,file_location,false,false); //th_getCollisionMeshData(file_location,false,false);
    }


    fn_vec2* tem_handles = th_make_handles(l->handles[level_imported_texture[i]],1);
    tem_handles = th_arenaManage(allocator,tem_handles,sizeof(fn_vec2));
    //fn_mat4 global_mat =
    fn_mat4* tem_mats = th_make_matrices(global_mat,1);
    tem_mats = th_arenaManage(allocator,tem_mats,sizeof(fn_mat4));

    th_CollisionMeshData* cmdataptr = NULL;
    if (!(obj_properties[i] & TH_NONCOLLIDING))
    {
      cmdataptr = colmesh;
    }
    th_BrushTuple tuple;
    if ((obj_properties[i] & TH_COLLIDER))
    {
      th_World* world = l->levelstate.world;

      th_createMeshBrushes(&l->allocator,*colmesh,&world->volumes,&world->aabbs,&world->volumecount,global_mat,"Invisible");
    }
    else if ((obj_properties[i] & TH_CUBEMAPMESH))
    {
      th_GpuData m = r_loadThorMesh(file_location,false,false,false);

      th_Vertex* verts = m.verts;


      fn_vec3 min = fn_transformVec3(verts[0].position,global_mat);
      fn_vec3 max = fn_transformVec3(verts[0].position,global_mat);

      for (GLuint k =0;k < m.vertcount;k++ )
      {
        verts[k].position = fn_transformVec3(verts[k].position,global_mat);
        if(verts[k].position.x < min.x)
          min.x = verts[k].position.x;
        if(verts[k].position.y < min.y)
          min.y = verts[k].position.y;
        if(verts[k].position.z < min.z)
          min.z = verts[k].position.z;

        if(verts[k].position.x > max.x)
          max.x = verts[k].position.x;
        if(verts[k].position.y > max.y)
          max.y = verts[k].position.y;
        if(verts[k].position.z > max.z)
          max.z = verts[k].position.z;
      }
      // max = fn_transformVec3(max,global_mat);
      // min = fn_transformVec3(min,global_mat);
      fn_printVec3(max);
      fn_printVec3(min);
      l->cube_positions[cube_count] = fn_multVec3s(fn_addVec3(min,max),0.5);
      l->cube_mins[cube_count] = fn_subVec3(min,fn_createVec3s(500));
      l->cube_maxs[cube_count] = fn_addVec3(max,fn_createVec3s(500));
      cube_count++;

      free(m.verts);
      free(m.indices);
    }
    else
    {
      th_BrushFlags flags_additional = 0;
      if (obj_properties[i] & TH_USE_NONCONVEX)
      {
        flags_additional = flags_additional | TH_NON_CONVEX;
      }
      //printf("Cacheing brush%s\n",file_location);
      tuple = th_cacheBrushDefault(wadname,l,file_location,tem_handles,tem_mats,1,cmdataptr,TH_STATIC | TH_NOCENTER | flags_additional,NULL,fn_createVec3s(0));
      // printf("%i\n",tuple.culldata_offset );
      if (i == SCENE_OFFSET)
      {
        start_mindex = tuple.id;
      }
      end_mindex = tuple.id;
    }

  }
  printf("%s %i\n","Begining cubemap load",l->cube_count);
  if (!gencubemaps && !sharm && l->cube_count > 0)
  {
    // th_loadCapturedCubemap("th1/cubemaps_gen.float",512,5,cubemapDepth,cubemapColor,l->cube_mins,l->cube_maxs,l->cube_count,"th1/wad/demoscene/");
    th_loadCubemapCompressed(512,5,cubemapDepth,cubemapColor,NULL,NULL,l->cube_count,wadname);
  }
  else
  {
    if (l->cube_count > 0)
    {
      th_blankCubemap(512,5,cubemapDepth,cubemapColor,NULL,NULL,l->cube_count);
    }

  }

  //  th_exportMWADs(&l->meshes[start_mindex],scene_objs_count,"th1/wad/leveldream/");
  printf("%i %i\n",start_mindex,end_mindex );


  /*
   *
   * CREATE WORLD
   * So that dynamic object creation can do raycasts for path finding
   */


  world->aabbs = th_arenaManage(allocator,world->aabbs,sizeof(th_AABB)*world->volumecount);
  world->volumes = th_arenaManage(allocator,world->volumes,sizeof(th_CollidableVolume)*world->volumecount);
  world->octree =  fn_createOctree(world->aabbs,world->volumecount);
  printf("Collidable Tris %d\n",world->volumecount);




  th_allocatePhysicsMemory(allocator,world,th_getNumThreads());
  th_allocateEntityPointers(allocator,world,th_getNumThreads());


  /*
   *
   *DYNAMIC OBJECTS
   *
   */



  th_levelLoadDynamicObjects(l,wadname_dynamic);

  //remanages
  l->meshes = th_arenaManage(allocator,l->meshes,sizeof(th_GpuData)*l->meshes_count);
  l->culldata->min_x = th_arenaManage(allocator,l->culldata->min_x,sizeof(float)*(l->culldata->aabbCount));
  l->culldata->max_x = th_arenaManage(allocator,l->culldata->max_x,sizeof(float)*(l->culldata->aabbCount));
  l->culldata->min_y = th_arenaManage(allocator,l->culldata->min_y,sizeof(float)*(l->culldata->aabbCount));
  l->culldata->max_y = th_arenaManage(allocator,l->culldata->max_y,sizeof(float)*(l->culldata->aabbCount));
  l->culldata->min_z = th_arenaManage(allocator,l->culldata->min_z,sizeof(float)*(l->culldata->aabbCount));
  l->culldata->max_z = th_arenaManage(allocator,l->culldata->max_z,sizeof(float)*(l->culldata->aabbCount));
  l->culldata->skip_culling_flag = th_arenaManage(allocator,l->culldata->skip_culling_flag,sizeof(bool)*(l->culldata->aabbCount));
  l->culldata->oc = th_arenaManage(allocator,l->culldata->oc,sizeof(th_ObjectClass)*(l->culldata->aabbCount));


  l->cull_commands = th_arenaManage(allocator,l->cull_commands,sizeof(th_FrustumCullCommand)*(l->cull_commands_count));
  l->staticdynamic = th_arenaManage(allocator,l->staticdynamic,sizeof(int)*l->staticdynamic_count);
  l->dynamic_shadowcaster= th_arenaManage(allocator,l->dynamic_shadowcaster,sizeof(int)*l->dynamic_shadowcaster_count);
  l->tesselated = th_arenaManage(allocator,l->tesselated ,sizeof(int)*l->tesselated_count);
  l->lights = th_arenaManage(allocator,l->lights ,sizeof(int)*l->light_count);
  l->render_commands = th_arenaManage(allocator,l->render_commands,sizeof(th_RenderCommand)*l->render_commands_count);
  l->anim_render_commands = th_arenaManage(allocator,l->anim_render_commands,sizeof(th_RenderCommand)*l->anim_render_commands_count);
  l->levelstate.animated_models = th_arenaManage(allocator,l->levelstate.animated_models,sizeof(th_Model)*l->levelstate.animated_models_count);

  l->streamed = th_arenaManage(allocator,l->streamed,sizeof(th_GpuData)*l->streamed_meshes);
  l->render_commands_streamed = th_arenaManage(allocator,l->render_commands_streamed,sizeof(th_RenderCommand)*l->render_commands_streamed_count);


  printf("MESHES %i\n",l->meshes_count );




  /*
   * NOTE!!!!!: JUST FREE THE OCTREE IF IT ALREADY EXISTS! (DURING UNLOAD)
   */

  /*
   * NOTE!!!!!: Write routines in the unload to free GL resources
   */

  /*
   * NOTE!!!!!: Write stuff to reset level dynamic objects and state
   * Lights, decals, playerstate, dynamic objects, audio sources
   */

  /*
   * NOTE!!!!!: invoke code to clear entity edicts
   */

  /*
   * NOTE!!!!!: clear the audio system files
   */

  /*
   * NOTE!!!!!: reset visibility_data memory, it depends in level aabb count
   */



  free(level_imported_texture);
  free(obj_properties);

  if (l->ls.num_spawners_total > 0 && l->ls.tutorial == NULL)
  {
    l->ls.progress_state = TH_BAR_ONE;
  }
}


void th_loadSpawnSetNamed(th_LevelDescriptor* l,const char* name)
{
  l->spawnset = th_spawnSetLoad(&l->allocator,name,&l->spawnset_entries);
}



static double integrate_alpha(double a, double b, double c, double x_max, int steps) {
  double dx = x_max / steps;
  double sum = 0.0;

  for (int i = 0; i <= steps; i++) {
    double x = i * dx;
    double fx = a * pow(c, x / b);

    // Trapezoidal rule weights
    if (i == 0 || i == steps)
      sum += fx * 0.5;
    else
      sum += fx;
  }

  return sum * dx;
}

static double integrate_mu(double a, double b, double c, double x_max, int steps) {
  double dx = x_max / steps;
  double sum = 0.0;
  double inner_integral = 0.0;

  for (int i = 0; i <= steps; i++) {
    double x = i * dx;
    double fx_inner = a * pow(c, x / b);

    // Update inner integral using trapezoidal rule
    if (i > 0)
      inner_integral += 0.5 * (fx_inner + a * pow(c, (x - dx) / b)) * dx;

    // Compute the outer integrand
    double fx_outer = a * pow(c, x / b) * exp(-inner_integral);

    // Trapezoidal rule for outer integral
    if (i == 0 || i == steps)
      sum += fx_outer * 0.5;
    else
      sum += fx_outer;
  }

  return sum * dx;
}

void th_loadLevelDefNamed(th_LevelDescriptor* l,const char* name,int fog_quality )
{
  int keyval_count = 0;
  th_KeyValuePair* keyvals = th_keyValueLoad(&l->allocator,name,&keyval_count);
  printf("KEYVALS %i\n",keyval_count );

  l->night_sky = th_keyValueGetBool(keyvals,keyval_count,"night_sky");

  l->autoGrid = th_keyValueGetBool(keyvals,keyval_count,"autoGrid");


  l->atm_rayleigh = th_keyValueGetVec3(keyvals,keyval_count,"atm_rayleigh");
  l->atm_sun_intensity = th_keyValueGetFloat(keyvals,keyval_count,"atm_sun_intensity");
  l->atm_sun_color = th_keyValueGetVec3(keyvals,keyval_count,"atm_sun_color");


  l->fog_color = th_keyValueGetVec3(keyvals,keyval_count,"fog_color");
  l->fog_gain = th_keyValueGetFloat(keyvals,keyval_count,"fog_gain");
  l->fog_density = th_keyValueGetFloat(keyvals,keyval_count,"fog_density");
  l->fog_ambient_density = th_keyValueGetFloat(keyvals,keyval_count,"fog_ambient_density");

  float fog_distances[] = {TH_FOG_MIN_DIST,TH_FOG_MED_DIST,TH_FOG_MAX_DIST};
  float fdist = fog_distances[fog_quality];
  l->mu = (float)integrate_mu(l->fog_density,fdist,0.5,fdist,10000);
  l->alpha = (float)integrate_alpha(l->fog_density,fdist,0.5,fdist,10000);

  //light coordinates
  //696.825439 -586.182129 -99.587448

  l->sundir_sky = th_keyValueGetVec3(keyvals,keyval_count,"sundir_sky");
  l->sundir_shadow = fn_normalizeVec3(th_keyValueGetVec3(keyvals,keyval_count,"sundir_shadow"));
  l->sundir_light = fn_normalizeVec3(th_keyValueGetVec3(keyvals,keyval_count,"sundir_light"));//fn_normalizeVec3(fn_createVec3(0.25,-1.0,0.15));
  l->suncolor_light = th_keyValueGetVec3(keyvals,keyval_count,"suncolor_light");

  // th_KeyValuePair* ortho_values = th_keyValueFind(keyvals,keyval_count, "orthomat_sunlight");
  // l->orthomat_sunlight = fn_ortho3D(ortho_values.values[0].flt_value,ortho_values.values[1].flt_value,ortho_values.values[2].flt_value,ortho_values.values[3].flt_value,ortho_values.values[4].flt_value,ortho_values.values[5].flt_value);




  // fn_vec3 gridmin =fn_createVec3(-2325,-3729,-1039);
  // fn_vec3 gridmax = fn_createVec3(1667,414,1524);
  // gridmax = fn_addVec3(gridmax,fn_createVec3s(300));
  // gridmin = fn_subVec3(gridmin,fn_createVec3s(300));
  fn_vec3 gridmin = th_keyValueGetVec3(keyvals,keyval_count,"gridmin");
  fn_vec3 gridmax = th_keyValueGetVec3(keyvals,keyval_count,"gridmax");
  l->gridsize = th_keyValueGetFloatDefault(keyvals,keyval_count,"gridsize",100.0);

  float dx = ceil(fabs(gridmax.x - gridmin.x)/l->gridsize);
  float dy = ceil(fabs(gridmax.y - gridmin.y)/l->gridsize);
  float dz = ceil(fabs(gridmax.z - gridmin.z)/l->gridsize);
    printf("GDIMS %f %f %f\n",dx,dy,dz );

  l->gridpos = fn_multVec3s(fn_addVec3(gridmax,gridmin),0.5);
  l->griddims = fn_createVec3((int)dx,(int)dy,(int)dz);



  l->occ_gridsize = 25.0;
  float dx_occ = ceil(fabs(gridmax.x - gridmin.x)/l->occ_gridsize);
  float dy_occ = ceil(fabs(gridmax.y - gridmin.y)/l->occ_gridsize);
  float dz_occ = ceil(fabs(gridmax.z - gridmin.z)/l->occ_gridsize);
    printf("GDIMS %f %f %f\n",dx_occ,dy_occ,dz_occ );

  l->occ_gridpos = fn_multVec3s(fn_addVec3(gridmax,gridmin),0.5);
  l->occ_griddims = fn_createVec3((int)dx_occ,(int)dy_occ,(int)dz_occ);


  // l->spawnpoint = fn_createVec3(-1210.439697, -509.828125, -235.214294);
//  l->spawnpoint = fn_createVec3(-308.159912, -938.359436, -227.510132);
  l->spawnpoint = th_keyValueGetVec3(keyvals,keyval_count,"spawnpoint");

  l->player_pos_titlescreen = th_keyValueGetVec3Default(keyvals,keyval_count,"player_pos_titlescreen",fn_createVec3(0,0,0));

  l->spawnangles = fn_createVec2(th_keyValueGetFloat(keyvals,keyval_count,"angle_x"),th_keyValueGetFloat(keyvals,keyval_count,"angle_y"));

  // l->omni_light_count = 0;
  // l->omni_light_positions = NULL;//malloc(sizeof(fn_vec3)*l->omni_light_count);
  // l->omni_light_atlascoords = NULL;//malloc(sizeof(fn_vec3)*l->omni_light_count);
  // l->omni_light_masks = NULL;
  l->center_sunlight = l->gridpos;

  l->omni_light_count = (int)th_keyValueGetFloat(keyvals,keyval_count,"omni_light_count");
  l->omni_light_masks = NULL;
  l->omni_light_positions = NULL;
  l->omni_light_atlascoords = NULL;
  if (l->omni_light_count > 0)
  {
    l->omni_light_masks = th_alloc(&l->allocator,sizeof(int)*l->omni_light_count);
    l->omni_light_positions = th_alloc(&l->allocator,sizeof(fn_vec3)*l->omni_light_count);
    l->omni_light_atlascoords = th_alloc(&l->allocator,sizeof(fn_vec3)*l->omni_light_count);

    //posx negz posy negy
    // 0 5   are 0
    // l->omni_light_masks[0] = 0b011110;
    // l->omni_light_positions[0] = fn_createVec3(-346.736298,-276.792847,289.134003);
    // l->omni_light_atlascoords[0] = fn_createVec3(0,0,0);
    // l->pointlights[2].pos = fn_createVec4(-346.736298,-276.792847,289.134003,0);
    // l->pointlights[2].color = fn_createVec4(70000*2,50000*2,50000*2,0);
    // l->pointlights[2].lightmat = fn_identityMat4();
    // l->pointlights[2].shadowindex = fn_createVec4(1,0,0,0);
    // l->pointlights[2].pos2 = fn_createVec4(0,0,0,0);
    //
    // l->omni_light_masks[1] = 0b001000;
    // l->omni_light_positions[1] = fn_createVec3(-847.951721,-1657.498535,39.311981);
    // l->omni_light_atlascoords[1] = fn_createVec3(1,0,1);
    // l->pointlights[3].pos = fn_createVec4(-847.951721,-1657.498535,39.311981,0);
    // l->pointlights[3].color = fn_createVec4(50000,25000,0,0);
    // l->pointlights[3].lightmat = fn_identityMat4();
    // l->pointlights[3].shadowindex = fn_createVec4(1,1,0,1);
    // l->pointlights[3].pos2 = fn_createVec4(0,0,0,0);

    for (int i = 0; i < l->omni_light_count; i++) {
      l->omni_light_masks[i] = 0b000000;
      char buffer_temp[128];
      sprintf(buffer_temp,"%s%i","omni_light_positions_",i);
      char buffer_temp2[128];
      sprintf(buffer_temp2,"%s%i","omni_light_atlascoords_",i);

      char buffer_temp3[128];
      sprintf(buffer_temp3,"%s%i","omni_light_color_",i);

      char buffer_temp4[128];
      sprintf(buffer_temp4,"%s%i","omni_light_direction_",i);

      char buffer_temp5[128];
      sprintf(buffer_temp5,"%s%i","omni_light_static_",i);

      l->omni_light_positions[i] = th_keyValueGetVec3(keyvals,keyval_count,buffer_temp);
      l->omni_light_atlascoords[i] = th_keyValueGetVec3(keyvals,keyval_count,buffer_temp2);

      fn_vec3 ligdir = th_keyValueGetVec3Default(keyvals,keyval_count,buffer_temp4,fn_createVec3(0,0,0));

      bool is_static = th_keyValueGetBool(keyvals,keyval_count,buffer_temp5);

      if (!fn_equalVec3(ligdir,fn_createVec3s(0.0)))
      {
        int pos_x_mask = 0b000001;
        int neg_x_mask = 0b000010;
        int pos_y_mask = 0b000100;
        int neg_y_mask = 0b001000;
        int pos_z_mask = 0b010000;
        int neg_z_mask = 0b100000;

        if (ligdir.x > 0.0)
        {
          l->omni_light_masks[i] = l->omni_light_masks[i] | neg_x_mask;
        }
        if (ligdir.x < 0.0)
        {
          l->omni_light_masks[i] = l->omni_light_masks[i] | pos_x_mask;
        }

        if (ligdir.y > 0.0)
        {
          l->omni_light_masks[i] = l->omni_light_masks[i] | neg_y_mask;
        }
        if (ligdir.y < 0.0)
        {
          l->omni_light_masks[i] = l->omni_light_masks[i] | pos_y_mask;
        }

        if (ligdir.z > 0.0)
        {
          l->omni_light_masks[i] = l->omni_light_masks[i] | neg_z_mask;
        }
        if (ligdir.z < 0.0)
        {
          l->omni_light_masks[i] = l->omni_light_masks[i] | pos_z_mask;
        }
      }

      if (is_static)
      {
        l->omni_light_masks[i] = 0b111111;
      }

      l->pointlights[3 + i].pos = fn_createVec4Vec3(l->omni_light_positions[i],0);
      l->pointlights[3 + i].color = fn_createVec4Vec3(th_keyValueGetVec3(keyvals,keyval_count,buffer_temp3),0);
      l->pointlights[3 + i].lightmat = fn_identityMat4();
      l->pointlights[3 + i].shadowindex = fn_createVec4(1,l->omni_light_atlascoords[i].x,l->omni_light_atlascoords[i].y,l->omni_light_atlascoords[i].z);
      l->pointlights[3 + i].pos2 = fn_createVec4(0,0,0,0);
    }
  }

  l->brass_standard = TH_DEFAULT_VICTORY_STATS;
  l->brass_standard.airtime = th_keyValueGetFloatDefault(keyvals,keyval_count,"bronze_std_airtime",l->brass_standard.airtime);
  l->brass_standard.damage_taken = th_keyValueGetFloatDefault(keyvals,keyval_count,"bronze_std_damage",l->brass_standard.damage_taken);
  l->brass_standard.completiontime = th_keyValueGetFloatDefault(keyvals,keyval_count,"bronze_std_completiontime",l->brass_standard.completiontime);

  l->silver_standard = TH_DEFAULT_VICTORY_STATS;
  l->silver_standard.airtime = th_keyValueGetFloatDefault(keyvals,keyval_count,"silver_std_airtime",l->silver_standard.airtime);
  l->silver_standard.damage_taken = th_keyValueGetFloatDefault(keyvals,keyval_count,"silver_std_damage",l->silver_standard.damage_taken);
  l->silver_standard.completiontime = th_keyValueGetFloatDefault(keyvals,keyval_count,"silver_std_completiontime",l->silver_standard.completiontime);

  l->gold_standard = TH_DEFAULT_VICTORY_STATS;
  l->gold_standard.airtime = th_keyValueGetFloatDefault(keyvals,keyval_count,"gold_std_airtime",l->gold_standard.airtime);
  l->gold_standard.damage_taken = th_keyValueGetFloatDefault(keyvals,keyval_count,"gold_std_damage",l->gold_standard.damage_taken);
  l->gold_standard.completiontime = th_keyValueGetFloatDefault(keyvals,keyval_count,"gold_std_completiontime",l->gold_standard.completiontime);

  l->skyboost = th_keyValueGetFloatDefault(keyvals,keyval_count,"sky_boost",1.0);

  l->trackname = th_keyValueGetStrDefault(keyvals,keyval_count,"track_name","random");

  l->horizon_height = th_keyValueGetFloatDefault(keyvals,keyval_count,"horizon_height",6372e3);
  l->cloud_enable = th_keyValueGetFloatDefault(keyvals,keyval_count,"cloud_enable",0.0);

  l->sun_shadow_scale = th_keyValueGetFloatDefault(keyvals,keyval_count,"sun_shadow_atlas_scale",1.0);
  l->omni_shadow_scale = th_keyValueGetFloatDefault(keyvals,keyval_count,"omni_shadow_atlas_scale",1.0);
  l->omni_light_jitter = th_keyValueGetFloatDefault(keyvals,keyval_count,"omni_light_jitter",10.0);
  l->cube_nohit_sky = th_keyValueGetFloatDefault(keyvals,keyval_count,"cube_nohit_sky",1.0);

  l->shadow_radius = th_keyValueGetFloatDefault(keyvals,keyval_count,"shadow_blur",3.5);
  l->shadow_radius_fog = th_keyValueGetFloatDefault(keyvals,keyval_count,"shadow_blur_fog",4.0);

}

void th_setrecacheMode(bool recache_override)
{
  if (NORECACHE_MODE && recache_override)
  {
    NORECACHE_MODE = false;
  }
}

void th_loadLevel(th_LevelDescriptor* l, bool gencubemaps,bool sharm,GLuint* cubemapDepth,GLuint* cubemapColor,const char* levelname,const char* spawnsetname,fn_vec3 scale,bool respawn,int fog_quality)
{



  l->cube_nohit_sky = 1.0;
  l->omni_light_jitter = 10.0;
  l->omni_shadow_scale = 1.0;
  l->sun_shadow_scale = 4.0;
  l->shadow_radius_fog = 0.001;
  l->shadow_radius = 0.001;

  texture_folder_count = 0;
  th_createAllocator(&l->allocator);
  l->trackname = "";
  l->skyboost = 1.0;
  l->pointlightcount = 256;
  l->ls.level_start_time =th_time();

  l->pointlights = th_alloc(&l->allocator,sizeof(th_PointLight)*256);
  l->light_query = th_alloc(&l->allocator,sizeof(th_LightQuery));
  l->lights_reserved = 4;
  th_initLightQuery(l->light_query,l->pointlights,l->pointlightcount,l->lights_reserved,&l->allocator);
  l->ls.general_light_query = l->light_query;

  l->pointlights[0].pos =  fn_createVec4(100000,0,0,0);
  l->pointlights[0].color = fn_createVec4(0,1000*5,2000*5,0);
  l->pointlights[0].lightmat = fn_identityMat4();
  l->pointlights[0].shadowindex = fn_createVec4(0,0,0,0);
  l->pointlights[0].pos2 = fn_createVec4(0,0,0,0);

  l->ls.barrel_color_interp = 0.0;
  l->ls.blackbody_barrel_pointa = fn_createVec3(0,0,0);
  l->ls.blackbody_barrel_pointb = fn_createVec3(1,0,0);
  l->ls.barrel_shape_min = 0.34;
  l->ls.barrel_shape_max = 0.41;
  l->ls.barrel_maxtemp = 3000.0;

  l->material_properties = th_alloc(&l->allocator,sizeof(int)*4*TH_MATERIAL_PROPERTIES_DIV4);
  for (int i = 0 ; i < 4*TH_MATERIAL_PROPERTIES_DIV4;i++)
  {
    l->material_properties[i] = 0;
  }



  for (int i = 0 ; i < l->pointlightcount;i++)
  {
    float x = (float)th_random()/(float)(RAND_MAX/(2*640)) - (2*640*0.5);
    float y = (float)th_random()/(float)(RAND_MAX/(2*640)) - (2*640*0.5);
    float z = (float)th_random()/(float)(RAND_MAX/(2*640)) - (2*640*0.5);

    float r = (float)th_random()/(float)(RAND_MAX/200.0);
    float g = (float)th_random()/(float)(RAND_MAX/200.0);
    float b = (float)th_random()/(float)(RAND_MAX/200.0);


    l->pointlights[i].pos = fn_createVec4(100000,0,0,0);
    l->pointlights[i].color = fn_createVec4(0,0,0,0);
    l->pointlights[i].lightmat = fn_identityMat4();
    l->pointlights[i].shadowindex = fn_createVec4(0,0,0,0);
    l->pointlights[i].pos2 = fn_createVec4(0,0,0,0);
  }

  //1506.086914 -3178.334229 -1678.932007
  //-1648.875977 45.780716 1521.267944

  fn_mat4 global_mata = fn_translaterotatescale(fn_createVec3(0,-100,0),TH_PI,fn_createVec3(0,1,0),fn_createVec3s(3.0));
  fn_mat4 global_matb = fn_translaterotatescale(fn_createVec3(0,-100,0),TH_PI,fn_createVec3(0,1,0),fn_createVec3s(2.5));

   // th_levelDefA(l);
   // th_loadLevelNamed(l,"th1/wad/leveldream/","th1/models/leveldream/",gencubemaps,sharm,cubemapDepth,cubemapColor,global_mata);

  // th_levelDefB(l);
  // th_loadLevelNamed(l,"th1/wad/levelgalleria/","th1/models/levelgalleria/",gencubemaps,sharm,cubemapDepth,cubemapColor,global_matb);
  //return th_loadLevelAir(gencubemaps,sharm,cubemapDepth,cubemapColor);


  // th_levelDefC(l);
  // th_loadLevelNamed(l,"th1/wad/levelskatepark/","th1/models/levelskatepark/",gencubemaps,sharm,cubemapDepth,cubemapColor,fn_makescale(fn_createVec3(1.5,1.5,1.5)));

  // th_levelDefDune(l);

  // th_loadLevelDefNamed(l,"th1/models/leveldune/leveldef.txt");
  // th_loadSpawnSetNamed(l,"th1/spawnsets/spawnset_dune.txt");
  // th_loadLevelNamed(l,"th1/wad/leveldune/","th1/models/leveldune/",gencubemaps,sharm,cubemapDepth,cubemapColor,fn_makescale(fn_createVec3(1.0,1.0,1.0)));

  char* leveldef_string = th_alloc(&l->allocator,1024);
  char* spawnset_string = th_alloc(&l->allocator,1024);
  char* wad_string = th_alloc(&l->allocator,1024);

  char* wad_string_dynamic = th_alloc(&l->allocator,1024);

  char* model_string = th_alloc(&l->allocator,1024);

  int def_err = snprintf(leveldef_string,1024,"th1/models/%s/leveldef.txt",levelname);
  int spw_err = snprintf(spawnset_string,1024,"th1/spawnsets/%s.txt",spawnsetname);
  int mdl_err = snprintf(model_string,1024,"th1/models/%s/",levelname);
  int wad_err = snprintf(wad_string,1024,"th1/wad/%s/",levelname);

  snprintf(wad_string_dynamic,1024,"th1/wad/%s/","dyn");

  if (def_err >= 1024 || def_err < 0)
  {
    printf("levelname too long\n");
  }
  if (mdl_err >= 1024 || mdl_err < 0)
  {
    printf("levelname too long\n");
  }
  if (wad_err >= 1024 || wad_err < 0)
  {
    printf("levelname too long\n");
  }
  if (spw_err >= 1024 || spw_err < 0)
  {
    printf("spawnsetname too long\n");
  }

  // th_loadLevelDefNamed(l,"th1/models/levelskatepark/leveldef.txt");
  // th_loadSpawnSetNamed(l,"th1/spawnsets/spawnset_skatepark.txt");
  // th_loadLevelNamed(l,"th1/wad/levelskatepark/","th1/models/levelskatepark/",gencubemaps,sharm,cubemapDepth,cubemapColor,fn_makescale(fn_createVec3(1.5,1.5,1.5)));
//fn_createVec3(1.5,1.5,1.5)
  th_loadLevelDefNamed(l,leveldef_string,fog_quality);
  th_loadSpawnSetNamed(l,spawnset_string);
  th_loadLevelNamed(l,wad_string,wad_string_dynamic,model_string,gencubemaps,sharm,cubemapDepth,cubemapColor,fn_makescale(scale),respawn);


  /*
   *  MATERIAL PROPERTIES
   */
  bool canset = true;
  for (int i = 0 ; i < l->handles_count;i++)
  {
    if ((int)l->handles[i].x >= 4*TH_MATERIAL_PROPERTIES_DIV4)
    {
      printf("Error, not enough material property slots!\n");
      canset = false;
      break;
    }
  }

  if (canset)
  {
    l->material_properties[(int)(l->handles[mat_plasma].x)] = TH_GLOW_MAT_FLAG;
    l->material_properties[(int)(l->handles[mat_health].x)] = TH_GLOW_MAT_FLAG;
    l->material_properties[(int)(l->handles[mat_plugblock].x)] = TH_IRIDESCENT_MAT_FLAG;
    l->material_properties[(int)(l->handles[mat_hotbarel].x)] = TH_BARREL_GLOW_FLAG;
    l->material_properties[(int)(l->handles[mat_chrome_red_hotbarel].x)] = TH_BARREL_GLOW_FLAG;
    l->material_properties[(int)(l->handles[mat_brass_bullet].x)] = TH_IRIDESCENT_MAT_FLAG;

    l->material_properties[(int)(l->handles[mat_gold_glow].x)] = TH_GLOW_MAT_FLAG;
    l->material_properties[(int)(l->handles[mat_chrome_glow].x)] = TH_GLOW_MAT_FLAG;

  }






  fn_vec3 max_shadow;
  fn_vec3 min_shadow;
  l->center_sunlight = fn_multVec3s(fn_addVec3(l->ls.world->octree.max_position,l->ls.world->octree.min_position),0.5);
  getShadowBounds(l,&max_shadow,&min_shadow);
  l->orthomat_sunlight = fn_ortho3D(min_shadow.x,max_shadow.x,min_shadow.y,max_shadow.y,max_shadow.z,min_shadow.z);

  float dim_ortho = fmax(fabs(max_shadow.x - min_shadow.x),fabs(max_shadow.y - min_shadow.y));
  printf("SHADOW MAP SCALE %f\n",dim_ortho);
  //28302.97119*dim_ortho;//0.0005*dim_ortho;
  //28302.97119*dim_ortho;//0.0005*dim_ortho;

  if (l->autoGrid)
  {
    fn_vec3 gridmin = l->ls.world->octree.min_position;
    fn_vec3 gridmax = l->ls.world->octree.max_position;
    // gridmax = fn_addVec3(gridmax,fn_createVec3s(300));
    // gridmin = fn_subVec3(gridmin,fn_createVec3s(300));
    float dx = ceil(fabs(gridmax.x - gridmin.x)/l->gridsize);
    float dy = ceil(fabs(gridmax.y - gridmin.y)/l->gridsize);
    float dz = ceil(fabs(gridmax.z - gridmin.z)/l->gridsize);
    printf("GDIMS AUTO %f %f %f\n",dx,dy,dz );

    l->gridpos = fn_multVec3s(fn_addVec3(gridmax,gridmin),0.5);
    l->griddims = fn_createVec3((int)dx,(int)dy,(int)dz);
    //l->gridsize = 100.0;
  }


  // printf("%s\n","shadow bounds" );
  // fn_printVec3(l->ls.world->octree.max_position);
  // fn_printVec3(l->ls.world->octree.min_position);
  // fn_printVec3(max_shadow);
  // fn_printVec3(min_shadow);
  //return th_loadLevelTitleScreen(gencubemaps,sharm,cubemapDepth,cubemapColor);

  //printf("GDIMS %f %f %f\n",dx,dy,dz );
  printf("Track name %s\n",l->trackname);
  #ifndef DISABLE_AUDIO
  #define MATCH_TRACK(name)                     \
  do {                                      \
    if (strcmp(l->trackname, #name) == 0) \
      mus_track = name;                 \
  } while (0)


  int mus_track = music_track_1999;
  MATCH_TRACK(music_track_1999);
  MATCH_TRACK(music_track_nowhere);
  MATCH_TRACK(music_track_outmyface);
  MATCH_TRACK(music_track_printer);
  MATCH_TRACK(music_track_punch);
  MATCH_TRACK(music_track_thehum);

  if (strcmp(l->trackname,"random") == 0)
  {
    mus_track = a_getRandomMusicTrack();
  }

  #undef MATCH_TRACK


  a_VirtualSource* track = a_playMusicTrack(mus_track,0);
  a_setGainMusicTrack(0.0,0);
  a_pauseVS(track);
#endif

}
