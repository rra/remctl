/*
 * Test suite for setting a specific Kerberos credential cache.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/concat.h>
#include <util/xmalloc.h>


int
main(void)
{
    const char *principal;
    char *config, *path;
    const char *cache;
    pid_t remctld;
    struct remctl *r;
    struct remctl_output *output;
    int status;
    const char *command[] = { "test", "test", NULL };

    if (chdir(getenv("SOURCE")) < 0)
        bail("can't chdir to SOURCE");
    principal = kerberos_setup();
    if (principal == NULL)
        skip_all("Kerberos tests not configured");
    plan(12);
    config = concatpath(getenv("SOURCE"), "data/conf-simple");
    path = concatpath(getenv("BUILD"), "../server/remctld");
    remctld = remctld_start(path, principal, config, NULL);

    /* Get the current ticket cache and then change KRB5CCNAME. */
    cache = getenv("KRB5CCNAME");
    if (cache == NULL)
        bail("failed to set KRB5CCNAME");
    putenv((char *) "KRB5CCNAME=./nonexistent-file");

    /* Connecting without setting the ticket cache should fail. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(!remctl_open(r, "127.0.0.1", 14373, principal),
       "remctl_open to 127.0.0.1");

    /* Set the ticket cache and connect to 127.0.0.1 and run a command. */
    status = remctl_set_ccache(r, cache);
    if (!status) {
        is_string("setting credential cache not supported", remctl_error(r),
                  "remctl_set_ccache failed with correct error");
        skip_block(8, "credential cache setting not supported");
    } else {
        ok(remctl_set_ccache(r, cache), "remctl_set_ccache");
        ok(remctl_open(r, "127.0.0.1", 14373, principal),
           "remctl_open to 127.0.0.1");
        ok(remctl_command(r, command), "remctl_command");
        output = remctl_output(r);
        ok(output != NULL, "remctl_output #1");
        if (output == NULL)
            ok_block(3, 0, "remctl_output failed");
        else {
            is_int(REMCTL_OUT_OUTPUT, output->type, "output token");
            ok(memcmp("hello world\n", output->data,
                      strlen("hello world\n")) == 0,
               "correct output");
            is_int(strlen("hello world\n"), output->length, "correct length");
        }
        output = remctl_output(r);
        ok(output != NULL, "remctl_output #2");
        if (output == NULL)
            ok_block(2, 0, "remctl_output failed");
        else {
            is_int(REMCTL_OUT_STATUS, output->type, "status token");
            is_int(0, output->status, "status is correct");
        }
    }
    remctl_close(r);

    /* Clean up. */
    remctld_stop(remctld);
    return 0;
}
