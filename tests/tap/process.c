/*
 * Utility functions for tests that use subprocesses.
 *
 * Provides utility functions for subprocess manipulation.  Currently, only
 * one utility function is provided: is_function_output, which runs a function
 * in a subprocess and checks its output and exit status against expected
 * values.
 *
 * Copyright 2009, 2010 Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <sys/wait.h>

#include <tests/tap/basic.h>
#include <tests/tap/process.h>
#include <util/xmalloc.h>


/*
 * Given a function, an expected exit status, and expected output, runs that
 * function in a subprocess, capturing stdout and stderr via a pipe, and
 * compare the combination of stdout and stderr with the expected output and
 * the exit status with the expected status.  Expects the function to always
 * exit (not die from a signal).
 */
void
is_function_output(test_function_type function, int status, const char *output,
                   const char *format, ...)
{
    int fds[2];
    pid_t child;
    char *buf, *msg;
    ssize_t count, ret, buflen;
    int rval;
    va_list args;

    /* Flush stdout before we start to avoid odd forking issues. */
    fflush(stdout);

    /* Set up the pipe and call the function, collecting its output. */
    if (pipe(fds) == -1)
        sysbail("can't create pipe");
    child = fork();
    if (child == (pid_t) -1) {
        sysbail("can't fork");
    } else if (child == 0) {
        /* In child.  Set up our stdout and stderr. */
        close(fds[0]);
        if (dup2(fds[1], 1) == -1)
            _exit(255);
        if (dup2(fds[1], 2) == -1)
            _exit(255);

        /* Now, run the function and exit successfully if it returns. */
        (*function)();
        fflush(stdout);
        _exit(0);
    } else {
        /*
         * In the parent; close the extra file descriptor, read the output if
         * any, and then collect the exit status.
         */
        close(fds[1]);
        buflen = BUFSIZ;
        buf = xmalloc(buflen);
        count = 0;
        do {
            ret = read(fds[0], buf + count, buflen - count - 1);
            if (ret > 0)
                count += ret;
            if (count >= buflen - 1) {
                buflen += BUFSIZ;
                buf = xrealloc(buf, buflen);
            }
        } while (ret > 0);
        buf[count < 0 ? 0 : count] = '\0';
        if (waitpid(child, &rval, 0) == (pid_t) -1)
            sysbail("waitpid failed");
    }

    /* Now, check the results against what we expected. */
    va_start(args, format);
    if (xvasprintf(&msg, format, args) < 0)
        bail("cannot format test description");
    va_end(args);
    ok(WIFEXITED(rval), "%s (exited)", msg);
    is_int(status, WEXITSTATUS(rval), "%s (status)", msg);
    is_string(output, buf, "%s (output)", msg);
    free(buf);
    free(msg);
}
