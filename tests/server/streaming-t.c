/*
 * Test suite for streaming data from the server.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2009, 2010, 2012, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <client/internal.h>
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
    const char *command[] = { "test", "streaming", NULL };

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(32);

    /* First, version 2. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");
    ok(remctl_command(r, command), "remctl_command");
    output = remctl_output(r);
    if (output == NULL)
        ok_block(5, false, "output is not null");
    else {
        ok(output != NULL, "output is not null");
        is_int(REMCTL_OUT_OUTPUT, output->type, "...and is correct type");
        is_int(23, output->length, "right length for first line");
        if (output->data == NULL)
            ok(0, "...right data for first line");
        else
            ok(memcmp("This is the first line\n", output->data, 23) == 0,
               "...right data for first line");
        is_int(1, output->stream, "...right stream");
    }
    output = remctl_output(r);
    if (output == NULL)
        ok_block(5, false, "second output is not null");
    else {
        ok(output != NULL, "second output is not null");
        is_int(REMCTL_OUT_OUTPUT, output->type, "...and is correct type");
        is_int(24, output->length, "right length for second line");
        if (output->data == NULL)
            ok(0, "...right data for second line");
        else
            ok(memcmp("This is the second line\n", output->data, 24) == 0,
               "...right data for second line");
        is_int(2, output->stream, "...right stream");
    }
    output = remctl_output(r);
    if (output == NULL)
        ok_block(5, false, "third output is not null");
    else {
        ok(output != NULL, "third output is not null");
        is_int(REMCTL_OUT_OUTPUT, output->type, "...and is correct type");
        is_int(23, output->length, "right length for third line");
        if (output->data == NULL)
            ok(0, "...right data for third line");
        else
            ok(memcmp("This is the third line\n", output->data, 23) == 0,
               "...right data for third line");
        is_int(1, output->stream, "...right stream");
    }
    output = remctl_output(r);
    if (output == NULL)
        ok_block(3, false, "status is not null");
    else {
        ok(output != NULL, "status is not null");
        is_int(REMCTL_OUT_STATUS, output->type, "...and is right type");
        is_int(0, output->status, "...and is right status");
    }
    remctl_close(r);

    /* Now, version 1. */
    r = remctl_new();
    ok(r != NULL, "remctl_new protocol version 1");
    if (r == NULL)
        bail("remctl_new returned NULL");
    r->protocol = 1;
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");
    ok(remctl_command(r, command), "remctl_command");
    output = remctl_output(r);
    if (output == NULL)
        ok_block(5, false, "output is not null");
    else {
        ok(output != NULL, "output is not null");
        is_int(REMCTL_OUT_OUTPUT, output->type, "...and is right type");
        is_int(70, output->length, "...and right length");
        if (output->data == NULL)
            ok(0, "...and right data");
        else
            ok(memcmp("This is the first line\nThis is the second line\n"
                      "This is the third line\n", output->data, 70) == 0,
               "...and right data");
        is_int(1, output->stream, "...and right stream");
    }
    output = remctl_output(r);
    if (output == NULL)
        ok_block(3, false, "status token is not null");
    else {
        ok(output != NULL, "status token is not null");
        is_int(REMCTL_OUT_STATUS, output->type, "...and is right type");
        is_int(0, output->status, "...and is right status");
    }
    remctl_close(r);

    return 0;
}
