#include "r_shader.h"
#include "th_gpu.h"
#include "r_mesh.h"
#include <stdlib.h>
#include "../fn_window.h"
#include "r_texture.h"
#include "../fn_input.h"
#include "th_time.h"
#include "th_harmonics.h"
#include "th_cubemap.h"
#include <math.h>
#include <pthread.h>
#include "th_iqm.h"
#include "th_physics.h"
#include <time.h>

#include <limits.h>

#include "th_clusters.h"
#include "th_level.h"
#include "../fn_game/th_boids.h"
#include "../fn_game/th_builtins.h"
#include "th_decal.h"
#include "th_particle.h"

#include "bluenoise.h"

#include "th_audio.h"
#include "th_program.h"

#include <ft2build.h>

#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftstroke.h>
#include "../fn_math/fn_spline.h"
#include "th_system.h"
#include "th_baking.h"
#include "th_program.h"
#include "../th_fopen.h"

// fn_vec3 quantify(fn_vec3 position)
// {
//     return fn_subVec3(fn_floorVec3(fn_addVec3(fn_multVec3s(fn_subVec3(position,state->gridpos),1.0/state->gridsize),fn_multVec3s(state->griddims,0.5))) , fn_createVec3(1,1,1));
// }
// float indexify(fn_vec3 quant)
// {
//     return fn_clamp((quant.x * state->griddims.z * state->griddims.y) + (quant.y * state->griddims.z) + quant.z,0,state->griddims.x*state->griddims.y*state->griddims.z);
// }

static fn_mat4 r_camera(fn_vec3 pos,fn_vec2 angles)
{

    angles.y = fn_clamp(angles.y,-fn_radians(90),fn_radians(90));
    fn_mat4 modelView = fn_identityMat4();//glm::lookAt(-getLocation(),getLook(),glm::vec3(0,-1,0));
    //modelView = glm::lookAt(getLocation(),getLook(),glm::vec3(0,1,0));
    //  modelView = fn_rotate(modelView, -glm::radians(camRoll), glm::vec3(0.0f, 0.0f, 1.0f));
    modelView = fn_rotate(modelView, angles.y, fn_createVec3(1.0f, 0.0f, 0.0f));
    modelView = fn_rotate(modelView, angles.x, fn_createVec3(0.0f, 1.0f, 0.0f));
    //  modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
    modelView = fn_translate(modelView,fn_multVec3(pos,fn_createVec3(1,1,1)));//*glm::vec3(-1,1,-1));
    modelView = fn_scale(modelView,fn_createVec3(-1,-1,-1));
    return modelView;
}

static fn_vec3 fn_getPositionFromDepth(float depth,fn_vec2 texCoord,fn_mat4 view,fn_mat4 proj)
{
    fn_mat4 invproj = fn_inverse(proj);
    fn_mat4 invview = fn_inverse(view);

    float z = depth * 2.0 - 1.0;

    fn_vec4 clipSpacePosition;
    clipSpacePosition.x = texCoord.x *2.0 - 1.0;
    clipSpacePosition.y = texCoord.y *2.0 - 1.0;
    clipSpacePosition.z = z;
    clipSpacePosition.w = 1.0;
    fn_vec4 viewSpacePosition = fn_multVec4Mat4(invproj,clipSpacePosition);

    // Perspective division
    viewSpacePosition.xyz = fn_multVec3s(viewSpacePosition.xyz,1.f/viewSpacePosition.w);
    viewSpacePosition.w = 1.0;

    fn_vec4 worldSpacePosition = fn_multVec4Mat4(invview , viewSpacePosition);

    return worldSpacePosition.xyz;
}



th_CubemapGenData th_generateCubemaps(int resolution,SDL_Window* window,th_RendererState* state)
{
    #define miplevels 5
    th_CubemapGenData ret;
    ret.cube_count = state->cube_count;

    ret.minmaxdata = malloc(sizeof(GLfloat)*6*state->cube_count);
    ret.wad_data = malloc(sizeof(GLfloat)*6*resolution*resolution*state->cube_count);
    ret.color_data = malloc(sizeof(GLfloat*)*miplevels);
    for (int i = 0 ; i < miplevels;i++)
    {
        unsigned int mipWidth = resolution * powf(0.5, i);
        unsigned int mipHeight = resolution * powf(0.5, i);
        ret.color_data[i] = malloc(sizeof(GLfloat)*6*mipWidth*mipHeight*state->cube_count*3);
    }


    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    fn_vec3 cubemapFaceNormals[6][3];
    cubemapFaceNormals[0][0]=fn_createVec3(0,0,-1);cubemapFaceNormals[0][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[0][2]=fn_createVec3(1,0,0);
    cubemapFaceNormals[1][0]=fn_createVec3(0,0,1);cubemapFaceNormals[1][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[1][2]=fn_createVec3(-1,0,0);
    cubemapFaceNormals[2][0]=fn_createVec3(1,0,0);cubemapFaceNormals[2][1]=fn_createVec3(0,0,1);cubemapFaceNormals[2][2]=fn_createVec3(0,1,0);
    cubemapFaceNormals[3][0]=fn_createVec3(1,0,0);cubemapFaceNormals[3][1]=fn_createVec3(0,0,-1);cubemapFaceNormals[3][2]=fn_createVec3(0,-1,0);
    cubemapFaceNormals[4][0]=fn_createVec3(1,0,0);cubemapFaceNormals[4][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[4][2]=fn_createVec3(0,0,1);
    cubemapFaceNormals[5][0]=fn_createVec3(-1,0,0);cubemapFaceNormals[5][1]=fn_createVec3(0,-1,0);cubemapFaceNormals[5][2]=fn_createVec3(0,0,-1);


    th_FrameBuffer frame;
    th_PixelPackBuffer pbos =  th_createPixelPackBuffers(sizeof(GLfloat)*resolution*resolution*1,6);
    th_createFramebuffer(&frame,resolution,resolution);
    fn_mat4 proj = fn_perspective(fn_radians(90),1,state->zNear,state->zFar);
    state->postmatrix = fn_ortho(0,resolution,0,resolution);

    th_FrameBuffer prefilterbuffer;
    th_createFramebufferPrefilter(&prefilterbuffer,resolution,resolution);

    unsigned int cubeTex;
    glGenTextures(1, &cubeTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);


    for( int i = 0; i < 6; i++)
    {

        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, NULL
        );

    }

    unsigned int prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);


    for( int i = 0; i < 6; i++)
    {

        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, NULL
        );

    }
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);


    glBindTexture( GL_TEXTURE_CUBE_MAP, 0);

    th_bindGlobalTextures(state);

    fn_mat4 mats[6];

    GLuint mipPbos[6];
    GLfloat* mipPboData[6];
    r_VboSync syncs[6];

    glGenBuffers(6, mipPbos);
    for (int i = 0 ; i < 6;i++)
    {
        syncs[i].used = false;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, mipPbos[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(GLfloat)*resolution*resolution*3, 0, GL_STREAM_READ);
        mipPboData[i] = pboMapBuffer(sizeof(GLfloat)*resolution*resolution*3);
    }




    Uint32 start = SDL_GetTicks();
    //foreach position



    GLfloat* data[6];
    for (int i = 0 ; i < 6;i++)
    {
        data[i] = malloc(sizeof(GLfloat)*resolution*resolution*1);//depth buffer used for dist calc
    }


    GLfloat* filterData[miplevels][6];

    printf("%s\n","starting" );
    // FILE* fp = th_fopen("th1/cubemaps_gen.float","w");
    for (int l = 0 ; l < state->cube_count;l++)
    {
        printf("%s%i\n","Cube ",l );
        glBindTexture( GL_TEXTURE_CUBE_MAP, 0);

        th_bindGlobalTextures(state);


        state->pos = state->cubePositions[l];

        mats[1] = r_camera(state->pos,fn_createVec2(fn_radians(-90),fn_radians(0)));// negx
        mats[0] = r_camera(state->pos,fn_createVec2(fn_radians(90),fn_radians(0)));// posx
        mats[4] = r_camera(state->pos,fn_createVec2(fn_radians(0),fn_radians(0))); //posz
        mats[5] =  r_camera(state->pos,fn_createVec2(fn_radians(180),fn_radians(0)));//negz
        mats[3] = r_camera(state->pos,fn_createVec2(fn_radians(0),fn_radians(90)));//negy
        mats[2] = r_camera(state->pos,fn_createVec2(fn_radians(0),fn_radians(-90)));//posy
        state->th_render_to_cubemap = true;
        for (int i = 0 ; i < 6;i++)
        {
            state->th_render_to_cubemap_face = i;
            state->th_render_to_cubemap_texture = cubeTex;

            //
            // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + state->th_render_to_cubemap_face, state->th_render_to_cubemap_texture, 0);
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            fn_mat4 view = mats[i];
            fn_mat4 modelViewprojection = fn_multMat4(view,proj);
            glViewport(0,0,resolution,resolution);
            state->postmatrix = fn_ortho(0,resolution,0,resolution);


            // r_bindTextureForce(frame.textures[0],GL_TEXTURE_2D,0);
            th_render(modelViewprojection,proj,view,fn_createVec2(resolution,resolution),state);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos.pbos[i]);

            glGetTextureImage(	state->noshading_fb.depthTexture,	0,GL_DEPTH_COMPONENT,	GL_FLOAT,	sizeof(GLfloat)*resolution*resolution*1,0);
            if (syncs[i].used)
                glDeleteSync(syncs[i].sync);
            syncs[i].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
            syncs[i].used = true;

            th_tickFrame();
            SDL_GL_SwapWindow( window);

        }
        state->th_render_to_cubemap = false;
        for (int i = 0 ; i < 6;i++)
        {
            if (syncs[i].used)
            {

                while (1)
                {
                    GLenum waitReturn = glClientWaitSync(syncs[i].sync,
                                                            GL_SYNC_FLUSH_COMMANDS_BIT, 0);//
                    if (waitReturn == GL_ALREADY_SIGNALED ||
                        waitReturn == GL_CONDITION_SATISFIED)
                    {

                        break;
                    }


                }
            }
            memcpy(data[i],pbos.pbodata[i],sizeof(GLfloat)*resolution*resolution*1);
        }



        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

        glBindFramebuffer(GL_FRAMEBUFFER, prefilterbuffer.framebuffer);

        glDisable(GL_CULL_FACE);

        glDisable(GL_DEPTH_TEST);


        r_bindShader(&state->prefiltershader);
        r_sendTextureUniform(&state->prefiltershader,0,"environmentMap");
        r_sendf(&state->prefiltershader,resolution,"resolution");

        for (int i = 0 ; i < miplevels;i++)
        {
            unsigned int mipWidth = resolution * powf(0.5, i);
            unsigned int mipHeight = resolution * powf(0.5, i);
            glViewport(0, 0, mipWidth, mipHeight);
            float roughness = (float)i / (float)(miplevels - 1);
            r_sendf(&state->prefiltershader,roughness,"roughness");
            state->postmatrix = fn_ortho(0,mipWidth,0,mipHeight);
            r_sendmat4(&state->prefiltershader,state->postmatrix,"modelViewprojection");
            r_send2f(&state->prefiltershader,fn_createVec2(1.0/mipWidth,1.0/mipHeight),"invResolution");
            for (int j = 0; j < 6;j++)
            {
                r_send3f(&state->prefiltershader,cubemapFaceNormals[j][0],"Normcomp0");
                r_send3f(&state->prefiltershader,cubemapFaceNormals[j][1],"Normcomp1");
                r_send3f(&state->prefiltershader,cubemapFaceNormals[j][2],"Normcomp2");
                //TODO fix problem
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, prefilterMap, i);
                glClear(GL_COLOR_BUFFER_BIT);
                th_renderArray(&state->post_gpu,3,0,0,0);

                glBindBuffer(GL_PIXEL_PACK_BUFFER, mipPbos[j]);

                glGetTexImage(	GL_TEXTURE_CUBE_MAP_POSITIVE_X + j,	i,GL_RGB,	GL_FLOAT,0);
                if (syncs[j].used)
                    glDeleteSync(syncs[j].sync);
                syncs[j].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
                syncs[j].used = true;
                SDL_GL_SwapWindow( window);
            }
            for (int j = 0 ; j < 6;j++)
            {
                filterData[i][j] = malloc(sizeof(GLfloat)*mipWidth*mipHeight*3);
                if (syncs[j].used)
                {

                    while (1)
                    {
                        GLenum waitReturn = glClientWaitSync(syncs[j].sync,
                                                                GL_SYNC_FLUSH_COMMANDS_BIT, 0);//
                        if (waitReturn == GL_ALREADY_SIGNALED ||
                            waitReturn == GL_CONDITION_SATISFIED)
                        {

                            break;
                        }


                    }
                }
                memcpy(filterData[i][j],mipPboData[j],sizeof(GLfloat)*mipWidth*mipHeight*3);
            }
        }
        // glActiveTexture(GL_TEXTURE0);
        // glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);


        //process file
        fn_vec3 max = fn_createVec3s(0.f);
        fn_vec3 min = fn_createVec3s(0.f);
        bool setmin = false;
        bool setmax = false;
        for (int i = 0 ; i < 6;i++)
        {
            for (int j = 0 ; j <resolution*resolution;j++)
            {
                int x = j%resolution;
                int y = (j - x)/resolution;
                float delta = (1.0/resolution)/2.0;
                fn_vec3 worldpos =  fn_getPositionFromDepth(data[i][j],fn_createVec2((float)x/(float)resolution + delta,(float)y/(float)resolution + delta),mats[i],proj);
                float l = fn_length(fn_subVec3(state->pos,worldpos));
                if (data[i][j] != 1.0)
                {
                    if (worldpos.x > max.x || !setmax)
                    {
                        max.x = worldpos.x;
                    }
                    if (worldpos.y > max.y || !setmax)
                    {
                        max.y = worldpos.y;
                    }
                    if (worldpos.z > max.z || !setmax)
                    {
                        max.z = worldpos.z;
                    }

                    if (worldpos.x < min.x || !setmin)
                    {
                        min.x = worldpos.x;
                    }
                    if (worldpos.y < min.y || !setmin)
                    {
                        min.y = worldpos.y;
                    }
                    if (worldpos.z < min.z || !setmin)
                    {
                        min.z = worldpos.z;
                    }
                    setmin = true;
                    setmax = true;
                }
                // else
                // {
                data[i][j] = l;
                //  }



            }
        }


        // fprintf(fp, "%f %f %f ",min.x -10,min.y-10,min.z-10 );
        // fprintf(fp, "%f %f %f ",max.x + 10,max.y + 10,max.z + 10 );
        ret.minmaxdata[l*6 + 0] = min.x -10;ret.minmaxdata[l*6 + 1] = min.y -10;ret.minmaxdata[l*6 + 2] = min.z -10;
        ret.minmaxdata[l*6 + 3] = max.x + 10;ret.minmaxdata[l*6 + 4] = max.y + 10;ret.minmaxdata[l*6 + 5] = max.z + 10;
        for( int i = 0; i < 6; i++)
        {
            for (int j = 0 ; j < resolution*resolution;j++)
            {
               // fprintf(fp, "%f ",data[i][j] );
                ret.wad_data[l*6*resolution*resolution + i*resolution*resolution + j] = data[i][j];
            }

        }

        //output cube
        for (int i = 0 ; i <miplevels;i++)
        {
            unsigned int mipWidth = resolution * powf(0.5, i);
            unsigned int mipHeight = resolution * powf(0.5, i);
            for (int j = 0 ; j < 6;j++)
            {

                for (unsigned int k = 0 ; k < mipWidth*mipHeight;k++)
                {
                   // fprintf(fp, "%f %f %f ",filterData[i][j][k*3 + 0],filterData[i][j][k*3 + 1],filterData[i][j][k*3 + 2]);
                    ret.color_data[i][l*6*mipWidth*mipHeight*3 + j*mipWidth*mipHeight*3 + k*3 + 0] = filterData[i][j][k*3 + 0];
                    ret.color_data[i][l*6*mipWidth*mipHeight*3 + j*mipWidth*mipHeight*3 + k*3 + 1] = filterData[i][j][k*3 + 1];
                    ret.color_data[i][l*6*mipWidth*mipHeight*3 + j*mipWidth*mipHeight*3 + k*3 + 2] = filterData[i][j][k*3 + 2];
                }
            }
        }
    }

  //  fclose(fp);


    printf("Finished in %f Seconds\n",(SDL_GetTicks() - start)/1000.f );
    return ret;
    #undef miplevels
}

void th_generateHarmonics(int resolution,SDL_Window* window,char* wadlocation,th_RendererState* state)
{
    int batch_res = state->batch_atlas_res;


    //th_FrameBuffer frame;
    th_PixelPackBuffer pbos =  th_createPixelPackBuffers(sizeof(GLfloat)*batch_res*batch_res*3,1);
    //th_createFramebuffer(&frame,resolution,resolution);
    fn_mat4 proj = fn_perspective(fn_radians(90),1,state->zNear,state->zFar);
    state->postmatrix = fn_ortho(0,resolution,0,resolution);

    state->projection = proj;

    th_bindGlobalTextures(state);


    fn_mat4 mats[6];


    Uint32 start = SDL_GetTicks();
    //foreach position

    #define NUM_PBOSYNC 1
    r_VboSync syncs[NUM_PBOSYNC];
    GLfloat* data[NUM_PBOSYNC];
    for (int i = 0 ; i < NUM_PBOSYNC;i++)
    {
        syncs[i].used =false;
        data[i] = malloc(sizeof(GLfloat)*batch_res*batch_res*3);
    }
    // GLsync syncs[2];
    // bool used[2];
    // memset(used,false,sizeof(bool)*2);
    printf("%s\n","starting" );
    state->th_render_to_buffer = true;
    th_Harmonic* harmonics = malloc(sizeof(th_Harmonic)*state->griddims.x*state->griddims.y*state->griddims.z);
    int index = 0;


    int batch_counter = 0;

    for (int x = 1; x <= state->griddims.x;x++)
    {
        for (int y = 1; y <= state->griddims.y;y++)
        {
            for (int z = 1; z <= state->griddims.z;z++)
            {
                //state->pos =
                fn_vec3 pos = fn_createVec3((x-(state->griddims.x*0.5))*state->gridsize + state->gridpos.x,(y-(state->griddims.y*0.5))*state->gridsize + state->gridpos.y,(z-(state->griddims.z*0.5))*state->gridsize + state->gridpos.z);
                // pos = fn_createVec3(42.034225, -77.741920, -78.344536);
                mats[1] = r_camera(pos,fn_createVec2(fn_radians(-90),fn_radians(0)));// negx
                mats[0] = r_camera(pos,fn_createVec2(fn_radians(90),fn_radians(0)));// posx
                mats[4] = r_camera(pos,fn_createVec2(fn_radians(0),fn_radians(0))); //posz
                mats[5] = r_camera(pos,fn_createVec2(fn_radians(180),fn_radians(0)));//negz
                mats[3] = r_camera(pos,fn_createVec2(fn_radians(0),fn_radians(90)));//negy
                mats[2] = r_camera(pos,fn_createVec2(fn_radians(0),fn_radians(-90)));//posy

                for (int i = 0 ; i < 6;i++)
                {
                    fn_mat4 view = mats[i];
                    state->viewmats[batch_counter] = mats[i];
                    state->positions[batch_counter] = pos;
                    batch_counter++;

                    // fn_mat4 view = mats[i];
                    // fn_mat4 modelViewprojection = fn_multMat4(view,proj);
                    // fn_mat4 invProj = fn_inverse(proj);
                    // fn_mat4 invView = fn_inverse(view);
                    // glViewport(0,0,resolution,resolution);
                    // th_render(modelViewprojection,proj,view,fn_createVec2(resolution,resolution),state);
                    //
                    // glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos.pbos[i]);
                    //
                    // glGetTextureImage(	state->cubetarget_fb.textures[0],	0,GL_RGB,	GL_FLOAT,	sizeof(GLfloat)*resolution*resolution*3,0);
                    // if (syncs[i].used)
                    //     glDeleteSync(syncs[i].sync);
                    // syncs[i].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
                    // syncs[i].used = true;
                    //
                    // th_tickFrame();
                    // SDL_GL_SwapWindow( window);

                }


                if (batch_counter == MAX_BATCH_LENGTH || (x == state->griddims.x &&
                    y == state->griddims.y &&
                    z == state->griddims.z))
                {
                    state->num_batches = batch_counter;

                    // fn_mat4 view = mats[i];
                    // fn_mat4 modelViewprojection = fn_multMat4(view,proj);
                    // fn_mat4 invProj = fn_inverse(proj);
                    // fn_mat4 invView = fn_inverse(view);
                    //glViewport(0,0,resolution,resolution);
                    th_render( state->viewmats[0],proj,state->viewmats[0],fn_createVec2(resolution,resolution),state);

                    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos.pbos[0]);

                    glPixelStorei(GL_PACK_ALIGNMENT, 1);  // Or 4?
                    //glPixelStorei(GL_PACK_ROW_LENGTH, 0);

                    glGetTextureImage(	state->cubetarget_fb.textures[0],	0,GL_RGB,	GL_FLOAT,	sizeof(GLfloat)*batch_res*batch_res*3,0);
                    if (syncs[0].used)
                        glDeleteSync(syncs[0].sync);
                    syncs[0].sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
                    syncs[0].used = true;

                    th_tickFrame();
                    SDL_GL_SwapWindow( window);


                    if (syncs[0].used)
                    {

                        while (1)
                        {
                            GLenum waitReturn = glClientWaitSync(syncs[0].sync,
                                                                    GL_SYNC_FLUSH_COMMANDS_BIT, 0);//
                            if (waitReturn == GL_ALREADY_SIGNALED ||
                                waitReturn == GL_CONDITION_SATISFIED)
                            {

                                break;
                            }


                        }
                    }

                    memcpy(data[0],pbos.pbodata[0],sizeof(GLfloat)*state->batch_atlas_res*state->batch_atlas_res*3);


                    // th_Harmonic harm = th_CubemaptoHarmonic(data,resolution);
                    //
                    // harmonics[index] = harm;

                    //TODO
                    th_CuebmapToHarmonicsBatch(harmonics,batch_counter/6,index,data[0],resolution);

                    index += batch_counter/6;


                    printf("%f\n",((float)index/(state->griddims.x*state->griddims.y*state->griddims.z))*100.f );
                    batch_counter = 0;

                }
                // for (int i = 0 ; i < 6;i++)
                // {
                //     if (syncs[i].used)
                //     {
                //
                //         while (1)
                //         {
                //             GLenum waitReturn = glClientWaitSync(syncs[i].sync,
                //                                                  GL_SYNC_FLUSH_COMMANDS_BIT, 0);//
                //             if (waitReturn == GL_ALREADY_SIGNALED ||
                //                 waitReturn == GL_CONDITION_SATISFIED)
                //             {
                //
                //                 break;
                //             }
                //
                //
                //         }
                //     }
                //
                //     memcpy(data[i],pbos.pbodata[i],sizeof(GLfloat)*resolution*resolution*3);
                // }




            }
        }
    }

    char filename[1024];
    if (wadlocation == NULL)
    {
        sprintf(filename,"th1/harmonics.hwad");
    }
    else
    {
        sprintf(filename,"th1/wad/%s/harmonics.hwad",wadlocation);
    }


    th_exportHarmonicsBinary(harmonics,index,filename);

    free(harmonics);
    state->th_render_to_buffer = false;



    printf("Finished in %f Seconds\n",(SDL_GetTicks() - start)/1000.f );
}
