#pragma once
#include "../fn_engine/th_physics.h"
#include "../fn_math/fn_grid.h"
#include "../fn_input.h"
#include "../fn_engine/th_clusters.h"
#include "../fn_engine/th_allocator.h"

struct th_LevelState;
typedef struct th_LevelState th_LevelState;


typedef struct
{
  fn_mat4* transforms;
  int count_allocated;
  fn_vec3** points;
  int* point_count;

  fn_vec3* points_default;
  int point_count_default;
}th_Debugger;

void th_DebuggerInit(th_Allocator* alloc,th_Debugger* object,int count,fn_vec3** points,int* points_count);

void th_DebuggerUpdate(th_Debugger* object);

void th_setDefaultDebugger(th_Debugger* object);
th_Debugger* th_getDefaultDebugger();
