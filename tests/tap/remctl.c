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
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/wait.h>

#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <tests/tap/string.h>

/* May be defined by the build system. */
#ifndef PATH_FAKEROOT
# define PATH_FAKEROOT ""
#endif
#ifndef PATH_REMCTLD
# define PATH_REMCTLD ""
#endif

/*
 * The PID of the running remctld process and the temporary directory used to
 * store the PID file, stored in static variables so that we can clean up in
 * an atexit handler.  If is_child is false, we can't use waitpid to tell if
 * the child is still running and will just have to kill and hope.
 */
static pid_t remctld = 0;
static char *tmpdir_pid = NULL;
static bool is_child = true;


/*
 * Stop remctld.  Normally called via an atexit handler.  We give remctld at
 * most five seconds to exit before we commit suicide with an alarm.
 */
void
remctld_stop(void)
{
    char *pidfile;

    diag("remctld_stop called with pid %lu", (unsigned long) remctld);
    if (remctld == 0)
        return;
    if (!is_child)
        kill(remctld, SIGTERM);
    else {
        alarm(5);
        if (waitpid(remctld, NULL, WNOHANG) == 0) {
            kill(remctld, SIGTERM);
            waitpid(remctld, NULL, 0);
        }
        alarm(0);
    }
    remctld = 0;
    basprintf(&pidfile, "%s/remctld.pid", tmpdir_pid);
    unlink(pidfile);
    free(pidfile);
    test_tmpdir_free(tmpdir_pid);
    tmpdir_pid = NULL;
}


/*
 * Read the PID of remctld from a file.  This is necessary when running under
 * fakeroot or valgrind to get the actual PID of the remctld process.
 */
static long
read_pidfile(const char *path)
{
    FILE *file;
    char buffer[BUFSIZ];
    long pid;

    file = fopen(path, "r");
    if (file == NULL)
        sysbail("cannot open %s", path);
    if (fgets(buffer, sizeof(buffer), file) == NULL)
        sysbail("cannot read from %s", path);
    fclose(file);
    pid = strtol(buffer, NULL, 10);
    if (pid == 0)
        bail("cannot read PID from %s", path);
    return pid;
}


/*
 * Internal helper function for remctld_start and remctld_start_fakeroot.
 *
 * Takes the Kerberos test configuration (the keytab principal is used as the
 * server principal), the configuration file to use (found via
 * test_file_path), and then any additional arguments to pass to remctld,
 * ending with a NULL.  Returns the PID of the running remctld process.  If
 * anything fails, calls bail.
 *
 * If VALGRIND is set in the environment, starts remctld under the program
 * given in that environment variable, assuming valgrind arguments.
 *
 * The path to remctld is obtained from the PATH_REMCTLD #define.  If this is
 * not set, remctld_start_internal calls skip_all.
 *
 * If the last argument is true, remctld is started under fakeroot.  If
 * PATH_FAKEROOT is not defined, remctld_start_internal calls skip_all.
 */
static pid_t
remctld_start_internal(struct kerberos_config *krbconf, const char *config,
                       va_list args, bool fakeroot)
{
    va_list args_copy;
    char *pidfile, *confpath;
    struct timeval tv;
    size_t n, i;
    const char *arg, **argv;
    size_t length;
    const char *path_fakeroot = PATH_FAKEROOT;
    const char *path_remctld = PATH_REMCTLD;

    /* Check prerequisites. */
    if (path_remctld[0] == '\0')
        skip_all("remctld not found");
    if (fakeroot && path_fakeroot[0] == '\0')
        skip_all("fakeroot not found");

    /* Ensure that we're not already running a remctld. */
    if (remctld != 0)
        bail("remctld already running (PID %lu)", (unsigned long) remctld);

    /* Set up the arguments and run remctld. */
    tmpdir_pid = test_tmpdir();
    basprintf(&pidfile, "%s/remctld.pid", tmpdir_pid);
    if (access(pidfile, F_OK) == 0)
        if (unlink(pidfile) != 0)
            sysbail("cannot delete %s", pidfile);
    confpath = test_file_path(config);
    if (confpath == NULL)
        bail("cannot find remctld config %s", config);
    length = 11;
    if (getenv("VALGRIND") != NULL)
        length += 3;
    if (fakeroot)
        length += 2;
    va_copy(args_copy, args);
    while ((arg = va_arg(args_copy, const char *)) != NULL)
        length++;
    va_end(args_copy);
    argv = bmalloc(length * sizeof(const char *));
    i = 0;
    if (getenv("VALGRIND") != NULL) {
        argv[i++] = "valgrind";
        argv[i++] = "--log-file=valgrind.%p";
        argv[i++] = "--leak-check=full";
    }
    if (fakeroot) {
        argv[i++] = path_fakeroot;
        argv[i++] = "--";
    }
    argv[i++] = path_remctld;
    argv[i++] = "-mdSF";
    argv[i++] = "-p";
    argv[i++] = "14373";
    argv[i++] = "-s";
    argv[i++] = krbconf->principal;
    argv[i++] = "-P";
    argv[i++] = pidfile;
    argv[i++] = "-f";
    argv[i++] = confpath;
    while ((arg = va_arg(args, const char *)) != NULL)
        argv[i++] = arg;
    argv[i] = NULL;
    remctld = fork();
    if (remctld < 0)
        sysbail("fork failed");
    else if (remctld == 0) {
        if (getenv("VALGRIND") != NULL)
            execv(getenv("VALGRIND"), (char * const *) argv);
        else if (fakeroot)
            execv(path_fakeroot, (char * const *) argv);
        else
            execv(path_remctld, (char * const *) argv);
        sysbail("exec failed");
    } else {
        free(argv);
        test_file_path_free(confpath);
        for (n = 0; n < 100 && access(pidfile, F_OK) != 0; n++) {
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            select(0, NULL, NULL, NULL, &tv);
        }
        if (access(pidfile, F_OK) != 0) {
            kill(remctld, SIGTERM);
            alarm(5);
            waitpid(remctld, NULL, 0);
            alarm(0);
            bail("cannot start remctld");
        }

        /*
         * When running under fakeroot, there may be internal forks that
         * change the PID of the final running process, and when running under
         * valgrind, we want to grab the final PID.
         */
        if (fakeroot || getenv("VALGRIND") != NULL) {
            remctld = read_pidfile(pidfile);
            is_child = false;
        }
        free(pidfile);
        if (atexit(remctld_stop) != 0)
            sysdiag("cannot register cleanup function");
        return remctld;
    }
}


/*
 * Just calls remctld_start_internal without enabling fakeroot support.
 */
pid_t
remctld_start(struct kerberos_config *krbconf, const char *config, ...)
{
    va_list args;
    pid_t child;

    va_start(args, config);
    child = remctld_start_internal(krbconf, config, args, false);
    va_end(args);
    return child;
}


/*
 * Just calls remctld_start_internal with fakeroot enabled.
 */
pid_t
remctld_start_fakeroot(struct kerberos_config *krbconf, const char *config,
                       ...)
{
    va_list args;
    pid_t child;

    va_start(args, config);
    child = remctld_start_internal(krbconf, config, args, true);
    va_end(args);
    return child;
}
