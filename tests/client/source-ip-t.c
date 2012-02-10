/*
 * Test suite for setting a source IP for the client.
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
#include <tests/tap/kinit.h>
#include <tests/tap/messages.h>
#include <tests/tap/remctl.h>
#include <util/concat.h>
#include <util/xmalloc.h>


int
main(void)
{
    const char *principal;
    char *config, *path;
    const char *err;
    pid_t remctld;
    struct remctl *r;
    struct remctl_output *output;
    const char *command[] = { "test", "env", "REMOTE_ADDR", NULL };

    if (chdir(getenv("SOURCE")) < 0)
        bail("can't chdir to SOURCE");
    principal = kerberos_setup();
    if (principal == NULL)
        skip_all("Kerberos tests not configured");
    plan(10);
    config = concatpath(getenv("SOURCE"), "data/conf-simple");
    path = concatpath(getenv("BUILD"), "../server/remctld");
    remctld = remctld_start(path, principal, config, NULL);

    /* Successful connection to 127.0.0.1. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_set_source_ip(r, "127.0.0.1"), "remctl_set_source_ip");
    ok(remctl_open(r, "127.0.0.1", 14373, principal),
       "remctl_open to 127.0.0.1");
    ok(remctl_command(r, command), "remctl_command");
    output = remctl_output(r);
    ok(output != NULL, "remctl_output");
    if (output == NULL)
        ok_block(3, 0, "remctl_output failed");
    else {
        is_int(REMCTL_OUT_OUTPUT, output->type, "output token");
        ok(memcmp("127.0.0.1\n", output->data, strlen("127.0.0.1\n")) == 0,
           "correct IP address");
        is_int(strlen("127.0.0.1\n"), output->length, "correct length");
    }

    /* Failed connection to ::1. */
    ok(!remctl_open(r, "::1", 14373, principal), "remctl_open to ::1");
    err = remctl_error(r);
    diag("error: %s", err);
    ok(((strncmp("cannot connect to ::1 (port 14373): ", err,
                strlen("cannot connect to ::1 (port 14373): ")) == 0)
        || (strncmp("unknown host ::1: ", err,
                    strlen("unknown host ::1: ")) == 0)),
       "failed with correct error");

    /* Clean up. */
    remctld_stop(remctld);
    return 0;
}
