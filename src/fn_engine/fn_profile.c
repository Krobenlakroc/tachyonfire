#include "fn_profile.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define FN_MAX_PVARS 1024
fn_ProfileVar pvars[FN_MAX_PVARS];
int pvarCount = 0;

void fn_deleteProfileVarID(int id)
{
  pvars[id].deleted = true;
  // pvarCount--;
}

void fn_freeDeletedProfileMem()
{
  int i = 0;
  bool done = false;
  while (done != true) {
    if (pvars[i].deleted)
    {
      pvars[i] = pvars[pvarCount - 1];
      pvarCount--;
      i = 0 ;
    }
    else
    {
      i++;
    }

    if (i == pvarCount)
    {
      done = true;
      return;
    }
  }
}

void fn_printProfileVar(const char* name)
{
  int i;
  for (i =0 ;i < pvarCount;i++)
  {
    if (strcmp(pvars[i].name,name) == 0)
    {
      printf("%s : %f\n",pvars[i].name,pvars[i].data );
      break;
    }
  }
}

int fn_setProfileVar(const char* name,float f)
{
  int i;

  for (i =0 ;i < pvarCount;i++)
  {
    if (strcmp(pvars[i].name,name) == 0)
    {
      pvars[i].data = f;

      return i;
    }
  }

  if (pvarCount == 0)
  {
    // pvars = malloc(sizeof(fn_ProfileVar)*1);
    pvars[0].name = name;
    pvars[0].data = f;
    pvars[0].deleted = false;
    pvarCount = 1;
    return 0;
  }
  else
  {
    pvarCount++;
  //  pvars = realloc(pvars,sizeof(fn_ProfileVar)*pvarCount);
    pvars[pvarCount-1].name = name;
    pvars[pvarCount-1].data = f;
    pvars[pvarCount-1].deleted = false;
    return pvarCount-1;
  }
}

float fn_getProfileVar(const char* name)
{
  int i;
  for (i =0 ;i < pvarCount;i++)
  {
    if (strcmp(pvars[i].name,name) == 0)
    {
      return pvars[i].data;
    }
  }
  return 0;
}

fn_ProfileVar* fn_getProfileRef(const char* name)
{
  int i;
  for (i =0 ;i < pvarCount;i++)
  {
    if (strcmp(pvars[i].name,name) == 0)
    {
      return &pvars[i];
    }
  }
  return 0;
}
