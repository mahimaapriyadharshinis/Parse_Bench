#include "watchdog.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)

#include <windows.h>

static volatile LONG wd_disarmed = 0;
static unsigned      wd_seconds = 5;

static DWORD WINAPI wd_thread(LPVOID arg)
{
    (void)arg;
    for (unsigned i = 0; i < wd_seconds * 10; i++) {
        Sleep(100);
        if (InterlockedCompareExchange(&wd_disarmed, 0, 0)) return 0;
    }
    fprintf(stderr, "\nWATCHDOG: timed out after %u seconds -- the parser hung.\n",
            wd_seconds);
    fflush(stderr);
    _exit(2);
    return 0;
}

void watchdog_arm(unsigned seconds)
{
    wd_seconds = seconds;
    InterlockedExchange(&wd_disarmed, 0);
    HANDLE h = CreateThread(NULL, 0, wd_thread, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

void watchdog_disarm(void)
{
    InterlockedExchange(&wd_disarmed, 1);
}

#else /* POSIX */

#include <signal.h>
#include <unistd.h>

#define WD_MSG "\nWATCHDOG: timed out -- the parser hung.\n"

static void wd_handler(int sig)
{
    (void)sig;
    ssize_t ignored = write(2, WD_MSG, sizeof WD_MSG - 1);
    (void)ignored;
    _exit(2);
}

void watchdog_arm(unsigned seconds)
{
    signal(SIGALRM, wd_handler);
    alarm(seconds);
}

void watchdog_disarm(void)
{
    alarm(0);
}

#endif
