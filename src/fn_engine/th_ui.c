#include "th_ui.h"
#include "th_system.h"
#include "th_spawnset.h"
#include "th_allocator.h"
#include "th_audio.h"
#include "th_globals.h"
#include "../th_fopen.h"

static th_UiNagInfo naginfo;


static th_UiNagInfo naginfo_n[N_NAGBARS];

void th_initUINagbar()
{
    naginfo.text = "test";
    naginfo.position = fn_createVec2(0,0);
    naginfo.size = 0.5;
    naginfo.color = fn_createVec3(1,1,1);
    naginfo.active = false;
    naginfo.timeset = 0.0;

    for (int i = 0 ; i < N_NAGBARS;i++)
    {
        naginfo_n[i].text = "test";
        naginfo_n[i].position = fn_createVec2(0,0);
        naginfo_n[i].size = 0.5;
        naginfo_n[i].color = fn_createVec3(1,1,1);
        naginfo_n[i].active = false;
        naginfo_n[i].timeset = 0.0;
    }
}

static fn_vec2 stringDims(const char* string,float size,th_Character* cmap)
{
    int len = strlen(string);
    float w = 0;
    float h_max = 0;
    for (int i =0;i <len;i++)
    {
        th_Character ch = cmap[(unsigned char)string[i]];
        w += (ch.advance >> 6) * size;
        float h = ch.size.y * size;
        if (h > h_max)
        {
            h_max = h;
        }
    }
    return fn_createVec2(w,h_max);
}

fn_vec2 th_stringDims(const char* string,float size,th_Character* cmap)
{
    return stringDims(string,size,cmap);
}

static fn_vec2 textElementDims(th_UIElement* elem)
{
    return stringDims(elem->text,elem->size,elem->cmap);
}

static void elemPressButton(th_UIElement* elem)
{
    if ( elem->disable_button_when_true != NULL &&  *elem->disable_button_when_true)
    {
        return;
    }


    if (elem->callback_on_click != NULL)
    {
        elem->callback_on_click(elem->callback_data);
    }



    if (elem->depressed)
    {
        if (elem->dropdown)
        {
            elem->dynamic_text[1] = '.';
        }
        else
        {
            elem->dynamic_text[1] = ' ';
        }

        elem->depressed = false;
    }
    else
    {
        if (elem->dropdown)
        {
            elem->dynamic_text[1] = ':';
        }
        else
        {
            elem->dynamic_text[1] = '-';
        }
        elem->depressed = true;
    }
}

static th_UIProperty blankProperty()
{
    th_UIProperty p;
    p.value = 0;
    p.f_value = 0;
    p.sc_value = SDL_SCANCODE_W;
    p.ui_offset = 0;
    return p;
}

void th_initLayoutProperties(th_UIlayout* layout)
{
    memset(layout->properties,0,sizeof(th_OptionsMenuProperties));
    layout->properties->resolution = blankProperty();
    layout->properties->dynamic_shadows = blankProperty();
    layout->properties->shadow_quality = blankProperty();
    layout->properties->reflection_quality = blankProperty();
    layout->properties->fog_quality = blankProperty();
    layout->properties->reflection_scale = blankProperty();
    layout->properties->fullscreen = blankProperty();
    layout->properties->vsync = blankProperty();

    layout->properties->music_volume = blankProperty();
    layout->properties->sfx_volume = blankProperty();

    layout->properties->forward_key = blankProperty();
    layout->properties->backward_key = blankProperty();
    layout->properties->left_key = blankProperty();
    layout->properties->right_key = blankProperty();
    layout->properties->crouch_key = blankProperty();

    layout->properties->machinegun_key = blankProperty();
    layout->properties->shotgun_key = blankProperty();
    layout->properties->hammer_key = blankProperty();

    layout->properties->mouse_sensitivity = blankProperty();
    layout->properties->fov = blankProperty();
    layout->properties->jump_key = blankProperty();

    layout->properties->particle_lighting = blankProperty();
    layout->properties->invert_mouse_y = blankProperty();

    layout->properties->gamma = blankProperty();

    layout->properties->exposure = blankProperty();

    layout->properties->fov.value = 90;

    layout->properties->gamma.value = 2.2;

    layout->properties->exposure.value = 0.0;
}

void th_initUIElement(th_UIElement* elem)
{
    elem->highlighted = false;
    elem->f_writeback = NULL;
    elem->i_writeback = NULL;
    elem->key_writeback = NULL;

    elem->writeback_data = NULL;

    elem->conditional_vis_index = 0;
    elem->vis_condition = NULL;

    elem->conditional_vis_index2 = 0;
    elem->vis_condition2 = NULL;

    elem->custom_shader = NULL;
    elem->alpha = 1.0;

    elem->alpha_target = 1.0;
    elem->alpha_target_timer = 0.0;
    elem->alpha_target_delay = 0.0;
    elem->lerptime = 500;
    elem->magnitude_wavy = 35.0;

    elem->tint = fn_createVec3(1,1,1);
    elem->nofade = false;
    elem->slider_alpha_slowdown = false;
    elem->scale_central = 1.0;
    elem->old_alpha = 0.0;
    elem->dropdown = false;
    elem->dropdown_element = false;
    elem->z_order = -1;
    elem->disable_button_when_true = NULL;

    for (int i = 0 ; i < TH_Y_OFFSETS_UI;i++)
    {
        elem->y_offsets[i] = 0.0;
    }

    elem->fmin = 0.0;
    elem->fmax = 1.0;

    elem->instant_writeback = false;
}

void th_createUIElementText(th_UIElement* elem,const char* string,fn_vec2 pos_tx,float size,fn_vec3 color,th_Character* cmap)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_TEXT;
    elem->position = pos_tx;
    elem->text = string;
    elem->size = size;
    elem->color = color;
    elem->color_highlight = color;
    elem->cmap = cmap;
    elem->dims = textElementDims(elem);
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
}

void th_createUIElementButton(th_UIElement* elem,const char* tooltip,fn_vec2 pos_tx,float size,fn_vec3 color,th_Character* cmap)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_BUTTON;
    elem->position = pos_tx;
    elem->text = "[ ]";
    elem->dynamic_text = th_strdup("[ ]");
    elem->size = size;
    elem->color = color;
    elem->color_highlight = color;
    elem->cmap = cmap;
    elem->dims = stringDims(elem->dynamic_text,elem->size,elem->cmap);
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->depressed = false;
    elem->tooltip = tooltip;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
}

void th_createUIElementSlider(th_UIElement* elem,const char* string,fn_vec2 pos_tx,float size,fn_vec3 color,th_Character* cmap)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_SLIDER;
    elem->position = pos_tx;
    elem->text = string;
    elem->size = size;
    elem->color = color;
    elem->color_highlight = color;
    elem->cmap = cmap;
    elem->dims = textElementDims(elem);
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
    elem->slider_position = 0;
    elem->min_x = elem->position.x + stringDims("[",size,cmap).x;
    elem->max_x = elem->position.x + elem->dims.x - stringDims("]",size,cmap).x - stringDims("I",size,cmap).x;
    elem->cursor_dims = stringDims("I",size,cmap);
    elem->selected_cursor = false;
    elem->selection_offset = 0;
    elem->integer_slider = false;
    elem->imin = 45;
    elem->imax = 175;
}


void th_createUIElementKeybind(th_UIElement* elem,fn_vec2 pos_tx,float size,fn_vec3 color,th_Character* cmap)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_KEYBIND;
    elem->position = pos_tx;
    elem->text = "[PRESS ANY KEY]";
    elem->dynamic_text = th_strdup("[PRESS ANY KEY]");
    elem->size = size;
    elem->color = color;
    elem->color_highlight = color;
    elem->cmap = cmap;
    elem->dims = stringDims(elem->dynamic_text,elem->size,elem->cmap);
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->depressed = false;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
    elem->sc_update = SDL_SCANCODE_W;
}

void th_createUIElementImageTexture(th_UIElement* elem,fn_vec2 pos_tx,fn_vec2 dim,GLuint tex)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_IMAGE;
    elem->position = pos_tx;
    elem->dims = dim;
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->depressed = false;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
    elem->textures[0] = tex;
}

void th_createUIElementImage(th_UIElement* elem,fn_vec2 pos_tx,fn_vec2 dim,const char* filename)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_IMAGE;
    elem->position = pos_tx;
    elem->dims = dim;
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->depressed = false;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
    elem->textures[0] = fn_loadTexture(filename);
}

void th_createUIElementProgress(th_UIElement* elem,fn_vec2 pos_tx,fn_vec2 dim)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_PROGRESS;
    elem->position = pos_tx;
    elem->dims = dim;
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->depressed = false;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
}

void th_createUIElementGradient(th_UIElement* elem,fn_vec2 pos_tx,fn_vec2 dim)
{
    th_initUIElement(elem);
    elem->save_on_click = false;
    elem->type = TH_UI_GRADIENT;
    elem->position = pos_tx;
    elem->dims = dim;
    elem->selectable = false;
    elem->callback_on_click = NULL;
    elem->callback_data = NULL;
    elem->visible = true;
    elem->depressed = false;
    elem->hovered = false;
    elem->hover_pos = fn_createVec2(0,0);
}

void th_button_print(void* data)
{
    printf("Hello\n");
}

void th_createMainMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_MainMenuCallbacks callbacks)
{
    layout->properties = NULL;
    layout->grabbed_cursor = false;
    layout->grabbed_keybind = false;

    const float phi = 0.618;
    const float oneminusphi = 0.3819;
    layout->elements = malloc(sizeof(th_UIElement)*100);
    layout->element_count = 0;

    // w_op = screenSize.x*0.333;
    // h_op = screenSize.y*0.012;
    // op_pos = fn_multVec2(fn_createVec2(0.5,0.95),screenSize);
    // op_pos.x = op_pos.x - w_op*0.5;
    // op_pos.y = op_pos.y - h_op*0.5;
    th_createUIElementGradient(&layout->elements[layout->element_count],fn_createVec2(0,0),screenSize);
    layout->airtime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count ].alpha = 1.0;
    layout->elements[layout->element_count ].tint = fn_createVec3s(0.0);
    layout->element_count++;

    th_createUIElementText(&layout->elements[layout->element_count],"TACHYON FIRE",fn_multVec2(fn_createVec2(0.01,phi),screenSize),1,fn_createVec3(1,1,1),cmap);
    float w_tex = textElementDims(&layout->elements[layout->element_count]).x;
    layout->element_count++;

    fn_vec2 spos = fn_multVec2(fn_createVec2(0.46,phi),screenSize);
    float y2 =  spos.y - 0.8*64.0*((th_getAspectScale()));
    th_createUIElementText(&layout->elements[layout->element_count],"V1.2",fn_createVec2(spos.x,y2),0.333,fn_createVec3(0,0.8,1.0),cmap);
    layout->element_count++;

    fn_vec3 hcolor = fn_createVec3(1,0.8,0);
    fn_vec3 bcolor = fn_createVec3(1*0.5,0.8*0.5,0);

    fn_vec2 selpos = fn_multVec2(fn_createVec2(0.1,phi*0.66666*oneminusphi + phi*0.5),screenSize);
    selpos.x = 0.01*screenSize.x + oneminusphi*w_tex;
    th_createUIElementText(&layout->elements[layout->element_count],"Level Select",selpos,phi,bcolor,cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = hcolor;
    layout->elements[layout->element_count].callback_on_click = callbacks.levelselect_callback;
    layout->elements[layout->element_count].callback_data = callbacks.levelselect_callback_data;
    layout->element_count++;

    selpos = fn_multVec2(fn_createVec2(0.1,phi*0.333333*oneminusphi + phi*0.5),screenSize);
    selpos.x = 0.01*screenSize.x + oneminusphi*w_tex;
    th_createUIElementText(&layout->elements[layout->element_count],"Options",selpos,phi,bcolor,cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = hcolor;
    layout->elements[layout->element_count].callback_on_click = callbacks.options_callback;
    layout->elements[layout->element_count].callback_data = callbacks.options_callback_data;
    layout->element_count++;

    selpos = fn_multVec2(fn_createVec2(0.1,phi*0.5),screenSize);
    selpos.x = 0.01*screenSize.x + oneminusphi*w_tex;
    th_createUIElementText(&layout->elements[layout->element_count],"Quit",selpos,phi,bcolor,cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = hcolor;
    layout->elements[layout->element_count].callback_on_click = callbacks.quit_callback;
    layout->elements[layout->element_count].callback_data = callbacks.quit_callback_data;
    layout->element_count++;

    layout->highlighted_element = 3;
}


void th_button_list_callback(void* d_in)
{
    th_UIButtonData* data = (th_UIButtonData*)d_in;
    data->property->value = data->idx;
    for (int j = 0 ; j < data->to_clear_count;j++)
    {
        data->to_clear[j]->dynamic_text[1] = ' ';
        data->to_clear[j]->depressed = false;
    }

}

void th_resolution_callback(void* d_in)
{
    th_UIButtonData* data = (th_UIButtonData*)d_in;
    data->property->value = data->idx;
    for (int j = 0 ; j < data->to_clear_count;j++)
    {
        if (data->to_clear[j]->callback_on_click != NULL)
        {
            data->to_clear[j]->callback_on_click(data->to_clear[j]->callback_data);
        }

        data->to_clear[j]->dynamic_text[1] = '.';
        data->to_clear[j]->depressed = false;


    }
}

void th_button_preset_callback(void* d_in)
{
    th_UIPresetData* data = (th_UIPresetData*)d_in;

    for (int j = 0 ; j < data->to_clear_count;j++)
    {
        data->to_clear[j]->dynamic_text[1] = ' ';
        data->to_clear[j]->depressed = false;
    }

    if (data->idx == 0) //minimum
    {
        th_UIElement* elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_DYNSHADOWS] + 0];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_SHADQUALITY] + 0];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_REFQUALITY] + 0];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_FOGQUALITY] + 0];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_RTSCALE] + 0];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_PARTICLELIGHT] + 0];
        elemPressButton(elem);
    }
    else if (data->idx == 1)//midrange
    {
        th_UIElement* elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_DYNSHADOWS] + 1];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_SHADQUALITY] + 1];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_REFQUALITY] + 0];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_FOGQUALITY] + 1];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_RTSCALE] + 1];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_PARTICLELIGHT] + 1];
        elemPressButton(elem);
    }
    else if (data->idx == 2)//upgraded
    {
        th_UIElement* elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_DYNSHADOWS] + 1];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_SHADQUALITY] + 1];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_REFQUALITY] + 0];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_FOGQUALITY] + 2];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_RTSCALE] + 2];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_PARTICLELIGHT] + 1];
        elemPressButton(elem);
    }
    else if (data->idx == 3)
    {
        th_UIElement* elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_DYNSHADOWS] + 1];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_SHADQUALITY] + 2];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_REFQUALITY] + 2];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_FOGQUALITY] + 2];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_RTSCALE] + 2];
        elemPressButton(elem);

        elem = &data->layout->elements[data->button_ids[TH_BUTTON_UI_PARTICLELIGHT] + 1];
        elemPressButton(elem);
    }
}

static const char* getKeyName(SDL_Scancode sc)
{
    SDL_Keycode key = SDL_GetKeyFromScancode(sc);
    const char* sc_name = SDL_GetKeyName(key);


    bool ascii = true;
    for (const unsigned char* p = (const unsigned char*)sc_name; *p; ++p)
    {
        if (*p >= 128)
        {
            ascii = false;
            break;
        }
    }

    if (!ascii)
        sc_name = SDL_GetScancodeName(sc);

    return sc_name;
}

void th_keybind_callback(void* data)
{
    th_UIKeybindData* kdata = (th_UIKeybindData*)data;

    th_UIElement* elem = kdata->element;

    // const char* sc_name = SDL_GetScancodeName(elem->sc_update);
    const char* sc_name = getKeyName(elem->sc_update);
    // SDL_Keycode key = SDL_GetKeyFromScancode(elem->sc_update);
    // const char* sc_name = SDL_GetKeyName(key);

    //elem->dynamic_text = th_strdup("[PRESS ANY KEY]");
    //14 chars
    int to_copy = 14;
    if (strlen(sc_name) < 14)
    {
        to_copy = strlen(sc_name);
    }
    int i;
    for (i = 0 ; i < to_copy;i++)
    {
        elem->dynamic_text[i] = sc_name[i];
    }
    elem->dynamic_text[i] = '\0';

    elem->dims = stringDims(elem->dynamic_text,elem->size,elem->cmap);

    kdata->property->sc_value = elem->sc_update;

    printf("Keybind %i\n",elem->sc_update);

    if (elem->key_writeback != NULL)
    {
        elem->key_writeback(elem->writeback_data,elem->sc_update);
    }
}


void th_floatslider_callback(void* data)
{
    th_UIFloatSliderData* kdata = (th_UIFloatSliderData*)data;

    th_UIElement* elem = kdata->element;

    // kdata->property->f_value = elem->slider_position;

    float fval = ((float)elem->fmin + (elem->slider_position * (float)(elem->fmax - elem->fmin)));
    kdata->property->f_value = fval;

    if (elem->f_writeback != NULL)
    {
        elem->f_writeback(elem->writeback_data,fval);
    }
}

void th_intslider_callback(void* data)
{
    th_UIIntSliderData* kdata = (th_UIIntSliderData*)data;

    th_UIElement* elem = kdata->element;


    int ival = floor((float)elem->imin + (elem->slider_position * (float)(elem->imax - elem->imin)));
    kdata->property->value = ival;

    if (elem->i_writeback != NULL)
    {
        elem->i_writeback(elem->writeback_data,ival);
    }
}

static const float buttonsize = 0.38;

void th_make_alligned_slider(th_UIlayout* layout,int base_elem,int base_elem_x,int arr_offset,th_Character* cmap,fn_vec2 screenSize,th_UIProperty* property)
{
    float buttons_x_align = layout->elements[base_elem_x].position.x + layout->elements[base_elem_x].dims.x + screenSize.x*0.03;
    fn_vec2 spos;
    spos.y = layout->elements[base_elem].position.y;
    spos.x = buttons_x_align;
    th_createUIElementSlider(&layout->elements[arr_offset + 0],"[-----------]",spos,buttonsize,fn_createVec3(0.5,0.5,0.5),cmap);
    layout->elements[arr_offset + 0].color_highlight = fn_createVec3(1,1,1);
    layout->elements[arr_offset + 0].callback_on_click = th_floatslider_callback;
    th_UIFloatSliderData* data = malloc(sizeof(th_UIFloatSliderData));
    data->element = &layout->elements[arr_offset + 0];
    data->property = property;
    layout->elements[arr_offset + 0].callback_data = data;

    //push up property data
    layout->elements[arr_offset + 0].slider_position = fn_clamp(property->f_value,0,1);
    layout->elements[arr_offset + 0].callback_on_click(data);

}

void th_make_alligned_slider_minmax(th_UIlayout* layout,int base_elem,int base_elem_x,int arr_offset,th_Character* cmap,fn_vec2 screenSize,th_UIProperty* property,float minf,float maxf)
{
    float buttons_x_align = layout->elements[base_elem_x].position.x + layout->elements[base_elem_x].dims.x + screenSize.x*0.03;
    fn_vec2 spos;
    spos.y = layout->elements[base_elem].position.y;
    spos.x = buttons_x_align;
    th_createUIElementSlider(&layout->elements[arr_offset + 0],"[-----------]",spos,buttonsize,fn_createVec3(0.5,0.5,0.5),cmap);
    layout->elements[arr_offset + 0].color_highlight = fn_createVec3(1,1,1);

    layout->elements[arr_offset].fmin = minf;
    layout->elements[arr_offset].fmax = maxf;

    layout->elements[arr_offset + 0].callback_on_click = th_floatslider_callback;
    th_UIFloatSliderData* data = malloc(sizeof(th_UIFloatSliderData));
    data->element = &layout->elements[arr_offset + 0];
    data->property = property;
    layout->elements[arr_offset + 0].callback_data = data;

    //push up property data
    float fval = (float)property->f_value - (float)minf;
    fval = fval / (float)(maxf - minf);
    layout->elements[arr_offset + 0].slider_position = fn_clamp(fval,0,1);
    layout->elements[arr_offset + 0].callback_on_click(data);

    // layout->elements[arr_offset + 0].slider_position = fn_clamp(property->f_value,0,1);
    // layout->elements[arr_offset + 0].callback_on_click(data);

}

void th_make_alligned_slider_integer(th_UIlayout* layout,int base_elem,int arr_offset,th_Character* cmap,fn_vec2 screenSize,int imin,int imax,th_UIProperty* property)
{
    float buttons_x_align = layout->elements[base_elem].position.x + layout->elements[base_elem].dims.x + screenSize.x*0.03;
    fn_vec2 spos;
    spos.y = layout->elements[base_elem].position.y;
    spos.x = buttons_x_align;
    th_createUIElementSlider(&layout->elements[arr_offset + 0],"[-----------]",spos,buttonsize,fn_createVec3(0.5,0.5,0.5),cmap);
    layout->elements[arr_offset + 0].color_highlight = fn_createVec3(1,1,1);

    layout->elements[arr_offset].integer_slider = true;
    layout->elements[arr_offset].imin = imin;
    layout->elements[arr_offset].imax = imax;

    layout->elements[arr_offset + 0].callback_on_click = th_intslider_callback;
    th_UIIntSliderData* data = malloc(sizeof(th_UIIntSliderData));
    data->element = &layout->elements[arr_offset + 0];
    data->property = property;
    layout->elements[arr_offset + 0].callback_data = data;

    //push up property data

    float fval = (float)property->value - (float)imin;
    fval = fval / (float)(imax - imin);
    layout->elements[arr_offset + 0].slider_position = fn_clamp(fval,0,1);
    layout->elements[arr_offset + 0].callback_on_click(data);
}

void th_make_alligned_keybind(th_UIlayout* layout,int base_elem,int arr_offset,th_Character* cmap,fn_vec2 screenSize,th_UIProperty* property)
{
    float buttons_x_align = layout->elements[base_elem].position.x + layout->elements[base_elem].dims.x + screenSize.x*0.03;
    fn_vec2 spos;
    spos.y = layout->elements[base_elem].position.y;
    spos.x = buttons_x_align;
    th_createUIElementKeybind(&layout->elements[arr_offset + 0],spos,buttonsize,fn_createVec3(1,1,1),cmap);
    layout->elements[arr_offset + 0].color_highlight = fn_createVec3(0.5,0.5,0.5);
    layout->elements[arr_offset + 0].callback_on_click = th_keybind_callback;
    th_UIKeybindData* data = malloc(sizeof(th_UIKeybindData));
    data->element = &layout->elements[arr_offset + 0];
    data->property = property;
    layout->elements[arr_offset + 0].callback_data = data;

    //push up property data
    layout->elements[arr_offset + 0].sc_update = property->sc_value;
    layout->elements[arr_offset + 0].callback_on_click(data);
}

void th_make_n_buttons(th_UIlayout* layout,int base_elem,int arr_offset,int n,th_Character* cmap,fn_vec2 screenSize,const char ** tooltips,th_UIProperty* property)
{

    if (n == 0)
    {
        return;
    }

    property->ui_offset = arr_offset;

    float buttons_x_align = layout->elements[base_elem].position.x + layout->elements[base_elem].dims.x + screenSize.x*0.03;
    fn_vec2 spos;
    spos.y = layout->elements[base_elem].position.y;
    spos.x = buttons_x_align;
    th_createUIElementButton(&layout->elements[arr_offset + 0],tooltips[0],spos,buttonsize,fn_createVec3(0.5,0.5,0.5),cmap);
    layout->elements[arr_offset + 0].dynamic_text[1] = '-';
    layout->elements[arr_offset + 0].depressed = true;
    layout->elements[arr_offset + 0].color_highlight = fn_createVec3(1,1,1);

    th_UIButtonData* data = malloc(sizeof(th_UIButtonData));
    data->property = property;
    data->idx = 0;
    data->to_clear_count = n;
    for (int j = 0 ; j < n;j++)
    {
        data->to_clear[j] = &layout->elements[arr_offset + j];
    }

    layout->elements[arr_offset + 0].callback_on_click = th_button_list_callback;
    layout->elements[arr_offset + 0].callback_data = data;

    for (int i = 0 ; i < n - 1;i++)
    {
        spos.x = buttons_x_align + layout->elements[arr_offset + 0].dims.x*1.5*((float)(i + 1));
        th_createUIElementButton(&layout->elements[arr_offset + 1 + i],tooltips[i + 1],spos,buttonsize,fn_createVec3(0.5,0.5,0.5),cmap);
        layout->elements[arr_offset + 1 + i].dynamic_text[1] = ' ';
        layout->elements[arr_offset + 1 + i].depressed = false;
        layout->elements[arr_offset + 1 + i].color_highlight = fn_createVec3(1,1,1);

        th_UIButtonData* data = malloc(sizeof(th_UIButtonData));
        data->property = property;
        data->idx = i + 1;
        data->to_clear_count = n;
        for (int j = 0 ; j < n;j++)
        {
            data->to_clear[j] = &layout->elements[arr_offset + j];
        }

        layout->elements[arr_offset + 1 + i].callback_on_click = th_button_list_callback;
        layout->elements[arr_offset + 1 + i].callback_data = data;
    }

    //lift from property
    int idx = property->value;
    if (idx > n - 1)
    {
        idx = n -1;
    }
    if (idx <  0)
    {
        idx = 0;
    }
    th_button_list_callback(layout->elements[arr_offset + idx].callback_data);
    layout->elements[arr_offset + idx].dynamic_text[1] = '-';
    layout->elements[arr_offset + idx].depressed = true;

}

void th_make_n_buttons_presets(th_UIlayout* layout,int base_elem,int arr_offset,int n,th_Character* cmap,fn_vec2 screenSize,const char ** tooltips,th_OptionsMenuProperties* properties)
{

    if (n == 0)
    {
        return;
    }

    float buttons_x_align = layout->elements[base_elem].position.x + layout->elements[base_elem].dims.x + screenSize.x*0.03;
    fn_vec2 spos;
    spos.y = layout->elements[base_elem].position.y;
    spos.x = buttons_x_align;
    th_createUIElementButton(&layout->elements[arr_offset + 0],tooltips[0],spos,buttonsize,fn_createVec3(0.5,0.0,0.5),cmap);
    layout->elements[arr_offset + 0].dynamic_text[1] = ' ';
    layout->elements[arr_offset + 0].depressed = false;
    layout->elements[arr_offset + 0].color_highlight = fn_createVec3(1,1,1);

    th_UIPresetData* data = malloc(sizeof(th_UIPresetData));
    data->layout = layout;
    data->idx = 0;
    data->to_clear_count = n;
    for (int j = 0 ; j < n;j++)
    {
        data->to_clear[j] = &layout->elements[arr_offset + j];
    }
    data->button_ids[TH_BUTTON_UI_DYNSHADOWS] = layout->properties->dynamic_shadows.ui_offset;
    data->button_ids[TH_BUTTON_UI_SHADQUALITY] = layout->properties->shadow_quality.ui_offset;
    data->button_ids[TH_BUTTON_UI_REFQUALITY] = layout->properties->reflection_quality.ui_offset;
    data->button_ids[TH_BUTTON_UI_FOGQUALITY] = layout->properties->fog_quality.ui_offset;
    data->button_ids[TH_BUTTON_UI_RTSCALE] = layout->properties->reflection_scale.ui_offset;
    data->button_ids[TH_BUTTON_UI_PARTICLELIGHT] = layout->properties->particle_lighting.ui_offset;

    layout->elements[arr_offset + 0].callback_on_click = th_button_preset_callback;
    layout->elements[arr_offset + 0].callback_data = data;

    for (int i = 0 ; i < n - 1;i++)
    {
        spos.x = buttons_x_align + layout->elements[arr_offset + 0].dims.x*1.5*((float)(i + 1));
        th_createUIElementButton(&layout->elements[arr_offset + 1 + i],tooltips[i + 1],spos,buttonsize,fn_createVec3(0.5,0.0,0.5),cmap);
        layout->elements[arr_offset + 1 + i].dynamic_text[1] = ' ';
        layout->elements[arr_offset + 1 + i].depressed = false;
        layout->elements[arr_offset + 1 + i].color_highlight = fn_createVec3(1,1,1);

        th_UIPresetData* data = malloc(sizeof(th_UIPresetData));
        data->layout = layout;
        data->idx = i + 1;
        data->to_clear_count = n;
        for (int j = 0 ; j < n;j++)
        {
            data->to_clear[j] = &layout->elements[arr_offset + j];
        }
        data->button_ids[TH_BUTTON_UI_DYNSHADOWS] = layout->properties->dynamic_shadows.ui_offset;
        data->button_ids[TH_BUTTON_UI_SHADQUALITY] = layout->properties->shadow_quality.ui_offset;
        data->button_ids[TH_BUTTON_UI_REFQUALITY] = layout->properties->reflection_quality.ui_offset;
        data->button_ids[TH_BUTTON_UI_FOGQUALITY] = layout->properties->fog_quality.ui_offset;
        data->button_ids[TH_BUTTON_UI_RTSCALE] = layout->properties->reflection_scale.ui_offset;
        data->button_ids[TH_BUTTON_UI_PARTICLELIGHT] = layout->properties->particle_lighting.ui_offset;

        layout->elements[arr_offset + 1 + i].callback_on_click = th_button_preset_callback;
        layout->elements[arr_offset + 1 + i].callback_data = data;
    }

    // //lift from property
    // int idx = property->value;
    // if (idx > n - 1)
    // {
    //     idx = n -1;
    // }
    // if (idx <  0)
    // {
    //     idx = 0;
    // }
    // th_button_preset_callback(layout->elements[arr_offset + idx].callback_data);
    // layout->elements[arr_offset + idx].dynamic_text[1] = '-';
    // layout->elements[arr_offset + idx].depressed = true;

}

void th_selection_callback(void* data)
{
    th_UISelectionData* sel_data = (th_UISelectionData*)data;
    if (sel_data->layout->grabbed_keybind)
    {
        return;
    }

    if (sel_data->sub_menu_ptr != NULL)
    {
        *sel_data->sub_menu_ptr = sel_data->sub_menu_set;
    }


    for (int i = 0 ; i < sel_data->layout->element_count ;i++)
    {
        if (i >= sel_data->master_start && i < sel_data->master_end)
        {
            sel_data->layout->elements[i].visible = false;
            if (i >= sel_data->start && i < sel_data->end)
            {
                if (!sel_data->layout->elements[i].dropdown_element)
                {
                    sel_data->layout->elements[i].visible = true;
                }

            }
            else if (sel_data->layout->elements[i].dropdown)
            {
                sel_data->layout->elements[i].depressed = false;
                sel_data->layout->elements[i].dynamic_text[1] = '.';
            }
        }

    }
}

void th_dropdown_callback(void* data)
{
    th_UISelectionData* sel_data = (th_UISelectionData*)data;
    if (sel_data->layout->grabbed_keybind)
    {
        return;
    }

    for (int i = 0 ; i < sel_data->layout->element_count ;i++)
    {
        if (i >= sel_data->master_start && i < sel_data->master_end)
        {
            if (sel_data->depressed_ptr != NULL && *sel_data->depressed_ptr)
            {
                sel_data->layout->elements[i].visible = false;
                // if (i >= sel_data->start && i < sel_data->end)
                // {
                //     sel_data->layout->elements[i].visible = true;
                // }
            }
            else
            {
                sel_data->layout->elements[i].visible = true;
                // if (i >= sel_data->start && i < sel_data->end)
                // {
                //     sel_data->layout->elements[i].visible = false;
                // }
            }


        }

    }
}

static char** modestrings_local = NULL;
static int num_modestrings_local = 0;

void th_setModeStrings(char** modestrings,int num_modes)
{
    modestrings_local = modestrings;
    num_modestrings_local = num_modes;
}

void th_createOptionsMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_OptionsCallbacks callbacks,th_OptionsWritebacks writebacks)
{
    layout->sub_menu_idx = 0;
    layout->grabbed_cursor = false;
    layout->grabbed_keybind = false;
    th_liftPropertiesConfig(layout,th_getPathSettings());


    const float oneminusphi = 0.05;
    const int max_ui_elements = 512;
    layout->elements = malloc(sizeof(th_UIElement)*max_ui_elements);
    layout->element_count = 0;

    th_createUIElementGradient(&layout->elements[layout->element_count],fn_createVec2(0,0),screenSize);
    layout->airtime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count ].alpha = 1.0;
    layout->elements[layout->element_count ].tint = fn_createVec3s(0.0);
    layout->element_count++;

    float w_op = stringDims("Options",1,cmap).x;

    fn_vec2 op_pos = fn_multVec2(fn_createVec2(0.5,0.9),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;

    th_createUIElementText(&layout->elements[layout->element_count],"Options",op_pos,1,fn_createVec3(1,1,1),cmap);
    float w_tex = textElementDims(&layout->elements[layout->element_count]).x;
    int options_button = layout->element_count;
    layout->element_count++;

    fn_vec2 spos = fn_multVec2(fn_createVec2(0.05,0.05),screenSize);
    th_createUIElementText(&layout->elements[layout->element_count],"Save & Return",spos,0.45,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    layout->elements[layout->element_count].callback_on_click = callbacks.back_callback;
    layout->elements[layout->element_count].callback_data = callbacks.back_callback_data;
    layout->elements[layout->element_count].save_on_click = true;
    int back_button = layout->element_count;
    layout->element_count++;

    float h_skip = stringDims("Graphics",0.75,cmap).y*1.5;

    float y_align_top = screenSize.y*0.5 + h_skip*3*0.5;

    spos = fn_multVec2(fn_createVec2(oneminusphi,0.05),screenSize);
    spos.y = y_align_top - h_skip*0;
    th_createUIElementText(&layout->elements[layout->element_count],"Graphics",spos,0.75,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    int graphics_button = layout->element_count;
    layout->element_count++;

    spos = fn_multVec2(fn_createVec2(oneminusphi,0.05),screenSize);
    spos.y = y_align_top - h_skip*1;
    th_createUIElementText(&layout->elements[layout->element_count],"Sound",spos,0.75,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    int sound_button = layout->element_count;
    layout->element_count++;

    spos = fn_multVec2(fn_createVec2(oneminusphi,0.05),screenSize);
    spos.y = y_align_top - h_skip*2;
    th_createUIElementText(&layout->elements[layout->element_count],"Game",spos,0.75,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    int game_button = layout->element_count;
    layout->element_count++;


    /*
     * GRAPHICS
     * SETTINGS
     */

    const float x_align_graphics = 0.45;
    float g_skip = stringDims("Dynamic Shadows",buttonsize,cmap).y*2;

    #define CREATE_BUTTON(name_str, index_var, row)            \
    do {                                                  \
        spos = fn_multVec2(fn_createVec2(x_align_graphics, 0.05), screenSize); \
        spos.y = y_align_top - g_skip * (row);          \
        spos.x = layout->elements[graphics_button].position.x + layout->elements[graphics_button].dims.x + screenSize.x * 0.04; \
        th_createUIElementText(&layout->elements[layout->element_count], name_str, spos, buttonsize, fn_createVec3(1,0.8,0), cmap); \
        layout->elements[layout->element_count].selectable = false; \
        layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0); \
        index_var = layout->element_count;          \
        layout->element_count++;                         \
    } while(0)

    y_align_top = screenSize.y*0.45 + g_skip*8*0.5;


    int graphics_element_start = layout->element_count;
    int resolution_button, shadow_dyn_button, shadow_quality_button;
    int reflection_quality_button, fog_quality_button, reflection_scale_button;
    int vsync_button, fullscreen_button,plight_button;
    int preset_button;

    CREATE_BUTTON("PRESETS", preset_button, -1);
    layout->elements[layout->element_count - 1].color = fn_createVec3(0.8,0.5,0);

    CREATE_BUTTON("Resolution", resolution_button, 0);
    CREATE_BUTTON("Dynamic Shadows", shadow_dyn_button, 1);
    CREATE_BUTTON("Shadow Quality", shadow_quality_button, 2);
    CREATE_BUTTON("Reflection Quality", reflection_quality_button, 3);
    CREATE_BUTTON("Fog Quality", fog_quality_button, 4);
    CREATE_BUTTON("Raytracing Scale", reflection_scale_button, 5);
    CREATE_BUTTON("Vsync", vsync_button, 6);
    CREATE_BUTTON("Fullscreen", fullscreen_button, 7);
    CREATE_BUTTON("Particle Lighting", plight_button, 8);


    spos = fn_multVec2(fn_createVec2(x_align_graphics, 0.05), screenSize);
    spos.y = y_align_top - g_skip * (9.5);
    spos.x = layout->elements[graphics_button].position.x + layout->elements[graphics_button].dims.x + screenSize.x * -0.04;
    th_createUIElementText(&layout->elements[layout->element_count], "Restart Game To Take Effect", spos, 0.5, fn_createVec3(0.9,0.9,0.9), cmap);
    layout->elements[layout->element_count].selectable = false;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    layout->element_count++;


    //Fog quality buttons

    const char *tooltips[] = {
        "min",
        "med",
        "max"
    };

    const char *tooltips_scale[] = {
        "0.5x",
        "0.75x",
        "1.0x"
    };

    //TODO add more reolutions, 6 in total
    //add ultrawide
    const char *tooltips_resolution[] = {
        "1280x720",
        "1366x768",
        "1600x900",
        "1920x1080",
        "2560x1080",
        "2560x1440",
        "3440x1440",
        "3840x2160",
    };

    const char *tooltips_shadows[] = {
        "off",
        "on",
    };

    const char *tooltips_fullscreen[] = {
        "off",
        "exclusive",
        "borderless"
    };

    const char *tooltips_presets[] = {
        "minimum",
        "midrange",
        "upgraded",
        "extreme",
    };

    float buttons_x_align = layout->elements[resolution_button].position.x + layout->elements[resolution_button].dims.x + screenSize.x*0.03;
    fn_vec2 spos_choices;
    spos_choices.y = layout->elements[resolution_button].position.y;
    spos_choices.x = buttons_x_align + screenSize.x*0.2;
    th_createUIElementButton(&layout->elements[layout->element_count],"",spos_choices,buttonsize,fn_createVec3(0.4,0.3,0.6),cmap);
    layout->elements[layout->element_count].depressed = false;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(0.3,0.3,1);
    layout->elements[layout->element_count].dropdown = true;
    layout->elements[layout->element_count].text = "( )";
    layout->elements[layout->element_count].dynamic_text = th_strdup("( )");
    layout->elements[layout->element_count].dynamic_text[1] = '.';
    layout->elements[layout->element_count].dims = stringDims(layout->elements[layout->element_count].dynamic_text,layout->elements[layout->element_count].size,cmap);

    int resolution_select = layout->element_count;
    layout->element_count++;

    int resolution_choices_start = layout->element_count;

    float y_offset_reschoice = stringDims("1920x1080",buttonsize,cmap).y;
    float x_offset_reschoice = stringDims("1920x1080",buttonsize,cmap).x;

    #define TH_CREATE_RESOLUTION_ELEMENT(y_off, text, index,property_in)                             \
    do {                                                                          \
        op_pos = fn_createVec2(0,0);                                              \
        op_pos.y = layout->elements[resolution_button].position.y - (y_off*y_offset_reschoice*1.5);      \
        op_pos.x = layout->elements[resolution_button].position.x +               \
        layout->elements[resolution_button].dims.x +                   \
        screenSize.x * 0.03f;                                          \
        \
        th_createUIElementText(                                                   \
        &layout->elements[layout->element_count],                             \
        (text),                                                               \
        op_pos,                                                               \
        buttonsize,                                                                 \
        fn_createVec3(0.5f, 0.5, 0.5),                                            \
        cmap);                                                                \
        \
        layout->elements[layout->element_count].selectable = true;                \
        layout->elements[layout->element_count].color_highlight =                 \
        fn_createVec3(0.8, 0.8, 0.8);                                               \
        th_UIButtonData* data = malloc(sizeof(th_UIButtonData));\
        data->property = property_in;\
        data->idx = index;\
        data->to_clear_count = 1; \
        data->to_clear[0] = &layout->elements[resolution_select];\
        layout->elements[layout->element_count].callback_on_click = th_resolution_callback;\
        layout->elements[layout->element_count].callback_data = data;\
        layout->elements[layout->element_count].visible = false;                  \
        layout->elements[layout->element_count].dropdown_element = true;          \
        layout->elements[layout->element_count].z_order = -2;                     \
        layout->element_count++;                                                  \
    } while (0)

    //first resolution choice
    int num_resolution_modes = num_modestrings_local;
    char** modes = modestrings_local;

    op_pos = fn_createVec2(0,0);
    op_pos.y = layout->elements[resolution_button].position.y - y_offset_reschoice*1.5*num_resolution_modes - screenSize.y*0.01;
    op_pos.x = layout->elements[resolution_button].position.x + layout->elements[resolution_button].dims.x + screenSize.x*0.025;

    fn_vec2 dims_dropdown = fn_createVec2(x_offset_reschoice*1.5,y_offset_reschoice*1.5*num_resolution_modes + screenSize.y*0.01);

    th_createUIElementGradient(&layout->elements[layout->element_count],op_pos,dims_dropdown);
    layout->airtime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count ].alpha = 1.0;
    layout->elements[layout->element_count ].tint = fn_createVec3(0.1,0.01,0.6);
    layout->elements[layout->element_count].z_order = -2;
    layout->elements[layout->element_count].dropdown_element = true;
    layout->elements[layout->element_count].visible = false;
    layout->element_count++;



    th_UIProperty* res_property = &layout->properties->resolution;


    for (int k = 0 ; k < num_resolution_modes; k++)
    {
        TH_CREATE_RESOLUTION_ELEMENT((k + 1),modes[k],k,res_property);
    };
    //TH_CREATE_RESOLUTION_ELEMENT(2,modes[1],1,res_property);




    int resolution_choices_end = layout->element_count;

    for (int k = 0 ; k < num_resolution_modes; k++)
    {

        op_pos = fn_createVec2(0,0);
        op_pos.y = layout->elements[resolution_button].position.y ;
        op_pos.x = layout->elements[resolution_button].position.x + layout->elements[resolution_button].dims.x + screenSize.x*0.03;
        th_createUIElementText(&layout->elements[layout->element_count],modes[k],op_pos,buttonsize,fn_createVec3(1,1,1),cmap);
        layout->elements[layout->element_count].selectable = false;
        layout->elements[layout->element_count].color_highlight = fn_createVec3(1,1,1);
        layout->elements[layout->element_count].callback_on_click = NULL;
        layout->elements[layout->element_count].callback_data = NULL;
        layout->elements[layout->element_count].visible = false;
        layout->elements[layout->element_count].conditional_vis_index = k;
        layout->elements[layout->element_count].vis_condition = &layout->properties->resolution.value;
        layout->elements[layout->element_count].conditional_vis_index2 = 0;
        layout->elements[layout->element_count].vis_condition2 = &layout->sub_menu_idx;
        layout->element_count++;
    }



    th_UISelectionData* sel_data_resolutions = malloc(sizeof(th_UISelectionData));
    sel_data_resolutions->layout = layout;
    sel_data_resolutions->start = resolution_choices_start;
    sel_data_resolutions->end = resolution_choices_end;
    sel_data_resolutions->master_start = resolution_choices_start;
    sel_data_resolutions->master_end = resolution_choices_end;
    sel_data_resolutions->depressed_ptr = &layout->elements[resolution_select].depressed;

    layout->elements[resolution_select].callback_on_click = th_dropdown_callback;
    layout->elements[resolution_select].callback_data = sel_data_resolutions;

    int start_disable_section = layout->element_count;

    th_make_n_buttons(layout,fog_quality_button,layout->element_count,3,cmap,screenSize,tooltips,&layout->properties->fog_quality);
    int fog_quality_selector = layout->element_count;
    layout->element_count+=3;

    th_make_n_buttons(layout,reflection_scale_button,layout->element_count,3,cmap,screenSize,tooltips_scale,&layout->properties->reflection_scale);
    int reflection_scale_selector = layout->element_count;
    layout->element_count+=3;

    th_make_n_buttons(layout,reflection_quality_button,layout->element_count,3,cmap,screenSize,tooltips,&layout->properties->reflection_quality);
    int reflection_quality_selector = layout->element_count;
    layout->element_count+=3;

    th_make_n_buttons(layout,shadow_quality_button,layout->element_count,3,cmap,screenSize,tooltips,&layout->properties->shadow_quality);
    int shadow_quality_selector = layout->element_count;
    layout->element_count+=3;

    // th_make_n_buttons(layout,resolution_button,layout->element_count,8,cmap,screenSize,tooltips_resolution,&layout->properties->resolution);
    // int resolution_selector = layout->element_count;
    // layout->element_count+=8;



    th_make_n_buttons(layout,shadow_dyn_button,layout->element_count,2,cmap,screenSize,tooltips_shadows,&layout->properties->dynamic_shadows);
    int dynamic_shadow_selector = layout->element_count;
    layout->element_count+=2;

    th_make_n_buttons(layout,vsync_button,layout->element_count,2,cmap,screenSize,tooltips_shadows,&layout->properties->vsync);
    int vsync_selector = layout->element_count;
    layout->element_count+=2;

    th_make_n_buttons(layout,fullscreen_button,layout->element_count,3,cmap,screenSize,tooltips_fullscreen,&layout->properties->fullscreen);
    int fullscreen_selector = layout->element_count;
    layout->element_count+=3;

    th_make_n_buttons(layout,plight_button,layout->element_count,2,cmap,screenSize,tooltips_shadows,&layout->properties->particle_lighting);
    int plight_selector = layout->element_count;
    layout->element_count+=2;

    th_make_n_buttons_presets(layout,preset_button,layout->element_count,4,cmap,screenSize,tooltips_presets,layout->properties);
    layout->element_count+=4;

    int graphics_element_end = layout->element_count;

    //prevent accidental button presses when selecting from menu
    for (int k = start_disable_section; k < graphics_element_end;k++)
    {
        layout->elements[k].disable_button_when_true = &layout->elements[resolution_select].depressed;
    }


    /*
     * SOUND
     * SETTINGS
     */


    y_align_top = screenSize.y*0.5 + g_skip*2*0.5;

    int sound_element_start = layout->element_count;


    int sfx_button, music_button;

    CREATE_BUTTON("SFX Volume", sfx_button, 0);
    CREATE_BUTTON("Music Volume", music_button, 1);

    //th_UIlayout* layout,int base_elem,int arr_offset,th_Character* cmap,fn_vec2 screenSize
    th_make_alligned_slider(layout,sfx_button,music_button,layout->element_count,cmap,screenSize,&layout->properties->sfx_volume);
    int sfx_slider = layout->element_count;
    layout->elements[sfx_slider].writeback_data = writebacks.sfx_writeback_data;
    layout->elements[sfx_slider].f_writeback = writebacks.sfx_writeback;
    layout->element_count+=1;

    th_make_alligned_slider(layout,music_button,music_button,layout->element_count,cmap,screenSize,&layout->properties->music_volume);
    int music_slider = layout->element_count;
    layout->elements[music_slider].writeback_data = writebacks.music_writeback_data;
    layout->elements[music_slider].f_writeback = writebacks.music_writeback;
    layout->element_count+=1;


    int sound_element_end = layout->element_count;

    /*
     * GAME
     * SETTINGS
     */

    y_align_top = screenSize.y*0.5 + g_skip*11*0.5;

    int game_element_start = layout->element_count;

    int forward_button, backward_button, left_button, right_button;
    int crouch_button, jump_button, machinegun_button, shotgun_button;
    int hammer_button, mouse_sens_button, fov_button;
    int invert_button,gamma_button,exposure_button;



    CREATE_BUTTON("Forward", forward_button, 0);
    CREATE_BUTTON("Backward", backward_button, 1);
    CREATE_BUTTON("Left", left_button, 2);
    CREATE_BUTTON("Right", right_button, 3);
    CREATE_BUTTON("Crouch/Slide", crouch_button, 4);
    CREATE_BUTTON("Jump", jump_button, 5);
    CREATE_BUTTON("Machine gun", machinegun_button, 6);
    CREATE_BUTTON("Shotgun", shotgun_button, 7);
    CREATE_BUTTON("Hammer", hammer_button, 8);
    CREATE_BUTTON("Mouse Sensitivity", mouse_sens_button, 9);
    CREATE_BUTTON("FOV", fov_button, 10);
    CREATE_BUTTON("Invert Mouse Y", invert_button, 11);
    CREATE_BUTTON("Display Exposure", exposure_button, 12);
    CREATE_BUTTON("Display Gamma", gamma_button, 13);


    th_make_alligned_slider_minmax(layout,gamma_button,gamma_button,layout->element_count,cmap,screenSize,&layout->properties->gamma,1.8,2.6);
    layout->elements[layout->element_count].writeback_data = writebacks.gamma_writeback_data;
    layout->elements[layout->element_count].f_writeback = writebacks.gamma_writeback;
    layout->elements[layout->element_count].instant_writeback = true;

    layout->element_count+=1;

    th_make_alligned_slider_minmax(layout,exposure_button,exposure_button,layout->element_count,cmap,screenSize,&layout->properties->exposure,-1.0,1.0);
    layout->elements[layout->element_count].writeback_data = writebacks.exposure_writeback_data;
    layout->elements[layout->element_count].f_writeback = writebacks.exposure_writeback;
    layout->elements[layout->element_count].instant_writeback = true;

    layout->element_count+=1;



    th_make_alligned_slider_minmax(layout,mouse_sens_button,mouse_sens_button,layout->element_count,cmap,screenSize,&layout->properties->mouse_sensitivity,0.01,2.0);
    int msens_slider = layout->element_count;
    layout->elements[msens_slider].writeback_data = writebacks.sensitivity_writeback_data;
    layout->elements[msens_slider].f_writeback = writebacks.sensitivity_writeback;

    layout->element_count+=1;

    th_make_alligned_slider_integer(layout,fov_button,layout->element_count,cmap,screenSize,75,120,&layout->properties->fov);
    int fov_slider = layout->element_count;
    layout->elements[fov_slider].writeback_data = writebacks.fov_writeback_data;
    layout->elements[fov_slider].i_writeback = writebacks.fov_writeback;
    layout->elements[fov_slider].instant_writeback = true;
    layout->element_count+=1;


    #define MAKE_KEYBIND(name, button, keyprop) \
    th_make_alligned_keybind(layout, button, layout->element_count, cmap, screenSize, &layout->properties->keyprop); \
    int name##_keybind = layout->element_count; \
    layout->elements[name##_keybind].writeback_data = writebacks.name##_writeback_data; \
    layout->elements[name##_keybind].key_writeback = writebacks.name##_writeback; \
    layout->element_count += 1;

    MAKE_KEYBIND(forward, forward_button, forward_key);
    MAKE_KEYBIND(backward, backward_button, backward_key);
    MAKE_KEYBIND(left, left_button, left_key);
    MAKE_KEYBIND(right, right_button, right_key);
    MAKE_KEYBIND(crouch, crouch_button, crouch_key);
    MAKE_KEYBIND(machinegun, machinegun_button, machinegun_key);
    MAKE_KEYBIND(shotgun, shotgun_button, shotgun_key);
    MAKE_KEYBIND(hammer, hammer_button, hammer_key);
    MAKE_KEYBIND(jump, jump_button, jump_key);

    th_make_n_buttons(layout,invert_button,layout->element_count,2,cmap,screenSize,tooltips_shadows,&layout->properties->invert_mouse_y);
    int invert_selector = layout->element_count;
    layout->element_count+=2;


    int game_element_end = layout->element_count;
    int all_elements_end = layout->element_count;
    th_UISelectionData* sel_data_graphics = malloc(sizeof(th_UISelectionData));
    sel_data_graphics->layout = layout;
    sel_data_graphics->start = graphics_element_start;
    sel_data_graphics->end = graphics_element_end;
    sel_data_graphics->master_start = graphics_element_start;
    sel_data_graphics->master_end = all_elements_end;
    sel_data_graphics->sub_menu_ptr = &layout->sub_menu_idx;
    sel_data_graphics->sub_menu_set = 0;

    layout->elements[graphics_button].callback_on_click = th_selection_callback;
    layout->elements[graphics_button].callback_data = sel_data_graphics;


    th_UISelectionData* sel_data_sound = malloc(sizeof(th_UISelectionData));
    sel_data_sound->layout = layout;
    sel_data_sound->start = sound_element_start;
    sel_data_sound->end = sound_element_end;
    sel_data_sound->master_start = graphics_element_start;
    sel_data_sound->master_end = all_elements_end;
    sel_data_sound->sub_menu_ptr = &layout->sub_menu_idx;
    sel_data_sound->sub_menu_set = 1;

    layout->elements[sound_button].callback_on_click = th_selection_callback;
    layout->elements[sound_button].callback_data = sel_data_sound;

    th_UISelectionData* sel_data_game = malloc(sizeof(th_UISelectionData));
    sel_data_game->layout = layout;
    sel_data_game->start = game_element_start;
    sel_data_game->end = game_element_end;
    sel_data_game->master_start = graphics_element_start;
    sel_data_game->master_end = all_elements_end;
    sel_data_game->sub_menu_ptr = &layout->sub_menu_idx;
    sel_data_game->sub_menu_set = 2;

    layout->elements[game_button].callback_on_click = th_selection_callback;
    layout->elements[game_button].callback_data = sel_data_game;


    layout->highlighted_element = 1;

    layout->elements[graphics_button].callback_on_click(sel_data_graphics);
}

void th_prev_level_callback(void* data)
{
    th_UIlayout* layout = (th_UIlayout*) data;
    if (layout->selected_level - 1 >= 0)
    {
        layout->selected_level = layout->selected_level - 1;
    }
    else
    {
        layout->selected_level =  layout->num_levels - 1;
    }
}

void th_next_level_callback(void* data)
{
    th_UIlayout* layout = (th_UIlayout*) data;
    if (layout->selected_level + 1 < layout->num_levels)
    {
        layout->selected_level = layout->selected_level + 1;
    }
    else
    {
        layout->selected_level = 0;
    }
}


void th_createLevelSelectMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_LevelSelectCallbacks callbacks,th_LevelManifest levels,r_Shader* deathtext)
{
    layout->victory_state = malloc(sizeof(th_LevelVictoryState)*MAX_LEVEL_COUNT);
    for (int i = 0 ; i < MAX_LEVEL_COUNT ; i++)
    {
        layout->victory_state[i] = (th_LevelVictoryState){0};
    }
    layout->grabbed_cursor = false;
    layout->grabbed_keybind = false;
    layout->properties = NULL;
    layout->selected_level = 0;
    layout->num_levels = levels.count;


    const float oneminusphi = 0.05;
    const int max_ui_elements = 512;
    layout->elements = malloc(sizeof(th_UIElement)*max_ui_elements);
    layout->element_count = 0;

    th_createUIElementGradient(&layout->elements[layout->element_count],fn_createVec2(0,0),screenSize);
    layout->airtime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count ].alpha = 1.0;
    layout->elements[layout->element_count ].tint = fn_createVec3s(0.0);
    layout->element_count++;

    float w_op = stringDims("Level Select",1,cmap).x;

    fn_vec2 op_pos = fn_multVec2(fn_createVec2(0.5,0.9),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;

    th_createUIElementText(&layout->elements[layout->element_count],"Level Select",op_pos,1,fn_createVec3(1,1,1),cmap);
    float w_tex = textElementDims(&layout->elements[layout->element_count]).x;
    int options_button = layout->element_count;
    layout->element_count++;

    fn_vec2 spos = fn_multVec2(fn_createVec2(0.05,0.05),screenSize);
    th_createUIElementText(&layout->elements[layout->element_count],"Back",spos,0.45,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    layout->elements[layout->element_count].callback_on_click = callbacks.back_callback;
    layout->elements[layout->element_count].callback_data = callbacks.back_callback_data;
    int back_button = layout->element_count;
    layout->element_count++;


    GLuint* skull_texs = malloc(sizeof(GLuint)*3);
    skull_texs[0] = fn_loadTexture("th1/skull_icons/bronzeskull.png");
    skull_texs[1] = fn_loadTexture("th1/skull_icons/silverskull.png");
    skull_texs[2] = fn_loadTexture("th1/skull_icons/goldskull.png");


    GLuint* level_thumbnails = malloc(sizeof(GLuint)*levels.count);
    for (int i = 0 ; i < levels.count;i++)
    {
        level_thumbnails[i] = fn_loadTexture(levels.levelfiles[i].thumbnail_path);
    }

    for (int i = 0 ; i < levels.count;i++)
    {
        float image_scale_factor = 2.0*((th_getAspectScale()));
        fn_vec2 im_pos = fn_multVec2(fn_createVec2(0.5,0.65),screenSize);
        im_pos.x = im_pos.x - 320*0.5*image_scale_factor;
        im_pos.y = im_pos.y - 180*0.5*image_scale_factor;
        th_createUIElementImageTexture(&layout->elements[layout->element_count],im_pos,fn_createVec2(320*image_scale_factor,180*image_scale_factor),level_thumbnails[i]);
        int image_elem = layout->element_count;
        layout->elements[image_elem].conditional_vis_index = i;
        layout->elements[image_elem].vis_condition = &layout->selected_level;
        layout->element_count++;

        //next vid
        image_scale_factor = 1.25*((th_getAspectScale()));
        im_pos = fn_multVec2(fn_createVec2(0.8,0.4),screenSize);
        im_pos.x = im_pos.x - 320*0.5*image_scale_factor;
        im_pos.y = im_pos.y - 180*0.5*image_scale_factor;
        //int i_next = (i + 1) % levels.count;
        int i_next = ((i - 1) % levels.count + levels.count) % levels.count;
        th_createUIElementImageTexture(&layout->elements[layout->element_count],im_pos,fn_createVec2(320*image_scale_factor,180*image_scale_factor),level_thumbnails[i_next]);
        image_elem = layout->element_count;
        layout->elements[image_elem].conditional_vis_index = i;
        layout->elements[image_elem].vis_condition = &layout->selected_level;
        layout->elements[image_elem].alpha = 0.75;
        layout->element_count++;


        //next vid 2
        image_scale_factor = 0.65*((th_getAspectScale()));
        im_pos = fn_multVec2(fn_createVec2(0.9,0.2),screenSize);
        im_pos.x = im_pos.x - 320*0.5*image_scale_factor;
        im_pos.y = im_pos.y - 180*0.5*image_scale_factor;
        //int i_next = (i + 1) % levels.count;
        int i_next2 = ((i - 2) % levels.count + levels.count) % levels.count;
        th_createUIElementImageTexture(&layout->elements[layout->element_count],im_pos,fn_createVec2(320*image_scale_factor,180*image_scale_factor),level_thumbnails[i_next2]);
        image_elem = layout->element_count;
        layout->elements[image_elem].conditional_vis_index = i;
        layout->elements[image_elem].vis_condition = &layout->selected_level;
        layout->elements[image_elem].alpha = 0.75;
        layout->element_count++;

        //prev vid
        image_scale_factor = 1.25*((th_getAspectScale()));
        im_pos = fn_multVec2(fn_createVec2(0.2,0.4),screenSize);
        im_pos.x = im_pos.x - 320*0.5*image_scale_factor;
        im_pos.y = im_pos.y - 180*0.5*image_scale_factor;
        int i_prev = (i + 1) % levels.count;
        //int i_next = ((i - 1) % levels.count + levels.count) % levels.count;
        th_createUIElementImageTexture(&layout->elements[layout->element_count],im_pos,fn_createVec2(320*image_scale_factor,180*image_scale_factor),level_thumbnails[i_prev]);
        image_elem = layout->element_count;
        layout->elements[image_elem].conditional_vis_index = i;
        layout->elements[image_elem].vis_condition = &layout->selected_level;
        layout->elements[image_elem].alpha = 0.75;
        layout->element_count++;

        //prev vid 2
        image_scale_factor = 0.65*((th_getAspectScale()));
        im_pos = fn_multVec2(fn_createVec2(0.1,0.2),screenSize);
        im_pos.x = im_pos.x - 320*0.5*image_scale_factor;
        im_pos.y = im_pos.y - 180*0.5*image_scale_factor;
        int i_prev2 = (i + 2) % levels.count;
        //int i_next = ((i - 1) % levels.count + levels.count) % levels.count;
        th_createUIElementImageTexture(&layout->elements[layout->element_count],im_pos,fn_createVec2(320*image_scale_factor,180*image_scale_factor),level_thumbnails[i_prev2]);
        image_elem = layout->element_count;
        layout->elements[image_elem].conditional_vis_index = i;
        layout->elements[image_elem].vis_condition = &layout->selected_level;
        layout->elements[image_elem].alpha = 0.75;
        layout->element_count++;


        w_op = stringDims(levels.levelfiles[i].display_name,0.6,cmap).x;
        op_pos = fn_multVec2(fn_createVec2(0.5,0.31 + 0.15),screenSize);
        op_pos.x = op_pos.x - w_op*0.5;
        th_createUIElementText(&layout->elements[layout->element_count],levels.levelfiles[i].display_name,op_pos,0.6,fn_createVec3(1,0.8,0),cmap);
        int label_elem = layout->element_count;
        layout->elements[label_elem].conditional_vis_index = i;
        layout->elements[label_elem].vis_condition = &layout->selected_level;
        layout->element_count++;

        w_op = stringDims(levels.levelfiles[i].difficulty_name,0.33,cmap).x;
        op_pos = fn_multVec2(fn_createVec2(0.5,0.31 + 0.112),screenSize);
        op_pos.x = op_pos.x - w_op*0.5;
        fn_vec3 difficulty_color = fn_createVec3(1,0.8,0);
        if (strcmp(levels.levelfiles[i].difficulty_name,"Easy") == 0)
        {
            difficulty_color = fn_createVec3(0,0.8,0);
        }
        else if (strcmp(levels.levelfiles[i].difficulty_name,"Medium") == 0)
        {
            difficulty_color = fn_createVec3(1.0,0.8,0);
        }
        else if (strcmp(levels.levelfiles[i].difficulty_name,"Hard") == 0)
        {
            difficulty_color = fn_createVec3(0.9,0.1,0);
        }
        else if (strcmp(levels.levelfiles[i].difficulty_name,"Nightmare") == 0)
        {
            difficulty_color = fn_createVec3(0.01,0.01,0.01);
        }
        else if (strcmp(levels.levelfiles[i].difficulty_name,"Horse-Shit") == 0)
        {
            difficulty_color = fn_createVec3(0.76,0.352,0.0);
        }
        else if (strcmp(levels.levelfiles[i].difficulty_name,"Grandmaster") == 0)
        {
            difficulty_color = fn_createVec3(0.76,0,0.352);
        }


        th_createUIElementText(&layout->elements[layout->element_count],levels.levelfiles[i].difficulty_name,op_pos,0.33,difficulty_color,cmap);
        label_elem = layout->element_count;
        layout->elements[label_elem].conditional_vis_index = i;
        layout->elements[label_elem].vis_condition = &layout->selected_level;
        layout->elements[label_elem].custom_shader = deathtext;
        layout->elements[label_elem].magnitude_wavy = 12.0;
        layout->element_count++;





    }

    //TODO medals
    //TODO victory tracking w/ file
    //TODO victory screen

    float srcldwon = 0.2;
    float y_off_3 = stringDims("Damage Taken",0.4,cmap).y;

    float image_scale_factor = 2.0*((th_getAspectScale()));
    fn_vec2 im_pos = fn_multVec2(fn_createVec2(0.425,0.65),screenSize);
    im_pos.y = (0.51 - srcldwon)*screenSize.y - y_off_3*1.5;
    im_pos.x = im_pos.x - 320*0.5*image_scale_factor*1.25;
    im_pos.y = im_pos.y - 180*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(320*image_scale_factor*1.25,180*image_scale_factor),"th1/alpha_gradient.png");
    //int image_elem = layout->element_count;
    // layout->elements[image_elem].conditional_vis_index = i;
    // layout->elements[image_elem].vis_condition = &layout->selected_level;
    layout->element_count++;




    w_op = stringDims("Clear Time",0.4,cmap).x;
    op_pos = fn_multVec2(fn_createVec2(0.4,0.51 - srcldwon),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - y_off_3*2.0;
    float jstify_x = op_pos.x;
    th_createUIElementText(&layout->elements[layout->element_count],"Clear Time",op_pos,0.4,fn_createVec3s(0.9),cmap);
    //int label_elem = layout->element_count;
    layout->element_count++;

    w_op = stringDims("Airtime",0.4,cmap).x;
    op_pos = fn_multVec2(fn_createVec2(0.5,0.51 - srcldwon),screenSize);
    op_pos.x = jstify_x;//op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - y_off_3*4.0;
    th_createUIElementText(&layout->elements[layout->element_count],"Airtime",op_pos,0.4,fn_createVec3s(0.9),cmap);
    //label_elem = layout->element_count;
    layout->element_count++;

    w_op = stringDims("Health Kept",0.4,cmap).x;
    op_pos = fn_multVec2(fn_createVec2(0.5,0.51 - srcldwon),screenSize);
    op_pos.x = jstify_x;//op_pos.x - w_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Health Kept",op_pos,0.4,fn_createVec3s(0.9),cmap);
    //label_elem = layout->element_count;
    layout->element_count++;


    for (int i = 0 ; i < levels.count;i++)
    {
        w_op = stringDims("UNCLEARED",0.7,cmap).x;
        op_pos = fn_multVec2(fn_createVec2(0.5,0.31 + 0.07),screenSize);
        op_pos.x = op_pos.x - w_op*0.5;
        op_pos.y = op_pos.y - stringDims("UNCLEARED",0.7,cmap).y*0.5;
        float jstify_x = op_pos.x;
        th_createUIElementText(&layout->elements[layout->element_count],"UNCLEARED",op_pos,0.7,fn_createVec3(0.8,0.0,0.0),cmap);
        int label_elem = layout->element_count;
        layout->elements[label_elem].conditional_vis_index = 0;
        layout->elements[label_elem].vis_condition = &layout->victory_state[i].level_speed;
        layout->elements[label_elem].conditional_vis_index2 = i;
        layout->elements[label_elem].vis_condition2 = &layout->selected_level;
        layout->element_count++;

        float offs_y[3] = {y_off_3*2.0,0,-y_off_3*2.0};
        float offs_x[3] = {-screenSize.x*0.065,-screenSize.x*0.065,-screenSize.x*0.065};

        int* addrs[3] = {&layout->victory_state[i].level_damagetaken,&layout->victory_state[i].level_speed,&layout->victory_state[i].level_airtime};
        //damage taken
        //clear time
        //air time

        for (int j = 0 ; j < 3; j++)
        {
            op_pos = fn_multVec2(fn_createVec2(0.49,0.51 - srcldwon),screenSize);
            op_pos.y = op_pos.y - y_off_3*2.0 - 32*image_scale_factor*0.4 + offs_y[j];
            op_pos.x = op_pos.x - offs_x[j];
            th_createUIElementImageTexture(&layout->elements[layout->element_count],op_pos,fn_createVec2(32*image_scale_factor,32*image_scale_factor),skull_texs[0]);
            layout->elements[layout->element_count].conditional_vis_index = 2;
            layout->elements[layout->element_count].vis_condition = addrs[j];
            layout->elements[layout->element_count].conditional_vis_index2 = i;
            layout->elements[layout->element_count].vis_condition2 = &layout->selected_level;
            layout->elements[layout->element_count].nofade = true;
            layout->element_count++;

            op_pos = fn_multVec2(fn_createVec2(0.49,0.51 - srcldwon),screenSize);
            op_pos.y = op_pos.y - y_off_3*2.0 - 32*image_scale_factor*0.4 + offs_y[j];
            op_pos.x = op_pos.x + 32*image_scale_factor*1.0 - offs_x[j];
            th_createUIElementImageTexture(&layout->elements[layout->element_count],op_pos,fn_createVec2(32*image_scale_factor,32*image_scale_factor),skull_texs[1]);
            layout->elements[layout->element_count].conditional_vis_index = 3;
            layout->elements[layout->element_count].vis_condition = addrs[j];
            layout->elements[layout->element_count].conditional_vis_index2 = i;
            layout->elements[layout->element_count].vis_condition2 = &layout->selected_level;
            layout->elements[layout->element_count].nofade = true;
            layout->element_count++;

            op_pos = fn_multVec2(fn_createVec2(0.49,0.51 - srcldwon),screenSize);
            op_pos.y = op_pos.y - y_off_3*2.0 - 32*image_scale_factor*0.4 + offs_y[j];
            op_pos.x = op_pos.x + 32*image_scale_factor*2.0 - offs_x[j];
            th_createUIElementImageTexture(&layout->elements[layout->element_count],op_pos,fn_createVec2(32*image_scale_factor,32*image_scale_factor),skull_texs[2]);
            layout->elements[layout->element_count].conditional_vis_index = 4;
            layout->elements[layout->element_count].vis_condition = addrs[j];
            layout->elements[layout->element_count].conditional_vis_index2 = i;
            layout->elements[layout->element_count].vis_condition2 = &layout->selected_level;
            layout->elements[layout->element_count].nofade = true;
            layout->element_count++;
        }


        w_op = stringDims("FLAWLESS",0.9,cmap).x;
        op_pos = fn_multVec2(fn_createVec2(0.48,0.31 + 0.07),screenSize);
        op_pos.x = op_pos.x - w_op*0.5;
        op_pos.y = op_pos.y - stringDims("FLAWLESS",0.9,cmap).y*0.5;
        jstify_x = op_pos.x;
        th_createUIElementText(&layout->elements[layout->element_count],"FLAWLESS",op_pos,0.9,fn_createVec3(0.5,0.76,0.76),cmap);
        label_elem = layout->element_count;
        layout->elements[label_elem].conditional_vis_index = 1;
        layout->elements[label_elem].vis_condition = &layout->victory_state[i].flawless;
        layout->elements[label_elem].conditional_vis_index2 = i;
        layout->elements[label_elem].vis_condition2 = &layout->selected_level;
        layout->elements[label_elem].custom_shader = deathtext;
        layout->elements[label_elem].magnitude_wavy = 12.0;
        layout->element_count++;




    }







    w_op = stringDims("Start",0.681,cmap).x;
    op_pos = fn_multVec2(fn_createVec2(0.5,0.1),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Start",op_pos,0.681,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    layout->elements[layout->element_count].callback_on_click = callbacks.start_callback;
    layout->elements[layout->element_count].callback_data = callbacks.start_callback_data;
    int start_button = layout->element_count;
    layout->element_count++;

    float h_op = stringDims("Prev",0.68,cmap).y;
    op_pos = fn_multVec2(fn_createVec2(0.03,0.65),screenSize);
    op_pos.y = op_pos.y - h_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Prev",op_pos,0.68,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    layout->elements[layout->element_count].callback_on_click = th_prev_level_callback;
    layout->elements[layout->element_count].callback_data = layout;
    int back_paddle = layout->element_count;
    layout->element_count++;

    h_op = stringDims("Next",0.68,cmap).y;
    op_pos = fn_multVec2(fn_createVec2(0.97,0.65),screenSize);
    op_pos.y = op_pos.y - h_op*0.5;
    op_pos.x = op_pos.x - stringDims("Next",0.68,cmap).x;
    th_createUIElementText(&layout->elements[layout->element_count],"Next",op_pos,0.68,fn_createVec3(0.68,0,0),cmap);
    layout->elements[layout->element_count].selectable = true;
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0);
    layout->elements[layout->element_count].callback_on_click = th_next_level_callback;
    layout->elements[layout->element_count].callback_data = layout;
    int forward_paddle = layout->element_count;
    layout->element_count++;


    layout->highlighted_element = 1;
    free(level_thumbnails);
    free(skull_texs);

}

void th_createHUD(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize)
{
    layout->grabbed_cursor = false;
    layout->grabbed_keybind = false;
    layout->properties = NULL;
    layout->selected_level = 0;
    layout->hammer_level = 0;
    layout->machinegun_level = 0;
    layout->shotgun_level = 0;


    const float icosize = 32;
    const float oneminusphi = 0.05;
    const int max_ui_elements = 512;
    layout->elements = malloc(sizeof(th_UIElement)*max_ui_elements);
    layout->element_count = 0;
    fn_vec2 scr_pos = fn_createVec2(0.51,0.15);

    float image_scale_factor = 2.0*((th_getAspectScale()));
    fn_vec2 im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/hammer_icon.png");
    layout->elements[layout->element_count].alpha = 0.2;
    layout->elements[layout->element_count].conditional_vis_index = 1;
    layout->elements[layout->element_count].vis_condition = &layout->hammer_level;
    layout->highlighted_element = layout->element_count;
    layout->element_count++;

    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/hammer2_icon.png");
    layout->elements[layout->element_count].alpha = 0.2;
    layout->elements[layout->element_count].conditional_vis_index = 2;
    layout->elements[layout->element_count].vis_condition = &layout->hammer_level;
    layout->element_count++;

    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/hammer3_icon.png");
    layout->elements[layout->element_count].alpha = 0.2;
    layout->elements[layout->element_count].conditional_vis_index = 3;
    layout->elements[layout->element_count].vis_condition = &layout->hammer_level;
    layout->element_count++;


    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor + icosize*1.2*image_scale_factor;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/machinegun_icon.png");
    layout->elements[layout->element_count].alpha = 1.0;
    layout->elements[layout->element_count].conditional_vis_index = 1;
    layout->elements[layout->element_count].vis_condition = &layout->machinegun_level;
    layout->element_count++;

    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor + icosize*1.2*image_scale_factor;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/machinegun2_icon.png");
    layout->elements[layout->element_count].alpha = 1.0;
    layout->elements[layout->element_count].conditional_vis_index = 2;
    layout->elements[layout->element_count].vis_condition = &layout->machinegun_level;
    layout->element_count++;

    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor + icosize*1.2*image_scale_factor;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/machinegun3_icon.png");
    layout->elements[layout->element_count].alpha = 1.0;
    layout->elements[layout->element_count].conditional_vis_index = 3;
    layout->elements[layout->element_count].vis_condition = &layout->machinegun_level;
    layout->element_count++;

    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor + icosize*1.2*image_scale_factor*2.0;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/shotgun_icon.png");
    layout->elements[layout->element_count].alpha = 0.2;
    layout->elements[layout->element_count].conditional_vis_index = 1;
    layout->elements[layout->element_count].vis_condition = &layout->shotgun_level;
    layout->element_count++;

    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor + icosize*1.2*image_scale_factor*2.0;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/shotgun2_icon.png");
    layout->elements[layout->element_count].alpha = 0.2;
    layout->elements[layout->element_count].conditional_vis_index = 2;
    layout->elements[layout->element_count].vis_condition = &layout->shotgun_level;
    layout->element_count++;

    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor + icosize*1.2*image_scale_factor*2.0;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(icosize*image_scale_factor,icosize*image_scale_factor),"th1/weapon_icons/shotgun3_icon.png");
    layout->elements[layout->element_count].alpha = 0.2;
    layout->elements[layout->element_count].conditional_vis_index = 3;
    layout->elements[layout->element_count].vis_condition = &layout->shotgun_level;
    layout->element_count++;


    float w_op = stringDims("Incoming Spawners",0.333,cmap).x;
    float h_op = stringDims("Incoming Spawners",0.333,cmap).y;
    fn_vec2 op_pos = fn_multVec2(fn_createVec2(0.5,0.98),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - h_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Incoming Spawners",op_pos,0.333,fn_createVec3s(0.9),cmap);
    layout->elements[layout->element_count].visible = true;
    //label_elem = layout->element_count;
    layout->status_offset = layout->element_count;
    layout->element_count++;

    w_op = stringDims("Spawners Remaining",0.333,cmap).x;
    h_op = stringDims("Spawners Remaining",0.333,cmap).y;
    op_pos = fn_multVec2(fn_createVec2(0.5,0.98),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - h_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Spawners Remaining",op_pos,0.333,fn_createVec3(0.9,0.2,0),cmap);
    layout->elements[layout->element_count].visible = false;
    //label_elem = layout->element_count;
    layout->element_count++;

    w_op = stringDims("Enemies Remaining",0.333,cmap).x;
    h_op = stringDims("Enemies Remaining",0.333,cmap).y;
    op_pos = fn_multVec2(fn_createVec2(0.5,0.98),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - h_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Enemies Remaining",op_pos,0.333,fn_createVec3(0.2,0.7,0.2),cmap);
    layout->elements[layout->element_count].visible = false;
    //label_elem = layout->element_count;
    layout->element_count++;


    //fn_createVec3(0.9,0.2,0.0); erosion tint
    //fn_createVec3(0.2,0.7,0.2); catharsis tint
    //fn_createVec3s(0.1); intrusion tint

    w_op = screenSize.x*0.333;
    h_op = screenSize.y*0.012;
    op_pos = fn_multVec2(fn_createVec2(0.5,0.95),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - h_op*0.5;
    th_createUIElementProgress(&layout->elements[layout->element_count],op_pos,fn_createVec2(w_op,h_op));
    layout->airtime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count].alpha = 0.0;
    layout->elements[layout->element_count].tint = fn_createVec3s(0.1);
    layout->element_count++;

    w_op = stringDims("Tutorial Progress",0.333,cmap).x;
    h_op = stringDims("Tutorial Progress",0.333,cmap).y;
    op_pos = fn_multVec2(fn_createVec2(0.5,0.98),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - h_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Tutorial Progress",op_pos,0.333,fn_lerpVec3(fn_createVec3(1,0.64,0),fn_createVec3(1,1,1),0.2),cmap);
    layout->elements[layout->element_count].visible = false;
    //label_elem = layout->element_count;
    layout->element_count++;




    w_op = stringDims("HEALTH",0.4,cmap).x;

    float fraction_charsize_x = 128.0/1920.0;
    fn_vec2 health_label = fn_createVec2(0.18*screenSize.x - w_op*0.5,fraction_charsize_x*0.25 + screenSize.y*0.1);
    //fn_lerpVec3(fn_createVec3(1,0.64,0),fn_createVec3(1,1,1),0.2)
    th_createUIElementText(&layout->elements[layout->element_count],"HEALTH",health_label,0.4,fn_createVec3(1,1,1),cmap);
    //label_elem = layout->element_count;
    layout->element_count++;

    fn_vec2 gemiconscale = fn_createVec2(378,276);
    gemiconscale = fn_multVec2s(gemiconscale,0.25*1.3*((th_getAspectScale())));

    fraction_charsize_x = 128.0/1920.0;
    fraction_charsize_x = fraction_charsize_x*screenSize.x;

    fn_vec2 gem_pos = fn_createVec2(fraction_charsize_x*10.3 - gemiconscale.x  , screenSize.y*0.05 - gemiconscale.y*0.5 );

    th_createUIElementImage(&layout->elements[layout->element_count],gem_pos,gemiconscale,"th1/weapon_icons/gemicon.png");
    layout->elements[layout->element_count].nofade = true;
    layout->element_count++;


    im_pos = fn_multVec2(scr_pos,screenSize);
    im_pos.x = im_pos.x - icosize*0.5*image_scale_factor + icosize*1.2*image_scale_factor*3.0;
    im_pos.y = im_pos.y - icosize*0.5*image_scale_factor;
    th_createUIElementText(&layout->elements[layout->element_count],"DASH",im_pos,0.333,fn_createVec3(0.8,0.6,0.8),cmap);
    layout->elements[layout->element_count].color_highlight = fn_createVec3(0.1,0.1,0.1);
    layout->elements[layout->element_count].highlighted = true;
    layout->slide_element_indicator = &layout->elements[layout->element_count];
    // layout->elements[layout->element_count].alpha = 0.2;
    // layout->elements[layout->element_count].conditional_vis_index = 3;
    // layout->elements[layout->element_count].vis_condition = &layout->shotgun_level;
    layout->element_count++;
}

void th_createPauseMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_PauseMenuCallbacks callbacks)
{
    layout->grabbed_cursor = false;
    layout->grabbed_keybind = false;
    layout->properties = NULL;
    layout->selected_level = 0;



    const float oneminusphi = 0.05;
    const int max_ui_elements = 512;
    layout->elements = malloc(sizeof(th_UIElement)*max_ui_elements);
    layout->element_count = 0;

    th_createUIElementGradient(&layout->elements[layout->element_count],fn_createVec2(0,0),screenSize);
    layout->airtime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count ].alpha = 1.0;
    layout->elements[layout->element_count ].tint = fn_createVec3s(0.0);
    layout->element_count++;

    float w_op = stringDims("Pause",1,cmap).x;

    fn_vec2 op_pos = fn_multVec2(fn_createVec2(0.5,0.9),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;

    th_createUIElementText(&layout->elements[layout->element_count],"Pause",op_pos,1,fn_createVec3(1,1,1),cmap);
    int pause_button = layout->element_count;
    layout->element_count++;


    float g_skip = stringDims("Resume",0.68,cmap).y*2;

    #define MAKE_BUTTON_JUSTIFIED(VAR, LABEL, OFFSET, CALLBACK, CALLBACK_DATA) do { \
    float w_op = stringDims(LABEL,0.68,cmap).x; \
    fn_vec2 op_pos = fn_multVec2(fn_createVec2(0.5,0.31),screenSize); \
    op_pos.x = op_pos.x - w_op*0.5; \
    op_pos.y += g_skip*(OFFSET); \
    th_createUIElementText(&layout->elements[layout->element_count],LABEL,op_pos,0.68,fn_createVec3(0.68,0,0),cmap); \
    int VAR = layout->element_count; \
    layout->elements[layout->element_count].selectable = true; \
    layout->elements[layout->element_count].color_highlight = fn_createVec3(1,0,0); \
    layout->elements[layout->element_count].callback_on_click = (CALLBACK); \
    layout->elements[layout->element_count].callback_data = (CALLBACK_DATA); \
    layout->element_count++; \
    } while(0)

    MAKE_BUTTON_JUSTIFIED(resume_button, "Resume", 3,
                callbacks.resume_callback,
                callbacks.resume_callback_data);

    MAKE_BUTTON_JUSTIFIED(restart_button, "Restart", 2,
                callbacks.restart_callback,
                callbacks.restart_callback_data);

    MAKE_BUTTON_JUSTIFIED(title_button, "Title Screen", 1,
                callbacks.titlescreen_callback,
                callbacks.titlescreen_callback_data);

    MAKE_BUTTON_JUSTIFIED(options_button, "Options", 0,
                callbacks.options_callback,
                callbacks.options_callback_data);


    layout->highlighted_element = 3;
}

void th_createDeathMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_DeathMenuCallbacks callbacks,r_Shader* deathtext)
{
    layout->grabbed_cursor = false;
    layout->grabbed_keybind = false;
    layout->properties = NULL;
    layout->selected_level = 0;



    const float oneminusphi = 0.05;
    const int max_ui_elements = 512;
    layout->elements = malloc(sizeof(th_UIElement)*max_ui_elements);
    layout->element_count = 0;

    float w_op = stringDims("SNUFFED OUT",1.5,cmap).x;

    fn_vec2 op_pos = fn_multVec2(fn_createVec2(0.5,0.75),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;

    th_createUIElementText(&layout->elements[layout->element_count],"SNUFFED OUT",op_pos,1.5,fn_createVec3(0,0.8,1),cmap);
    int pause_button = layout->element_count;
    layout->elements[pause_button].custom_shader = deathtext;
    layout->element_count++;


    float g_skip = stringDims("Resume",0.68,cmap).y*2;



    MAKE_BUTTON_JUSTIFIED(restart_button, "Restart", 2,
                          callbacks.restart_callback,
                          callbacks.restart_callback_data);

    MAKE_BUTTON_JUSTIFIED(title_button, "Title Screen", 1,
                          callbacks.titlescreen_callback,
                          callbacks.titlescreen_callback_data);




    layout->highlighted_element = 1;
}

void th_createVictoryMenu(th_UIlayout* layout,th_Character* cmap,fn_vec2 screenSize,th_DeathMenuCallbacks callbacks,r_Shader* deathtext,th_LevelVictoryState* victory_state)
{
    layout->victory_state = victory_state;
    layout->grabbed_cursor = false;
    layout->grabbed_keybind = false;
    layout->properties = NULL;
    layout->selected_level = 0;
    layout->flawless_enabled = 0;






    const float oneminusphi = 0.05;
    const int max_ui_elements = 512;
    layout->elements = malloc(sizeof(th_UIElement)*max_ui_elements);
    layout->element_count = 0;


    float srcldwon = 0.15;
    float y_off_3 = stringDims("Damage Taken",0.4,cmap).y*2.2;

    float image_scale_factor = 2.0*((th_getAspectScale()));
    fn_vec2 im_pos = fn_multVec2(fn_createVec2(0.5,0.45),screenSize);
    im_pos.y = (0.5 - srcldwon)*screenSize.y - y_off_3*1.5;
    im_pos.x = im_pos.x - 640*0.5*image_scale_factor*1.25;
    im_pos.y = im_pos.y - 200*0.5*image_scale_factor;
    th_createUIElementImage(&layout->elements[layout->element_count],im_pos,fn_createVec2(640*image_scale_factor*1.25,200*image_scale_factor),"th1/alpha_gradient.png");
    layout->elements[layout->element_count].nofade = true;
    layout->element_count++;

    float w_op = stringDims("LEVEL CLEARED",1.2,cmap).x;

    fn_vec2 op_pos = fn_multVec2(fn_createVec2(0.5,0.75),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;

    th_createUIElementText(&layout->elements[layout->element_count],"LEVEL CLEARED",op_pos,1.2,fn_createVec3(1,0.8,0),cmap);
    int pause_button = layout->element_count;
    layout->elements[pause_button].custom_shader = deathtext;
    layout->elements[layout->element_count].conditional_vis_index = 0;
    layout->elements[layout->element_count].vis_condition = &layout->flawless_enabled;
    layout->element_count++;





    float g_skip = stringDims("Resume",0.68,cmap).y*2;

    //TODO slider x 3 showing progress to 3 skulls

    // MAKE_BUTTON_JUSTIFIED(restart_button, "Restart", 2,
    //                       callbacks.restart_callback,
    //                       callbacks.restart_callback_data);

    MAKE_BUTTON_JUSTIFIED(title_button, "Title Screen", 3,
                          callbacks.titlescreen_callback,
                          callbacks.titlescreen_callback_data);



    //float image_scale_factor = 2.0*((th_getAspectScale()));


     w_op = stringDims("Clear Time",0.4,cmap).x;
     op_pos = fn_multVec2(fn_createVec2(0.25,0.51 - srcldwon),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - y_off_3*2.0;
    float jstify_x = op_pos.x;
    th_createUIElementText(&layout->elements[layout->element_count],"Clear Time",op_pos,0.4,fn_createVec3s(0.9),cmap);
    //int label_elem = layout->element_count;
    layout->element_count++;

    w_op = stringDims("Airtime",0.4,cmap).x;
    op_pos = fn_multVec2(fn_createVec2(0.25,0.51 - srcldwon),screenSize);
    op_pos.x = jstify_x;//op_pos.x - w_op*0.5;
    op_pos.y = op_pos.y - y_off_3*4.0;
    th_createUIElementText(&layout->elements[layout->element_count],"Airtime",op_pos,0.4,fn_createVec3s(0.9),cmap);
    //label_elem = layout->element_count;
    layout->element_count++;

    w_op = stringDims("Health Kept",0.4,cmap).x;
    op_pos = fn_multVec2(fn_createVec2(0.25,0.51 - srcldwon),screenSize);
    op_pos.x = jstify_x;//op_pos.x - w_op*0.5;
    th_createUIElementText(&layout->elements[layout->element_count],"Health Kept",op_pos,0.4,fn_createVec3s(0.9),cmap);
    //label_elem = layout->element_count;
    layout->element_count++;

    w_op = stringDims("Health Kept",0.4,cmap).x;
    op_pos = fn_multVec2(fn_createVec2(0.25,0.51 - srcldwon),screenSize);
    op_pos.x = jstify_x + w_op + screenSize.x*0.0425*0.5;
    op_pos.y = op_pos.y - y_off_3*0.0;
    th_createUIElementProgress(&layout->elements[layout->element_count],op_pos,fn_createVec2(screenSize.x*0.4,screenSize.y*0.02));
    layout->damagetaken_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count].alpha = 0.0;
    layout->elements[layout->element_count].tint = fn_createVec3(0.9,0.0,0.2);
    layout->element_count++;

    // th_UIElement* airtime_slider;
    // th_UIElement* cleartime_slider;
    // th_UIElement* damagetaken_slider;

    op_pos = fn_multVec2(fn_createVec2(0.25,0.51 - srcldwon),screenSize);
    op_pos.x = jstify_x + w_op + screenSize.x*0.0425*0.5;
    op_pos.y = op_pos.y - y_off_3*2.0;
    th_createUIElementProgress(&layout->elements[layout->element_count],op_pos,fn_createVec2(screenSize.x*0.4,screenSize.y*0.02));
    layout->cleartime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count].alpha = 0.0;
    layout->elements[layout->element_count].tint = fn_createVec3(0.9,0.0,0.2);
    layout->element_count++;

    //0.0 no medal 0.25 bronze 0.58 silver 1.0 gold

    op_pos = fn_multVec2(fn_createVec2(0.25,0.51 - srcldwon),screenSize);
    op_pos.x = jstify_x + w_op + screenSize.x*0.0425*0.5;
    op_pos.y = op_pos.y - y_off_3*4.0;
    th_createUIElementProgress(&layout->elements[layout->element_count],op_pos,fn_createVec2(screenSize.x*0.4,screenSize.y*0.02));
    layout->airtime_slider = &layout->elements[layout->element_count];
    layout->elements[layout->element_count].alpha = 0.0;
    layout->elements[layout->element_count].tint = fn_createVec3(0.9,0.0,0.2);
    layout->element_count++;

    GLuint* skull_texs = malloc(sizeof(GLuint)*3);
    skull_texs[0] = fn_loadTexture("th1/skull_icons/bronzeskull.png");
    skull_texs[1] = fn_loadTexture("th1/skull_icons/silverskull.png");
    skull_texs[2] = fn_loadTexture("th1/skull_icons/goldskull.png");

        float offs_y[3] = {y_off_3*2.0 - y_off_3*0.9 , -y_off_3*0.9,-y_off_3*2.0  - y_off_3*0.9};
        float offs_x[3] = {screenSize.x*0.0425,screenSize.x*0.0425,screenSize.x*0.0425};

       // int* addrs[3] = {&layout->victory_state[i].level_damagetaken,&layout->victory_state[i].level_speed,&layout->victory_state[i].level_airtime};
        //damage taken
        //clear time
        //air time
        float skull_y_off = -screenSize.y*0.0525;

        for (int j = 0 ; j < 3; j++)
        {
            op_pos = fn_multVec2(fn_createVec2(0.49,0.51 - srcldwon),screenSize);
            op_pos.y = op_pos.y - y_off_3*2.0 - 32*image_scale_factor*0.4 + offs_y[j] - skull_y_off;
            op_pos.x = op_pos.x - offs_x[j];
            th_createUIElementImageTexture(&layout->elements[layout->element_count],op_pos,fn_createVec2(32*image_scale_factor,32*image_scale_factor),skull_texs[0]);
            layout->elements[layout->element_count].nofade = true;
            // layout->elements[layout->element_count].conditional_vis_index = 2;
            // layout->elements[layout->element_count].vis_condition = addrs[j];
            // layout->elements[layout->element_count].conditional_vis_index2 = i;
            // layout->elements[layout->element_count].vis_condition2 = &layout->selected_level;
            layout->medals_images[j] = &layout->elements[layout->element_count];
            layout->element_count++;

            op_pos = fn_multVec2(fn_createVec2(0.49,0.51 - srcldwon),screenSize);
            op_pos.y = op_pos.y - y_off_3*2.0 - 32*image_scale_factor*0.4 + offs_y[j] - skull_y_off;
            op_pos.x = op_pos.x + 32*image_scale_factor*4.0 - offs_x[j];
            th_createUIElementImageTexture(&layout->elements[layout->element_count],op_pos,fn_createVec2(32*image_scale_factor,32*image_scale_factor),skull_texs[1]);
            layout->elements[layout->element_count].nofade = true;
            // layout->elements[layout->element_count].conditional_vis_index = 3;
            // layout->elements[layout->element_count].vis_condition = addrs[j];
            // layout->elements[layout->element_count].conditional_vis_index2 = i;
            // layout->elements[layout->element_count].vis_condition2 = &layout->selected_level;
            layout->medals_images[3 + j] = &layout->elements[layout->element_count];
            layout->element_count++;

            op_pos = fn_multVec2(fn_createVec2(0.49,0.51 - srcldwon),screenSize);
            op_pos.y = op_pos.y - y_off_3*2.0 - 32*image_scale_factor*0.4 + offs_y[j] - skull_y_off;
            op_pos.x = op_pos.x + 32*image_scale_factor*8.0 - offs_x[j];
            th_createUIElementImageTexture(&layout->elements[layout->element_count],op_pos,fn_createVec2(32*image_scale_factor,32*image_scale_factor),skull_texs[2]);
            layout->elements[layout->element_count].nofade = true;
            // layout->elements[layout->element_count].conditional_vis_index = 4;
            // layout->elements[layout->element_count].vis_condition = addrs[j];
            // layout->elements[layout->element_count].conditional_vis_index2 = i;
            // layout->elements[layout->element_count].vis_condition2 = &layout->selected_level;
            layout->medals_images[6 + j] = &layout->elements[layout->element_count];
            layout->element_count++;
        }


    w_op = stringDims("FLAWLESS",2.0,cmap).x;

    op_pos = fn_multVec2(fn_createVec2(0.5,0.7),screenSize);
    op_pos.x = op_pos.x - w_op*0.5;

    th_createUIElementText(&layout->elements[layout->element_count],"FLAWLESS",op_pos,2.0,fn_createVec3(0.5,0.76,0.76),cmap);
    pause_button = layout->element_count;
    layout->elements[pause_button].custom_shader = deathtext;
    layout->elements[layout->element_count].conditional_vis_index = 1;
    layout->elements[layout->element_count].vis_condition = &layout->flawless_enabled;
    layout->elements[layout->element_count].visible = false;
    layout->flawless_text = &layout->elements[layout->element_count];
    layout->element_count++;


    free(skull_texs);
    layout->highlighted_element = 1;
}


static bool boxpointtest(fn_vec2 pos,fn_vec2 dims,fn_vec2 point)
{
    if (point.x >= pos.x && point.x <= pos.x + dims.x && point.y >= pos.y && point.y <= pos.y + dims.y )
    {
        return true;
    }
    return false;
}


void th_processUINoInput(th_UIlayout* layout,fn_vec2 screenSize)
{
    for (int i = 0; i < layout->element_count;i++)
    {
        th_UIElement* elem = &layout->elements[i];
        if ( elem->vis_condition != NULL)
        {
            if ( elem->vis_condition2 != NULL)
            {
                elem->visible =  (elem->conditional_vis_index == *elem->vis_condition) && (elem->conditional_vis_index2 == *elem->vis_condition2);
            }
            else
            {
                elem->visible = elem->conditional_vis_index == *elem->vis_condition;
            }

        }
    }
}

void th_processUI(th_UIlayout* layout,fn_RawInput* input,fn_vec2 screenSize)
{
    int old_selected = layout->highlighted_element;
    th_processUINoInput(layout,screenSize);

    for (int i = 0; i < layout->element_count;i++)
    {
        th_UIElement* elem = &layout->elements[i];

        if (!elem->visible)
        {
            continue;
        }
        fn_vec2 mouse_pos = fn_createVec2(input->xpos_win,screenSize.y - input->ypos_win);
        bool hover = boxpointtest(elem->position,elem->dims,mouse_pos);

        if (elem->type == TH_UI_TEXT)
        {
            if (elem->selectable)
            {
                if (hover)
                {
                    layout->highlighted_element = i;
                    if (input->leftPressed)
                    {
                        input->leftPressed = false; //prevent mutli click on overlaping elements
                        if (elem->save_on_click)
                        {
                            printf("Saving Properties\n");
                            th_dumpPropertiesConfig(layout,th_getPathSettings());
                        }
                        if (elem->callback_on_click != NULL)
                        {
                            elem->callback_on_click(elem->callback_data);
                        }

                        a_VirtualSource* track = a_playMusicTrack(sound_clicked,1);

                        a_setGainMusicTrack(1.0,1);
                        a_setVSPitch(track,1.0/a_getPitch());

                    }
                }
            }

            //falling text animation
            if (th_time() < elem->alpha_target_timer && th_time() > elem->alpha_target_delay)
            {
                float t_offset = 0.0;
                for (int j = 0 ; j < TH_Y_OFFSETS_UI;j++)
                {
                    if (th_time() > elem->alpha_target_delay + t_offset)
                    {
                        float t = 1.0 - fn_clamp((elem->alpha_target_timer + t_offset - th_time()) / (elem->lerptime), 0.0, 1.0);
                        t = t*2.75*2.5;
                        t = fmax(0.0,t);
                        //t = fn_clamp(t,0.0,1.0);
                        float bounce = 1.0 - (t*t) + fmax(0.0,3*(t - 1.0)) + fmax(0.0,1.5*(t - 2.0)) + fmax(0.0,0.75*(t - 2.5));

                        elem->y_offsets[j] = fn_clamp(bounce,0.0,1.0)*(screenSize.y*0.4);
                    }
                    else
                    {
                        elem->y_offsets[j] = (screenSize.y*0.4);
                    }

                    //printf("%i\n",elem->y_offsets[j]);

                    t_offset = t_offset + 35.0;
                }


            }
        }
        else if (elem->type == TH_UI_BUTTON)
        {
            if (hover)
            {
                elem->hovered = true;

                //check for screen edge
                float width_elem = stringDims(elem->tooltip,0.3,elem->cmap).x;

                elem->hover_pos = mouse_pos;

                if (mouse_pos.x + width_elem > screenSize.x)
                {
                    elem->hover_pos.x = elem->hover_pos.x - (mouse_pos.x + width_elem - screenSize.x);
                }

                if (input->leftPressed)
                {
                    input->leftPressed = false; //prevent mutli click on overlaping elements
                    elemPressButton(elem);

                    a_VirtualSource* track = a_playMusicTrack(sound_clicked,1);

                    a_setGainMusicTrack(1.0,1);
                    a_setVSPitch(track,1.0/a_getPitch());
                }
            }
            else
            {
                elem->hovered = false;
            }
        }
        else if (elem->type == TH_UI_SLIDER)
        {
            float cursor_x = fn_lerp(elem->min_x,elem->max_x,elem->slider_position);
            bool hover_cursor = boxpointtest(fn_createVec2(cursor_x,elem->position.y),elem->cursor_dims,mouse_pos);
            if (hover_cursor && input->left && !elem->selected_cursor && !layout->grabbed_cursor)
            {
                layout->grabbed_cursor = true;
                elem->selected_cursor = true;
                elem->selection_offset = input->xpos_win - cursor_x;
            }
            if (elem->selected_cursor && !input->left)
            {
                layout->grabbed_cursor = false;
                elem->selected_cursor = false;
                if (elem->callback_on_click != NULL)
                {
                    elem->callback_on_click(elem->callback_data);
                }
            }
            if (elem->selected_cursor)
            {
                elem->hover_pos = mouse_pos;
                elem->slider_position = fn_clamp(fn_unlerp(elem->min_x,elem->max_x,input->xpos_win - elem->selection_offset),0,1);

                if (elem->instant_writeback && elem->callback_on_click != NULL)
                {
                    elem->callback_on_click(elem->callback_data);
                }
            }
        }
        else if (elem->type == TH_UI_PROGRESS)
        {
            if (th_time() < elem->alpha_target_timer && th_time() > elem->alpha_target_delay)
            {
                float alpha = fn_lerp(0.0,elem->alpha_target,1.0 - fn_clamp((elem->alpha_target_timer - th_time())/(elem->lerptime),0.0,1.0  ));;
                if (elem->slider_alpha_slowdown)
                {
                    float t = (elem->alpha_target != 0.0f) ? fn_clamp(alpha / elem->alpha_target, 0.0, 1.0) : 0.0f;
                    t = fn_clamp(1.0f - (1.0f - t) * (1.0f - t),0.0,1.0);
                    alpha = t * elem->alpha_target;
                }
                elem->old_alpha = elem->alpha;
                elem->alpha = alpha;
            }
        }
        else if (elem->type == TH_UI_KEYBIND)
        {
            if (elem->depressed)
            {
                for (int k = 0 ; k < SDL_NUM_SCANCODES;k++)
                {
                    if (input->currentKeyStates[k] && !input->currentKeyStatesPrev[k])
                    {
                        //update string
                        elem->sc_update = k;
                        if (elem->callback_on_click != NULL)
                        {
                            elem->callback_on_click(elem->callback_data);
                        }
                        elem->depressed = false;
                        layout->grabbed_keybind = false;
                        break;
                    }
                }
            }

            if (hover)
            {
                elem->hovered = true;
                elem->hover_pos = mouse_pos;
                if (input->leftPressed)
                {
                    input->leftPressed = false; //prevent mutli click on overlaping elements

                    if (!elem->depressed && !layout->grabbed_keybind)
                    {
                        layout->grabbed_keybind = true;
                        elem->depressed = true;
                    }

                }
            }
            else
            {
                elem->hovered = false;
            }
        }
        if (elem->type == TH_UI_IMAGE)
        {
            if (th_time() < elem->alpha_target_timer && th_time() > elem->alpha_target_delay)
            {
                // float alpha = fn_lerp(0.0,elem->alpha_target,1.0 - fn_clamp((elem->alpha_target_timer - th_time())/(elem->lerptime),0.0,1.0  ));;
                // alpha = elem->alpha_target*(1- (alpha/elem->alpha_target))*(1 - (alpha/elem->alpha_target));
                // elem->scale_central =1.0 + alpha;

                float t = 1.0 - fn_clamp((elem->alpha_target_timer - th_time()) / (elem->lerptime), 0.0, 1.0);
                float arch = sinf(powf(t, 0.4f) * 3.14159f);
                float alpha = elem->alpha_target * arch;
                elem->scale_central = 1.0 + alpha;

            }

        }
    }

    if (layout->highlighted_element != old_selected)
    {
        a_VirtualSource* track = a_playMusicTrack(sound_mouseover,1);

        a_setGainMusicTrack(1.0,1);
        a_setVSPitch(track,1.0/0.3);
    }

    th_processUINoInput(layout,screenSize);//dirty hack to prevent flickering elements that have chained visibility conditions
}

void th_liftPropertiesConfig(th_UIlayout* layout,const char* path)
{
    th_Allocator allocator;
    th_createAllocator(&allocator);

    int num_keyvals = 0;
    th_KeyValuePair* keyvals = th_keyValueLoad(&allocator,path,&num_keyvals);

    layout->properties = malloc(sizeof(th_OptionsMenuProperties)*1);
    th_initLayoutProperties(layout);
    layout->properties->resolution.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"resolution");
    if (layout->properties->resolution.value >= num_modestrings_local)
    {
        layout->properties->resolution.value = num_modestrings_local - 1;
    }

    layout->properties->dynamic_shadows.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"dynamic_shadows");
    layout->properties->shadow_quality.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"shadow_quality");
    layout->properties->reflection_quality.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"reflection_quality");
    layout->properties->fog_quality.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"fog_quality");
    layout->properties->reflection_scale.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"reflection_scale");
    layout->properties->fullscreen.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"fullscreen");
    layout->properties->vsync.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"vsync");

    layout->properties->music_volume.f_value = th_keyValueGetFloat(keyvals,num_keyvals,"music_volume");
    layout->properties->sfx_volume.f_value = th_keyValueGetFloat(keyvals,num_keyvals,"sfx_volume");

    layout->properties->forward_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"forward_key");
    layout->properties->backward_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"backward_key");
    layout->properties->left_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"left_key");
    layout->properties->right_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"right_key");
    layout->properties->crouch_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"crouch_key");
    layout->properties->jump_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"jump_key");

    layout->properties->machinegun_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"machinegun_key");
    layout->properties->shotgun_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"shotgun_key");
    layout->properties->hammer_key.sc_value = (SDL_Scancode)th_keyValueGetFloat(keyvals,num_keyvals,"hammer_key");

    layout->properties->mouse_sensitivity.f_value = th_keyValueGetFloat(keyvals,num_keyvals,"mouse_sensitivity");
    layout->properties->fov.value = (int)th_keyValueGetFloat(keyvals,num_keyvals,"fov");

    layout->properties->particle_lighting.value = (int)th_keyValueGetFloatDefault(keyvals,num_keyvals,"particle_lighting",1.0);

    layout->maxFPS = (int)th_keyValueGetFloatDefault(keyvals,num_keyvals,"max_fps",200.0);

    layout->properties->invert_mouse_y.value = (int)th_keyValueGetFloatDefault(keyvals,num_keyvals,"invert_mouse_y",0.0);

    layout->properties->gamma.f_value = th_keyValueGetFloatDefault(keyvals,num_keyvals,"gamma",2.2);

    layout->properties->exposure.f_value = th_keyValueGetFloatDefault(keyvals,num_keyvals,"exposure",0.0);
    th_free(&allocator);
}

void th_dumpPropertiesConfig(th_UIlayout* layout,const char* path)
{
    FILE* fp = th_fopen(path,"w");
    if (fp == NULL)
    {
        th_uiNagbar("Could not write settings.txt",fn_createVec2(0,0),0.333,5000);
        return;
    }
    else
    {
        th_uiNagbar("Writing settings.txt",fn_createVec2(0,16),0.333,500);
    }

    fprintf(fp, "resolution %d\n", layout->properties->resolution.value);
    fprintf(fp, "dynamic_shadows %d\n", layout->properties->dynamic_shadows.value);
    fprintf(fp, "shadow_quality %d\n", layout->properties->shadow_quality.value);
    fprintf(fp, "reflection_quality %d\n", layout->properties->reflection_quality.value);
    fprintf(fp, "fog_quality %d\n", layout->properties->fog_quality.value);
    fprintf(fp, "reflection_scale %d\n", layout->properties->reflection_scale.value);
    fprintf(fp, "fullscreen %d\n", layout->properties->fullscreen.value);
    fprintf(fp, "vsync %d\n", layout->properties->vsync.value);

    fprintf(fp, "music_volume %f\n", layout->properties->music_volume.f_value);
    fprintf(fp, "sfx_volume %f\n", layout->properties->sfx_volume.f_value);

    fprintf(fp, "forward_key %d\n", layout->properties->forward_key.sc_value);
    fprintf(fp, "backward_key %d\n", layout->properties->backward_key.sc_value);
    fprintf(fp, "left_key %d\n", layout->properties->left_key.sc_value);
    fprintf(fp, "right_key %d\n", layout->properties->right_key.sc_value);
    fprintf(fp, "crouch_key %d\n", layout->properties->crouch_key.sc_value);
    fprintf(fp, "jump_key %d\n", layout->properties->jump_key.sc_value);

    fprintf(fp, "machinegun_key %d\n", layout->properties->machinegun_key.sc_value);
    fprintf(fp, "shotgun_key %d\n", layout->properties->shotgun_key.sc_value);
    fprintf(fp, "hammer_key %d\n", layout->properties->hammer_key.sc_value);

    fprintf(fp, "mouse_sensitivity %f\n", layout->properties->mouse_sensitivity.f_value);
    fprintf(fp, "fov %d\n", layout->properties->fov.value);
    fprintf(fp, "particle_lighting %d\n", layout->properties->particle_lighting.value);
    fprintf(fp, "max_fps %d\n", layout->maxFPS);
    fprintf(fp, "invert_mouse_y %d\n", layout->properties->invert_mouse_y.value);

    fprintf(fp, "exposure %f\n", layout->properties->exposure.f_value);

    fprintf(fp, "gamma %f\n", layout->properties->gamma.f_value);


    fclose(fp);
}



void th_uiNagbar(const char* text,fn_vec2 position,float size,float timeout)
{
    naginfo.timeset = th_time() + timeout;
    naginfo.text = text;
    naginfo.position = position;
    naginfo.size = size;
    naginfo.active = true;
}

void th_uiNagbarN(const char* text,fn_vec2 position,float size,float timeout,int i)
{
    naginfo_n[i].timeset = th_time() + timeout;
    naginfo_n[i].text = text;
    naginfo_n[i].position = position;
    naginfo_n[i].size = size;
    naginfo_n[i].active = true;
}



void th_getUiNagbar(th_UiNagInfo* out)
{
    if (th_time() > naginfo.timeset )
    {
        naginfo.active = false;
    }
    *out = naginfo;
}

void th_getUiNagbarN(th_UiNagInfo* out,int i)
{
    if (th_time() > naginfo_n[i].timeset )
    {
        naginfo_n[i].active = false;
    }
    *out = naginfo_n[i];
}

static float aspect_scale_local = 1.0;
void th_setAspectScale(float aspect_scale)
{
    aspect_scale_local = aspect_scale;
}

float th_getAspectScale()
{
    return aspect_scale_local;
}
