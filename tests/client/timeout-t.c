/*
 * Test suite for setting a timeout for the client.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
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
    struct remctl *r;
    struct remctl_output *output;
    const char *command[] = { "test", "sleep", NULL, NULL };

    /* Set up Kerberos and remctld. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", (char *) 0);

    plan(11);

    /*
     * Send the command with no arguments, which means we'll time out right
     * away without receiving a reply.
     */
    r = remctl_new();
    ok(remctl_set_timeout(r, 1), "set timeout");
    ok(remctl_open(r, "127.0.0.1", 14373, config->principal), "open");
    ok(remctl_command(r, command), "sent test sleep command");
    output = remctl_output(r);
    ok(output == NULL, "output is NULL");
    is_string("error receiving token: timed out", remctl_error(r),
              "correct error");

    /*
     * Send the command with an argument and ensure we can read one token and
     * then time out when waiting for the next token.  This also tests that we
     * close the socket and restart properly after a timeout error when
     * reusing the same remctl client struct.
     */
    command[2] = "hello";
    ok(remctl_command(r, command), "sent test sleep command with output");
    output = remctl_output(r);
    if (output == NULL)
        ok_block(3, 0, "output is NULL (%s)", remctl_error(r));
    else {
        is_int(REMCTL_OUT_OUTPUT, output->type, "got output token");
        ok(memcmp("hello\n", output->data, strlen("hello\n")) == 0,
           "  correct output");
        is_int(strlen("hello\n"), output->length, "  correct length");
    }
    output = remctl_output(r);
    ok(output == NULL, "second output token is NULL");
    is_string("error receiving token: timed out", remctl_error(r),
              "correct error");
    remctl_close(r);

    return 0;
}
