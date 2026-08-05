#include "th_tutorial.h"
#include "../fn_engine/th_system.h"
#include "../fn_engine/th_level.h"
#include "../fn_engine/th_ui.h"
#include "th_eyeball.h"

#define GEMS_SPAWN_TUTORIAL 40

static const float gems_to_spawn = GEMS_SPAWN_TUTORIAL;//WARNING DO NOT CHANGE THIS
static char** msg_mem = NULL;

void th_tutorialInitialize(th_Allocator* alloc,th_TutorialObject* c,th_LevelState* levelstate,th_TutorialType type)
{
    c->levelstate = levelstate;
    c->sequence_progression = 1;
    c->eyeball_id = -1;

    if (msg_mem == NULL)
    {
        msg_mem = malloc(sizeof(char*)*10);
        for (int i = 0; i < 10;i++)
        {
            msg_mem[i] = malloc(sizeof(char)*1024);
        }
    }

    c->gem_ids = th_alloc(alloc,sizeof(int)*gems_to_spawn);
    c->gem_id_count = gems_to_spawn;

    for (int i = 0; i < c->gem_id_count;i++)
    {
        c->gem_ids[i] = -1;
    }

    c->tutorial_progress_pct = 1.0;
    c->displayed_mesg_question = -1;

    c->type = type;

    if (type == TH_TUT_KILL_BEETLE || type == TH_TUT_KILL_CENTI)
    {
        c->sequence_progression = 0;
    }
}



#define N_LINES_TUT 4
static void renderTextMessages(int seq,int n_lines,fn_vec2 screenSize,th_Character* cmap,const char* messages[][N_LINES_TUT],int replace_mode[][N_LINES_TUT],fn_RawInput* input)
{









    float fontsize = 0.4;
    float line_off = 0.0;

    for (int idx = 0 ; idx < n_lines;idx++)
    {
        if (strlen(messages[seq][idx]) > 0)
        {
            int repmode = replace_mode[seq][idx] ;
            if (repmode == 0)
            {
                strcpy(msg_mem[idx], messages[seq][idx]);
            }
            else if (repmode == 1)
            {
                const char* crouch_key = th_getKeyName(input->binding_crouch);
                int buffer_size = snprintf(NULL, 0,  messages[seq][idx],crouch_key) + 1;

                if (buffer_size > 1024)
                {
                    return;
                }
                sprintf(msg_mem[idx], messages[seq][idx],crouch_key);
            }
            else if (repmode == 2)
            {
                const char* shotgun_key = th_getKeyName(input->binding_weapon2);
                int buffer_size = snprintf(NULL, 0,  messages[seq][idx],shotgun_key) + 1;

                if (buffer_size > 1024)
                {
                    return;
                }
                sprintf(msg_mem[idx], messages[seq][idx],shotgun_key);
            }
            else if (repmode == 3)
            {
                const char* machinegun_key = th_getKeyName(input->binding_weapon1);
                int buffer_size = snprintf(NULL, 0,  messages[seq][idx],machinegun_key) + 1;

                if (buffer_size > 1024)
                {
                    return;
                }
                sprintf(msg_mem[idx], messages[seq][idx],machinegun_key);
            }
            else if (repmode == 4)
            {
                const char* hammer_key = th_getKeyName(input->binding_weapon3);
                int buffer_size = snprintf(NULL, 0,  messages[seq][idx],hammer_key) + 1;

                if (buffer_size > 1024)
                {
                    return;
                }
                sprintf(msg_mem[idx], messages[seq][idx],hammer_key);
            }
            else if (repmode == 5)
            {
                const char* next_key = th_getKeyName(SDL_SCANCODE_N);
                int buffer_size = snprintf(NULL, 0,  messages[seq][idx],next_key) + 1;

                if (buffer_size > 1024)
                {
                    return;
                }
                sprintf(msg_mem[idx], messages[seq][idx],next_key);
            }

            //messages[seq][idx]

            fn_vec2 dims = th_stringDims(msg_mem[idx],fontsize,cmap);

            fn_vec2 mpos = fn_multVec2(screenSize,fn_createVec2(0.5,0.93));

            mpos.x = mpos.x - dims.x*0.5;
            mpos.y = mpos.y - dims.y + line_off;
            line_off -= dims.y*1.5;


            th_uiNagbarN(msg_mem[idx],mpos,fontsize,0,idx);
        }
        else
        {
            fn_vec2 dims = th_stringDims("X",fontsize,cmap);
            line_off -= dims.y*1.5;
        }

    }
}

void th_tutorialUpdateMovement(th_TutorialObject* c,float dt,fn_RawInput* input,fn_vec2 screenSize,th_Character* cmap)
{
    c->levelstate->progress_state = TH_BAR_FOUR;

    bool picked_all_questions = false;
    int last_picked_question = -1;
    int num_questions_picked = 0;

    if (c->levelstate->question != NULL)
    {
        last_picked_question = c->levelstate->question->index_last_touched;
        bool pickedall = true;
        for (int k = 0 ; k < c->levelstate->question->entity_count;k++)
        {
            pickedall = pickedall && c->levelstate->question->is_collected[k];
            if (c->levelstate->question->is_collected[k])
            {
                num_questions_picked++;
            }
        }
        picked_all_questions = pickedall;
    }

    //this is really dirty

    #define SEQ_BLNK_FIRST 0
    #define SEQ_SLIDE1 1
    #define SEQ_SLIDE2 2
    #define SEQ_HOLD 3
    #define SEQ_SHTGN 4
    #define SEQ_MCHGN 5
    #define SEQ_HAMMER 6
    #define SEQ_HAMMER_RCKT 7
    #define SEQ_HAMMER_RCKT_BLNK 8
    #define SEQ_HAMMER_GEMS 9
    #define SEQ_HAMMER_GEMS_BLNK 10
    #define SEQ_HAMMER_KNOCKOUT 11
    #define SEQ_HAMMER_KNOCKOUT_BLNK 12
    #define SEQ_BLNK_END 13

    if (!picked_all_questions && c->sequence_progression < SEQ_HAMMER )
    {
        float p_pct = 0.0;
        p_pct = (float)(num_questions_picked + 1) / (float)13.0;

        c->tutorial_progress_pct = 1.0 - p_pct;
    }
    else if (picked_all_questions && c->sequence_progression < SEQ_HAMMER )
    {
        float p_pct = 0.0;
        p_pct = (float)5.0 / (float)13.0;

        c->tutorial_progress_pct = 1.0 - p_pct;
    }
    else
    {
        float p_pct = 0.0;
        p_pct = (float)c->sequence_progression / (float)13.0;

         c->tutorial_progress_pct = 1.0 - p_pct;
    }


    #define N_MESSAGES_TUT 14
    const char* messages[N_MESSAGES_TUT][N_LINES_TUT] = {{"","","",""},
        {"Hold [%s] while running","to slide-dash and slow time.","The slide-dash recharges every 2 seconds.","Press [%s] to continue."},
        {"Keep holding [%s] while sliding","to preserve speed.","Releasing stops the slide.","Press [%s] to continue."},
        {"Holding [%s] in the air","makes you accelerate downwards.","Use the dash sparingly, or you will loose control.","Press [%s] to continue."},
        {"Press [%s] for the SHOTGUN.","Shoot the ground while jumping for more height.","Hold [Mouse 1] to fire continuously.","Press [%s] to continue."},
        {"Press [%s] for the MACHINEGUN.","Shoot a wall at a downwards angle to climb it.","","Press [%s] to continue."},
        {"Press [%s] for the HAMMER.","You can throw it, and it comes back!","Hold [Mouse 1] to throw continuously.","Press [%s] to continue."},
        {"The HAMMER is the most powerful weapon you have.","It can destroy rockets mid-air.","","Press [%s] to continue."},
        {"","","",""},
        {"The HAMMER also attracts GEMS on impact.","Shooting repels GEMS.","","Press [%s] to continue."},
        {"","","",""},
        {"The HAMMER is good for knocking out GEMS.","Bullets and shells must hit GEMS head-on.","The HAMMER can impact a GEM from any angle.","Press [%s] to continue."},
        {"","","",""},
        {"","","",""},
    };

    int replace_mode[N_MESSAGES_TUT][N_LINES_TUT] = {{0,0,0,0},{1,0,0,5},{1,0,0,5},{1,0,0,5},{2,0,0,5},{3,0,0,5},{4,0,0,5},{0,0,0,5},{0,0,0,0},{0,0,0,5},{0,0,0,0},{0,0,0,5},{0,0,0,0},{0,0,0,0}};



    bool candisplay = (c->sequence_progression != SEQ_BLNK_FIRST) || (c->displayed_mesg_question != last_picked_question);

    if (last_picked_question == 0 && c->sequence_progression < SEQ_HAMMER && candisplay)
    {
        c->sequence_progression = SEQ_HOLD;
        c->displayed_mesg_question = last_picked_question;
    }
    else if (last_picked_question == 1 && c->sequence_progression < SEQ_HAMMER && candisplay)
    {
        c->sequence_progression = SEQ_SHTGN;
        c->displayed_mesg_question = last_picked_question;
    }
    else if (last_picked_question == 2 && c->sequence_progression < SEQ_HAMMER && candisplay)
    {
        c->sequence_progression = SEQ_MCHGN;
        c->displayed_mesg_question = last_picked_question;
    }


    //render the text
     renderTextMessages(c->sequence_progression,N_LINES_TUT,screenSize,cmap,messages,replace_mode,input);

    //fn_vec3 spawnpos = fn_createVec3(225.751251,-307.574493,777.663330);
    fn_vec3 spawnpos = fn_createVec3(-6067.446777, 2059.199463, -1068.170410);


    if (input->currentKeyStates[SDL_SCANCODE_N] && !input->currentKeyStatesPrev[SDL_SCANCODE_N] && c->sequence_progression != SEQ_HAMMER_RCKT_BLNK && c->sequence_progression != SEQ_HAMMER_GEMS_BLNK && c->sequence_progression != SEQ_HAMMER_KNOCKOUT_BLNK && c->sequence_progression != SEQ_BLNK_FIRST && c->sequence_progression != SEQ_BLNK_END)
    {
        if (c->sequence_progression == SEQ_SLIDE1 && !picked_all_questions)
        {
            c->sequence_progression = SEQ_SLIDE2;
        }
        else if (c->sequence_progression < SEQ_HAMMER && !picked_all_questions)
        {
            c->sequence_progression = SEQ_BLNK_FIRST;
        }
        else if (c->sequence_progression < SEQ_HAMMER && picked_all_questions)
        {
            c->sequence_progression = SEQ_HAMMER;
        }
        else
        {
            c->sequence_progression++;

            if (c->sequence_progression >= N_MESSAGES_TUT)
            {
                c->sequence_progression--;
            }

            if (c->sequence_progression == SEQ_HAMMER_RCKT_BLNK)
            {

                int eye_id = th_eyeballSpawn(c->levelstate->eyeball,spawnpos);
                if (eye_id > 0)
                {
                    c->levelstate->eyeball->data[eye_id].target = spawnpos;
                    c->eyeball_id = eye_id;
                }
            }

            if (c->sequence_progression == SEQ_HAMMER_GEMS_BLNK)
            {
                //c->levelstate->horse->data[0].spawn_when = (th_time() - c->levelstate->level_start_time)+ 5000.0;
                fn_vec3 velg = fn_createVec3(0,-0.1,0);

                fn_vec3 gem_positions[GEMS_SPAWN_TUTORIAL];
                gem_positions[0] = fn_createVec3(-3662.686523, 2163.589600, 924.470337 );
                gem_positions[1] = fn_createVec3(-4070.108398, 2163.589600, 654.474243 );
                gem_positions[2] = fn_createVec3(-5081.095703, 2163.589600, 431.305695 );
                gem_positions[3] = fn_createVec3(-5545.008789, 2163.589600, 429.279663 );
                gem_positions[4] = fn_createVec3(-6357.562988, 2163.589600, 501.986511 );
                gem_positions[5] = fn_createVec3(-6438.187500, 2163.589600, 926.912598 );
                gem_positions[6] = fn_createVec3(-6095.592773, 2022.065918, 1427.110718 );
                gem_positions[7] = fn_createVec3(-5419.595215, 2163.589600, -1043.586426 );
                gem_positions[8] = fn_createVec3(-6282.917969, 1321.690918, 110.612709 );
                gem_positions[9] = fn_createVec3(-6211.994629, 1321.690918, 588.432983 );
                gem_positions[10] = fn_createVec3(-6131.176270, 1321.690918, 1027.805908 );
                gem_positions[11] = fn_createVec3(-3826.989990, 2163.589600, 531.549744 );
                gem_positions[12] = fn_createVec3(-3720.828613, 2163.589600, -516.031555 );
                gem_positions[13] = fn_createVec3(-3991.003174, 2163.589600, -1076.335205 );
                gem_positions[14] = fn_createVec3(-4644.507812, 2453.384033, -882.336365 );
                gem_positions[15] = fn_createVec3(-4938.809082, 2384.555176, -954.685303 );
                gem_positions[16] = fn_createVec3(-4988.250000, 2291.416992, -1205.819214 );
                gem_positions[17] = fn_createVec3(-4040.613281, 2163.589600, -547.046692 );
                gem_positions[18] = fn_createVec3(-4592.180664, 1924.959229, 28.760960 );
                gem_positions[19] = fn_createVec3(-4182.054199, 2163.589600, 619.110474);

                for (int k = 20 ;k < GEMS_SPAWN_TUTORIAL;k++)
                {
                    float osf = (k / 20)*1.0;
                    fn_vec3 oset = fn_createVec3(0,-100*osf,0);
                    gem_positions[k] = fn_addVec3(gem_positions[k % 20],oset);
                }

                for (int k = 0 ; k < GEMS_SPAWN_TUTORIAL;k++)
                {
                    c->gem_ids[k] = th_gemSpawn(c->levelstate->gems,gem_positions[k],velg);
                }

            }

            if (c->sequence_progression == SEQ_HAMMER_KNOCKOUT_BLNK)
            {
                c->levelstate->horse->data[0].spawn_when = (th_time() - c->levelstate->level_start_time)+ 5000.0;

                //< c->data[i].spawn_when
            }
        }


    }

    if (c->eyeball_id > 0)
    {
        c->levelstate->eyeball->data[c->eyeball_id].target = spawnpos;
        //WALLRIDING
        //TRAVELING
        if (c->levelstate->eyeball->data[c->eyeball_id].state == WALLRIDING || c->levelstate->eyeball->data[c->eyeball_id].state == TRAVELING)
        {
            c->levelstate->eyeball->data[c->eyeball_id].state = PREPARINGROCKET;
        }

        if (c->levelstate->eyeball->data[c->eyeball_id].gibbed)
        {
            c->eyeball_id = -1;
            c->sequence_progression++;

            if (c->sequence_progression >= N_MESSAGES_TUT)
            {
                c->sequence_progression--;
            }
        }
    }

    if (c->sequence_progression == SEQ_HAMMER_GEMS_BLNK)
    {
        bool done = true;
        for (int i = 0 ; i < c->gem_id_count;i++)
        {
            if (c->levelstate->gems->lifes[c->gem_ids[i]] > 0.0)
            {
                done = false;
            }
        }

        if (done)
        {
            c->sequence_progression++;

            if (c->sequence_progression >= N_MESSAGES_TUT)
            {
                c->sequence_progression--;
            }
        }
    }




}

void th_tutorialUpdateBeetle(th_TutorialObject* c,float dt,fn_RawInput* input,fn_vec2 screenSize,th_Character* cmap)
{


    bool picked_all_questions = false;
    int last_picked_question = -1;
    int num_questions_picked = 0;

    if (c->levelstate->question != NULL)
    {
        last_picked_question = c->levelstate->question->index_last_touched;
        bool pickedall = true;
        for (int k = 0 ; k < c->levelstate->question->entity_count;k++)
        {
            pickedall = pickedall && c->levelstate->question->is_collected[k];
            if (c->levelstate->question->is_collected[k])
            {
                num_questions_picked++;
            }
        }
        picked_all_questions = pickedall;
    }

    //this is really dirty

    #define SEQ_BLNK_FIRST 0
    #define SEQ_BEETLE 1
    #define SEQ_BLNK_LAST 2



    #undef N_MESSAGES_TUT

    #define N_MESSAGES_TUT 3
    const char* messages[N_MESSAGES_TUT][N_LINES_TUT] = {{"","","",""},{"To kill a BEETLE, shoot out all of its GEMS.","The HAMMER does the most damage to GEMS.","High ground makes them easier to hit.","Press [%s] to continue."},
    {"","","",""},
    };

    int replace_mode[N_MESSAGES_TUT][N_LINES_TUT] = {{0,0,0,0},{0,0,0,5},{0,0,0,0}};

    // const char* crouch_key = th_getKeyName(input->binding_crouch);
    //
    // const char* machinegun_key = th_getKeyName(input->binding_weapon1);
    // const char* hammer_key = th_getKeyName(input->binding_weapon3);
    // const char* shotgun_key = th_getKeyName(input->binding_weapon2);

    bool candisplay = (c->sequence_progression != SEQ_BLNK_FIRST) || (c->displayed_mesg_question != last_picked_question);

    if (last_picked_question == 0 && c->sequence_progression < SEQ_BEETLE && candisplay)
    {
        c->sequence_progression = SEQ_BEETLE;
        c->displayed_mesg_question = last_picked_question;
        c->tutorial_progress_pct = 0.5;
    }

    if (c->sequence_progression == 0)
    {
        c->levelstate->progress_state = TH_BAR_FOUR;
    }



   renderTextMessages(c->sequence_progression,N_LINES_TUT, screenSize,cmap,messages,replace_mode,input);





    if (input->currentKeyStates[SDL_SCANCODE_N] && !input->currentKeyStatesPrev[SDL_SCANCODE_N] && c->sequence_progression != SEQ_BLNK_FIRST && c->sequence_progression != SEQ_BLNK_LAST)
    {
        if (picked_all_questions)
        {
            c->sequence_progression++;

            if (c->sequence_progression >= N_MESSAGES_TUT)
            {
                c->sequence_progression--;
            }


            if (c->sequence_progression == SEQ_BLNK_LAST)
            {
                c->levelstate->horse->data[0].spawn_when = (th_time() - c->levelstate->level_start_time)+ 5000.0;

                c->levelstate->progress_state = TH_BAR_TWO;
            }
        }


    }


}


void th_tutorialUpdateSnake(th_TutorialObject* c,float dt,fn_RawInput* input,fn_vec2 screenSize,th_Character* cmap)
{


    bool picked_all_questions = false;
    int last_picked_question = -1;
    int num_questions_picked = 0;

    if (c->levelstate->question != NULL)
    {
        last_picked_question = c->levelstate->question->index_last_touched;
        bool pickedall = true;
        for (int k = 0 ; k < c->levelstate->question->entity_count;k++)
        {
            pickedall = pickedall && c->levelstate->question->is_collected[k];
            if (c->levelstate->question->is_collected[k])
            {
                num_questions_picked++;
            }
        }
        picked_all_questions = pickedall;
    }

    //this is really dirty

    // #define SEQ_BLNK_FIRST 0
    // #define SEQ_BEETLE 1
    // #define SEQ_BLNK_LAST 2



    #undef N_MESSAGES_TUT

    #define N_MESSAGES_TUT 3
    const char* messages[N_MESSAGES_TUT][N_LINES_TUT] = {{"","","",""},{"To kill a CENTIPEDE, shoot out all of its GEMS.","The machine gun is good for this.","","Press [%s] to continue."},
    {"","","",""},
    };

    int replace_mode[N_MESSAGES_TUT][N_LINES_TUT] = {{0,0,0,0},{0,0,0,5},{0,0,0,0}};

    // const char* crouch_key = th_getKeyName(input->binding_crouch);
    //
    // const char* machinegun_key = th_getKeyName(input->binding_weapon1);
    // const char* hammer_key = th_getKeyName(input->binding_weapon3);
    // const char* shotgun_key = th_getKeyName(input->binding_weapon2);

    bool candisplay = (c->sequence_progression != SEQ_BLNK_FIRST) || (c->displayed_mesg_question != last_picked_question);

    if (last_picked_question == 0 && c->sequence_progression < SEQ_BEETLE && candisplay)
    {
        c->sequence_progression = SEQ_BEETLE;
        c->displayed_mesg_question = last_picked_question;
        c->tutorial_progress_pct = 0.5;
    }

    if (c->sequence_progression == 0)
    {
        c->levelstate->progress_state = TH_BAR_FOUR;
    }



    renderTextMessages(c->sequence_progression,N_LINES_TUT, screenSize,cmap,messages,replace_mode,input);





    if (input->currentKeyStates[SDL_SCANCODE_N] && !input->currentKeyStatesPrev[SDL_SCANCODE_N] && c->sequence_progression != SEQ_BLNK_FIRST && c->sequence_progression != SEQ_BLNK_LAST)
    {
        if (picked_all_questions)
        {
            c->sequence_progression++;

            if (c->sequence_progression >= N_MESSAGES_TUT)
            {
                c->sequence_progression--;
            }


            if (c->sequence_progression == SEQ_BLNK_LAST)
            {
                c->levelstate->centipede->masters[0].spawn_when = (th_time() - c->levelstate->level_start_time)+ 5000.0;

                c->levelstate->progress_state = TH_BAR_THREE;
            }
        }


    }








}

void th_tutorialUpdate(th_TutorialObject* c,float dt,fn_RawInput* input,fn_vec2 screenSize,th_Character* cmap)
{
    if (c->type == TH_TUT_MOVEMENT)
    {
        th_tutorialUpdateMovement(c,dt,input,screenSize,cmap);
    }
    else if (c->type == TH_TUT_KILL_BEETLE)
    {
        th_tutorialUpdateBeetle(c,dt,input,screenSize,cmap);
    }
    else if (c->type == TH_TUT_KILL_CENTI)
    {
        th_tutorialUpdateSnake(c,dt,input,screenSize,cmap);
    }
}
