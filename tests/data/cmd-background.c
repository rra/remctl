/*
 * Small C program that outputs a string, forks off a process that sleeps for
 * ten seconds and outputs another string, and meanwhile immediately exits.
 * Used to test that remctld stops listening as soon as its child has exited
 * and doesn't wait forever for output to be closed.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2007, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <tests/tap/basic.h>
#include <util/xmalloc.h>


int
main(void)
{
    pid_t pid;
    char *tmpdir, *path;
    FILE *pidfile;

    printf("Parent\n");
    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Cannot fork child\n");
        exit(1);
    } else if (pid == 0) {
        tmpdir = test_tmpdir();
        xasprintf(&path, "%s/cmd-background.pid", tmpdir);
        pidfile = fopen(path, "w");
        if (pidfile != NULL) {
            fprintf(pidfile, "%lu\n", (unsigned long) getpid());
            fclose(pidfile);
        }
        sleep(10);
        printf("Child\n");
        unlink(path);
        free(path);
        test_tmpdir_free(tmpdir);
        exit(0);
    }
    return 0;
}
