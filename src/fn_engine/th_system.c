#include "th_system.h"
#include <string.h>
#include "../miniz.h"

#include "../fn_input.h"
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>
#include <utime.h>
#include <math.h>
#include <stdlib.h>

#include <SDL.h>
#include "../th_fopen.h"

#ifndef _WIN32
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

void print_stacktrace() {
  // void *buffer[100];
  // int nptrs = backtrace(buffer, 100);
  //
  // char **strings = backtrace_symbols(buffer, nptrs);
  // if (strings == NULL) {
  //   perror("backtrace_symbols");
  //   exit(EXIT_FAILURE);
  // }
  //
  // for (int i = 0; i < nptrs; i++) {
  //   printf("%s\n", strings[i]);
  // }
  //
  // free(strings);
}

#else

void print_stacktrace() {

}


#endif

static unsigned long next = 1;

static pthread_mutex_t rand_lock;

void th_srandom(unsigned int seed)
{
  pthread_mutex_init(&rand_lock, NULL);
  //next = seed;
  srand(seed);
}

int th_random()
{
  pthread_mutex_lock(&rand_lock);
  //next = next * 1103515245 + 12345;
  int ret = rand();//((unsigned)(next/65536) % 32768);
  pthread_mutex_unlock(&rand_lock);
  return ret;

}

size_t th_getFileModTime(const char* filename)
{
  struct stat foo;
  time_t mtime;
  struct utimbuf new_times;

  if (stat(filename, &foo) < 0) {
    perror(filename);
    return 1;
  }
  mtime = foo.st_mtime; /* seconds since the epoch */

  return mtime;
}



bool th_fileExists(const char* filename)
{
  FILE *file;
if ((file = th_fopen(filename, "r")))
{
    fclose(file);
    return true;
}
return false;
}

float th_randomFloat(float mi,float ma)
{
  return (float)th_random()/(float)(RAND_MAX/(ma - mi)) + mi;
}

int th_randomInt(int mi,int ma)
{
  return (th_random() % (ma - mi + 1)) + mi;
}

#define CONSOLE_LINES 5
static char** console;
static bool init_console = false;


void th_printlnDevConsole(const char *fmt, ...)
{
  if (!init_console)
  {
    console = malloc(sizeof(char*)*CONSOLE_LINES);
    for (size_t i = 0; i < CONSOLE_LINES; i++) {
      console[i] = malloc(sizeof(char)*1024);
      console[i][0] = '\0';
    }
    init_console = true;
  }
  va_list args;
  va_start(args, fmt);
  char buffer[1024];
  vsprintf(buffer,fmt,args);
  va_end(args);

  for (int i = CONSOLE_LINES - 1; i >= 1; i--) {
    strcpy(console[i],console[i - 1]);
  }
  strcpy(console[0],buffer);

}

char** th_getDevConsole(int* lines)
{
  if (!init_console)
  {
    console = malloc(sizeof(char*)*CONSOLE_LINES);
    for (size_t i = 0; i < CONSOLE_LINES; i++) {
      console[i] = malloc(sizeof(char)*1024);
      console[i][0] = '\0';
    }
    init_console = true;
  }

  *lines = CONSOLE_LINES;
  return console;
}

void th_stackInit(th_Allocator* alloc,th_Stack* stack,int size)
{
  stack->max_size = size;
  stack->stack = th_alloc(alloc,sizeof(int)*size);
  stack->stack_count = 0;
}

void th_stackPush(th_Stack* stack,int x)
{
  #if STACK_ERRORCHECK
  if (stack->stack_count >= stack->max_size )
  {
    printf("%s\n","Stack overflow in th_Stack" );
  }

  #endif
  stack->stack[stack->stack_count] = x;
  stack->stack_count++;

}

int th_stackPop(th_Stack* stack)
{
  stack->stack_count--;
  #if STACK_ERRORCHECK
  if (stack->stack_count < 0 )
  {
    printf("%s\n","Stack underflow in th_Stack" );
  }
  #endif
  return stack->stack[stack->stack_count];
}

void th_compress_and_write(const char *filename, const unsigned char *data, size_t data_length) {
    FILE *file = th_fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file for writing");
    }

    uLong compressed_length = compressBound(data_length);
    unsigned char *compressed_data = (unsigned char *)malloc(compressed_length);
    if (!compressed_data) {
        perror("Failed to allocate memory for compressed data");
        fclose(file);
        return;
    }

    int res = compress(compressed_data, &compressed_length, data, data_length);
    if (res != Z_OK) {
        fprintf(stderr, "Failed to compress data: %d\n", res);
        free(compressed_data);
        fclose(file);
        return;
    }

    size_t written = fwrite(compressed_data, 1, compressed_length, file);
    if (written != compressed_length) {
        perror("Failed to write compressed data to file");
        free(compressed_data);
        fclose(file);
        return;
    }

    free(compressed_data);
    fclose(file);
}
//
//
void th_read_and_decompress(const char *filename, unsigned char **data, size_t *data_length) {
    FILE *file = th_fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for reading");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *compressed_data = (unsigned char *)malloc(file_size);
    if (!compressed_data) {
        perror("Failed to allocate memory for compressed data");
        fclose(file);
        return;
    }

    size_t read = fread(compressed_data, 1, file_size, file);
    if (read != file_size) {
        perror("Failed to read compressed data from file");
        free(compressed_data);
        fclose(file);
        return;
    }

    *data_length = file_size * 2; // Initial guess for decompressed size
    *data = (unsigned char *)malloc(*data_length);
    if (!*data) {
        perror("Failed to allocate memory for decompressed data");
        free(compressed_data);
        fclose(file);
        return;
    }

    mz_ulong dlen = (mz_ulong)*data_length;
    int res = uncompress(*data, &dlen, compressed_data, file_size);
    *data_length = (size_t)dlen;

    while (res == Z_BUF_ERROR) {
        // Need more space for decompressed data
        *data_length *= 2;
        *data = (unsigned char *)realloc(*data, *data_length);
        if (!*data) {
            perror("Failed to reallocate memory for decompressed data");
            free(compressed_data);
            fclose(file);
            return;
        }

        mz_ulong dlen = (mz_ulong)*data_length;
        res = uncompress(*data, &dlen, compressed_data, file_size);
        *data_length = (size_t)dlen;
    }

    if (res != Z_OK) {
        fprintf(stderr, "Failed to decompress data: %d\n", res);
        free(compressed_data);
        free(*data);
        fclose(file);
        return;
    }

    free(compressed_data);
    fclose(file);
}

void th_write_buffer(void* in_data,size_t bytes_size,size_t n,size_t* total_bytes,unsigned char** total_buffer)
{
  *total_buffer = realloc(*total_buffer,*total_bytes + bytes_size*n);
  memcpy(&((*total_buffer)[*total_bytes]),(unsigned char*)in_data,bytes_size*n);
  *total_bytes = *total_bytes + bytes_size*n;
}

void th_read_buffer(void* out_data,size_t bytes_size,size_t n,size_t* total_bytes,unsigned char* total_buffer)
{
  memcpy((unsigned char*)out_data,&((total_buffer)[*total_bytes]),bytes_size*n);
  *total_bytes = *total_bytes + bytes_size*n;
}

char* th_strdup(const char* str)
{
  size_t len = strlen(str) + 1;       // String plus '\0'
  char *dst = malloc(len);            // Allocate space
  if (dst == NULL) return NULL;       // No memory
  memcpy (dst, str, len);             // Copy the block
  return dst;                         // Return the new string
}




static inline uint32_t rotl(uint32_t x, int k) {
  return (x << k) | (x >> (32 - k));
}

uint32_t th_xoshiro128p_next(th_xoshiro128p_state *state) {
  uint32_t result = state->s[0] + state->s[3];
  uint32_t t = state->s[1] << 9;

  state->s[2] ^= state->s[0];
  state->s[3] ^= state->s[1];
  state->s[1] ^= state->s[2];
  state->s[0] ^= state->s[3];
  state->s[2] ^= t;
  state->s[3] = rotl(state->s[3], 11);

  return result;
}

// Initialize with non-zero seed
void th_xoshiro128p_init(th_xoshiro128p_state *state, uint32_t seed) {
  state->s[0] = seed;
  state->s[1] = seed ^ 0x9E3779B9;
  state->s[2] = seed ^ 0x85EBCA77;
  state->s[3] = seed ^ 0xC2B2AE3D;
  // Warm up
  for (int i = 0; i < 16; i++) th_xoshiro128p_next(state);
}

int th_limitedQueuePush(int* arr,int* top,int val,int max_elements)
{
  if (*top >= max_elements)
  {
    return 1; //failed push
  }
  arr[*top] = val;
  *top = *top + 1;
  return 0;
}

int th_limitedQueuePop(int* arr,int* top,int max_elements)
{
  int ref = *top - 1;
  if (ref < 0 || ref >= max_elements)
  {
    return -1; //failed pop
  }

  *top = *top - 1;
  return arr[*top];
}

char* th_getPathConfig(const char* filename,th_Allocator* alloc)
{
  char *path = SDL_GetPrefPath("tachyonfire", "tachyonfire");


  int buffer_size = snprintf(NULL, 0, "%s%s", path, filename) + 1;


  char* out = NULL;
  size_t out_size = buffer_size;
  if (alloc == NULL)
  {
    out = malloc(sizeof(char)*buffer_size);
  }
  else
  {
    out = th_alloc(alloc,sizeof(char)*buffer_size);
  }

  int written = snprintf(out, out_size, "%s%s", path, filename);
  if (written < 0 || (size_t)written >= out_size) {
    fprintf(stderr, "Could not get config path for '%s'\n", filename);

    SDL_free(path);
    free(out);
    return NULL;
  }

  SDL_free(path);
  return out;


}

static char* settings_path_global = NULL;
static char* victory_path_global = NULL;

void th_setPathSettings(char* path)
{
  settings_path_global = path;
}

void th_setPathVictory(char* path)
{
  victory_path_global = path;
}

char* th_getPathSettings()
{
  return settings_path_global;
}
char* th_getPathVictory()
{
  return victory_path_global;
}

const char* th_getKeyName(SDL_Scancode sc)
{
  if ((int)sc >= TH_SCANCODE_CUSTOM)
  {

    static const char *mouse_button_names[] = {
      "MOUSE L", "MOUSE3", "MOUSE R", "MOUSE4", "MOUSE5", "MOUSE6", "MOUSE7", "MOUSE8", "MOUSE9", "MOUSE10",
      "MOUSE11", "MOUSE12", "MOUSE13", "MOUSE14", "MOUSE15", "MOUSE16", "MOUSE17", "MOUSE18", "MOUSE19", "MOUSE20",
      "MOUSE21", "MOUSE22", "MOUSE23", "MOUSE24", "MOUSE25", "MOUSE26", "MOUSE27", "MOUSE28", "MOUSE29", "MOUSE30",
      "MOUSE31", "MOUSE32"
    };

    #define MOUSE_BUTTON_COUNT (sizeof(mouse_button_names) / sizeof(mouse_button_names[0]))

    if (sc >= TH_SCANCODE_MOUSE(0) && sc < TH_SCANCODE_MOUSE(MOUSE_BUTTON_COUNT))
      return mouse_button_names[sc - TH_SCANCODE_MOUSE(0)];


    return "UNKNOWN";

  }

  SDL_Keycode key = SDL_GetKeyFromScancode(sc);
  const char* sc_name = SDL_GetKeyName(key);


  bool ascii = true;
  for (const unsigned char* p = (const unsigned char*)sc_name; *p; ++p)
  {
    if (*p >= 128)
    {
      ascii = false;
      break;
    }
  }

  if (!ascii)
    sc_name = SDL_GetScancodeName(sc);

  return sc_name;
}
