/* Parse Bench's frontend: a full-screen terminal UI in plain C.
 *
 * No curses, no external libraries -- just ANSI escape sequences, plus a
 * little platform code to put the terminal into raw mode and read keys.
 */
#ifndef TUI_H
#define TUI_H

/* Run the interactive UI. `path`, if non-NULL, is a token-stream file to
 * load at startup; otherwise the first built-in sample is used. Returns a
 * process exit code. */
int tui_run(const char *path);

#endif /* TUI_H */
