#pragma once
#include "../fn_engine/th_gpu.h"
#include "../fn_engine/th_allocator.h"
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "th_brass.h"
#include "th_player.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;

typedef enum
{
    TH_TUT_MOVEMENT,
    TH_TUT_KILL_BEETLE,
    TH_TUT_KILL_CENTI
}th_TutorialType;

typedef struct
{
    th_LevelState* levelstate;
    int sequence_progression;
    int eyeball_id;

    int* gem_ids;
    int gem_id_count;

    float tutorial_progress_pct;
    int displayed_mesg_question;

    th_TutorialType type;
}th_TutorialObject;

void th_tutorialInitialize(th_Allocator* alloc,th_TutorialObject* c,th_LevelState* levelstate,th_TutorialType type);

void th_tutorialUpdate(th_TutorialObject* c,float dt,fn_RawInput* input,fn_vec2 screenSize,th_Character* cmap);
