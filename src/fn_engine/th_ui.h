#pragma once
#include "th_time.h"
#include "th_gpu.h"
#include "r_shader.h"
#include "th_allocator.h"
#include "th_level.h"
#include "../fn_math/fn_spline.h"
#include "th_replay.h"
#include "../fn_config.h"
#include "../fn_input.h"
#include "th_spawnset.h"


#define MAX_LEVEL_COUNT 1024
#define N_NAGBARS 20

typedef enum
{
    TH_UI_TEXT,
    TH_UI_BUTTON,
    TH_UI_SLIDER,
    TH_UI_KEYBIND,
    TH_UI_IMAGE,
    TH_UI_PROGRESS,
    TH_UI_GRADIENT,
}th_UIEnum;


typedef void (*th_click_callback)(void* data);

typedef void (*th_slider_writeback)(void* data,float value);

typedef void (*th_int_writeback)(void* data,int value);

typedef void (*th_keybind_writeback)(void* data,SDL_Scancode value);




typedef struct
{
    th_click_callback quit_callback;
    void* quit_callback_data;
    th_click_callback options_callback;
    void* options_callback_data;
    th_click_callback levelselect_callback;
    void* levelselect_callback_data;
}th_MainMenuCallbacks;


typedef struct
{
    th_click_callback back_callback;
    void* back_callback_data;
}th_OptionsCallbacks;


typedef struct
{
    th_click_callback back_callback;
    void* back_callback_data;

    th_click_callback start_callback;
    void* start_callback_data;
}th_LevelSelectCallbacks;


typedef struct
{
    th_click_callback resume_callback;
    void* resume_callback_data;
    th_click_callback restart_callback;
    void* restart_callback_data;
    th_click_callback titlescreen_callback;
    void* titlescreen_callback_data;
    th_click_callback options_callback;
    void* options_callback_data;
}th_PauseMenuCallbacks;

typedef struct
{
    th_click_callback restart_callback;
    void* restart_callback_data;
    th_click_callback titlescreen_callback;
    void* titlescreen_callback_data;
}th_DeathMenuCallbacks;

#define TH_Y_OFFSETS_UI 8

typedef struct
{
    fn_vec2 position;
    fn_vec2 dims;
    fn_vec3 color;
    fn_vec3 color_highlight;
    const char* text;
    float size;
    th_UIEnum type;
    GLuint textures[12];
    const char* strings[12];
    th_Character* cmap;
    bool selectable;
    bool visible;
    float alpha;
    float old_alpha;

    th_click_callback callback_on_click;
    void* callback_data;

    /*
     * BUTTONS
     */
    bool save_on_click;
    bool depressed;
    char* dynamic_text;
    const char* tooltip;

    bool hovered;
    fn_vec2 hover_pos;
    bool dropdown;
    bool dropdown_element;

    /*
     * SLIDER
     */
    float slider_position;
    float max_x;
    float min_x;
    fn_vec2 cursor_dims;
    bool selected_cursor;
    float selection_offset;

    bool integer_slider;
    int imin;
    int imax;

    float fmin;
    float fmax;

    /*
     * KEYBIND
     */
    SDL_Scancode sc_update;


    /*
     * WRITEBACK
     * for dynamicly configurable properties
     */
    th_slider_writeback f_writeback;
    th_int_writeback i_writeback;
    th_keybind_writeback key_writeback;

    void* writeback_data;

    /*
     * Conditional Visibility
     */
    int conditional_vis_index;
    int* vis_condition;

    int conditional_vis_index2;
    int* vis_condition2;

    /*
     * shader
     */
    r_Shader* custom_shader;

    float alpha_target;
    th_timer_t alpha_target_timer;

    th_timer_t alpha_target_delay;
    float lerptime;

    float magnitude_wavy;

    fn_vec3 tint;

    bool nofade;
    bool slider_alpha_slowdown;

    float scale_central;

    /*
     * Falling text
     */
    float y_offsets[TH_Y_OFFSETS_UI];

    bool highlighted;

    /* z order */
    int z_order; //-1 is auto, -2 is always on top


    /*dirty hack */
    bool* disable_button_when_true;

    bool instant_writeback;

}th_UIElement;


typedef struct
{
    int value;
    float f_value;

    SDL_Scancode sc_value;

    int ui_offset;
}th_UIProperty;

typedef struct
{
    th_UIProperty resolution;
    th_UIProperty dynamic_shadows;
    th_UIProperty vsync;
    th_UIProperty fullscreen;
    th_UIProperty shadow_quality;
    th_UIProperty reflection_quality;
    th_UIProperty fog_quality;
    th_UIProperty reflection_scale;

    th_UIProperty music_volume;
    th_UIProperty sfx_volume;

    th_UIProperty forward_key;
    th_UIProperty backward_key;
    th_UIProperty left_key;
    th_UIProperty right_key;
    th_UIProperty crouch_key;
    th_UIProperty jump_key;

    th_UIProperty machinegun_key;
    th_UIProperty shotgun_key;
    th_UIProperty hammer_key;

    th_UIProperty mouse_sensitivity;

    th_UIProperty fov;

    th_UIProperty particle_lighting;

    th_UIProperty invert_mouse_y;

    th_UIProperty exposure;

    th_UIProperty gamma;
}th_OptionsMenuProperties;

typedef struct
{
    th_UIElement* elements;
    int element_count;

    th_OptionsMenuProperties* properties;

    int maxFPS; //setting for keeping maxfps, not configurable in UI


    int highlighted_element;
    bool grabbed_cursor;
    bool grabbed_keybind;

    /*
     * Level Select
     */
    int selected_level;
    int num_levels;

    th_LevelVictoryState* victory_state;

    /*
     * Victory Screen
     */
    th_UIElement* airtime_slider;
    th_UIElement* cleartime_slider;
    th_UIElement* damagetaken_slider;

    th_UIElement* medals_images[9];

    int flawless_enabled;

    th_UIElement* flawless_text;

    /*
     * HUD
     */
    int hammer_level;
    int machinegun_level;
    int shotgun_level;

    /*
     * HUD STATUS BAR
     */
    int status_offset;

    /*
     * SLIDE INDICATOR FOR HUD
     */
    th_UIElement* slide_element_indicator;

    /*
     * hack to allow for conditional visibility of sub_menu elements
     */
    int sub_menu_idx;

}th_UIlayout;

typedef struct
{
    th_UIProperty* property;
    int idx;
    th_UIElement* to_clear[10];
    int to_clear_count;

}th_UIButtonData;

#define TH_BUTTON_UI_DYNSHADOWS 0
#define TH_BUTTON_UI_SHADQUALITY 1
#define TH_BUTTON_UI_REFQUALITY 2
#define TH_BUTTON_UI_FOGQUALITY 3
#define TH_BUTTON_UI_RTSCALE 4
#define TH_BUTTON_UI_PARTICLELIGHT 5

typedef struct
{
    th_UIlayout* layout;
    int idx;
    th_UIElement* to_clear[10];
    int to_clear_count;
    int button_ids[6];//manually press the buttons
}th_UIPresetData;

typedef struct
{
    th_UIlayout* layout;
    int start;
    int end;
    int master_start;
    int master_end;
    //used for dropdowns
    bool* depressed_ptr;
    int* sub_menu_ptr;
    int sub_menu_set;
}th_UISelectionData;

typedef struct
{
    th_UIProperty* property;
    th_UIElement* element;
}th_UIKeybindData;


typedef th_UIKeybindData th_UIFloatSliderData;

typedef th_UIKeybindData th_UIIntSliderData;

typedef struct
{
    th_slider_writeback sfx_writeback;
    void* sfx_writeback_data;

    th_slider_writeback music_writeback;
    void* music_writeback_data;

    th_int_writeback fov_writeback;
    void* fov_writeback_data;

    th_slider_writeback sensitivity_writeback;
    void* sensitivity_writeback_data;

    th_slider_writeback exposure_writeback;
    void* exposure_writeback_data;

    th_slider_writeback gamma_writeback;
    void* gamma_writeback_data;

    th_keybind_writeback forward_writeback;
    th_keybind_writeback backward_writeback;
    th_keybind_writeback left_writeback;
    th_keybind_writeback right_writeback;
    th_keybind_writeback crouch_writeback;
    th_keybind_writeback jump_writeback;
    th_keybind_writeback machinegun_writeback;
    th_keybind_writeback shotgun_writeback;
    th_keybind_writeback hammer_writeback;
    void* forward_writeback_data;
    void* backward_writeback_data;
    void* left_writeback_data;
    void* right_writeback_data;
    void* crouch_writeback_data;
    void* jump_writeback_data;
    void* machinegun_writeback_data;
    void* shotgun_writeback_data;
    void* hammer_writeback_data;
}th_OptionsWritebacks;

// typedef struct
// {
//     const char* thumbnail;
//     const char* name;
// }th_LevelSelectPair;


void th_createUIElementText(th_UIElement* elem,const char* string,fn_vec2 pos_tx,float size,fn_vec3 color,th_Character* cmap);

void th_createMainMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_MainMenuCallbacks callbacks);

void th_createOptionsMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_OptionsCallbacks callbacks,th_OptionsWritebacks writebacks);

void th_createLevelSelectMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_LevelSelectCallbacks callbacks,th_LevelManifest levels,r_Shader* deathtext);

void th_createPauseMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_PauseMenuCallbacks callbacks);

void th_createDeathMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_DeathMenuCallbacks callbacks,r_Shader* deathtext);

void th_createVictoryMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_DeathMenuCallbacks callbacks,r_Shader* deathtext,th_LevelVictoryState* victory_state);

void th_processUI(th_UIlayout* layout,fn_RawInput* input,fn_vec2 screenSize);

void th_processUINoInput(th_UIlayout* layout,fn_vec2 screenSize);

void th_liftPropertiesConfig(th_UIlayout* layout,const char* path);

void th_dumpPropertiesConfig(th_UIlayout* layout,const char* path);

fn_vec2 th_stringDims(const char* string,float size,th_Character* cmap);

void th_uiNagbar(const char* text,fn_vec2 position,float size,float timeout);

void th_uiNagbarN(const char* text,fn_vec2 position,float size,float timeout,int i);

void th_createHUD(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize);

typedef struct
{
    const char* text;
    fn_vec2 position;
    float size;
    fn_vec3 color;
    th_timer_t timeset;
    bool active;
}th_UiNagInfo;

void th_initUINagbar();

void th_getUiNagbar(th_UiNagInfo* out);

void th_getUiNagbarN(th_UiNagInfo* out,int i);

void th_setModeStrings(char** modestrings,int num_modes);

void th_setAspectScale(float aspect_scale);

float th_getAspectScale();
