#include "th_replay.h"
#include "../th_fopen.h"

void th_tickReplayFrameCapture(th_ReplayData* rp)
{
    unsigned int frame_idx;
    th_timer_t frame_last_time;
    bool capture_this_frame;
    rp->capture_this_frame = false;
    if (rp->frame_last_time == -1 || th_time() - rp->frame_last_time >= 33.33)
    {
        rp->frame_idx = rp->frame_idx + 1;
        rp->capture_this_frame = true;
        rp->frame_last_time = th_time();
    }
}


void th_getMatsPerFrame(th_ReplayData* rp,th_LevelDescriptor* level)
{
    rp->th_rendercommands_capture = level->render_commands_count;
    rp->th_mat_per_frame = 0;
    for (int i = 0 ; i < level->render_commands_count;i++)
    {
        th_RenderCommand r = level->render_commands[i];
        int mats_chunk_count = *r.matcount;
        rp->th_mat_per_frame += mats_chunk_count;
    }

    rp->th_anim_mat_per_frame = 0;
    for (int i = 0 ; i < level->anim_render_commands_count;i++)
    {
        th_RenderCommand r = level->anim_render_commands[i];
        int mats_chunk_count = *r.matcount;
        rp->th_anim_mat_per_frame += mats_chunk_count;
    }
    rp->th_anim_rendercommands_capture = level->anim_render_commands_count;

    rp->th_anim_captured_model_instances = 0;
    for (int i = 0; i <level->ls.animated_models_count ; i++) {
        rp->th_anim_captured_model_instances += level->ls.animated_models[i].instanceCount;
    }
}

void th_cleanupReplay(th_ReplayData* rp)
{
    if (rp->needs_cleanup)
    {
        th_free(rp->alloc);
        free(rp->alloc);
        rp->needs_cleanup = false;
        *rp = DEFAULT_REPLAY_STATE;
    }
}

void th_startCapture(th_ReplayData* rp)
{
    rp->needs_cleanup = true;
    rp->alloc = malloc(sizeof(th_Allocator));

    th_createAllocator(rp->alloc);
    th_Allocator* alloc = rp->alloc;

    rp->th_capture_mats_flag = true;

    rp->th_mat_capture = th_alloc(alloc,sizeof(fn_mat4*)*rp->th_mat_capture_maxframe);

    rp->th_mat_capture_time = th_alloc(alloc,sizeof(th_timer_t)*rp->th_mat_capture_maxframe);

    rp->th_particle_mat_capture = th_alloc(alloc,sizeof(fn_mat4*)*rp->th_mat_capture_maxframe);
    rp->th_particle_mat_capture_count = th_alloc(alloc,sizeof(int)*rp->th_mat_capture_maxframe);
    rp->th_particle_ids_capture= th_alloc(alloc,sizeof(uint64_t*)*rp->th_mat_capture_maxframe);


    rp->th_mat_capture_count = th_alloc(alloc,sizeof(int*)*rp->th_mat_capture_maxframe);

    rp->th_anim_mat_capture = th_alloc(alloc,sizeof(fn_mat4*)*rp->th_mat_capture_maxframe);
    rp->th_anim_mat_capture_count = th_alloc(alloc,sizeof(int*)*rp->th_mat_capture_maxframe);
    rp->th_anim_time_capture = th_alloc(alloc,sizeof(float*)*rp->th_mat_capture_maxframe);
    rp->th_anim_framenum_capture = th_alloc(alloc,sizeof(uint32_t*)*rp->th_mat_capture_maxframe);
    rp->th_anim_nextframe_capture = th_alloc(alloc,sizeof(uint32_t*)*rp->th_mat_capture_maxframe);

    rp->th_anim_is_ragdoll = th_alloc(alloc,sizeof(char*)*rp->th_mat_capture_maxframe);
    rp->th_anim_ragdoll_velcenter = th_alloc(alloc,sizeof(fn_vec3*)*rp->th_mat_capture_maxframe);
    rp->th_anim_ragdoll_vel = th_alloc(alloc,sizeof(fn_vec3*)*rp->th_mat_capture_maxframe);

    rp->th_pointlight_capture = th_alloc(alloc,sizeof(th_PointLight*)*rp->th_mat_capture_maxframe);
    rp->th_pointlight_capture_count = th_alloc(alloc,sizeof(int)*rp->th_mat_capture_maxframe);

    rp->th_frame_captured = 0;
    for (size_t i = 0; i < rp->th_mat_capture_maxframe; i++) {
        rp->th_mat_capture_time[i] = 0;
        rp->th_mat_capture[i] = th_alloc(alloc,sizeof(fn_mat4)*rp->th_mat_per_frame);

        rp->th_particle_mat_capture[i] = th_alloc(alloc,sizeof(fn_mat4)*MAX_PARTICLES_C);
        rp->th_particle_ids_capture[i] = th_alloc(alloc,sizeof(uint64_t)*MAX_PARTICLES_C);
        rp->th_particle_mat_capture_count[i] = 0;

        rp->th_mat_capture_count[i] = th_alloc(alloc,sizeof(int)*rp->th_rendercommands_capture);

        for (size_t j = 0; j < rp->th_mat_per_frame; j++) {
            rp->th_mat_capture[i][j] = fn_identityMat4();
        }

        for (size_t j = 0; j < MAX_PARTICLES_C; j++) {
            rp->th_particle_mat_capture[i][j] = fn_identityMat4();
            rp->th_particle_ids_capture[i][j] = 0;
        }

        for (size_t j = 0; j < rp->th_rendercommands_capture; j++) {
            rp->th_mat_capture_count[i][j] = 0;
        }

        rp->th_anim_mat_capture[i] = th_alloc(alloc,sizeof(fn_mat4)*rp->th_anim_mat_per_frame);
        rp->th_anim_mat_capture_count[i] = th_alloc(alloc,sizeof(int)*rp->th_anim_rendercommands_capture);
        rp->th_anim_time_capture[i] = th_alloc(alloc,sizeof(float)*rp->th_anim_captured_model_instances);
        rp->th_anim_framenum_capture[i] = th_alloc(alloc,sizeof(uint32_t)*rp->th_anim_captured_model_instances);
        rp->th_anim_nextframe_capture[i] = th_alloc(alloc,sizeof(uint32_t)*rp->th_anim_captured_model_instances);

        rp->th_anim_is_ragdoll[i] = th_alloc(alloc,sizeof(char)*rp->th_anim_captured_model_instances);;
        rp->th_anim_ragdoll_velcenter[i] = th_alloc(alloc,sizeof(fn_vec3)*rp->th_anim_captured_model_instances);
        rp->th_anim_ragdoll_vel[i] = th_alloc(alloc,sizeof(fn_vec3)*rp->th_anim_captured_model_instances);


        rp->th_pointlight_capture_count[i] = 0;
        //MAX_LIGHTS
        rp->th_pointlight_capture[i] = th_alloc(alloc,sizeof(th_PointLight)*256);
    }

    rp->frame_idx = 0;
    rp->frame_last_time = -1;
    rp->capture_this_frame = false;
}

void th_saveCaptureUncompressed(th_ReplayData* rp,const char* filename)
{
    FILE* capture_file = th_fopen(filename,"wb");
    if (capture_file == NULL)
    {
        printf("%s\n","CANNOT OPEN" );
    }

    // fwrite(harmonics,sizeof(th_Harmonic),count,mwad);
    fwrite(&rp->th_frame_captured,sizeof(unsigned int),1,capture_file);

    fwrite(rp->th_mat_capture_time,sizeof(th_timer_t),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_mat_capture[i],sizeof(fn_mat4),rp->th_mat_per_frame,capture_file);
    }

    fwrite(rp->th_particle_mat_capture_count,sizeof(int),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_particle_mat_capture[i],sizeof(fn_mat4),rp->th_particle_mat_capture_count[i],capture_file);
    }
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_particle_ids_capture[i],sizeof(uint64_t),rp->th_particle_mat_capture_count[i],capture_file);
    }

    fwrite(&rp->th_rendercommands_capture,sizeof(int),1,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_mat_capture_count[i],sizeof(int),rp->th_rendercommands_capture,capture_file);
    }


    fwrite(&rp->th_anim_mat_per_frame,sizeof(unsigned int),1,capture_file);
    fwrite(&rp->th_anim_rendercommands_capture,sizeof(unsigned int),1,capture_file);
    fwrite(&rp->th_anim_captured_model_instances,sizeof(unsigned int),1,capture_file);


    //fwrite(rp->th_mat_capture_time,sizeof(float),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_mat_capture[i],sizeof(fn_mat4),rp->th_anim_mat_per_frame,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_time_capture[i],sizeof(float),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_framenum_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_nextframe_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_mat_capture_count[i],sizeof(int),rp->th_anim_rendercommands_capture,capture_file);
    }
    fwrite(rp->th_pointlight_capture_count,sizeof(int),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_pointlight_capture[i],sizeof(th_PointLight),rp->th_pointlight_capture_count[i],capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_is_ragdoll[i],sizeof(char),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_ragdoll_velcenter[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fwrite(rp->th_anim_ragdoll_vel[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,capture_file);
    }

    fclose(capture_file);
}

void th_saveCapture(th_ReplayData* rp,const char* filename,bool compression)
{
    if (!compression)
    {
        th_saveCaptureUncompressed(rp,filename);
    }
    else
    {
        size_t num_bytes = 0;
        unsigned char* data_to_compress = NULL;
        // fwrite(harmonics,sizeof(th_Harmonic),count,mwad);
        th_write_buffer(&rp->th_frame_captured,sizeof(unsigned int),1,&num_bytes,&data_to_compress);

        th_write_buffer(rp->th_mat_capture_time,sizeof(th_timer_t),rp->th_frame_captured,&num_bytes,&data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_mat_capture[i],sizeof(fn_mat4),rp->th_mat_per_frame,&num_bytes,&data_to_compress);
        }

        th_write_buffer(rp->th_particle_mat_capture_count,sizeof(int),rp->th_frame_captured,&num_bytes,&data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_particle_mat_capture[i],sizeof(fn_mat4),rp->th_particle_mat_capture_count[i],&num_bytes,&data_to_compress);
        }
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_particle_ids_capture[i],sizeof(uint64_t),rp->th_particle_mat_capture_count[i],&num_bytes,&data_to_compress);
        }

        th_write_buffer(&rp->th_rendercommands_capture,sizeof(int),1,&num_bytes,&data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_mat_capture_count[i],sizeof(int),rp->th_rendercommands_capture,&num_bytes,&data_to_compress);
        }


        th_write_buffer(&rp->th_anim_mat_per_frame,sizeof(unsigned int),1,&num_bytes,&data_to_compress);
        th_write_buffer(&rp->th_anim_rendercommands_capture,sizeof(unsigned int),1,&num_bytes,&data_to_compress);
        th_write_buffer(&rp->th_anim_captured_model_instances,sizeof(unsigned int),1,&num_bytes,&data_to_compress);


        //th_write_buffer(rp->th_mat_capture_time,sizeof(float),rp->th_frame_captured,&num_bytes,&data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_mat_capture[i],sizeof(fn_mat4),rp->th_anim_mat_per_frame,&num_bytes,&data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_time_capture[i],sizeof(float),rp->th_anim_captured_model_instances,&num_bytes,&data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_framenum_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,&num_bytes,&data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_nextframe_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,&num_bytes,&data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_mat_capture_count[i],sizeof(int),rp->th_anim_rendercommands_capture,&num_bytes,&data_to_compress);
        }
        th_write_buffer(rp->th_pointlight_capture_count,sizeof(int),rp->th_frame_captured,&num_bytes,&data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_pointlight_capture[i],sizeof(th_PointLight),rp->th_pointlight_capture_count[i],&num_bytes,&data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_is_ragdoll[i],sizeof(char),rp->th_anim_captured_model_instances,&num_bytes,&data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_ragdoll_velcenter[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,&num_bytes,&data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_write_buffer(rp->th_anim_ragdoll_vel[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,&num_bytes,&data_to_compress);
        }

        th_compress_and_write(filename,data_to_compress,num_bytes);

    }
}

void th_loadCaptureUncompressed(th_ReplayData* rp,const char* filename)
{
    FILE* capture_file = th_fopen(filename,"rb");
    if (capture_file == NULL)
    {
        printf("%s\n","CANNOT OPEN" );
    }

    fread(&rp->th_frame_captured,sizeof(unsigned int),1,capture_file);

    fread(rp->th_mat_capture_time,sizeof(th_timer_t),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_mat_capture[i],sizeof(fn_mat4),rp->th_mat_per_frame,capture_file);
    }

    fread(rp->th_particle_mat_capture_count,sizeof(int),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_particle_mat_capture[i],sizeof(fn_mat4),rp->th_particle_mat_capture_count[i],capture_file);
    }
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_particle_ids_capture[i],sizeof(uint64_t),rp->th_particle_mat_capture_count[i],capture_file);
    }

    fread(&rp->th_rendercommands_capture,sizeof(int),1,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_mat_capture_count[i],sizeof(int),rp->th_rendercommands_capture,capture_file);
    }

    fread(&rp->th_anim_mat_per_frame,sizeof(unsigned int),1,capture_file);
    fread(&rp->th_anim_rendercommands_capture,sizeof(unsigned int),1,capture_file);
    fread(&rp->th_anim_captured_model_instances,sizeof(unsigned int),1,capture_file);


    //fread(rp->th_mat_capture_time,sizeof(float),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_mat_capture[i],sizeof(fn_mat4),rp->th_anim_mat_per_frame,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_time_capture[i],sizeof(float),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_framenum_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_nextframe_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_mat_capture_count[i],sizeof(int),rp->th_anim_rendercommands_capture,capture_file);
    }

    fread(rp->th_pointlight_capture_count,sizeof(int),rp->th_frame_captured,capture_file);
    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_pointlight_capture[i],sizeof(th_PointLight),rp->th_pointlight_capture_count[i],capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_is_ragdoll[i],sizeof(char),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_ragdoll_velcenter[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,capture_file);
    }

    for (size_t i = 0; i < rp->th_frame_captured; i++) {
        fread(rp->th_anim_ragdoll_vel[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,capture_file);
    }


    fclose(capture_file);
}

void th_loadCapture(th_ReplayData* rp,const char* filename,bool compression)
{


    if (!compression)
    {
        th_loadCaptureUncompressed(rp,filename);
    }
    else
    {
        size_t num_bytes_read = 0;
        size_t num_bytes = 0;
        unsigned char* data_to_compress = NULL;

        th_read_and_decompress(filename,&data_to_compress,&num_bytes_read);

        th_read_buffer(&rp->th_frame_captured,sizeof(unsigned int),1,&num_bytes,data_to_compress);
        unsigned int frcap = rp->th_frame_captured;

        rp->th_play_mats_flag = true;
        rp->th_disable_frustum_culling = true;

        rp->th_mat_capture_maxframe = rp->th_frame_captured + 5;
        th_startCapture(rp);
        rp->th_capture_mats_flag = false;
        rp->th_frame_captured = frcap;

        printf("Loading %i frames\n",rp->th_frame_captured);
        // printf("Loading %li bytes\n",num_bytes_read);
        //
        // printf("mat4 %li bytes\n",sizeof(fn_mat4));

        printf("debug %i bytes\n",rp->th_mat_per_frame);

        th_read_buffer(rp->th_mat_capture_time,sizeof(th_timer_t),rp->th_frame_captured,&num_bytes,data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_mat_capture[i],sizeof(fn_mat4),rp->th_mat_per_frame,&num_bytes,data_to_compress);
        }

        // printf("Loading %li bytes\n",num_bytes);

         // printf("dbg %lli bytes\n",sizeof(int));
         //  printf("dbg %lli bytes\n",sizeof(fn_mat4));
         //   printf("dbg %lli bytes\n",sizeof(particle_id_t));

        th_read_buffer(rp->th_particle_mat_capture_count,sizeof(int),rp->th_frame_captured,&num_bytes,data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_particle_mat_capture[i],sizeof(fn_mat4),rp->th_particle_mat_capture_count[i],&num_bytes,data_to_compress);
        }
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_particle_ids_capture[i],sizeof(uint64_t),rp->th_particle_mat_capture_count[i],&num_bytes,data_to_compress);
        }

        //printf("Loading %li bytes\n",num_bytes);
        th_read_buffer(&rp->th_rendercommands_capture,sizeof(int),1,&num_bytes,data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_mat_capture_count[i],sizeof(int),rp->th_rendercommands_capture,&num_bytes,data_to_compress);
        }

        th_read_buffer(&rp->th_anim_mat_per_frame,sizeof(unsigned int),1,&num_bytes,data_to_compress);
        th_read_buffer(&rp->th_anim_rendercommands_capture,sizeof(unsigned int),1,&num_bytes,data_to_compress);
        th_read_buffer(&rp->th_anim_captured_model_instances,sizeof(unsigned int),1,&num_bytes,data_to_compress);


        //th_read_buffer(rp->th_mat_capture_time,sizeof(float),rp->th_frame_captured,&num_bytes,data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_mat_capture[i],sizeof(fn_mat4),rp->th_anim_mat_per_frame,&num_bytes,data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_time_capture[i],sizeof(float),rp->th_anim_captured_model_instances,&num_bytes,data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_framenum_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,&num_bytes,data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_nextframe_capture[i],sizeof(uint32_t),rp->th_anim_captured_model_instances,&num_bytes,data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_mat_capture_count[i],sizeof(int),rp->th_anim_rendercommands_capture,&num_bytes,data_to_compress);
        }

        th_read_buffer(rp->th_pointlight_capture_count,sizeof(int),rp->th_frame_captured,&num_bytes,data_to_compress);
        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_pointlight_capture[i],sizeof(th_PointLight),rp->th_pointlight_capture_count[i],&num_bytes,data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_is_ragdoll[i],sizeof(char),rp->th_anim_captured_model_instances,&num_bytes,data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_ragdoll_velcenter[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,&num_bytes,data_to_compress);
        }

        for (size_t i = 0; i < rp->th_frame_captured; i++) {
            th_read_buffer(rp->th_anim_ragdoll_vel[i],sizeof(fn_vec3),rp->th_anim_captured_model_instances,&num_bytes,data_to_compress);
        }
    }

}

fn_mat4 th_interpMat4(fn_mat4 a, fn_mat4 b,float f)
{
    if (fn_isIdentity(a))
    {
        return b;
    }
    // else if (fn_isIdentity(b))
    // {
    //     return a;
    // }

    if (fn_isNullMatrix(a))
    {
        return b;
    }

    fn_vec3 tr_a = fn_createVec3(a.m[12],a.m[13],a.m[14]);
    fn_vec3 tr_b = fn_createVec3(b.m[12],b.m[13],b.m[14]);

    //detect "teleportation"
    if (fn_distance(tr_a,tr_b) > 1000)
    {
        if (f > 0.5)
        {
            return b;
        }
        else
        {
            return a;
        }
    }

    fn_vec4 a_x_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(a.m[0],a.m[1],a.m[2]));
    fn_vec4 a_y_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(a.m[4],a.m[5],a.m[6]));
    fn_vec4 a_z_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(a.m[8],a.m[9],a.m[10]));

    fn_vec4 b_x_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(b.m[0],b.m[1],b.m[2]));
    fn_vec4 b_y_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(b.m[4],b.m[5],b.m[6]));
    fn_vec4 b_z_basis_mag = fn_normalizeVec3Magnitude(fn_createVec3(b.m[8],b.m[9],b.m[10]));

    fn_vec3 s_a = fn_createVec3(a_x_basis_mag.w,a_y_basis_mag.w,a_z_basis_mag.w);
    fn_vec3 s_b = fn_createVec3(b_x_basis_mag.w,b_y_basis_mag.w,b_z_basis_mag.w);

    fn_vec3 a_x_basis = a_x_basis_mag.xyz;
    fn_vec3 a_y_basis = a_y_basis_mag.xyz;
    fn_vec3 a_z_basis = a_z_basis_mag.xyz;

    fn_vec3 b_x_basis = b_x_basis_mag.xyz;
    fn_vec3 b_y_basis = b_y_basis_mag.xyz;
    fn_vec3 b_z_basis = b_z_basis_mag.xyz;

    fn_vec3 tr = fn_lerpVec3(tr_a,tr_b,f);
    fn_vec3 s = fn_lerpVec3(s_a,s_b,f);

    fn_quat qa = fn_mat4toquat(fn_createMat4(a_x_basis.x,a_x_basis.y,a_x_basis.z,0,a_y_basis.x,a_y_basis.y,a_y_basis.z,0,a_z_basis.x,a_z_basis.y,a_z_basis.z,0,0,0,0,1));
    fn_quat qb = fn_mat4toquat(fn_createMat4(b_x_basis.x,b_x_basis.y,b_x_basis.z,0,b_y_basis.x,b_y_basis.y,b_y_basis.z,0,b_z_basis.x,b_z_basis.y,b_z_basis.z,0,0,0,0,1));

    fn_vec4 r;
    r = fn_slerpVec4(fn_normalizeVec4(qa), fn_normalizeVec4(qb), f);
    r= fn_normalizeVec4(r);

    return fn_translaterotatescaleq(tr,r,s);
}

typedef struct
{
    int start;
    int range;
    float f;
    fn_mat4* a;
    fn_mat4* b;
    fn_mat4* dest;
}th_interpFrameData;

void th_interpFrameThread(void* data)
{
    th_interpFrameData* dat = (th_interpFrameData*)data;
    int start = dat->start ;
    int range = dat->range ;
    fn_mat4* a = dat->a ;
    fn_mat4* b = dat->b ;
    float f = dat->f ;
    fn_mat4* dest = dat->dest ;

    for (int i = start; i < start + range; i++) {
        dest[i] = th_interpMat4(a[i],b[i],f);
    }
}

void th_performCapture(th_ReplayData* rp,th_LevelDescriptor* level)
{
    if (rp->th_capture_mats_flag && rp->capture_this_frame)
    {
        int mat_index = 0;

        if (rp->frame_idx - 1 < rp->th_mat_capture_maxframe)
        {
            for (int i = 0 ; i < level->render_commands_count;i++)
            {
                th_RenderCommand r = level->render_commands[i];

                fn_mat4* mats_chunk = *r.mats;
                int mats_chunk_count = *r.matcount;

                memcpy(&rp->th_mat_capture[rp->frame_idx - 1][mat_index],mats_chunk,mats_chunk_count*sizeof(fn_mat4));

                rp->th_mat_capture_count[rp->frame_idx - 1][i] = *r.matcount;
                mat_index += mats_chunk_count;
            }

            rp->th_mat_capture_time[rp->frame_idx - 1] = th_time();
            rp->th_frame_captured = rp->frame_idx;
        }

    }

    int a = rp->a;
    int b = rp->b;
    float tween_value = rp->tween_value;

    if (rp->th_play_mats_flag)
    {
        int mat_index = 0;

        if (rp->th_playback_old_frame < rp->th_frame_captured)
        {

            for (int i = 0 ; i < level->render_commands_count;i++)
            {
                th_RenderCommand r = level->render_commands[i];

                fn_mat4* mats_chunk = *r.mats;

                *r.matcount = rp->th_mat_capture_count[rp->th_playback_old_frame][i];
                int mats_chunk_count = *r.matcount;


                //th_interpFrame(&rp->th_mat_capture[a][mat_index],&rp->th_mat_capture[b][mat_index],tween_value,mats_chunk,mats_chunk_count);

                uint num_threads = (uint)th_getNumThreads();
                TH_BEGIN_SCHEDULING(num_threads,th_interpFrameData,mats_chunk_count)
                data[th_thread_id].start = start_pos;
                data[th_thread_id].range = add;
                data[th_thread_id].a = &rp->th_mat_capture[a][mat_index];
                data[th_thread_id].b = &rp->th_mat_capture[b][mat_index];
                data[th_thread_id].f = tween_value;
                data[th_thread_id].dest = mats_chunk;
                TH_SCHEDULING_FUNC
                th_setThread(th_interpFrameThread,(void*)&data[th_thread_id],th_thread_id);
                TH_END_SCHEDULING

                //memcpy(mats_chunk,&rp->th_mat_capture[th_frame()][mat_index],mats_chunk_count*sizeof(fn_mat4));
                mat_index += mats_chunk_count;
            }

        }
    }

    /*
     *  ANIMATED PLAYBACK
     */

    if (rp->th_capture_mats_flag && rp->capture_this_frame)
    {
        int mat_index = 0;

        if (rp->frame_idx - 1 < rp->th_mat_capture_maxframe)
        {
            for (int i = 0 ; i < level->anim_render_commands_count;i++)
            {
                th_RenderCommand r = level->anim_render_commands[i];

                fn_mat4* mats_chunk = *r.mats;
                int mats_chunk_count = *r.matcount;

                memcpy(&rp->th_anim_mat_capture[rp->frame_idx - 1][mat_index],mats_chunk,mats_chunk_count*sizeof(fn_mat4));

                rp->th_anim_mat_capture_count[rp->frame_idx - 1][i] = *r.matcount;
                mat_index += mats_chunk_count;
            }

        }

    }



    if (rp->th_play_mats_flag)
    {
        int mat_index = 0;

        if (rp->th_playback_old_frame < rp->th_frame_captured)
        {

            for (int i = 0 ; i < level->anim_render_commands_count;i++)
            {
                th_RenderCommand r = level->anim_render_commands[i];

                fn_mat4* mats_chunk = *r.mats;

                *r.matcount = rp->th_anim_mat_capture_count[rp->th_playback_old_frame][i];
                int mats_chunk_count = *r.matcount;


                //th_interpFrame(&rp->th_mat_capture[a][mat_index],&rp->th_mat_capture[b][mat_index],tween_value,mats_chunk,mats_chunk_count);

                uint num_threads = (uint)th_getNumThreads();
                TH_BEGIN_SCHEDULING(num_threads,th_interpFrameData,mats_chunk_count)
                data[th_thread_id].start = start_pos;
                data[th_thread_id].range = add;
                data[th_thread_id].a = &rp->th_anim_mat_capture[a][mat_index];
                data[th_thread_id].b = &rp->th_anim_mat_capture[b][mat_index];
                data[th_thread_id].f = tween_value;
                data[th_thread_id].dest = mats_chunk;
                TH_SCHEDULING_FUNC
                th_setThread(th_interpFrameThread,(void*)&data[th_thread_id],th_thread_id);
                TH_END_SCHEDULING

                for (int j = 0 ; j < mats_chunk_count;j++)
                {
                    level->levelstate.animated_models[i].instances[j].ragdoll_mat = rp->th_anim_mat_capture[a][mat_index + j];
                }



                //memcpy(mats_chunk,&rp->th_mat_capture[th_frame()][mat_index],mats_chunk_count*sizeof(fn_mat4));
                mat_index += mats_chunk_count;
            }

        }
    }
}

void th_captureLights(th_ReplayData* rp,th_LevelDescriptor* level)
{
    if (rp->th_capture_mats_flag && rp->capture_this_frame)
    {
        if (rp->frame_idx - 1 < rp->th_mat_capture_maxframe)
        {
            rp->th_pointlight_capture_count[rp->frame_idx - 1] = level->pointlightcount;
            memcpy(rp->th_pointlight_capture[rp->frame_idx - 1],level->pointlights,sizeof(th_PointLight)*level->pointlightcount);
        }

    }
    else if (rp->th_play_mats_flag)
    {
        if (rp->b < rp->th_frame_captured)
        {
            int frame_index = rp->tween_value < 0.5 ? rp->a : rp->b;

            level->pointlightcount = rp->th_pointlight_capture_count[frame_index];
            memcpy(level->pointlights,rp->th_pointlight_capture[frame_index],sizeof(th_PointLight)*level->pointlightcount);
        }

    }
}

bool th_captureParticles(th_ReplayData* rp,r_DrawElementsIndirectCommand* drawcommands_particle,th_GpuDataOffsets final_particle,th_ArrayObject* particle_data,int particle_commands_count,fn_vec3 pos,fn_vec3 look_global )
{
    int a = rp->a;
    int b = rp->b;
    float tween_value = rp->tween_value;

    if (rp->th_capture_mats_flag && rp->frame_idx - 1 < rp->th_mat_capture_maxframe && rp->capture_this_frame)
    {

        // rp->th_particle_mat_capture_count[th_frame()] = th_particle_count();
        // memcpy(rp->th_particle_mat_capture[th_frame()],th_particle_matrices(),sizeof(fn_mat4)*th_particle_count());

        th_particle_framecap(rp->th_particle_ids_capture[rp->frame_idx - 1],rp->th_particle_mat_capture[rp->frame_idx - 1],&rp->th_particle_mat_capture_count[rp->frame_idx - 1]);

    }

    if (rp->th_play_mats_flag && rp->th_playback_old_frame < rp->th_frame_captured)
    {
        int pcount = 0;
        fn_mat4* play_mats = th_particle_playback(pos,look_global,rp->th_particle_ids_capture[a],rp->th_particle_ids_capture[b],rp->th_particle_mat_capture[a],rp->th_particle_mat_capture[b],rp->th_particle_mat_capture_count[a] ,rp->th_particle_mat_capture_count[b],&pcount,tween_value);
        for (int i = 0 ; i < particle_commands_count;i++)
        {
            drawcommands_particle[i].baseInstance = final_particle.offsets_instances[i] + final_particle.data.instancecount*(th_frame()%3);
            drawcommands_particle[i].instanceCount = pcount;//rp->th_particle_mat_capture_count[rp->th_playback_old_frame];
        }

        // th_setInstances(&particle_data,0,rp->th_particle_mat_capture[rp->th_playback_old_frame],rp->th_particle_mat_capture_count[rp->th_playback_old_frame],1);
        th_setInstances(particle_data,0,play_mats,pcount,1);
        return false;
    }
    else
    {
        //th_setInstances(&particle_data,0,th_particle_matrices(),th_particle_count(),1);
        return true;
    }
}

void th_updateCapturePlayback(th_ReplayData* rp)
{
    if (rp->th_play_mats_flag)
    {
        int mat_index = 0;

        if (rp->th_playback_old_frame < rp->th_frame_captured)
        {

            th_timer_t x_time = th_time();


            if (rp->playback_old_time == -1)
            {
                rp->playback_old_time = x_time;

                int found_frame = rp->th_frame_captured - 1;
                for (unsigned int i = 2; i < rp->th_frame_captured; i++) {
                    if (rp->th_mat_capture_time[i] > x_time)
                    {
                        found_frame = i;
                        break;
                    }
                }

                rp->th_playback_old_frame = found_frame - 1;
                rp->b = found_frame;
                rp->a = found_frame - 1;
                rp->tween_value = fn_unlerpd(rp->th_mat_capture_time[rp->a],rp->th_mat_capture_time[rp->b],x_time);
                if (rp->tween_value > 1)
                {
                    rp->tween_value = 1;
                }
            }
            else
            {

                rp->playback_old_time = x_time;

                int found_frame = rp->th_frame_captured - 1;
                for (unsigned int i = rp->th_playback_old_frame; i < rp->th_frame_captured; i++) {
                    if (rp->th_mat_capture_time[i] > x_time)
                    {
                        found_frame = i;
                        break;
                    }
                }

                rp->th_playback_old_frame = found_frame ;
                rp->b = found_frame;
                rp->a = found_frame - 1;
                rp->tween_value = fn_unlerp(rp->th_mat_capture_time[rp->a],rp->th_mat_capture_time[rp->b],x_time);
                if (rp->tween_value > 1)
                {
                    rp->tween_value = 1;
                }
            }

        }
    }
}

void th_captureModels(th_ReplayData* rp,th_LevelDescriptor* level)
{
    if (rp->th_capture_mats_flag && rp->capture_this_frame)
    {
        if (rp->frame_idx - 1 < rp->th_mat_capture_maxframe)
        {
            int index_models = 0;
            for (int i = 0 ; i < level->levelstate.animated_models_count;i++)
            {
                th_captureModel(&level->levelstate.animated_models[i],&rp->th_anim_time_capture[rp->frame_idx - 1][index_models],&rp->th_anim_framenum_capture[rp->frame_idx - 1][index_models],&rp->th_anim_nextframe_capture[rp->frame_idx - 1][index_models],&rp->th_anim_is_ragdoll[rp->frame_idx - 1][index_models],&rp->th_anim_ragdoll_velcenter[rp->frame_idx - 1][index_models],&rp->th_anim_ragdoll_vel[rp->frame_idx - 1][index_models]);
                index_models += level->levelstate.animated_models[i].instanceCount;
            }
        }

    }
}

bool th_playbackModels(th_ReplayData* rp,th_LevelDescriptor* level,float delta_time,th_World* world)
{
    if (rp->th_play_mats_flag)
    {
        if (rp->b < rp->th_mat_capture_maxframe)
        {
            int index_models = 0;
            for (int i = 0 ; i < level->levelstate.animated_models_count;i++)
            {
                th_updateModelPlayback(&level->levelstate.animated_models[i],&rp->th_anim_time_capture[rp->a][index_models],&rp->th_anim_framenum_capture[rp->a][index_models],&rp->th_anim_nextframe_capture[rp->a][index_models],&rp->th_anim_time_capture[rp->b][index_models],&rp->th_anim_framenum_capture[rp->b][index_models],&rp->th_anim_nextframe_capture[rp->b][index_models],rp->tween_value,&rp->th_anim_is_ragdoll[rp->a][index_models],&rp->th_anim_ragdoll_velcenter[rp->a][index_models],&rp->th_anim_ragdoll_vel[rp->a][index_models],delta_time*0.001,world);
                index_models += level->levelstate.animated_models[i].instanceCount;
            }
        }
        return false;
    }
    return true;
}
