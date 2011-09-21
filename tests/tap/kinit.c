/*
 * Utility functions for tests that use Kerberos, without using the libraries.
 *
 * This is an alternate implementation of tap/kerberos.c that does the same
 * setup but without using the Kerberos libraries.  It's suitable for packages
 * that need Kerberos tickets but only use GSS-API or some other interface and
 * therefore don't have Kerberos portability functions or library probes.
 *
 * Currently only provides kerberos_setup(), which assumes a particular set of
 * data files in either the SOURCE or BUILD directories and, using those,
 * obtains Kerberos credentials, sets up a ticket cache, and sets the
 * environment variable pointing to the Kerberos keytab to use for testing.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2010, 2011
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
#include <util/concat.h>
#include <util/xmalloc.h>

/*
 * Disable the requirement that format strings be literals, since it's easier
 * to handle the possible patterns for kinit commands as an array.
 */
#pragma GCC diagnostic ignored "-Wformat-nonliteral"


/*
 * These variables hold the allocated strings for the principal and the
 * environment to point to a different Kerberos ticket cache and keytab.  We
 * store them so that we can free them on exit for cleaner valgrind output,
 * making it easier to find real memory leaks in the tested programs.
 */
static char *principal = NULL;
static char *krb5ccname = NULL;
static char *krb5_ktname = NULL;


/*
 * Clean up at the end of a test.  This removes the ticket cache and resets
 * and frees the memory allocated for the environment variables so that
 * valgrind output on test suites is cleaner.
 */
void
kerberos_cleanup(void)
{
    char *path;

    path = concatpath(getenv("BUILD"), "data/test.cache");
    unlink(path);
    free(path);
    if (principal != NULL) {
        free(principal);
        principal = NULL;
    }
    putenv((char *) "KRB5CCNAME=");
    putenv((char *) "KRB5_KTNAME=");
    if (krb5ccname != NULL) {
        free(krb5ccname);
        krb5ccname = NULL;
    }
    if (krb5_ktname != NULL) {
        free(krb5_ktname);
        krb5_ktname = NULL;
    }
}


/*
 * Obtain Kerberos tickets for the principal specified in test.principal using
 * the keytab specified in test.keytab, both of which are presumed to be in
 * tests/data in either the build or the source tree.
 *
 * Returns the contents of test.principal in newly allocated memory or NULL if
 * Kerberos tests are apparently not configured.  If Kerberos tests are
 * configured but something else fails, calls bail().
 */
const char *
kerberos_setup(void)
{
    static const char * const format[3] = {
        "kinit -k -t %s %s >/dev/null 2>&1 </dev/null",
        "kinit -t %s %s >/dev/null 2>&1 </dev/null",
        "kinit -k -K %s %s >/dev/null 2>&1 </dev/null"
    };
    FILE *file;
    char *path;
    const char *build;
    char buffer[BUFSIZ], *command;
    size_t length, i;
    int status;

    /* Read the principal name and find the keytab file. */
    path = test_file_path("data/test.principal");
    if (path == NULL)
        return NULL;
    file = fopen(path, "r");
    if (file == NULL) {
        free(path);
        return NULL;
    }
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        bail("cannot read %s", path);
    }
    fclose(file);
    if (buffer[strlen(buffer) - 1] != '\n')
        bail("no newline in %s", path);
    test_file_path_free(path);
    buffer[strlen(buffer) - 1] = '\0';
    path = test_file_path("data/test.keytab");
    if (path == NULL)
        return NULL;

    /* Set the KRB5CCNAME and KRB5_KTNAME environment variables. */
    build = getenv("BUILD");
    if (build == NULL)
        build = ".";
    krb5ccname = concat("KRB5CCNAME=", build, "/data/test.cache", (char *) 0);
    krb5_ktname = concat("KRB5_KTNAME=", path, (char *) 0);
    putenv(krb5ccname);
    putenv(krb5_ktname);

    /* Now do the Kerberos initialization. */
    for (i = 0; i < ARRAY_SIZE(format); i++) {
        length = strlen(format[i]) + strlen(path) + strlen(buffer);
        command = xmalloc(length);
        snprintf(command, length, format[i], path, buffer);
        status = system(command);
        free(command);
        if (status != -1 && WEXITSTATUS(status) == 0)
            break;
    }
    if (status == -1 || WEXITSTATUS(status) != 0)
        return NULL;
    test_file_path_free(path);

    /*
     * Register the cleanup function as an atexit handler so that the caller
     * doesn't have to worry about cleanup.
     */
    if (atexit(kerberos_cleanup) != 0)
        sysdiag("cannot register cleanup function");

    /* Store the principal and return it. */
    principal = bstrdup(buffer);
    return principal;
}
