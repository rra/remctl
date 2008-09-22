/*
 * asprintf and vasprintf test suite.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2008 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <tests/libtest.h>

int test_asprintf(char **, const char *, ...);
int test_vasprintf(char **, const char *, va_list);

static int
vatest(char **result, const char *format, ...)
{
    va_list args;
    int status;

    va_start(args, format);
    status = test_vasprintf(result, format, args);
    va_end(args);
    return status;
}

int
main(void)
{
    char *result = NULL;

    test_init(12);

    ok_int(1, 7, test_asprintf(&result, "%s", "testing"));
    ok_string(2, "testing", result);
    free(result);
    ok(3, 1);
    ok_int(4, 0, test_asprintf(&result, "%s", ""));
    ok_string(5, "", result);
    free(result);
    ok(6, 1);

    ok_int(7, 6, vatest(&result, "%d %s", 2, "test"));
    ok_string(8, "2 test", result);
    free(result);
    ok(9, 1);
    ok_int(10, 0, vatest(&result, "%s", ""));
    ok_string(11, "", result);
    free(result);
    ok(12, 1);

    return 0;
}
