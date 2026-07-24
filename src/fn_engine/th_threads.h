#ifndef TH_THREADS_H_INCLUDED
#define TH_THREADS_H_INCLUDED
#include <math.h>
#include <assert.h>
typedef unsigned int uint;
/*
Scheduler variables:
th_thread_id
add
start_pos
used
sched_comps
nomore
*/

#define TH_CREATE_SCHEDULING_DATA(n,b) b data[n];
#define TH_CREATE_SCHEDULING_LOOP(n,b,num_comps) {uint add = floor((num_comps)/((double)(n))); uint left = (num_comps) - add*(n);uint start_pos = 0;int used = 0; int sched_comps = (num_comps); th_setThreadGroupBegin(n - 1); for (uint th_thread_id = 0 ; th_thread_id < (n);th_thread_id++){

//printf("Sched %s %i\n", #b,num_comps);
#define TH_BEGIN_SCHEDULING(n,b,num_comps)   {assert(sizeof(b)*n < 32*2048); b* data = (b*)th_getThreadScratch();uint add = floor((num_comps)/((double)(n))); uint left = (num_comps) - add*(n);uint start_pos = 0;uint used = 0; uint sched_comps = (num_comps);th_setThreadsLeft(n - 1); for (uint th_thread_id = 0 ; th_thread_id < (n);th_thread_id++){

#define TH_SCHEDULING_FUNC if (add == 0 && th_thread_id >= left){th_nonStartedThread(th_thread_id );break;} if (th_thread_id < left) {data[th_thread_id].range = add + 1;}

#define TH_END_SCHEDULING used++; start_pos += (add + (th_thread_id < left ? 1 : 0));}th_waitUntillDone(0,th_getNumThreads() - 1); }

#define TH_END_SCHEDULING_NO_WAIT used++; start_pos += (add + (th_thread_id < left ? 1 : 0));}}

#define TH_WAITTHREADS(count) th_waitUntillDone(0,(count));


#include <stdbool.h>

typedef void (*thread_func)( void*);
typedef struct
{
    thread_func funct;
    void* data;
    _Atomic bool done;
    char aldone;
    bool killed;
}th_ThreadInfo;

void* th_getThreadScratch();

void th_setThreadsLeft(int count);
void th_nonStartedThread(int count);
int th_getNumThreads();
void th_createThreads(int num);
void th_setThread(thread_func fun,void* data,int id);
void th_waitUntillDone(int starti,int endi);
void th_killThreads();

#endif // TH_THREADS_H_INCLUDED
