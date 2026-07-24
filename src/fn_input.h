#pragma once
#include "fn_math/fn_vec3.h"
#include "fn_math/fn_vec2.h"
#include <stdbool.h>
#include <SDL.h>

typedef struct
{
  Uint32 leftPressedT;
  Uint32 leftReleasedT;
  Uint32 rightPressedT;
  Uint32 rightReleasedT;

  int numKeys;
  Uint8 currentKeyStates[SDL_NUM_SCANCODES] ;
  Uint8 currentKeyStatesPrev[SDL_NUM_SCANCODES] ;
  Uint32 keyPresstime[SDL_NUM_SCANCODES];
  Uint32 keyReleasetime[SDL_NUM_SCANCODES];
  bool spaceTapped;
  bool spaceReleased;
  float xrel;
  float yrel;
  float wheel;
  bool quit;
  bool rightPressed;
  bool right;
  bool rightTapped;
  bool leftPressed;
  bool left;
  bool leftTapped;
  bool reset;
  bool changemap;

  float leftHoldDuration;
  float rightHoldDuration;

  float xpos_win;
  float ypos_win;
  bool leftdown_win;
  bool middledown_win;
  bool inFocus;
  bool left_win;
  float xrel_win;
  float yrel_win;

  bool gainedFocus;
  bool lostFocus;

  int binding_forward;
  int binding_back;
  int binding_left;
  int binding_right;
  int binding_jump;
  int binding_crouch;
  int binding_weapon1;
  int binding_weapon2;
  int binding_weapon3;
}fn_RawInput;

void fn_getInput(SDL_Event* event,fn_RawInput* input,float dt);

void fn_quit();

Uint8* fn_dummyInput();
