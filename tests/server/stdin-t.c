/*
 * Test suite for the server passing data to programs on standard input.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018-2019 Russ Allbery <eagle@eyrie.org>
 * Copyright 2009-2010, 2012-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>


/*
 * Run a stdin test case.  Takes the principal to use for the connection, the
 * first argument to the stdin program, and the data to send and ensures that
 * the client returns "Okay".
 */
static void
test_stdin(const char *principal, const char *test, const void *data,
           size_t length)
{
    struct remctl *r;
    struct iovec *command;
    struct remctl_output *output;

    command = bcalloc(4, sizeof(struct iovec));
    command[0].iov_base = (char *) "test";
    command[0].iov_len = strlen("test");
    command[1].iov_base = (char *) "stdin";
    command[1].iov_len = strlen("stdin");
    command[2].iov_base = (char *) test;
    command[2].iov_len = strlen(test);
    command[3].iov_base = (void *) data;
    command[3].iov_len = length;
    r = remctl_new();
    if (r == NULL)
        bail("cannot create remctl client");
    if (!remctl_open(r, "localhost", 14373, principal))
        bail("can't connect: %s", remctl_error(r));
    ok(remctl_commandv(r, command, 4), "sent command for %s", test);
    output = remctl_output(r);
    ok(output != NULL, "first output token is not null");
    if (output == NULL)
        ok_block(4, false, "first output token is not null");
    else {
        diag("status: %d", output->status);
        is_int(REMCTL_OUT_OUTPUT, output->type, "...and is right type");
        is_int(strlen("Okay"), output->length, "...and is right length");
        if (output->data == NULL)
            ok(0, "...and is right data");
        else {
            diag("data: %.*s", (int) output->length, output->data);
            ok(memcmp("Okay", output->data, 4) == 0, "...and is right data");
        }
        is_int(1, output->stream, "...and is right stream");
    }
    output = remctl_output(r);
    ok(output != NULL, "second output token is not null");
    if (output == NULL)
        ok_block(2, false, "second output token is not null");
    else {
        is_int(REMCTL_OUT_STATUS, output->type, "...and is right type");
        is_int(0, output->status, "...and is right status");
    }
    remctl_close(r);
    free(command);
}


int
main(void)
{
    struct kerberos_config *config;
    char *buffer;

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(9 * 9);

    /* Run the tests. */
    test_stdin(config->principal, "read", "Okay", 4);
    test_stdin(config->principal, "write", "Okay", 4);
    test_stdin(config->principal, "exit", "Okay", 4);
    buffer = bmalloc(1024 * 1024);
    memset(buffer, 'A', 1024 * 1024);
    test_stdin(config->principal, "exit", buffer, 1024 * 1024);
    test_stdin(config->principal, "close", "Okay", 4);
    test_stdin(config->principal, "close", buffer, 1024 * 1024);
    test_stdin(config->principal, "nuls", "T\0e\0s\0t\0", 8);
    test_stdin(config->principal, "large", buffer, 1024 * 1024);
    test_stdin(config->principal, "delay", buffer, 1024 * 1024);
    free(buffer);

    return 0;
}
