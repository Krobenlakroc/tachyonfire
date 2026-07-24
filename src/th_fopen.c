
// #include <stdio.h>
//
// #ifdef _WIN32
// #include <windows.h>
//
// static FILE *fopen_utf8(const char *utf8path, const char *mode) {
//     wchar_t wpath[1024];
//     wchar_t wmode[16];
//     MultiByteToWideChar(CP_UTF8, 0, utf8path, -1, wpath, 1024);
//     MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 16);
//     return _wfopen(wpath, wmode);
// }
// #else
// static FILE *fopen_utf8(const char *utf8path, const char *mode) {
//     return fopen(utf8path, mode); // Linux/macOS filesystem is UTF-8 natively
// }
// #endif


//taken from https://0xdeafc0de.wordpress.com/2013/07/21/wrapper-for-fopen-in-windows-and-posix-like-oses/
// && defined(UNICODE)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>


FILE *th_fopen(const char *path,const char *opt){
    wchar_t wpath[1024];
    wchar_t wopt[16];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024) == 0)
    {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, opt, -1, wopt, 16) == 0)
    {
        return NULL;
    }

    return _wfopen(wpath, wopt);
}
#endif
