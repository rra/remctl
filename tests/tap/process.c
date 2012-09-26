/*
 * Utility functions for tests that use subprocesses.
 *
 * Provides utility functions for subprocess manipulation.  Specifically,
 * provides a function, run_setup, which runs a command and bails if it fails,
 * using its error message as the bail output, and is_function_output, which
 * runs a function in a subprocess and checks its output and exit status
 * against expected values.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2002, 2004, 2005 Russ Allbery <rra@stanford.edu>
 * Copyright 2009, 2010, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <config.h>
#include <portable/system.h>

#include <sys/wait.h>

#include <tests/tap/basic.h>
#include <tests/tap/process.h>
#include <tests/tap/string.h>


/*
 * Given a function, an expected exit status, and expected output, runs that
 * function in a subprocess, capturing stdout and stderr via a pipe, and
 * returns the function output in newly allocated memory.  Also captures the
 * process exit status.
 */
static void
run_child_function(test_function_type function, void *data, int *status,
                   char **output)
{
    int fds[2];
    pid_t child;
    char *buf;
    ssize_t count, ret, buflen;
    int rval;

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
        (*function)(data);
        fflush(stdout);
        _exit(0);
    } else {
        /*
         * In the parent; close the extra file descriptor, read the output if
         * any, and then collect the exit status.
         */
        close(fds[1]);
        buflen = BUFSIZ;
        buf = bmalloc(buflen);
        count = 0;
        do {
            ret = read(fds[0], buf + count, buflen - count - 1);
            if (ret > 0)
                count += ret;
            if (count >= buflen - 1) {
                buflen += BUFSIZ;
                buf = brealloc(buf, buflen);
            }
        } while (ret > 0);
        buf[count < 0 ? 0 : count] = '\0';
        if (waitpid(child, &rval, 0) == (pid_t) -1)
            sysbail("waitpid failed");
        close(fds[0]);
    }

    /* Store the output and return. */
    *status = rval;
    *output = buf;
}


/*
 * Given a function, data to pass to that function, an expected exit status,
 * and expected output, runs that function in a subprocess, capturing stdout
 * and stderr via a pipe, and compare the combination of stdout and stderr
 * with the expected output and the exit status with the expected status.
 * Expects the function to always exit (not die from a signal).
 */
void
is_function_output(test_function_type function, void *data, int status,
                   const char *output, const char *format, ...)
{
    char *buf, *msg;
    int rval;
    va_list args;

    run_child_function(function, data, &rval, &buf);

    /* Now, check the results against what we expected. */
    va_start(args, format);
    bvasprintf(&msg, format, args);
    va_end(args);
    ok(WIFEXITED(rval), "%s (exited)", msg);
    is_int(status, WEXITSTATUS(rval), "%s (status)", msg);
    is_string(output, buf, "%s (output)", msg);
    free(buf);
    free(msg);
}


/*
 * A helper function for run_setup.  This is a function to run an external
 * command, suitable for passing into run_child_function.  The expected
 * argument must be an argv array, with argv[0] being the command to run.
 */
static void
exec_command(void *data)
{
    char *const *argv = data;

    execvp(argv[0], argv);
}


/*
 * Given a command expressed as an argv struct, with argv[0] the name or path
 * to the command, run that command.  If it exits with a non-zero status, use
 * the part of its output up to the first newline as the error message when
 * calling bail.
 */
void
run_setup(const char *const argv[])
{
    char *output, *p;
    int status;

    run_child_function(exec_command, (void *) argv, &status, &output);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        p = strchr(output, '\n');
        if (p != NULL)
            *p = '\0';
        bail("%s", output);
    }
    free(output);
}
