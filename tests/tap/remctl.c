/*
 * Utility functions for tests that use remctl.
 *
 * Provides functions to start and stop a remctl daemon that uses the test
 * Kerberos environment and runs on port 14373 instead of the default 4373.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2007, 2009, 2011, 2012, 2013, 2014
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

#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/macros.h>
#include <tests/tap/process.h>
#include <tests/tap/remctl.h>
#include <tests/tap/string.h>

/* May be defined by the build system. */
#ifndef PATH_REMCTLD
# define PATH_REMCTLD ""
#endif


/*
 * Internal helper function for remctld_start and remctld_start_fakeroot.
 *
 * Takes the Kerberos test configuration (the keytab principal is used as the
 * server principal), the configuration file to use (found via
 * test_file_path), and then any additional arguments to pass to remctld,
 * ending with a NULL.  Returns the PID of the running remctld process.  If
 * anything fails, calls bail.
 *
 * The path to remctld is obtained from the PATH_REMCTLD #define.  If this is
 * not set, remctld_start_internal calls skip_all.
 */
static struct process *
remctld_start_internal(struct kerberos_config *krbconf, const char *config,
                       va_list args, bool fakeroot)
{
    va_list args_copy;
    char *tmpdir, *pidfile, *confpath;
    size_t i, length;
    const char *arg, **argv;
    struct process *process;
    const char *path_remctld = PATH_REMCTLD;

    /* Check prerequisites. */
    if (path_remctld[0] == '\0')
        skip_all("remctld not found");

    /* Determine the location of the PID file and remove any old ones. */
    tmpdir = test_tmpdir();
    basprintf(&pidfile, "%s/remctld.pid", tmpdir);
    if (access(pidfile, F_OK) == 0)
        bail("remctld may already be running: %s exists", pidfile);

    /* Build the argv used to run remctld. */
    confpath = test_file_path(config);
    if (confpath == NULL)
        bail("cannot find remctld config %s", config);
    length = 11;
    va_copy(args_copy, args);
    while ((arg = va_arg(args_copy, const char *)) != NULL)
        length++;
    va_end(args_copy);
    argv = bcalloc(length, sizeof(const char *));
    i = 0;
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

    /* Start remctld using process_start or process_start_fakeroot. */
    if (fakeroot)
        process = process_start_fakeroot(argv, pidfile);
    else
        process = process_start(argv, pidfile);

    /* Clean up and return. */
    test_file_path_free(confpath);
    free(pidfile);
    test_tmpdir_free(tmpdir);
    free(argv);
    return process;
}


/*
 * Just calls remctld_start_internal without enabling fakeroot support.
 */
struct process *
remctld_start(struct kerberos_config *krbconf, const char *config, ...)
{
    va_list args;
    struct process *process;

    va_start(args, config);
    process = remctld_start_internal(krbconf, config, args, false);
    va_end(args);
    return process;
}


/*
 * Just calls remctld_start_internal with fakeroot enabled.
 */
struct process *
remctld_start_fakeroot(struct kerberos_config *krbconf, const char *config,
                       ...)
{
    va_list args;
    struct process *process;

    va_start(args, config);
    process = remctld_start_internal(krbconf, config, args, true);
    va_end(args);
    return process;
}
