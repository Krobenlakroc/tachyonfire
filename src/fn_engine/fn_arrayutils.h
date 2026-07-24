#pragma once
#include <stdlib.h>


int* remove_elementi(int *array, int index, int array_length);

void** remove_elementv(void** array,int index,int array_length);

void* concat_element(void* a,void* b,size_t aeize,int aCount,int bCount);

void* concat_elementNoFreeB(void* a,void* b,size_t aeize,int aCount,int bCount);
