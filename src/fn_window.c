#include "fn_window.h"
#include "fn_gl.h"
#include "fn_engine/fn_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
    typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
#endif

// Callback function for printing debug statements
void  GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,
                             GLenum severity, GLsizei length,
                             const GLchar *msg, const void *data)
{
  const char* _source;
  const char* _type;
  const char* _severity;

  switch (source) {
    case GL_DEBUG_SOURCE_API:
      _source = "API";
      break;

    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      _source = "WINDOW SYSTEM";
      break;

    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      _source = "SHADER COMPILER";
      break;

    case GL_DEBUG_SOURCE_THIRD_PARTY:
      _source = "THIRD PARTY";
      break;

    case GL_DEBUG_SOURCE_APPLICATION:
      _source = "APPLICATION";
      break;

    case GL_DEBUG_SOURCE_OTHER:
      _source = "UNKNOWN";
      break;

    default:
      _source = "UNKNOWN";
      break;
  }

  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      _type = "ERROR";
      break;

    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      _type = "DEPRECATED BEHAVIOR";
      break;

    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      _type = "UDEFINED BEHAVIOR";
      break;

    case GL_DEBUG_TYPE_PORTABILITY:
      _type = "PORTABILITY";
      break;

    case GL_DEBUG_TYPE_PERFORMANCE:
      _type = "PERFORMANCE";
      break;

    case GL_DEBUG_TYPE_OTHER:
      _type = "OTHER";
      break;

    case GL_DEBUG_TYPE_MARKER:
      _type = "MARKER";
      break;

    default:
      _type = "UNKNOWN";
      break;
  }

  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      _severity = "HIGH";
      break;

    case GL_DEBUG_SEVERITY_MEDIUM:
      _severity = "MEDIUM";
      break;

    case GL_DEBUG_SEVERITY_LOW:
      _severity = "LOW";
      break;

    case GL_DEBUG_SEVERITY_NOTIFICATION:
      _severity = "NOTIFICATION";
      break;

    default:
      _severity = "UNKNOWN";
      break;
  }
  if (strcmp(_type,"PERFORMANCE") == 0)
  {
    return;
  }
  printf("%d: %s of %s severity, raised from %s: %s\n",
         id, _type, _severity, _source, msg);
}

void SetupGLDebug()
{
  //glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT); // Make callback synchronous for easier debugging
  glDebugMessageCallback(GLDebugMessageCallback, NULL);

  // Enable all messages
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
                        0, NULL, GL_TRUE);
}

void fn_initWindow(fn_Window* window,fn_Config* config)
{

#ifdef _WIN32
void* userDLL;
BOOL(WINAPI *SetProcessDPIAware)(void); // Vista and later
void* shcoreDLL;
HRESULT(WINAPI *SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS dpiAwareness); // Windows 8.1 and later

userDLL = SDL_LoadObject("USER32.DLL");
if (userDLL) {
    SetProcessDPIAware = (BOOL(WINAPI *)(void)) SDL_LoadFunction(userDLL, "SetProcessDPIAware");
}

shcoreDLL = SDL_LoadObject("SHCORE.DLL");
if (shcoreDLL) {
    SetProcessDpiAwareness = (HRESULT(WINAPI *)(PROCESS_DPI_AWARENESS)) SDL_LoadFunction(shcoreDLL, "SetProcessDpiAwareness");
}

if (SetProcessDpiAwareness) {
    /* Try Windows 8.1+ version */
    HRESULT result = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    SDL_Log("called SetProcessDpiAwareness: %d", (result == S_OK) ? 1 : 0);
}
else if (SetProcessDPIAware) {
    /* Try Vista - Windows 8 version.
    This has a constant scale factor for all monitors.
    */
    BOOL success = SetProcessDPIAware();
    SDL_Log("called SetProcessDPIAware: %d", (int)success);
}
#endif





  // //display modes


    SDL_SetHint("SDL_HINT_VIDEO_HIGHDPI_DISABLED","0");
  SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, config->gl_major );
  SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, config->gl_minor );
  //SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR);
  SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  Uint32 flags = SDL_WINDOW_OPENGL  ;//| SDL_WINDOW_SHOWN
  if (config->window_hidden)
  {
    flags = flags | SDL_WINDOW_HIDDEN;
  }
  else if (config->borderless_fullscreen)
  {
    flags = flags | SDL_WINDOW_FULLSCREEN_DESKTOP;
    flags = flags | SDL_WINDOW_SHOWN;
    flags = flags | SDL_WINDOW_ALLOW_HIGHDPI;
  }
  else if (config->fullscreen)
  {
    flags = flags | SDL_WINDOW_FULLSCREEN;
    flags = flags | SDL_WINDOW_SHOWN;
    flags = flags | SDL_WINDOW_ALLOW_HIGHDPI;
  }
  else
  {
    flags = flags | SDL_WINDOW_BORDERLESS;
    flags = flags | SDL_WINDOW_SHOWN;
  }
  // if (fn_getProfileVar("borderless"))
  //   flags = flags | SDL_WINDOW_BORDERLESS;


  window->gWindow = SDL_CreateWindow( "OpenGL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config->d_width, config->d_height, flags );


  if (config->fullscreen && !config->borderless_fullscreen) {
    SDL_DisplayMode desired = {0};
    desired.w = config->width;
    desired.h = config->height;
    desired.refresh_rate = 0; // let SDL pick best refresh rate

    SDL_DisplayMode closest;
    if (SDL_GetClosestDisplayMode(0, &desired, &closest) != NULL) {
      SDL_SetWindowDisplayMode(window->gWindow, &closest);
    }
  }



  // SDL_ShowCursor(SDL_DISABLE); // disable cursor
  window->gContext = SDL_GL_CreateContext( window->gWindow );

  //high dpi insanity
  if (config->fullscreen && config->borderless_fullscreen)
  {
    int w_local = 0;
    int h_local = 0;
    SDL_GL_GetDrawableSize(window->gWindow,&w_local, &h_local);

    if (w_local != config->d_width || h_local != config->d_height)
    {
      config->d_width = w_local;
      config->d_height = h_local;
      config->dpi_nonsense = true;
    }

  }

  int version = gladLoadGL((GLADloadfunc) SDL_GL_GetProcAddress);
  printf("GL %d.%d\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

  if( window->gContext == NULL )
  {
    printf( "OpenGL context could not be created! SDL Error: %s\n", SDL_GetError() );
  }
  if (config->vsync)
  {
    printf("%s\n","Setting Vsync" );
    if( SDL_GL_SetSwapInterval( 1 ) < 0 )
    {
      printf( "Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError() );
    }
  }
  else
  {
    if( SDL_GL_SetSwapInterval( 0 ) < 0 )
    {
      printf( "Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError() );
    }
  }
  printf("SWAP INTERVAL %i\n",SDL_GL_GetSwapInterval() );

}
void fn_updateWindow(fn_Window* window)
{
  SDL_GL_SwapWindow( window->gWindow );
}

void fn_closeWindow(fn_Window* window)
{
  SDL_DestroyWindow( window->gWindow );
  window->gWindow = NULL;
  //Quit SDL subsystems
  SDL_Quit();
}

void fn_moveMouse(fn_Window* window,float x,float y)
{
  SDL_WarpMouseInWindow(window->gWindow,x,y);
}

const char * fn_errorString(GLenum error)
{


  if(error == GL_INVALID_ENUM)
  {
    const char* s =  "GL_INVALID_ENUM";
    return s;
  }
  if(error == GL_INVALID_VALUE)
  {
    const char* s   = "GL_INVALID_VALUE";
    return s;
  }
  if(error == GL_INVALID_OPERATION)
  {
    const char* s   = "GL_INVALID_OPERATION";
    return s;
  }
  if(error == GL_STACK_OVERFLOW)
  {
    const char* s   = "GL_STACK_OVERFLOW";
    return s;
  }
  if(error == GL_STACK_UNDERFLOW)
  {
    const char* s   = "GL_STACK_UNDERFLOW";
    return s;
  }
  if(error == GL_OUT_OF_MEMORY)
  {
    const char* s  = "GL_OUT_OF_MEMORY";
    return s;
  }
  // if(error == GL_TABLE_TOO_LARGE)
  // {
  //   const char* s   = "GL_TABLE_TOO_LARGE";
  //   return s;
  // }
  const char* s   = "GL_NO_ERROR";
  return s;

}

 void fn_getGLError()
{
  // GLenum error = GL_NO_ERROR;
  // error = glGetError();
  // if( error != GL_NO_ERROR )
  // {
  //     printf( "OpenGL Error! %s\n", fn_errorString( error ) );
  // }
}

 void callbackerror(GLenum source,
                            GLenum type,
                            unsigned int id,
                            GLenum severity,
                            GLsizei length,
                            const char *message,
                            const void *userParam)
   {
//     if (type != GL_DEBUG_TYPE_PERFORMANCE && type != GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR)
//     {
//       return;
//     }
     printf("OPENGL Error::");
     switch (source) {
       case GL_DEBUG_SOURCE_API:             printf("API"); break;
       case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   printf("Window System"); break;
       case GL_DEBUG_SOURCE_SHADER_COMPILER: printf("Shader Compiler"); break;
       case GL_DEBUG_SOURCE_THIRD_PARTY:     printf("Third Party"); break;
       case GL_DEBUG_SOURCE_APPLICATION:     printf("Application"); break;
       case GL_DEBUG_SOURCE_OTHER:           printf("Other"); break;
     }
     printf("::");

     switch (type)
     {
       case GL_DEBUG_TYPE_ERROR:               printf("Error"); break;
       case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: printf("Deprecated Behaviour"); break;
       case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  printf("Undefined Behaviour"); break;
       case GL_DEBUG_TYPE_PORTABILITY:         printf("Portability"); break;
       case GL_DEBUG_TYPE_PERFORMANCE:         printf("Performance"); break;
       case GL_DEBUG_TYPE_MARKER:              printf("Marker"); break;
       case GL_DEBUG_TYPE_PUSH_GROUP:          printf("Push Group"); break;
       case GL_DEBUG_TYPE_POP_GROUP:           printf("Pop Group"); break;
       case GL_DEBUG_TYPE_OTHER:               printf("Other"); break;
     }
     printf("::");
     switch (severity)
     {
       case GL_DEBUG_SEVERITY_HIGH:         printf("high"); break;
       case GL_DEBUG_SEVERITY_MEDIUM:       printf("medium"); break;
       case GL_DEBUG_SEVERITY_LOW:          printf("low"); break;
       case GL_DEBUG_SEVERITY_NOTIFICATION: printf("notification"); break;
     }
     printf("::%s\n",message);

   }




bool fn_initGL(fn_Config* config)
{

  GLenum error = GL_NO_ERROR;
  bool success = true;

  error = glGetError();
  if( error != GL_NO_ERROR )
  {
      printf( "Error initializing OpenGL2D! %s\n", fn_errorString( error ) );
      success = false;
  }

  // glEnable( GL_TEXTURE_3D );
  // glClearStencil( 0 );
  // //png alpha channel
  // // glEnable(GL_ALPHA_TEST);
  // // glAlphaFunc(GL_NOTEQUAL, 0.0);
  // // glEnable(GL_BLEND);
  // // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  // // glClearDepth(DEPTH_MAX);                   // Set background depth to farthest

  // glDisable(GL_STENCIL_TEST);
  // glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  glViewport(0, 0, config->width, config->height);
  //Check for error
  error = glGetError();
  if( error != GL_NO_ERROR )
  {
      printf( "Error initializing OpenGL! %s\n", fn_errorString( error ) );
      success = false;
  }
  //Initialize clear color


  //Check for error
  error = glGetError();
  if( error != GL_NO_ERROR )
  {
      printf( "Error initializing OpenGL! %s\n", fn_errorString( error ) );
      success = false;
  }

  //   glewExperimental = GL_TRUE;
  //
  //
  //
  // GLenum err=glewInit();
  // if(err!=GLEW_OK) {
  //   // Problem: glewInit failed, something is seriously wrong.
  //   printf("glewInit failed: %s\n",glewGetErrorString(err));
  //
  // }



  error = glGetError();
  if( error != GL_NO_ERROR )
  {
      printf( "Error initializing GLEW! %s\n", fn_errorString( error ) );
      success = false;
  }
  // if(!GLEW_ARB_vertex_array_object)
  // printf( "ARB_vertex_array_object not available.\n" );
  //
  // if(!GLEW_ARB_draw_instanced)
  // printf("ARB_instancing not available.\n" );
  //
  // if(!GLEW_ARB_instanced_arrays)
  // printf( "ARB_instancing_arrays not available.\n");
  //
  // if(!GLEW_ARB_buffer_storage)
  // printf("ARB_buffer_storage not available.\n" );
  //
  // if (!GLEW_ARB_draw_elements_base_vertex)
  // printf("GL_ARB_draw_elements_base_vertex.\n" );

  int n;
  // int n2;
  // glGetIntegerv(GL_NUM_EXTENSIONS, &n2);
  // printf("%i Extensions\n",n2 );
  // // for (i = 0; i < n2 ;i++)
  // //   printf("%s %i/%i \n",glGetStringi(GL_EXTENSIONS, i),i + 1,n2 );


  glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT,GL_NICEST);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &n);
  printf("Max Texture Size: %i\n",n );

  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &n);
  printf("Max vertex Attributes: %i\n",n );

  int texture_units;
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);
  printf("Max Texture Units: %i\n",texture_units );

  int maxUniformVectors;
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxUniformVectors);
  printf("Max Uniform Vectors: %i\n",maxUniformVectors );

  int maximageunits;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maximageunits);
  printf("Max Image Units: %i\n",maximageunits );


  int maxuniformblocksize;
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &maxuniformblocksize);
  printf("Max FRagment BLOCKS: %i\n",maxuniformblocksize );

  int maxtess;
  glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &maxtess);
  printf("Max Tesselation: %i\n",maxtess );





  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxuniformblocksize);
  printf("Max  BLOCKS size: %i\n",maxuniformblocksize );

  printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));//glewGetString(GLEW_VERSION)
  // int argc = 1;
  // char *argv[1] = {(char*)"Something"};
  // glutInit(&argc, argv);
  // GLuint VAOID ;
  // glGenVertexArrays(1, &VAOID);
  // glBindVertexArray(VAOID);


  glEnable(GL_DEPTH_TEST);   // Enable depth testing for z-culling
  glDepthFunc(GL_LEQUAL);    // Set the type of depth-test
  glFrontFace(GL_CW);
   // glEnable(GL_DEBUG_OUTPUT);
   // glDebugMessageCallback(callbackerror, NULL);
   // glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
  //SetupGLDebug();
  return success;
}

int compare_modes(const void *a, const void *b) {
  const SDL_DisplayMode *ma = (const SDL_DisplayMode *)a;
  const SDL_DisplayMode *mb = (const SDL_DisplayMode *)b;
  int64_t area_a = (int64_t)ma->w;// * ma->h;
  int64_t area_b = (int64_t)mb->w;// * mb->h;

  if (area_a == area_b)
  {
    area_a = (int64_t)ma->h;// * ma->h;
    area_b = (int64_t)mb->h;// * mb->h;
  }

  if (area_b > area_a) return 1;
  if (area_b < area_a) return -1;
  return 0;
}

void th_getWindowModes(int* x_out,int* y_out,int* used_modes)
{
  //enumerate resolutions
  int displayIndex = 0; // 0 = primary monitor
  int numModes = SDL_GetNumDisplayModes(displayIndex);
  int* x_modes = x_out;//[TH_MAX_MODES] = {1920};
  int* y_modes = y_out;//[TH_MAX_MODES] = {1080};
  int found_modes = 0;

  // int x_resolutions[] = {1280,1366,1600,1920,2560,2560,3440,3840};
  // int y_resolutions[] = {720,768,900,1080,1080,1440,1440,2160};

  int supported_modes[][2] = {{640,480},
  {800,600},
  {800,480},
  {854,480},
  {1024,576},
  {1024,768},
  {1280,720},
  {1280,768},
  {1280,800},
  {1280,854},
  {1366,768},
  {1400,1050},
  {1440,900},
  {1440,960},
  {1440,1080},
  {1600,900},
  {1600,1200},
  {1680,1050},
  {1920,1080},
  {1920,1200},
  {1920,1440},
  {2048,1080},
  {2048,1536},
  {2160,1440},
  {2560,1080},
  {2560,1440},
  {2560,1600},
  {2560,2048},
  {3200,1800},
  {3440,1440},
  {3840,1200},
  {3840,1080},
  {3840,2160},
  {3840,2400},
  {3840,1600},
  {4096,2160},
  {5120,1440},
  {5120,1600},
  {5120,2160},
  {5120, 2880},
  {7680,1440},
  {7680, 2160},
  {7680, 4320},
  };


  if (numModes > 0) {
    // 1. Collect all modes first
    SDL_DisplayMode* all_modes = malloc(sizeof(SDL_DisplayMode)*numModes);//[numModes];
    int total = 0;
    for (int i = 0; i < numModes; i++) {

      SDL_DisplayMode mode_test;

      if (SDL_GetDisplayMode(displayIndex, i, &mode_test) == 0) {
        //printf("%i %i\n", all_modes[total].w, all_modes[total].h);
        int num_supported = sizeof(supported_modes) / sizeof(supported_modes[0]);
        for (int j = 0 ; j < num_supported;j++)
        {
          if (mode_test.w == supported_modes[j][0] && mode_test.h == supported_modes[j][1] )
          {
            all_modes[total] = mode_test;
            total++;
            break;
          }
        }

      }


    }

    qsort(all_modes, total, sizeof(SDL_DisplayMode), compare_modes);

    // 3. Deduplicate, keep highest resolution at index 0
    for (int i = 0; i < total && found_modes < TH_MAX_MODES; i++) {
      if (found_modes == 0 ||
        (all_modes[i].w != x_modes[found_modes - 1] ||
        all_modes[i].h != y_modes[found_modes - 1]))
      {
        x_modes[found_modes] = all_modes[i].w;
        y_modes[found_modes] = all_modes[i].h;
        found_modes++;
        //printf("%i %i\n", all_modes[i].w, all_modes[i].h);
      }
    }

    // fill in the rest
    if (found_modes < TH_MAX_MODES && found_modes > 0) {
      for (int i = found_modes; i < TH_MAX_MODES; i++) {
        x_modes[i] = x_modes[found_modes - 1];
        y_modes[i] = y_modes[found_modes - 1];
      }
    }

    free (all_modes);
  }

  *used_modes = found_modes;
}

int th_getDesktopDisplayModeIndex()
{



  //enumerate resolutions
  int displayIndex = 0; // 0 = primary monitor

  SDL_DisplayMode desktop_mode;

  int err = SDL_GetDesktopDisplayMode(displayIndex, &desktop_mode );


  int numModes = SDL_GetNumDisplayModes(displayIndex);

  int x_modes[TH_MAX_MODES] = {1920};
  int y_modes[TH_MAX_MODES] = {1080};
  int found_modes = 0;

  th_getWindowModes(x_modes,y_modes,&found_modes);


  for (int i = 0 ; i < found_modes;i++)
  {
    if (x_modes[i] == desktop_mode.w && y_modes[i] == desktop_mode.h)
    {
      return i;
    }
  }
  // if (resolution_index >= found_modes)
  // {
  //   resolution_index = found_modes - 1;
  // }
  // if (resolution_index < 0)
  // {
  //   resolution_index = 0;
  // }
  return 0;
}
