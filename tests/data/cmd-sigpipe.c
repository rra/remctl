/*
 * Small C program to verify that we get the default SIGPIPE behavior rather
 * than ignoring the signal.  This ensures that the server resets SIGPIPE
 * before running a command.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

int
main(void)
{
    int fds[2];

    if (pipe(fds) < 0) {
        fprintf(stderr, "cannot create pipes\n");
        exit(1);
    }
    close(fds[0]);
    if (write(fds[1], "data", 4) < 0)
        fprintf(stderr, "write failed\n");

    /*
     * The previous call should cause a SIGPIPE signal and we should never
     * reach this code.
     */
    printf("no SIGPIPE seen");
    exit(0);
}
