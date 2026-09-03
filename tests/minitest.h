/* A ~60-line test harness, so the test suite needs no external framework:
 * `make test` works with nothing but a C compiler. */
#ifndef MINITEST_H
#define MINITEST_H

#include <stdio.h>
#include <string.h>

static int mt_tests_run = 0;
static int mt_tests_failed = 0;
static int mt_current_failed = 0;
static const char *mt_current_name = "";

#define TEST(name) static void name(void)

#define RUN(fn)                                                    \
    do {                                                           \
        mt_current_name = #fn;                                     \
        mt_current_failed = 0;                                     \
        mt_tests_run++;                                            \
        fn();                                                      \
        if (mt_current_failed) {                                   \
            mt_tests_failed++;                                     \
            printf("FAIL  %s\n", #fn);                             \
        } else {                                                   \
            printf("ok    %s\n", #fn);                             \
        }                                                          \
    } while (0)

#define MT_FAIL(fmt, ...)                                          \
    do {                                                           \
        mt_current_failed = 1;                                     \
        printf("      %s:%d: " fmt "\n", __FILE__, __LINE__, __VA_ARGS__); \
    } while (0)

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) MT_FAIL("expected: %s", #cond);               \
    } while (0)

#define CHECK_INT(actual, expected)                                \
    do {                                                           \
        long long a_ = (long long)(actual), e_ = (long long)(expected); \
        if (a_ != e_)                                              \
            MT_FAIL("%s == %lld, expected %lld", #actual, a_, e_);  \
    } while (0)

#define CHECK_STR(actual, expected)                                \
    do {                                                           \
        const char *a_ = (actual), *e_ = (expected);               \
        if (strcmp(a_, e_) != 0)                                   \
            MT_FAIL("%s == \"%s\", expected \"%s\"", #actual, a_, e_); \
    } while (0)

#define CHECK_SUBSTR(haystack, needle)                             \
    do {                                                           \
        const char *h_ = (haystack), *n_ = (needle);               \
        if (strstr(h_, n_) == NULL)                                \
            MT_FAIL("\"%s\" does not contain \"%s\"", h_, n_);     \
    } while (0)

static int mt_report(void)
{
    printf("\n%d test%s, %d failed\n",
           mt_tests_run, mt_tests_run == 1 ? "" : "s", mt_tests_failed);
    return mt_tests_failed == 0 ? 0 : 1;
}

#endif /* MINITEST_H */
