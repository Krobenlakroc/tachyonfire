#pragma once
#include <SDL.h>
#include <stdbool.h>
#include "fn_config.h"
#include "fn_gl.h"

#define TH_MAX_MODES 16

typedef struct
{
  SDL_Window* gWindow;//window
  SDL_GLContext gContext;//OpenGL context
}fn_Window;


void fn_initWindow(fn_Window* window,fn_Config* config);
void fn_closeWindow(fn_Window* window);
void fn_moveMouse(fn_Window* window,float x,float y);
bool fn_initGL(fn_Config* config);
const char * fn_errorString(GLenum error);
void fn_getGLError();
void fn_updateWindow(fn_Window* window);

int th_getDesktopDisplayModeIndex();
void th_getWindowModes(int* x_out,int* y_out,int* used_modes);
