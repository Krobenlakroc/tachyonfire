#pragma once
#include <stdint.h>

typedef double th_timer_t;
typedef int64_t th_timer_int_t;

unsigned int th_frame();
void th_tickFrame();
void th_addTime(float dt);
th_timer_t th_time();

void th_addTimeGlobal(float dt);
th_timer_t th_time_global();
