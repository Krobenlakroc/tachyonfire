#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct
{

  void** ptrs;
  int ptrs_count;

  int ptrs_alloc;
}th_CleanupList;

typedef struct
{
  uint8_t* mem_start;
  uint8_t* mem_current;
  size_t size;
  size_t allocated;
}th_AllocatorChunk;

typedef struct
{
  th_CleanupList cleanup;
  th_AllocatorChunk* chunks;
  size_t num_chunks;
  size_t capacity;
  pthread_mutex_t alloc_lock;

  bool is_temporary;
  bool baked;
  int temporary_serve_index;
  int temporary_serve_count;
  void* parent;

  size_t* temp_sizes;
}th_Allocator;

/*
 * A temporary allocator memorizes what allocations are made, and then when it is marked "done", it keeps those allocations and serves them in the same order
 * The main allocator keeps track of these sub-allocators and is responsible for actually freeing their memory (we register the pointers with the main allocator so it can go
 * out of scope)
 */
void th_createAllocatorTemporary(th_Allocator* allocator_parent,th_Allocator* allocator);
void th_finishAllocatorTemporary(th_Allocator* allocator);

void th_createAllocator(th_Allocator* allocator);
void* th_alloc(th_Allocator* allocator,size_t size);
void* th_arenaManage(th_Allocator* allocator,void* in,size_t size);

void th_registerCleanup(th_Allocator* allocator,void* mem);
void* th_reallocCleanup(th_Allocator* allocator,void* mem,size_t newsize);

void th_free(th_Allocator* allocator);

