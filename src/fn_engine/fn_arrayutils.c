#include "fn_arrayutils.h"
#include <stdlib.h>
#include <string.h>


int* remove_elementi(int *array, int index, int array_length)
{
   int i;
   for(i = index; i < array_length - 1; i++) array[i] = array[i + 1];
   int *tmp = realloc(array, (array_length - 1) * sizeof(int) );
   return tmp;
}

void** remove_elementv(void** array,int index,int array_length)
{
  int i;
  for(i = index; i < array_length - 1; i++) array[i] = array[i + 1];
  void** tmp = realloc(array, (array_length - 1) * sizeof(void*) );
  return tmp;
}

void* concat_element(void* a,void* b,size_t aeize,int aCount,int bCount)
{
  char* c = malloc(aeize*aCount + aeize*bCount);
  memcpy(c,(char*)a,aeize*aCount);
  memcpy(c + (aeize*aCount),(char*)b,aeize*bCount);
  free(a);
  if (a != b)
    free(b);
  return (void*)c;
}

void* concat_elementNoFreeB(void* a,void* b,size_t aeize,int aCount,int bCount)
{
  char* c = malloc(aeize*aCount + aeize*bCount);
  memcpy(c,(char*)a,aeize*aCount);
  memcpy(c + (aeize*aCount),(char*)b,aeize*bCount);
  free(a);
  return c;
}
