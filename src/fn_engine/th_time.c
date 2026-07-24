#include "th_time.h"
static unsigned int framecount = 0;
unsigned int th_frame()
{
  return framecount;
}
void th_tickFrame()
{
  framecount++;
}

static th_timer_t time_total = 0;
void th_addTime(float dt)
{
  time_total+= (th_timer_t)dt;
}
th_timer_t th_time()
{
  return time_total;
}


static th_timer_t time_total_global = 0;
void th_addTimeGlobal(float dt)
{
  time_total_global+= (th_timer_t)dt;
}
th_timer_t th_time_global()
{
  return time_total_global;
}
