#include "term.h"

#include <stdio.h>
#include <stdlib.h>

static int g_active = 0;

void term_home(void)         { fputs("\x1b[H", stdout); }
void term_clear_to_end(void) { fputs("\x1b[J", stdout); }

#if defined(_WIN32)

#include <conio.h>
#include <windows.h>

static DWORD g_saved_out_mode = 0;
static DWORD g_saved_in_mode  = 0;
static UINT  g_saved_out_cp   = 0;

void term_begin(void)
{
    if (g_active) return;

    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hin  = GetStdHandle(STD_INPUT_HANDLE);

    if (GetConsoleMode(hout, &g_saved_out_mode)) {
        SetConsoleMode(hout, g_saved_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    if (GetConsoleMode(hin, &g_saved_in_mode)) {
        /* drop line input and echo so keys arrive one at a time */
        SetConsoleMode(hin, g_saved_in_mode & ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
    }

    g_saved_out_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);   /* for the box-drawing characters */

    fputs("\x1b[?1049h", stdout);  /* alternate screen */
    fputs("\x1b[?25l", stdout);    /* hide cursor */
    fputs("\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    g_active = 1;
}

void term_end(void)
{
    if (!g_active) return;
    fputs("\x1b[?25h", stdout);    /* show cursor */
    fputs("\x1b[0m", stdout);
    fputs("\x1b[?1049l", stdout);  /* leave alternate screen */
    fflush(stdout);

    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hin  = GetStdHandle(STD_INPUT_HANDLE);
    if (g_saved_out_mode) SetConsoleMode(hout, g_saved_out_mode);
    if (g_saved_in_mode)  SetConsoleMode(hin, g_saved_in_mode);
    if (g_saved_out_cp)   SetConsoleOutputCP(g_saved_out_cp);

    g_active = 0;
}

void term_size(int *rows, int *cols)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        *cols = 80;
        *rows = 25;
    }
    if (*cols < 40) *cols = 40;
    if (*rows < 12) *rows = 12;
}

int term_getkey(int timeout_ms)
{
    if (timeout_ms >= 0) {
        /* poll in 10ms slices; _kbhit has no timeout of its own */
        int waited = 0;
        while (!_kbhit()) {
            if (waited >= timeout_ms) return KEY_NONE;
            Sleep(10);
            waited += 10;
        }
    }

    int c = _getch();
    if (c == 0 || c == 224) {          /* extended key: a second byte follows */
        int e = _getch();
        switch (e) {
        case 72: return KEY_UP;
        case 80: return KEY_DOWN;
        case 75: return KEY_LEFT;
        case 77: return KEY_RIGHT;
        case 71: return KEY_HOME;
        case 79: return KEY_END;
        case 73: return KEY_PGUP;
        case 81: return KEY_PGDN;
        default: return KEY_NONE;
        }
    }
    if (c == 27)  return KEY_ESC;
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == 8 || c == 127) return KEY_BACKSPACE;
    return c;
}

#else /* POSIX */

#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_saved_tio;

void term_begin(void)
{
    if (g_active) return;

    tcgetattr(STDIN_FILENO, &g_saved_tio);
    struct termios raw = g_saved_tio;
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    fputs("\x1b[?1049h", stdout);
    fputs("\x1b[?25l", stdout);
    fputs("\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    g_active = 1;
}

void term_end(void)
{
    if (!g_active) return;
    fputs("\x1b[?25h", stdout);
    fputs("\x1b[0m", stdout);
    fputs("\x1b[?1049l", stdout);
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved_tio);
    g_active = 0;
}

void term_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    } else {
        *cols = 80;
        *rows = 25;
    }
    if (*cols < 40) *cols = 40;
    if (*rows < 12) *rows = 12;
}

/* Read one byte, or -1 if none arrives within `timeout_ms`. */
static int read_byte(int timeout_ms)
{
    if (timeout_ms >= 0) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) return -1;
    }
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) return -1;
    return ch;
}

int term_getkey(int timeout_ms)
{
    int c = read_byte(timeout_ms);
    if (c < 0) return KEY_NONE;

    if (c == 27) {
        /* An escape sequence arrives all at once; a lone Esc does not. */
        int c1 = read_byte(30);
        if (c1 < 0) return KEY_ESC;
        if (c1 == '[' || c1 == 'O') {
            int c2 = read_byte(30);
            switch (c2) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            case '1': read_byte(30); return KEY_HOME;
            case '4': read_byte(30); return KEY_END;
            case '5': read_byte(30); return KEY_PGUP;
            case '6': read_byte(30); return KEY_PGDN;
            default:  return KEY_NONE;
            }
        }
        return KEY_ESC;
    }
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == 8 || c == 127) return KEY_BACKSPACE;
    return c;
}

#endif
