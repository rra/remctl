/*
 * Test suite for setting a specific Kerberos credential cache.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2011, 2012
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


int
main(void)
{
    struct kerberos_config *config;
    const char *cache;
    struct remctl *r;
    struct remctl_output *output;
    int status;
    const char *command[] = { "test", "test", NULL };

    /* Set up Kerberos and remctld. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", (char *) 0);

    plan(12);

    /* Get the current ticket cache and then change KRB5CCNAME. */
    cache = getenv("KRB5CCNAME");
    if (cache == NULL)
        bail("failed to set KRB5CCNAME");
    putenv((char *) "KRB5CCNAME=./nonexistent-file");

    /* Connecting without setting the ticket cache should fail. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(!remctl_open(r, "127.0.0.1", 14373, config->principal),
       "remctl_open to 127.0.0.1");

    /* Set the ticket cache and connect to 127.0.0.1 and run a command. */
    status = remctl_set_ccache(r, cache);
    if (!status) {
        is_string("setting credential cache not supported", remctl_error(r),
                  "remctl_set_ccache failed with correct error");
        skip_block(9, "credential cache setting not supported");
    } else {
        ok(remctl_set_ccache(r, cache), "remctl_set_ccache");
        ok(remctl_open(r, "127.0.0.1", 14373, config->principal),
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

    return 0;
}
