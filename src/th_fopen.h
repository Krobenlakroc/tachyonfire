#pragma once

#ifdef _WIN32
#include <stdio.h>
FILE *th_fopen(const char *path,const char *opt);
#define FOPEN_RB "rb"
#define FOPEN_WB "wb"
#define FOPEN_AB "wb+"
#else
#define th_fopen fopen
#define FOPEN_RB "r"
#define FOPEN_WB "w"
#define FOPEN_AB "w+"
#endif
