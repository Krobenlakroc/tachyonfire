#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include "th_allocator.h"

#ifndef _WIN32
#include <time.h>

typedef struct {
  uint64_t start;
  uint64_t *accum;
} ProfileScope;


static inline uint64_t now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

#define PROFILE_SCOPE(accuma) \
for (ProfileScope _ps = { now_ns(), &(accuma) }; \
  _ps.accum; \
  *_ps.accum += now_ns() - _ps.start, _ps.accum = NULL)

#endif

#define STACK_ERRORCHECK 1

typedef struct
{
  char* data;
  bool to_free;
}th_String;
#define TH_DEFAULT_STRING (th_Entity){.data = NULL,.to_free = false}

typedef struct
{
  int* stack;
  int stack_count;
  int max_size;
}th_Stack;

typedef struct {
  uint32_t s[4];
} th_xoshiro128p_state;

uint32_t th_xoshiro128p_next(th_xoshiro128p_state *state);

void th_xoshiro128p_init(th_xoshiro128p_state *state, uint32_t seed);

void th_srandom(unsigned int seed);

int th_random();

void th_stackInit(th_Allocator* alloc,th_Stack* stack,int size);
void th_stackPush(th_Stack* stack,int x);
int th_stackPop(th_Stack* stack);

size_t th_getFileModTime(const char* filename);

bool th_fileExists(const char* filename);

float th_randomFloat(float mi,float ma);

int th_randomInt(int mi,int ma);

void th_printlnDevConsole(const char *fmt, ...);

char** th_getDevConsole(int* lines);

void th_write_buffer(void* in_data,size_t bytes_size,size_t n,size_t* total_bytes,unsigned char** total_buffer);

void th_read_buffer(void* out_data,size_t bytes_size,size_t n,size_t* total_bytes,unsigned char* total_buffer);

void th_compress_and_write(const char *filename, const unsigned char *data, size_t data_length);

void th_read_and_decompress(const char *filename, unsigned char **data, size_t *data_length);

char* th_strdup(const char* str);

int th_limitedQueuePush(int* arr,int* top,int val,int max_elements);

int th_limitedQueuePop(int* arr,int* top,int max_elements);

void print_stacktrace();

char* th_getPathConfig(const char* filename,th_Allocator* alloc);

void th_setPathSettings(char* path);
void th_setPathVictory(char* path);

char* th_getPathSettings();
char* th_getPathVictory();

