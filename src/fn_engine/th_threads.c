#include "th_threads.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

//#define NO_THREADS

#include <immintrin.h>

#define SPIN_LIMIT 10

static pthread_t* threads_arr = NULL;
static int threads_count = 0;
static th_ThreadInfo* threads_data = NULL;
static int* num_done = 0;
static int* num_needed = 0;



static pthread_mutex_t jobMutex;
static pthread_cond_t  jobCv;
static bool jobsAvailable = false;

static int threadsLeft;

static inline int atmc_ld(int* addr,int memorder)
{
  int ret = 0;
  __atomic_load(addr,&ret,memorder);

  return ret;
}



void th_setThreadsLeft(int count)
{
  //threadsLeft = count;
  __atomic_store(&threadsLeft,&count,__ATOMIC_RELEASE);
}

void th_nonStartedThread(int count)
{
  //printf("non starter %i\n",count);
  // if (count == 0)
  // {
  //   threadsLeft = 0;
  //   return;
  // }
  // else
  // {
    int dec = ((threads_count - 1) - count);

    //printf("dec %i\n",dec);
    //threadsLeft = threadsLeft - dec;

    __atomic_sub_fetch(&threadsLeft,dec,__ATOMIC_RELEASE);
  //}

}

int th_getNumThreads()
{
  return threads_count;
}




static void* thread_generic(void* tdata)
{
    th_ThreadInfo* thread_data = (th_ThreadInfo*)tdata;
    int num_spins = 0;
    while (true)
    {
      bool lcl_done = false;

      if (thread_data->done)
      {
        #ifdef _WIN32
        Sleep(0);
        #else
        sched_yield();
        #endif
        num_spins++;

        if (num_spins > SPIN_LIMIT)
        {
          pthread_mutex_lock(&jobMutex);

          while (!jobsAvailable) {
            pthread_cond_wait(&jobCv, &jobMutex);
          }

          pthread_mutex_unlock (&jobMutex);
        }
      }
      else
      {
        thread_data->funct(thread_data->data);
        thread_data->done = true;

        __atomic_sub_fetch(&threadsLeft,1,__ATOMIC_RELEASE);
        //threadsLeft = threadsLeft - 1;
        num_spins = 0;
      }

    }
    return NULL;
}


static void* mem_scratch = NULL;

void th_createThreads(int num)
{
  mem_scratch = malloc(32*2048);
    threadsLeft = 0;
    num_done = malloc(sizeof(int));
    num_needed = malloc(sizeof(int));
    threads_data = malloc(sizeof(th_ThreadInfo)*num);
    threads_arr = malloc(sizeof(pthread_t)*num);
    threads_count = num;
      #ifndef NO_THREADS

    //jobMutex
    pthread_mutex_init(&jobMutex,NULL);
    //jobCv
    pthread_cond_init(&jobCv,NULL);
    jobsAvailable = false;

    for (int i = 0 ; i < num - 1;i++)
    {

         threads_data[i].done = true;
        threads_data[i].killed = false;
        pthread_attr_t attr;
        int s = pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr,4194304);
        pthread_create(&threads_arr[i], &attr, thread_generic, (void*)&threads_data[i]);
    }

        #endif

}



void th_setThread(thread_func fun,void* data,int id)
{


  #ifndef NO_THREADS


  if (id  == threads_count - 1)
  {
    fun(data);
  }
  else
  {
    threads_data[id].funct = fun;
    threads_data[id].data = data;
    threads_data[id].done = false;

  }

    #else
    fun(data);
    #endif
}

void* th_getThreadScratch()
{
  return mem_scratch;
}



void th_waitUntillDone(int starti,int endi)
{
  if (endi >= threads_count)
  {
    endi = threads_count - 1;
  }
  #ifndef NO_THREADS


  pthread_mutex_lock(&jobMutex);
  jobsAvailable = true;
  pthread_cond_broadcast(&jobCv);
  pthread_mutex_unlock(&jobMutex);


   // printf("Starting\n");
   //  bool alldone = false;
    // int waitcounter = 0;

    while (!(atmc_ld(&threadsLeft,__ATOMIC_ACQUIRE) == 0))
    {


      for (int i = 0; i < 10; i++)
      {
                _mm_pause();
      }


        // if (!(threadsLeft == 0))
        // {
          #ifdef _WIN32
          Sleep(0);
          #else
          sched_yield();
          #endif
       // }
    }

    pthread_mutex_lock(&jobMutex);
    jobsAvailable = false;
    pthread_mutex_unlock(&jobMutex);

    #endif
}

void th_killThreads()
{
  #ifndef NO_THREADS
    for (int i = 0 ; i < threads_count;i++)
    {

        threads_data[i].killed = true;
        pthread_join(threads_arr[i],NULL);
    }
    #endif
}
