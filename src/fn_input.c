#include "fn_input.h"
#include "fn_engine/th_time.h"
#define defaultFocus {false,false}
bool quit = false;
static Uint8 dummy[SDL_NUM_SCANCODES] = {0} ;
static float px = 0;
static float py = 0;
typedef struct
{
  bool looseFocus;
  bool gainFocus;
}fn_FocusType;

static fn_FocusType testForFocus(const SDL_Event * event)
{
  fn_FocusType focus;
  focus.looseFocus = false;
  focus.gainFocus = false;
    if (event->type == SDL_WINDOWEVENT) {
        switch (event->window.event) {



        case SDL_WINDOWEVENT_ENTER:
            focus.gainFocus = true;
            break;
        case SDL_WINDOWEVENT_LEAVE:
            focus.looseFocus = true;
            break;
        case SDL_WINDOWEVENT_FOCUS_GAINED:
          focus.gainFocus = true;
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            focus.looseFocus = true;
            break;


        }
    }
    return focus;
}

Uint8* fn_dummyInput()
{
  return dummy;
}

static bool mousedown_win = false;
void fn_getInput(SDL_Event* event,fn_RawInput* input,float dt)
{
  fn_FocusType totalFocus = defaultFocus;
  input->wheel = 0;
  input->quit = false;
  input->xrel = 0;
  input->yrel = 0;
  input->left = false;
  input->right = false;
  input->leftPressed = false;
  input->rightPressed = false;
  input->reset = false;
  input->changemap = false;
  input->leftTapped = false;
  input->rightTapped = false;
  input->spaceTapped = false;
  input->spaceReleased = false;
  input->gainedFocus = false;
  input->lostFocus = false;
  bool ml = false;
  bool mr = false;
  bool tr = false;
  bool tl = false;
  bool tm = false;


  bool lu = false;
  while(SDL_PollEvent(event) !=0)
  {
    fn_FocusType focus = testForFocus(event);
    if (focus.looseFocus)
    {
      totalFocus.looseFocus = true;
      totalFocus.gainFocus = false;

      input->lostFocus = true;
      input->gainedFocus = false;
    }
    if (focus.gainFocus)
    {
      totalFocus.gainFocus = true;
      totalFocus.looseFocus = false;

      input->gainedFocus = true;
      input->lostFocus = false;
    }
    //a event is happenging!
    switch (event->type)
    {
      case SDL_QUIT:
      input->quit = true;// quit game loop
      break;
      case SDL_MOUSEWHEEL:
      input->wheel = event->wheel.y;
      break;
      case SDL_MOUSEMOTION:
      {
        SDL_MouseMotionEvent motion = event->motion;
        input->xrel += motion.xrel;
        input->yrel += motion.yrel;


        break;
      }
      case SDL_MOUSEBUTTONDOWN:

          mr =( (int)event->button.button == (int)SDL_BUTTON_RIGHT) ;
          ml =( (int)event->button.button == (int)SDL_BUTTON_LEFT );
          if (mr)
          {
            input->rightPressedT = SDL_GetTicks();
          }
          if (ml)
          {
            input->leftPressedT = SDL_GetTicks();
          }
      break;
      case SDL_MOUSEBUTTONUP:
         tr =( (int)event->button.button == (int)SDL_BUTTON_RIGHT) ;
         tl =( (int)event->button.button == (int)SDL_BUTTON_LEFT );
         tm =( (int)event->button.button == (int)SDL_BUTTON_MIDDLE);
          if (tr)
          {
            input->rightReleasedT = SDL_GetTicks();
          }
          if (tl)
          {
            input->leftReleasedT = SDL_GetTicks();
          }
      break;
      case SDL_KEYDOWN:


          if (event->key.keysym.scancode == SDL_SCANCODE_SPACE)
          {
            input->spaceTapped = true;
          }
      break;
      case SDL_KEYUP:
      if (event->key.keysym.scancode == SDL_SCANCODE_SPACE)
      {
        input->spaceReleased = true;
      }
      break;
    }
  }

  if (totalFocus.looseFocus)
  {
    input->inFocus = false;
  }
  if (totalFocus.gainFocus)
  {
    input->inFocus = true;
  }



    input->leftPressed = ml;

    input->rightPressed = mr;
  int x,y;

  Uint32 i = SDL_GetMouseState(&x, &y);
  if(i & SDL_BUTTON(SDL_BUTTON_LEFT))
  {
    input->left = true;
  }
  if(i & SDL_BUTTON(SDL_BUTTON_RIGHT))
  {
    input->right = true;
  }
  if (th_frame() != 0)
  {
    memcpy(input->currentKeyStatesPrev,input->currentKeyStates,sizeof(Uint8)*SDL_NUM_SCANCODES);
  }
  else
  {
    memcpy(input->currentKeyStatesPrev,SDL_GetKeyboardState(NULL),sizeof(Uint8)*SDL_NUM_SCANCODES);
  }

  memcpy(input->currentKeyStates,SDL_GetKeyboardState(NULL),sizeof(Uint8)*SDL_NUM_SCANCODES);
//  input->currentKeyStates = SDL_GetKeyboardState( &input->numKeys );
  if (quit)
  {
    input->quit = true;
  }
  int diffr = input->rightReleasedT - input->rightPressedT;
  int diffl = input->leftReleasedT - input->leftPressedT;
  int delta = 100;
  if (diffr > 0 && diffr < delta)
  {
    input->rightTapped = true;
    input->rightReleasedT = 1;
    input->rightPressedT = 2;
  }
  if (diffl > 0 && diffl < delta)
  {
    input->leftTapped = true;
    input->leftReleasedT = 1;
    input->leftPressedT = 2;
  }

  if (input->left)
  {
    input->leftHoldDuration += 1.f*dt;//later multiply by delta time
  }
  else
  {
    input->leftHoldDuration = 0.f;
  }

  if (input->right)
  {
    input->rightHoldDuration += 1.f*dt;//later multiply by delta time
  }
  else
  {
    input->rightHoldDuration = 0.f;
  }

  input->left_win = input->left;
  input->leftdown_win = tl;
  input->middledown_win = tm;
  // i = SDL_GetMouseState(&x,&y);
  input->xpos_win = x;
  input->ypos_win = y;
  input->xrel_win = x- px;
  input->yrel_win = y -py;
  py= y;
  px = x;
  Uint32 ticks = SDL_GetTicks();
  // for (int i = 0 ; i < SDL_NUM_SCANCODES;i++)
  // {
  //   if (input->currentKeyStates[i] && !input->currentKeyStatesPrev[i])
  //   {
  //     input->keyPresstime[i] = ticks;
  //   }
  //   if (!input->currentKeyStates[i] && input->currentKeyStatesPrev[i])
  //   {
  //     input->keyReleasetime[i] = ticks;
  //   }
  // }
}

void fn_quit()
{
  quit = true;
}
