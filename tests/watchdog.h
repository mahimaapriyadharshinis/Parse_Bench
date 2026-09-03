/* A wall-clock watchdog for the panic-mode infinite-loop regression test.
 *
 * That bug made the parser spin forever rather than return a wrong answer,
 * so the only way to test for it is a timeout: arm the watchdog, run the
 * parse, disarm. If the parse hangs, the process is killed with a clear
 * message instead of wedging `make test` indefinitely.
 *
 * The implementation lives in watchdog.c rather than here because it needs
 * <windows.h>, which defines an enumerator of its own called TokenType and
 * would collide with src/token.h in any file that included both.
 */
#ifndef WATCHDOG_H
#define WATCHDOG_H

void watchdog_arm(unsigned seconds);
void watchdog_disarm(void);

#endif /* WATCHDOG_H */
