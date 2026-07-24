#pragma once
#include "th_gpu.h"
#include "r_shader.h"
#include "th_allocator.h"
#include "th_level.h"
#include "../fn_math/fn_spline.h"
#include "th_replay.h"
#include "../fn_config.h"
#define MAX_POINT_SHADOWS 48
#include "th_ui.h"
#define MAX_BATCH_LENGTH 63504
#include "th_audio.h"

typedef enum
{
    STATE_MAINMENU,
    STATE_GAMEPLAY,
    STATE_PAUSED,
    STATE_OPTIONS_MENU,
    STATE_LEVELSELECT,
    STATE_QUIT,
    STATE_DEATH,
    STATE_VICTORY,
}th_GameStateEnum;


typedef struct
{
    fn_vec3 gridpos;
    fn_vec3 griddims;
    float gridsize;

    int cube_count;




    bool th_pov_sun_shadow;
    bool th_devconsole;
    bool th_particles;
    bool th_dynamic_shadows;
    bool th_reflections;
    float reflections_scale;
    float fog_scale;
    bool th_fog;
    bool th_directional_occlusion;
    bool th_print_position;
    bool th_temporalfilter;
    bool th_dynamic_objs;
    bool th_spatial_filter;
    bool th_tesselation;
    bool th_render_to_buffer;
    bool th_render_to_cubemap;
    bool th_dynamiclights;
    GLuint th_render_to_cubemap_face;
    GLuint th_render_to_cubemap_texture;
    GLuint th_shadowmap_resolution;
    float th_skyRadianceBoost;
    bool th_flying;
    bool th_splinecam;
    bool th_renderui;
    bool th_do_rendering_flag;

    r_Shader image_shader_nofade;
    r_Shader image_shader;
    r_Shader text_shader;
    th_ArrayObject text_gpu;
    th_GpuData text_data;
    fn_mat4 textmatrix;

    r_Shader noshading;
    r_Shader spharmonics;
    r_Shader spharmonics2;
    r_Shader cubemaps;

    r_Shader post;
    th_ArrayObject post_gpu;
    th_GpuData post_data;
    fn_mat4 postmatrix;

    fn_mat4 projection;
    r_Shader pbr;
    th_ArrayObject model_data;
    //th_GpuData model;
    //th_GpuData model2;
    th_GpuDataOffsets final;
    fn_vec2 angles;
    fn_vec3 pos;
    fn_vec3 look_global;
    // th_FrameBuffer framebuffer;
    r_DrawElementsIndirectCommand* drawcommands;
    GLuint drawcommands_count;
    GLuint drawcommands_dynamic_count;

    r_DrawElementsIndirectCommand* drawcommands_fc;
    GLuint drawcommands_count_fc;
    GLuint drawcommands_dynamic_count_fc;

    r_DrawElementsIndirectCommand* drawcommands_tess;
    GLuint drawcommands_count_tess;

    fn_vec2* handles;
    GLuint handles_count;

    fn_vec2 dims_harmtex;
    unsigned int noiseTex;
    //unsigned int fontTex;
    // fn_vec3 cubepos = {{20.0,0.0,0.0}};
    fn_vec3* cubePositions;
    int cubePositionsCount;
    float cubedims;
    float cubeLODS;

    r_Shader prefiltershader;

    GLuint cmaptex;

    r_Shader ssrshader;
    r_Shader skyshader;
    r_Shader night_skyshader;
    r_Shader skyboxshader;
    GLuint skybox;

    float zNear;
    float zFar;

    GLuint occ_tex;



    unsigned int brdfLUTTexture;
    GLuint lightTex;
    GLuint cubemapDepth;
    GLuint cubemapColor;
    fn_vec3* cubeMins;
    fn_vec3* cubeMaxs;
    // fn_vec3 cubemin;
    // fn_vec3 cubemax;

    th_FrameBuffer shadowBuffer;
    fn_vec3 shadowCasterPos;
    fn_vec3 shadowCasterDir;
    r_Shader shadowOccluder;
    bool shadowSampled;
    bool staticSampled;
    bool staticSampledPoint[MAX_POINT_SHADOWS];
    GLuint shadowCache_tex;

    fn_vec2 screen_dims;

    r_Shader tess_shader;
    r_Shader tess_shader_noshading;
    r_Shader tess_shader_occluder;
    th_GpuDataOffsets final_tess;
    th_ArrayObject model_data_tess;
    th_CommandBuffer tess_shadowcaster_commands;
    GLuint drawcommands_count_tessshadow;
    r_DrawElementsIndirectCommand* drawcommands_tessshadow;
    GLuint* drawcommands_tessshadow_indices;

    th_CommandBuffer dynamic_shadowcaster_commands;
    GLuint drawcommands_count_dynamicshadowcaster;
    r_DrawElementsIndirectCommand* drawcommands_dynshadow;
    GLuint* drawcommands_dynshadow_indices;

    r_Shader pbr_deferred;
    r_Shader directional_occlusion;
    r_Shader occlusion_blur;
    r_Shader occlusion_upsample;
    float occlusion_scale;
    r_Shader deferredtex;

    th_ArrayObject guy_array;
    r_DrawElementsIndirectCommand* animcommands;
    th_GpuDataOffsets guy_data;
    GLuint animcommands_count;
    r_Shader noshading_anim;
    fn_mat4* mapped_AnimUBO;
    GLuint bonesUBO;
    r_VboSync bonesSyncs[3];
    th_Model guy_model;

    unsigned int scale_temp_store;

    r_Shader occluder_anim;

    // th_World world;
    // th_Entity player;

    GLuint* mapped_acessubocubes;
    GLuint accesscubesUBO;

    GLuint* mapped_acessubo;
    GLuint accessUBO;

    th_uvec4* mapped_offsetsubo;
    GLuint offsetsUBO;

    th_PointLight* mapped_pointlights;
    GLuint lightsUBO;

    th_FrameBuffer noshading_fb;
    th_FrameBuffer deferred_texture_fb;
    th_FrameBuffer cubetarget_fb;
    th_FrameBuffer cubepass_fb;
    th_FrameBuffer reflections_fb;
    th_FrameBuffer forward_fb;

    th_FrameBuffer blurpass_fb;

    th_FrameBuffer fogpass_fb;

    th_FrameBuffer cheap_ao_fb;

    th_ArrayObject cheap_ao_array;
    th_GpuOccluderData cheap_ao_data;
    r_Shader cheap_ao_shader;


    /*
     * 16x16 tiles
     * 120x68 tile buffer for 1080p
     */

    GLuint tiled_dummy_vao; //used to render with no vertex inputs

    GLuint tiled_data;//ssbo for tiled depth bounds, bitmask, aabb
    GLuint tiled_light_lists;//ssbo for light lists, count and an array of uints NEEDS TO BE CLEARED
    GLuint tiledaoUBO;//mapped ubo for uploading the AO volumes

    fn_vec4* mapped_ao_volumes;

    r_Shader tiled_depth_bound_shader;//calculate depth bounds and the 2.5d bitmask in compute
    r_Shader tiled_raster_shader;//render screen space quads for each light (instanced), do the light list building
    r_Shader tiled_ao_shading;//read light lists and shade AO

    int num_tiled_div4;
    int num_uints_div4;
    int num_tiles_x;
    int num_tiles_y;

    bool th_ambient_occlusion;

    th_QueryBuffer timer;
    th_BoidGroup boids_group;

    GLuint old_reflectiondata;
    GLuint old_depthtexture;
    GLuint oldnormalTex;

    r_Shader decal_apply;

    r_Shader gaussian_blur;

    r_Shader temporal_shader;

    r_Shader median_blur;

    r_Shader forward_particle;

    r_Shader forward_light;

    r_Shader glyph_shader;

    r_Shader progress_shader;

    r_Shader wavytext_shader;

    r_Shader fog_shader;

    r_Shader gradient_shader;

    GLuint fontcolortex;

    GLuint final_image_texture;

    th_FrameBuffer blurfog_a;
    th_FrameBuffer blurfog_b;

    r_Shader fog_blur_shader_a;
    r_Shader fog_blur_shader_b;

    th_FrameBuffer occlusion_fb;

    th_LevelDescriptor level;
    int max_bones;

    GLuint boneindicesUBO;
    GLuint* mapped_boneindices;

    fn_mat4 old_mvp;
    fn_mat4 projMatrixInv_old;
    fn_mat4 viewMatrixInv_old;

    GLuint offsetsDecalUBO;
    th_uvec4* mapped_offsetsDecal;
    GLuint accessDecalUBO;
    GLuint* mapped_accessDecal;
    GLuint decalsUBO;
    th_Decal* mappedDecals;

    th_ArrayObject particle_data;
    int particle_commands_count;
    th_GpuDataOffsets final_particle;
    r_DrawElementsIndirectCommand* drawcommands_particle;

    th_ArrayObject light_data;
    int light_commands_count;
    th_GpuDataOffsets final_light;
    r_DrawElementsIndirectCommand* drawcommands_light;

    fn_SplineState camera_spline;
    fn_Config* config_global;

    float orad;
    float odist;
    float theta;

    uint lights_new_count;
    th_PointLight* lights_new;
    GLuint* access_new;
    GLuint* access_cubes;
    GLuint* offsets_new;
    GLuint* offsets_new2;
    th_uvec2* bonesindices_new;

    GLuint* access_decals_new;
    int decals_new_count;
    th_Decal* decals_new;
    GLuint* offsets_decals_new;

    fn_mat4 view;
    fn_mat4 modelViewprojection;
    fn_mat4 invProj;
    fn_mat4 invView;

    char* visibility_data;

    GLuint EnvBoxesUbo;

    GLuint MaterialPropertiesUbo;

    th_ReplayData* rp;
    float th_crosshair_size;

    th_GameStateEnum gameState;
    th_GameStateEnum gameState_return;

    th_UIlayout mainMenuLayout;
    th_UIlayout optionsMenuLayout;
    th_UIlayout levelSelectLayout;


    th_UIlayout pauseMenuLayout;
    th_UIlayout deathMenuLayout;
    th_UIlayout victoryMenuLayout;

    th_UIlayout hudLayout;

    th_LevelManifest list_of_levels;
    th_VictoryManifest list_of_victory;

    fn_vec3 victory_alphas;

    //state transition flags
    bool th_load_new_level;
    bool th_resume_flag;
    bool th_respawn_flag; //if true, dont unload textures


    bool th_depth_prepass;
    bool th_decal_rendering;
    bool th_frustumcull_enabled;

    bool th_batch_render;
    int batch_atlas_res;

    int num_batches;
    fn_mat4* viewmats;
    fn_vec3* positions;

    bool th_no_hud;

    bool th_photomode_enabled;
    bool th_inphoto;
    fn_vec3 photo_camera_pos;
    fn_vec2 photo_camera_angles;

    fn_vec3 photo_camera_target;
    bool photo_camera_track;
    bool photo_camera_backup;
    bool photo_camera_done;
    bool photo_camera_follow;
    float photo_camera_time_to_pan;
    float photo_camera_pan_x;
    float photo_camera_pan_y;
    bool photo_camera_locked;//used for title screen

    float time_mult;


    //dynamically streamed geometry, not frustum culled
    //do merge in pre-processing, assume same layout in subsequent frames
    th_ArrayObject dynamic_geom_array;
    th_GpuDataOffsets dynamic_geom_offsets;
    //create gl commands like
    //base vert mesh 0 + frame offset, instance offset 0, 1 inst
    //base vert mesh 1 + frame offset, instance offset 1, 1 inst

    int stream_commands_count;
    r_DrawElementsIndirectCommand* streamcommands;

    float actual_dt;

    a_VirtualSource* bardrone_source;


    th_FrameBuffer internal_fb;

    float exposure_display;
    float gamma_display;

}th_RendererState;



void th_initRendererState(th_RendererState* state);

void fn_extractFrustum(fn_vec4 frustum[6] ,fn_mat4 c);
void th_cullVisible(th_RendererState* state,fn_vec4* planes_frust);

th_GpuDataOffsets th_mergeGpuData(th_Allocator* alloc,th_GpuData* data,int count,th_RenderEnum flags);

void th_renderUI(th_UIlayout* layout,th_RendererState* state);

void th_renderFontTextures(th_Character* character_map,fn_Config* config);
