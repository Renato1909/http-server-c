#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "log.h"

static CRITICAL_SECTION g_lock;
static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK init_lock_once(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    (void)once;
    (void)param;
    (void)ctx;
    InitializeCriticalSection(&g_lock);
    return TRUE;
}

static void log_write(FILE *stream, const char *tag, const char *fmt, va_list ap)
{
    InitOnceExecuteOnce(&g_once, init_lock_once, NULL, NULL);
    EnterCriticalSection(&g_lock);

    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(stream, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s ",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, tag);
    vfprintf(stream, fmt, ap);
    fputc('\n', stream);
    fflush(stream);

    LeaveCriticalSection(&g_lock);
}

void log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write(stdout, "INFO ", fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_write(stderr, "ERROR", fmt, ap);
    va_end(ap);
}
