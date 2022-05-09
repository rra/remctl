/*
 * Test suite for over-large commands.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2007, 2009-2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <sys/uio.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/protocol.h>


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    struct remctl_output *output;
    struct iovec command[7];

    /* Set up Kerberos and remctld. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", (char *) 0);

    plan(6);

    command[0].iov_len = strlen("test");
    command[0].iov_base = (char *) "test";
    command[1].iov_len = strlen("noauth");
    command[1].iov_base = (char *) "noauth";
    command[2].iov_len = TOKEN_MAX_DATA - 31;
    command[2].iov_base = bmalloc(command[2].iov_len);
    memset(command[2].iov_base, 'A', command[2].iov_len);
    command[3].iov_len = TOKEN_MAX_DATA;
    command[3].iov_base = bmalloc(command[3].iov_len);
    memset(command[3].iov_base, 'B', command[3].iov_len);
    command[4].iov_len = TOKEN_MAX_DATA - 20;
    command[4].iov_base = bmalloc(command[4].iov_len);
    memset(command[4].iov_base, 'C', command[4].iov_len);
    command[5].iov_len = 1;
    command[5].iov_base = (char *) "D";
    command[6].iov_len = 0;
    command[6].iov_base = (char *) "";

    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");
    ok(remctl_commandv(r, command, 7), "sending extra large command");
    output = remctl_output(r);
    printf("\n");
    ok(output != NULL, "...and got a response");
    if (output != NULL) {
        is_int(REMCTL_OUT_ERROR, output->type, "...with the right type");
        is_int(ERROR_ACCESS, output->error, "...and the right error");
    } else {
        ok(0, "...with the right type");
        ok(0, "...and the right error");
    }
    remctl_close(r);

    free(command[2].iov_base);
    free(command[3].iov_base);
    free(command[4].iov_base);
    return 0;
}
