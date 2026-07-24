#include "th_allocator.h"
#include "th_system.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdalign.h>
#include <stddef.h>
#include <assert.h>
#define ALLOC_PASSTHROUGH
// #define ALLOC_CHUNK_SIZE 131072

#define ALLOC_INITIAL_PTRS 512
#define ALLOC_CHUNK_SIZE 131072  // 128 KB default chunk size
#define INITIAL_CHUNK_CAPACITY 4
#define ALIGNMENT (alignof(max_align_t))

// Align up helper
static size_t align_up(size_t size, size_t alignment) {
    assert((alignment & (alignment - 1)) == 0);
    return (size + alignment - 1) & ~(alignment - 1);
}

void th_createAllocatorTemporary(th_Allocator* allocator_parent,th_Allocator* allocator)
{
    pthread_mutex_init(&allocator->alloc_lock, NULL);
    allocator->cleanup.ptrs = NULL;
    allocator->cleanup.ptrs_count = 0;
    allocator->cleanup.ptrs_alloc = ALLOC_INITIAL_PTRS;
    allocator->cleanup.ptrs = th_alloc(allocator_parent,sizeof(void*)*allocator->cleanup.ptrs_alloc);
    allocator->temp_sizes = th_alloc(allocator_parent,sizeof(size_t)*allocator->cleanup.ptrs_alloc);

    allocator->chunks = NULL;

    allocator->capacity = 0;
    allocator->num_chunks = 0;

    allocator->is_temporary = true;
    allocator->temporary_serve_index = 0;
    allocator->temporary_serve_count = 0;
    allocator->parent = (void*)allocator_parent;
    allocator->baked = false;
}

void th_finishAllocatorTemporary(th_Allocator* allocator)
{
    allocator->baked = true;
    allocator->temporary_serve_index = 0;
}

void th_createAllocator(th_Allocator* allocator) {
    pthread_mutex_init(&allocator->alloc_lock, NULL);
    allocator->cleanup.ptrs = NULL;
    allocator->cleanup.ptrs_count = 0;
    allocator->cleanup.ptrs_alloc = ALLOC_INITIAL_PTRS;
    allocator->cleanup.ptrs = malloc(sizeof(void*)*allocator->cleanup.ptrs_alloc);

    allocator->chunks = malloc(sizeof(th_AllocatorChunk) * INITIAL_CHUNK_CAPACITY);
    if (!allocator->chunks) {
        fprintf(stderr, "Allocator initialization failed.\n");
        exit(EXIT_FAILURE);
    }

    allocator->capacity = INITIAL_CHUNK_CAPACITY;
    allocator->num_chunks = 1;

    th_AllocatorChunk* chunk = &allocator->chunks[0];
    chunk->size = ALLOC_CHUNK_SIZE;
    chunk->allocated = 0;
    chunk->mem_start = malloc(chunk->size);
    if (!chunk->mem_start) {
        fprintf(stderr, "Chunk allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    chunk->mem_current = chunk->mem_start;

    allocator->is_temporary = false;
    allocator->temporary_serve_index = 0;
    allocator->temporary_serve_count = 0;
    allocator->parent = NULL;
    allocator->baked = false;
    allocator->temp_sizes = NULL;

    //printf("Alloc allignment %li\n",align_up(ALLOC_CHUNK_SIZE, ALIGNMENT));
}




void* th_alloc(th_Allocator* allocator, size_t size) {
    if (allocator->is_temporary)
    {
        if (allocator->baked)
        {
            if (allocator->temporary_serve_index >= allocator->temporary_serve_count)
            {
                printf("TEMP ALLOCATOR TOO MANY\n");
            }

            if (allocator->temp_sizes[allocator->temporary_serve_index] == size)
            {
                return allocator->cleanup.ptrs[allocator->temporary_serve_index++];
            }
            else
            {
                print_stacktrace();
                printf("TEMP ALLOCATOR SIZE OUR OF ORDER\n");
            }
        }
        else
        {
            void* ret2 = th_alloc((th_Allocator*)allocator->parent,size);
            th_registerCleanup(allocator,ret2);

            if (allocator->temporary_serve_count < allocator->cleanup.ptrs_alloc )
            {
                allocator->temp_sizes[allocator->temporary_serve_count++] = size;
            }
            return ret2;

        }

        return NULL;
    }


#ifdef ALLOC_PASSTHROUGH
void* ret2 = malloc(size);
//memset(ret2,0xCD,size);

th_registerCleanup(allocator,ret2);

return ret2;
#endif


    //printf("Allocing\n");
    //return malloc(size);
    size = align_up(size, ALIGNMENT);



    // Get current chunk after possible realloc/init
    th_AllocatorChunk* chunk = &allocator->chunks[allocator->num_chunks - 1];

    uintptr_t raw = (uintptr_t)chunk->mem_current;
    uintptr_t aligned = align_up(raw, ALIGNMENT);
    size_t adjustment = aligned - raw;

    if (adjustment != 0)
    {
        printf("Allignment correction\n");
    }

    // Check if there is enough space
    if (size + adjustment > chunk->size - chunk->allocated) {
        // Expand chunk array if full
        if (allocator->num_chunks == allocator->capacity) {
            size_t new_capacity = allocator->capacity * 2;
            if (new_capacity < allocator->capacity) {
                fprintf(stderr, "Chunk capacity overflow.\n");
                exit(EXIT_FAILURE);
            }
            //printf("MESSAGE: ARENA CHUNK REALLOC\n");
            th_AllocatorChunk* new_chunks = realloc(allocator->chunks, sizeof(th_AllocatorChunk) * new_capacity);
            if (!new_chunks) {
                fprintf(stderr, "Failed to expand chunk array.\n");
                exit(EXIT_FAILURE);
            }
            allocator->chunks = new_chunks;
            allocator->capacity = new_capacity;
        }

        // Allocate new chunk
        th_AllocatorChunk* newchunk = &allocator->chunks[allocator->num_chunks++];
        newchunk->size = size > ALLOC_CHUNK_SIZE ? size : ALLOC_CHUNK_SIZE;
        newchunk->allocated = 0;
        newchunk->mem_start = malloc(newchunk->size);

        if (!newchunk->mem_start) {
            fprintf(stderr, "Chunk memory allocation failed.\n");
            exit(EXIT_FAILURE);
        }
        newchunk->mem_current = newchunk->mem_start;
         //printf("MESSAGE: NEW ARENA CHUNK!!! %li %li\n",allocator->num_chunks,allocator->capacity);

        uintptr_t new_raw = (uintptr_t)newchunk->mem_start;
        uintptr_t new_aligned = align_up(new_raw, ALIGNMENT);
        size_t new_adjustment = new_aligned - new_raw;

        newchunk->mem_current = (uint8_t *)new_aligned + size;
        newchunk->allocated = new_adjustment + size;
        if (new_adjustment != 0)
        {
            printf("Allignment malloc error\n");
        }
        //printf("MESSAGE: ARENA ALLOC %p %zu %zu\n",(void *)new_aligned,size + new_adjustment,new_adjustment );
        return (void *)new_aligned;
    }

    // Allocate from current chunk
    void* ret = (void *)aligned;
    chunk->mem_current = (uint8_t *)ret + size;
    chunk->allocated += size + adjustment;

    //printf("MESSAGE: ARENA ALLOC %p %zu %zu\n",ret,size + adjustment,adjustment );

    return ret;
}

void* th_arenaManage(th_Allocator* allocator,void* in,size_t size)
{

    // #ifdef ALLOC_PASSTHROUGH
    // return in;
    // #endif


    if (in == NULL)
    {
        printf("NULL MANAGED\n");
        return NULL;
    }
    if (size == 0)
    {
        printf("0 MANAGED\n");
        return NULL;
    }
    void* ret = th_alloc(allocator,size);
    memcpy(ret,in,size);
    free(in);

    return ret;
}



void th_registerCleanup(th_Allocator* allocator,void* mem)
{


    pthread_mutex_lock(&allocator->alloc_lock);
    if (allocator->cleanup.ptrs == NULL)
    {
        allocator->cleanup.ptrs = malloc(sizeof(void*));
        allocator->cleanup.ptrs_count = 1;
        allocator->cleanup.ptrs[0] = mem;
    }
    else
    {
        if (allocator->cleanup.ptrs_count >= allocator->cleanup.ptrs_alloc)
        {
            allocator->cleanup.ptrs = realloc(allocator->cleanup.ptrs,sizeof(void*)*(allocator->cleanup.ptrs_alloc*2));
            allocator->cleanup.ptrs_alloc = allocator->cleanup.ptrs_alloc*2;

            if (allocator->is_temporary)
            {
                printf("CANNOT TEMPORARY ALLOC MORE THAN %i PTRS!\n",allocator->cleanup.ptrs_alloc);
                return;
            }

        }

        allocator->cleanup.ptrs_count++;
        allocator->cleanup.ptrs[allocator->cleanup.ptrs_count - 1] = mem;

    }
     pthread_mutex_unlock(&allocator->alloc_lock);
}

void* th_reallocCleanup(th_Allocator* allocator,void* mem,size_t newsize)
{
    if (allocator->is_temporary)
    {
        printf("CANNOT REALLOC ON TEMPORARY ALLOCATOR!\n");
        return NULL;
    }

    if (mem == NULL)
    {
        void* ret = malloc(newsize);
        th_registerCleanup(allocator,ret);
        return ret;
    }

    pthread_mutex_lock(&allocator->alloc_lock);
    if (allocator->cleanup.ptrs != NULL)
    {
        for (int i = 0 ; i < allocator->cleanup.ptrs_count;i++)
        {
           if (mem == allocator->cleanup.ptrs[i])
           {
                allocator->cleanup.ptrs[i] = realloc(mem,newsize);
                pthread_mutex_unlock(&allocator->alloc_lock);
                return allocator->cleanup.ptrs[i];
           }
        }
    }

    pthread_mutex_unlock(&allocator->alloc_lock);
    printf("PANIC: CANNOT FIND PTR TO REALLOC\n");
    return NULL;
}

void th_free(th_Allocator* allocator) {
    if (allocator->is_temporary)
    {
        printf("CANNOT FREE TEMPORARY ALLOCATOR!\n");
        return;
    }

    if (allocator->cleanup.ptrs != NULL)
    {
        for (int i = allocator->cleanup.ptrs_count - 1 ; i >= 0 ;i--)
        {
            free(allocator->cleanup.ptrs[i]);
        }
        free(allocator->cleanup.ptrs);
    }
    allocator->cleanup.ptrs_alloc = 0;
    allocator->cleanup.ptrs_count = 0;
    allocator->cleanup.ptrs = NULL;

    for (size_t i = 0; i < allocator->num_chunks; i++) {
        free(allocator->chunks[i].mem_start);
    }

    if (allocator->chunks != NULL)
    {
            free(allocator->chunks);
    }

    allocator->chunks = NULL;
    allocator->num_chunks = 0;
    allocator->capacity = 0;
    pthread_mutex_destroy(&allocator->alloc_lock);
}


