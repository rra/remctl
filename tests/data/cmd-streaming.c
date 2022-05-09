/*
 * Small C program to output three lines, one to stdout, one to stderr, and
 * then one to stdout again.  Used to test remctl streaming support.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#ifdef HAVE_SYS_SELECT_H
#    include <sys/select.h>
#endif
#include <sys/time.h>

int
main(void)
{
    struct timeval tv;

    fprintf(stdout, "This is the first line\n");
    fflush(stdout);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    select(0, NULL, NULL, NULL, &tv);
    fprintf(stderr, "This is the second line\n");
    fflush(stderr);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    select(0, NULL, NULL, NULL, &tv);
    fprintf(stdout, "This is the third line\n");

    return 0;
}
