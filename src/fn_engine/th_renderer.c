#include "th_renderer.h"
#include "th_time.h"
#include "../th_fopen.h"
#include <ft2build.h>

#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftstroke.h>

void th_initRendererState(th_RendererState* state)
{
    state->exposure_display = 0.0;
    state->gamma_display = 2.2;

    state->bardrone_source = NULL;
    state->victory_alphas = fn_createVec3(0,0,0);
    state->actual_dt = 1.0;
    state->time_mult = 1.0;
    state->th_photomode_enabled = false;
    state->th_inphoto = false;
    state->photo_camera_pos = fn_createVec3(0,0,0);
    state->photo_camera_angles = fn_createVec2(0,0);
    state->photo_camera_target = fn_createVec3(0,0,0);
    state->photo_camera_track = false;
    state->photo_camera_backup = false;
    state->photo_camera_done = false;
    state->photo_camera_time_to_pan = 1.0;
    state->photo_camera_pan_x = 0.0;
    state->photo_camera_pan_y = 0.0;
    state->photo_camera_follow = false;
    state->photo_camera_locked = false;


    state->th_no_hud = false;
    state->th_ambient_occlusion = true;
    state->th_depth_prepass = true;
    state->th_decal_rendering = true;
    state->th_frustumcull_enabled = true;
    state->gameState_return = STATE_MAINMENU;
    state->gameState = STATE_MAINMENU;
    state->th_crosshair_size = 1.0;
    state->gridpos = (fn_vec3){.x = -23,.y = -75,.z = 196};
    state->griddims = (fn_vec3){.x = 43,.y = 23,.z = 53};
    state->gridsize = 12;

    state->th_pov_sun_shadow = false;
    state->th_devconsole = true;
    state->th_particles = true;
    state->th_dynamic_shadows = true;
    state->th_reflections = true;
    state->reflections_scale = 1.0;
    state->fog_scale = 1.0;
    state->th_fog = true;
    state->th_directional_occlusion = false;
    state->th_print_position = true;
    state->th_temporalfilter = true;
    state->th_dynamic_objs = true;
    state->th_spatial_filter = true;
    state->th_tesselation = false;
    state->th_render_to_buffer = false;
    state->th_render_to_cubemap = false;
    state->th_dynamiclights = true;
    state->th_render_to_cubemap_face=0;
    state->th_render_to_cubemap_texture=0;
    state->th_shadowmap_resolution = 1024;
    state->th_skyRadianceBoost = 1.0;
    state->th_flying = false;
    state->th_splinecam = false;
    state->th_renderui = true;
    state->th_do_rendering_flag = true;

    state->cubePositions = NULL;
    state->cubePositionsCount = 0;
    state->cubedims = 512;
    state->cubeLODS = 5.0;
/*
    state->zNear = 5.0;
    state->zFar = 20000;*/

    state->zNear = 5.0;
    state->zFar = 28000;

    state->screen_dims = (fn_vec2){ .x = 1920.0f, .y = 1080.0f };

    state->occlusion_scale = 1;

    state->max_bones = 2048;

    state->particle_commands_count=0;

    state->light_commands_count=0;

    state->config_global = NULL;

    state->orad = 0;
    state->odist = 0;
    state->theta = 0;

    state->lights_new_count = 0;
    state->lights_new=NULL;
    state->access_new=NULL;
    state->access_cubes=NULL;
    state->offsets_new=NULL;
    state->offsets_new2=NULL;
    state->bonesindices_new = NULL;

    state->access_decals_new = NULL;
    state->decals_new_count = 0;
    state->decals_new = NULL;
    state->offsets_decals_new=NULL;

    state->visibility_data = NULL;
    state->th_load_new_level = false;
    state->th_resume_flag = false;
    state->th_respawn_flag = false;

    state->viewmats = malloc(sizeof(fn_mat4)*MAX_BATCH_LENGTH);
    state->positions = malloc(sizeof(fn_vec3)*MAX_BATCH_LENGTH);
    state->num_batches = 0;

    state->th_batch_render = false;
    state->stream_commands_count = 0;
    state->streamcommands = NULL;

}


void fn_extractFrustum(fn_vec4 frustum[6] ,fn_mat4 c)
{
    float   clip[16];
    float   t;


    memcpy(clip,c.m,sizeof(fn_mat4));



    /* Extract the numbers for the RIGHT plane */
    frustum[0].x = clip[ 3] - clip[ 0];
    frustum[0].y = clip[ 7] - clip[ 4];
    frustum[0].z = clip[11] - clip[ 8];
    frustum[0].w = clip[15] - clip[12];

    /* Normalize the result */
    t = sqrt( frustum[0].x * frustum[0].x + frustum[0].y * frustum[0].y + frustum[0].z * frustum[0].z );
    frustum[0].x /= t;
    frustum[0].y /= t;
    frustum[0].z /= t;
    frustum[0].w /= t;

    /* Extract the numbers for the LEFT plane */
    frustum[1].x = clip[ 3] + clip[ 0];
    frustum[1].y = clip[ 7] + clip[ 4];
    frustum[1].z = clip[11] + clip[ 8];
    frustum[1].w = clip[15] + clip[12];

    /* Normalize the result */
    t = sqrt( frustum[1].x * frustum[1].x + frustum[1].y * frustum[1].y + frustum[1].z * frustum[1].z );
    frustum[1].x /= t;
    frustum[1].y /= t;
    frustum[1].z /= t;
    frustum[1].w /= t;

    /* Extract the BOTTOM plane */
    frustum[2].x = clip[ 3] + clip[ 1];
    frustum[2].y = clip[ 7] + clip[ 5];
    frustum[2].z = clip[11] + clip[ 9];
    frustum[2].w = clip[15] + clip[13];

    /* Normalize the result */
    t = sqrt( frustum[2].x * frustum[2].x + frustum[2].y * frustum[2].y + frustum[2].z * frustum[2].z );
    frustum[2].x /= t;
    frustum[2].y /= t;
    frustum[2].z /= t;
    frustum[2].w /= t;

    /* Extract the TOP plane */
    frustum[3].x = clip[ 3] - clip[ 1];
    frustum[3].y = clip[ 7] - clip[ 5];
    frustum[3].z = clip[11] - clip[ 9];
    frustum[3].w = clip[15] - clip[13];

    /* Normalize the result */
    t = sqrt( frustum[3].x * frustum[3].x + frustum[3].y * frustum[3].y + frustum[3].z * frustum[3].z );
    frustum[3].x /= t;
    frustum[3].y /= t;
    frustum[3].z /= t;
    frustum[3].w /= t;

    /* Extract the FAR plane */
    frustum[4].x = clip[ 3] - clip[ 2];
    frustum[4].y = clip[ 7] - clip[ 6];
    frustum[4].z = clip[11] - clip[10];
    frustum[4].w = clip[15] - clip[14];

    /* Normalize the result */
    t = sqrt( frustum[4].x * frustum[4].x + frustum[4].y * frustum[4].y + frustum[4].z * frustum[4].z );
    frustum[4].x /= t;
    frustum[4].y /= t;
    frustum[4].z /= t;
    frustum[4].w /= t;

    /* Extract the NEAR plane */
    frustum[5].x = clip[ 3] + clip[ 2];
    frustum[5].y = clip[ 7] + clip[ 6];
    frustum[5].z = clip[11] + clip[10];
    frustum[5].w = clip[15] + clip[14];

    /* Normalize the result */
    t = sqrt( frustum[5].x * frustum[5].x + frustum[5].y * frustum[5].y + frustum[5].z * frustum[5].z );
    frustum[5].x /= t;
    frustum[5].y /= t;
    frustum[5].z /= t;
    frustum[5].w /= t;
}




void th_cullVisible(th_RendererState* state,fn_vec4* planes_frust)
{

    if (state->visibility_data == NULL)
    {
        state->visibility_data = malloc(sizeof(char)*state->level.culldata->aabbCount);
    }

    state->drawcommands_count_fc = 0;
    state->drawcommands_dynamic_count_fc = 0;
    int* drawcommands_fc_indices = calloc(256,sizeof(int));//[256];

    // if (th_frame() == 700)
    // {
    //     FILE* workloadfile = th_fopen("workload.dat","wb");
    //     fwrite(planes_frust,sizeof(fn_vec4),6,workloadfile);
    //     fwrite(&state->level.culldata->aabbCount,sizeof(int),1,workloadfile);
    //     fwrite(state->level.culldata->skip_culling_flag,sizeof(bool),state->level.culldata->aabbCount,workloadfile);
    //     fwrite(state->level.culldata->max_z,sizeof(float),state->level.culldata->aabbCount,workloadfile);
    //     fwrite(state->level.culldata->max_y,sizeof(float),state->level.culldata->aabbCount,workloadfile);
    //     fwrite(state->level.culldata->max_x,sizeof(float),state->level.culldata->aabbCount,workloadfile);
    //     fwrite(state->level.culldata->min_z,sizeof(float),state->level.culldata->aabbCount,workloadfile);
    //     fwrite(state->level.culldata->min_y,sizeof(float),state->level.culldata->aabbCount,workloadfile);
    //     fwrite(state->level.culldata->min_x,sizeof(float),state->level.culldata->aabbCount,workloadfile);
    //     fclose(workloadfile);
    // }

    if (state->th_frustumcull_enabled)
    {
        th_frustumCull(state->level.culldata,state->visibility_data,planes_frust);
    }


    for (int i = 0 ; i < state->level.cull_commands_count;i++)
    {
        bool visible = false;
        th_FrustumCullCommand c = state->level.cull_commands[i];
        th_RenderCommand r = state->level.render_commands[c.cpu_render_command_id];

        int model_id = r.model_id;
        if (c.obj_class == TH_STATIC_OBJECT)
        {
            model_id = c.gpu_render_command_id;
        }

        int matcount = 0;

        if (c.obj_class == TH_STATIC_OBJECT)
        {
            matcount = state->final.instance_counts[model_id];
        }
        else
        {
            matcount = (*r.matcount);
        }

        int matcount_fc_dynamic = 0;
        int matcount_fc = 0;
        if (c.obj_class == TH_UNCULLED_OBJECT || state->rp->th_disable_frustum_culling || !state->th_frustumcull_enabled)
        {
            visible = true;
            matcount_fc = matcount;
            th_setInstances(&state->model_data,r.offset + state->final.offsets_instances[r.model_id],*r.mats,*r.matcount,r.stride);
        }
        else
        {


            // printf("%i %i\n",c.cpu_render_command_id,c.gpu_render_command_id );
            // printf("%p\n",r.matcount );

            static int mats_fc_alloc = 0;
            static fn_mat4* mats_fc = NULL;
            static fn_mat4* mats_fc_dynamic = NULL;

            if (mats_fc == NULL)
            {
                mats_fc = malloc(sizeof(fn_mat4)*matcount);
                mats_fc_dynamic = malloc(sizeof(fn_mat4)*matcount);
                mats_fc_alloc = matcount;
            }
            if (matcount > mats_fc_alloc)
            {
                free(mats_fc);
                free(mats_fc_dynamic);
                mats_fc = malloc(sizeof(fn_mat4)*matcount);
                mats_fc_dynamic = malloc(sizeof(fn_mat4)*matcount);
                mats_fc_alloc = matcount;
            }

            fn_mat4* mptr = NULL;
            if (c.obj_class == TH_STATIC_OBJECT)
            {
                mptr = &state->final.data.instances[state->final.offsets_instances[model_id]];
            }
            else
            {
                mptr = (*r.mats);
            }

            for(int j = c.aabbs_start;j < c.aabbs_start + matcount;j++)
            {
                if (state->visibility_data[j] || (!state->staticSampled && c.obj_class == TH_STATIC_OBJECT) )
                {
                    visible = true;
                    mats_fc[matcount_fc] = mptr[j - c.aabbs_start];
                    matcount_fc++;
                }
                if (!state->visibility_data[j]  && c.obj_class == TH_DYNAMIC_OBJECT)
                {
                    mats_fc_dynamic[matcount_fc_dynamic] = mptr[j - c.aabbs_start];
                    matcount_fc_dynamic++;
                }
            }

            //allow for dynamic shadows, extra memory containign other instance data
            if (c.obj_class == TH_DYNAMIC_OBJECT)
            {
                memcpy(&mats_fc[matcount_fc],mats_fc_dynamic,sizeof(fn_mat4)*matcount_fc_dynamic);
            }

            th_setInstances(&state->model_data,0 + state->final.offsets_instances[model_id],mats_fc,matcount,1);


        }

        if (visible)
        {
            drawcommands_fc_indices[state->drawcommands_count_fc] = c.gpu_render_command_id;
            state->drawcommands_fc[state->drawcommands_count_fc] = state->drawcommands[c.gpu_render_command_id];
            state->drawcommands_fc[state->drawcommands_count_fc].instanceCount = matcount_fc;
            state->drawcommands_count_fc++;
            if (c.obj_class == TH_DYNAMIC_OBJECT || c.obj_class == TH_UNCULLED_OBJECT)
            {
                state->drawcommands_dynamic_count_fc++;
            }
        }
    }

    for (unsigned int i = 0 ; i < state->drawcommands_count_fc;i++)
    {
        state->drawcommands_fc[i].baseInstance = state->final.offsets_instances[drawcommands_fc_indices[i]] + state->final.data.instancecount*(th_frame()%3);
    }

    th_setCommandBuffer(&state->model_data,state->drawcommands_fc,state->drawcommands_count_fc);

    free(drawcommands_fc_indices);
}



th_GpuDataOffsets th_mergeGpuData(th_Allocator* alloc,th_GpuData* data,int count,th_RenderEnum flags)
{
    bool useIds = flags & TH_TEXTUREID;
    bool useanim = flags & TH_ANIMATED;
    bool increment_indices = flags & TH_INCREMENT_INDICES;//make it so that you can render different meshes in 1 call
    th_GpuDataOffsets ret;
    ret.offsets_verts = th_alloc(alloc,sizeof(GLuint)*count);
    ret.offsets_indices = th_alloc(alloc,sizeof(GLuint)*count);
    ret.offsets_instances = th_alloc(alloc,sizeof(GLuint)*count);
    ret.instance_counts = th_alloc(alloc,sizeof(GLuint)*count);
    ret.element_counts = th_alloc(alloc,sizeof(GLuint)*count);
    int total_verts = 0;
    int total_indices = 0;
    int total_instances = 0;
    for (int i = 0 ; i< count;i++)
    {
        ret.offsets_verts[i] = total_verts;
        ret.offsets_indices[i] = total_indices;
        ret.offsets_instances[i] = total_instances;
        ret.instance_counts[i] = data[i].instancecount;
        ret.element_counts[i] = data[i].indicecount;

        total_verts += data[i].vertcount;
        total_indices += data[i].indicecount;
        total_instances += data[i].instancecount;
    }
    if (useanim)
    {
        ret.data.animverts = th_alloc(alloc,sizeof(th_AnimVertex)*total_verts);
    }
    else
    {
        ret.data.verts = th_alloc(alloc,sizeof(th_Vertex)*total_verts);
    }

    ret.data.indices = th_alloc(alloc,sizeof(GLuint)*total_indices);
    ret.data.instances = th_alloc(alloc,sizeof(fn_mat4)*total_instances);
    if (useIds)
    {
        ret.data.ids = th_alloc(alloc,sizeof(fn_vec2)*total_instances);
    }
    ret.data.vertcount = total_verts;
    ret.data.indicecount = total_indices;
    ret.data.instancecount = total_instances;

    for (int i = 0 ; i< count;i++)
    {
        if (useanim)
        {
            if (data[i].vertcount > 0)
            {
                memcpy(&ret.data.animverts[ret.offsets_verts[i]],data[i].animverts,sizeof(th_AnimVertex)*data[i].vertcount);
            }

        }
        else
        {
            if (data[i].vertcount > 0)
            {
                memcpy(&ret.data.verts[ret.offsets_verts[i]],data[i].verts,sizeof(th_Vertex)*data[i].vertcount);
            }

        }
        if (data[i].indicecount > 0)
        {
            memcpy(&ret.data.indices[ret.offsets_indices[i]],data[i].indices,sizeof(GLuint)*data[i].indicecount);
            if (increment_indices)
            {
                for (GLuint j = 0 ; j < data[i].indicecount;j++)
                {
                    ret.data.indices[ret.offsets_indices[i] + j] = ret.data.indices[ret.offsets_indices[i] + j] + ret.offsets_verts[i];
                }
            }
        }

        if (data[i].instancecount > 0 )
        {
            memcpy(&ret.data.instances[ret.offsets_instances[i]],data[i].instances,sizeof(fn_mat4)*data[i].instancecount);
        }

        if (useIds)
        {
            if (data[i].instancecount)
            {
                memcpy(&ret.data.ids[ret.offsets_instances[i]],data[i].ids,sizeof(fn_vec2)*data[i].instancecount);
            }

        }
    }
    return ret;
}



static void renderUIElement(th_UIElement* elem,th_UIlayout* layout,th_RendererState* state,int i,bool* dynamic_tooltip,char* dynamic_tooltip_str,th_UIElement** tooltip_elem)
{
    r_Shader* default_shader = &state->glyph_shader;
    if (elem->custom_shader != NULL)
    {
        default_shader = elem->custom_shader;
        th_setTextGlyphMagnitude(elem->magnitude_wavy);
    }
    if (elem->type == TH_UI_TEXT)
    {
        th_setTextGlyphOffsets(elem->y_offsets,TH_Y_OFFSETS_UI);
        if (layout->highlighted_element == i || elem->highlighted)
        {
            th_renderTextGlyph(elem->text,elem->position,elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color_highlight);
        }
        else
        {
            th_renderTextGlyph(elem->text,elem->position,elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color);
        }

    }
    else if (elem->type == TH_UI_BUTTON)
    {
        if (elem->depressed)
        {
            th_renderTextGlyph(elem->dynamic_text,elem->position,elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color_highlight);
        }
        else
        {
            th_renderTextGlyph(elem->dynamic_text,elem->position,elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color);
        }

        if (elem->hovered)
        {
            if ( elem->disable_button_when_true == NULL || (elem->disable_button_when_true != NULL &&  !(*elem->disable_button_when_true)))
            {
                *tooltip_elem = elem;
            }

        }

    }
    else if (elem->type == TH_UI_SLIDER)
    {
        th_renderTextGlyph(elem->text,elem->position,elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color);
        float cursor_x = fn_lerp(elem->min_x,elem->max_x,elem->slider_position);
        th_renderTextGlyph("I",fn_createVec2(cursor_x,elem->position.y),elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color_highlight);

        if (elem->selected_cursor)
        {
            *dynamic_tooltip = true;
            *tooltip_elem = elem;

            if (elem->integer_slider)
            {
                int ival = floor((float)elem->imin + (elem->slider_position * (float)(elem->imax - elem->imin)));
                snprintf(dynamic_tooltip_str,128,"%i",ival);
            }
            else
            {
                float fval = ((float)elem->fmin + (elem->slider_position * (float)(elem->fmax - elem->fmin)));
                snprintf(dynamic_tooltip_str,128,"%.2f",fval);
            }

        }
    }
    else if (elem->type == TH_UI_KEYBIND)
    {
        if (elem->depressed)
        {
            th_renderTextGlyph(elem->dynamic_text,elem->position,elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color_highlight);
        }
        else
        {
            th_renderTextGlyph(elem->dynamic_text,elem->position,elem->size,1.0,default_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->cmap,elem->color);
        }
    }
    else if (elem->type == TH_UI_IMAGE)
    {
        r_Shader* imshader = elem->nofade ? &state->image_shader_nofade : &state->image_shader;

        fn_vec2 scale_offset = fn_multVec2s(elem->dims,(1.0 - elem->scale_central)*0.5);
        th_renderImage(elem->textures[0],fn_addVec2(elem->position,scale_offset),fn_multVec2s(elem->dims,elem->scale_central),imshader,state->postmatrix,&state->text_gpu,&state->text_data,elem->alpha);
    }
    else if (elem->type == TH_UI_PROGRESS)
    {
        fn_vec2 scale_offset = fn_multVec2(elem->dims,fn_createVec2(0.0,(1.0 - elem->scale_central)*0.5));
        th_renderShaderQuad(fn_addVec2(elem->position,scale_offset),fn_multVec2(elem->dims,fn_createVec2(1.0,elem->scale_central)),&state->progress_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->alpha,elem->tint);
    }
    else if (elem->type == TH_UI_GRADIENT)
    {
        //fn_vec2 scale_offset = fn_multVec2(elem->dims,fn_createVec2(0.0,(1.0 - elem->scale_central)*0.5));
        th_renderShaderQuad(elem->position,elem->dims,&state->gradient_shader,state->postmatrix,&state->text_gpu,&state->text_data,elem->alpha,elem->tint);
    }

    if (elem->custom_shader != NULL)
    {
        th_setTextGlyphMagnitude(35.0);
    }
    th_setTextGlyphOffsets(NULL,0);
}

void th_renderUI(th_UIlayout* layout,th_RendererState* state)
{
    th_UIElement* tooltip_elem = NULL;
    bool dynamic_tooltip = false;
    char dynamic_tooltip_str[128];




    for (int i = 0 ; i < layout->element_count;i++)
    {
        th_UIElement* elem = &layout->elements[i];
        if (!elem->visible)
        {
            continue;
        }

        if (elem->z_order != -2)
        {
            renderUIElement(elem,layout,state,i,&dynamic_tooltip,dynamic_tooltip_str,&tooltip_elem);
        }
    }

    for (int i = 0 ; i < layout->element_count;i++)
    {
        th_UIElement* elem = &layout->elements[i];
        if (!elem->visible)
        {
            continue;
        }

        if (elem->z_order == -2)
        {
            renderUIElement(elem,layout,state,i,&dynamic_tooltip,dynamic_tooltip_str,&tooltip_elem);
        }

    }



    if (dynamic_tooltip)
    {
        th_renderTextGlyph(dynamic_tooltip_str,tooltip_elem->hover_pos,0.3,1.0,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,tooltip_elem->cmap,fn_createVec3(1,1,1));
    }
    else if (tooltip_elem != NULL)
    {
        th_renderTextGlyph(tooltip_elem->tooltip,tooltip_elem->hover_pos,0.3,1.0,&state->glyph_shader,state->postmatrix,&state->text_gpu,&state->text_data,tooltip_elem->cmap,fn_createVec3(1,1,1));
    }
}

void th_renderFontTextures(th_Character* character_map,fn_Config* config)
{
    FT_Library ft;
    FT_Init_FreeType(&ft);

    FT_Face face;
    int result = FT_New_Face(ft, "th1/fonts/Cageroll-Standard.otf", 0, &face);
    if(result != 0)
    {
        printf("%s\n","font Failed" );
    }

    FT_Set_Pixel_Sizes(face, 0, 128.0*(th_getAspectScale()));



    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

    int bordersize = 3*(th_getAspectScale());

    for (unsigned int iter = 0; iter < 256; iter++)
    {
        unsigned char  c = iter % 128;
        // load character glyph


        FT_Stroker stroker;
        FT_Stroker_New(ft, &stroker);
        //  2 * 64 result in 2px outline
        FT_Stroker_Set(stroker, 128*bordersize, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

        if (FT_Load_Char(face, c, FT_LOAD_DEFAULT))
        {
            printf("Failed To load character %c\n",c );
            continue;
        }
        FT_Glyph glyph;
        FT_Get_Glyph(face->glyph, &glyph);
        if (iter >= 128)
        {
            FT_Glyph_StrokeBorder(&glyph, stroker, false,false);
        }

        // FT_Render_Glyph(face->glyph,FT_RENDER_MODE_NORMAL);

        FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, true);
        FT_BitmapGlyph bitmapGlyph = (FT_BitmapGlyph)(glyph);
        // if (glyph->format == FT_GLYPH_FORMAT_OUTLINE)
        // {
        //   printf("%s\n","OUU" );
        // }



        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            bitmapGlyph->bitmap.width,
            bitmapGlyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            bitmapGlyph->bitmap.buffer
        );


        // printf("%i\n",face->glyph->bitmap.width);
        // printf("%i\n",face->glyph->bitmap.rows);
        // // printf("%i\n",face->glyph->bitmap.buffer);
        // printf("%i\n",face->glyph->bitmap_left);
        // printf("%i\n",face->glyph->bitmap_top);

        character_map[iter].size.x = bitmapGlyph->bitmap.width;
        character_map[iter].size.y = bitmapGlyph->bitmap.rows;

        character_map[iter].bearing.x = face->glyph->bitmap_left;
        character_map[iter].bearing.y = face->glyph->bitmap_top;

        if (iter >= 128)
        {
            character_map[iter].bearing.x -= bordersize*2;
            character_map[iter].bearing.y += bordersize*2;
        }
        character_map[iter].textureID = texture;
        character_map[iter].advance = face->glyph->advance.x;
        //printf("%i %i %i %i %li\n",face->glyph->bitmap.width,face->glyph->bitmap.rows,face->glyph->bitmap_left,face->glyph->bitmap_top,face->glyph->advance.x >> 6 );
        FT_Stroker_Done(stroker);
        FT_Done_Glyph(glyph);
    }



    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}
