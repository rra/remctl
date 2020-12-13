/*
 * Test suite for setting a source IP for the client.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/messages.h>
#include <tests/tap/remctl.h>


int
main(void)
{
    struct kerberos_config *config;
    const char *err;
    struct remctl *r;
    struct remctl_output *output;
    const char *command[] = {"test", "env", "REMOTE_ADDR", NULL};

    /* Set up Kerberos and remctld. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", (char *) 0);

    plan(10);

    /* Successful connection to 127.0.0.1. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_set_source_ip(r, "127.0.0.1"), "remctl_set_source_ip");
    ok(remctl_open(r, "127.0.0.1", 14373, config->principal),
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
    ok(!remctl_open(r, "::1", 14373, config->principal), "remctl_open to ::1");
    err = remctl_error(r);
    diag("error: %s", err);
    ok(((strncmp("cannot connect to ::1 (port 14373): ", err,
                 strlen("cannot connect to ::1 (port 14373): "))
         == 0)
        || (strncmp("unknown host ::1: ", err, strlen("unknown host ::1: "))
            == 0)),
       "failed with correct error");

    remctl_close(r);
    return 0;
}
