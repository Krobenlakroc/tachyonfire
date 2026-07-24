#include "th_debugger.h"
#define DEBUGGER_ENABLED false
#define DEBUGGER_SCALE 10
//(3.04*10*6.4*0.6)
//40
void th_DebuggerInit(th_Allocator* alloc,th_Debugger* object,int count,fn_vec3** points,int* points_count)
{
  object->transforms = th_alloc(alloc,sizeof(fn_mat4)*count);
  object->count_allocated = count;
  object->points = points;
  object->point_count = points_count;
}

void th_DebuggerUpdate(th_Debugger* object)
{
//  printf("%s\n","DBG" );
  for (int i = 0 ; i < object->count_allocated;i++)
  {
    if ((i < *object->point_count) && DEBUGGER_ENABLED)
    {
      object->transforms[i] = fn_translaterotatescale((*object->points)[i],0,fn_createVec3(1,0,0),fn_createVec3s(DEBUGGER_SCALE));
    //  fn_printVec3((*object->points)[i]);
    }
    else {
      object->transforms[i] = fn_makescale(fn_createVec3s(0));
    }
  }
}

static th_Debugger* object_default;

void th_setDefaultDebugger(th_Debugger* object)
{
  object_default = object;
}
th_Debugger* th_getDefaultDebugger()
{
  return object_default;
}
