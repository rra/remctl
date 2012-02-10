/*
 * Utility functions for tests that use remctl.
 *
 * Provides functions to start and stop a remctl daemon that uses the test
 * Kerberos environment and runs on port 14373 instead of the default 4373.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2011, 2012
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

#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/concat.h>
#include <util/xmalloc.h>


/*
 * Start remctld.  Takes the path to remctld, the Kerberos test configuration
 * (the keytab principal is used as the server principal), the path to the
 * configuration file to use, and then any additional arguments to pass to
 * remctld, ending with a NULL.  Writes the PID file to tests/data/remctl.pid
 * in the BUILD directory and returns the PID file.  If anything fails, calls
 * bail.
 *
 * If VALGRIND is set in the environment, starts remctld under the program
 * given in that environment variable, assuming valgrind arguments.
 */
pid_t
remctld_start(const char *remctld, struct kerberos_config *krbconf,
              const char *config, ...)
{
    char *pidfile;
    pid_t child;
    struct timeval tv;
    size_t n, i;
    va_list args;
    const char *arg, **argv;
    size_t length;

    pidfile = concatpath(getenv("BUILD"), "data/remctld.pid");
    if (access(pidfile, F_OK) == 0)
        if (unlink(pidfile) != 0)
            sysbail("cannot delete %s", pidfile);
    length = 11;
    if (getenv("VALGRIND") != NULL)
        length += 3;
    va_start(args, config);
    while ((arg = va_arg(args, const char *)) != NULL)
        length++;
    va_end(args);
    argv = xmalloc(length * sizeof(const char *));
    i = 0;
    if (getenv("VALGRIND") != NULL) {
        argv[i++] = "valgrind";
        argv[i++] = "--log-file=valgrind.%p";
        argv[i++] = "--leak-check=full";
    }
    argv[i++] = remctld;
    argv[i++] = "-mdSF";
    argv[i++] = "-p";
    argv[i++] = "14373";
    argv[i++] = "-s";
    argv[i++] = krbconf->keytab_principal;
    argv[i++] = "-P";
    argv[i++] = pidfile;
    argv[i++] = "-f";
    argv[i++] = config;
    va_start(args, config);
    while ((arg = va_arg(args, const char *)) != NULL)
        argv[i++] = arg;
    va_end(args);
    argv[i] = NULL;
    child = fork();
    if (child < 0)
        sysbail("fork failed");
    else if (child == 0) {
        if (getenv("VALGRIND") != NULL)
            execv(getenv("VALGRIND"), (char * const *) argv);
        else
            execv(remctld, (char * const *) argv);
        _exit(1);
    } else {
        for (n = 0; n < 1000 && access(pidfile, F_OK) != 0; n++) {
            tv.tv_sec = (getenv("VALGRIND") != NULL) ? 1 : 0;
            tv.tv_usec = 10000;
            select(0, NULL, NULL, NULL, &tv);
        }
        if (access(pidfile, F_OK) != 0) {
            kill(child, SIGTERM);
            waitpid(child, NULL, 0);
            bail("cannot start remctld");
        }
        free(pidfile);
        return child;
    }
}


/*
 * Stop remctld.  Takes the PID file of the remctld process.
 */
void
remctld_stop(pid_t child)
{
    char *pidfile;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    select(0, NULL, NULL, NULL, &tv);
    if (waitpid(child, NULL, WNOHANG) == 0) {
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
    }
    pidfile = concatpath(getenv("BUILD"), "data/remctld.pid");
    unlink(pidfile);
    free(pidfile);
}
