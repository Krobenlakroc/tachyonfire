#pragma once
#include <stdbool.h>
typedef struct
{
  const char* name;
  float data;
  bool deleted;
}fn_ProfileVar;

void fn_deleteProfileVarID(int id);

void fn_freeDeletedProfileMem();

void fn_printProfileVar(const char* name);

int fn_setProfileVar( const char* name,float f);

float fn_getProfileVar(const char* name);

fn_ProfileVar* fn_getProfileRef(const char* name);
