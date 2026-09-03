/* Minimal terminal control: raw-mode keys, screen size, ANSI colours.
 *
 * Everything the UI needs from the platform lives behind this header, so
 * tui.c contains no #ifdefs.
 */
#ifndef TERM_H
#define TERM_H

#include <stddef.h>

/* Keys. Printable characters come back as themselves; these are the rest. */
enum {
    KEY_NONE  = -1,
    KEY_UP    = 1000,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PGUP,
    KEY_PGDN,
    KEY_ESC,
    KEY_ENTER,
    KEY_BACKSPACE
};

/* Put the terminal in raw mode, hide the cursor, switch to the alternate
 * screen, and (on Windows) enable VT sequences and UTF-8 output. */
void term_begin(void);
/* Undo all of that. Safe to call twice. */
void term_end(void);

void term_size(int *rows, int *cols);

/* Block up to `timeout_ms` for a key. Returns KEY_NONE on timeout. Pass a
 * negative timeout to block indefinitely. */
int  term_getkey(int timeout_ms);

/* Move the cursor home without clearing, so redrawing a frame overwrites the
 * previous one instead of flashing. */
void term_home(void);
/* Erase from the cursor to the end of the screen. */
void term_clear_to_end(void);

/* ANSI colour codes, used inline in rendered lines. */
#define C_RESET   "\x1b[0m"
#define C_BOLD    "\x1b[1m"
#define C_DIM     "\x1b[2m"
#define C_REV     "\x1b[7m"
#define C_RED     "\x1b[31m"
#define C_GREEN   "\x1b[32m"
#define C_YELLOW  "\x1b[33m"
#define C_BLUE    "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN    "\x1b[36m"
#define C_WHITE   "\x1b[37m"
#define C_GREY    "\x1b[90m"

#endif /* TERM_H */
