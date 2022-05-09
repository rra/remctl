/*
 * Test suite for continued commands.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018-2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2009-2010, 2012-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <signal.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/gss-tokens.h>
#include <util/protocol.h>


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    struct remctl_output *output;
    static const char prefix_first[] = {2, MESSAGE_COMMAND, 1, 1};
    static const char prefix_next[] = {2, MESSAGE_COMMAND, 1, 2};
    static const char prefix_last[] = {2, MESSAGE_COMMAND, 1, 3};
    /* clang-format off */
    static const char data[] = {
        0, 0, 0, 3,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's',
        0, 0, 0, 1, '2'
    };
    /* clang-format on */
    char buffer[BUFSIZ];
    gss_buffer_desc token;
    OM_uint32 major, minor;
    int status;

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(9);

    /* Open a connection. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    if (r == NULL)
        bail("remctl_new returned NULL");
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");

    /* Send the command broken in the middle of protocol elements. */
    memcpy(buffer, prefix_first, sizeof(prefix_first));
    memcpy(buffer + sizeof(prefix_first), data, 2);
    token.value = buffer;
    token.length = sizeof(prefix_first) + 2;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, 0, &major, &minor);
    is_int(TOKEN_OK, status, "first token sent okay");
    memcpy(buffer, prefix_next, sizeof(prefix_next));
    memcpy(buffer + sizeof(prefix_next), data + 2, 4);
    token.value = buffer;
    token.length = sizeof(prefix_next) + 4;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, 0, &major, &minor);
    is_int(TOKEN_OK, status, "second token sent okay");
    memcpy(buffer, prefix_next, sizeof(prefix_next));
    memcpy(buffer + sizeof(prefix_next), data + 6, 13);
    token.value = buffer;
    token.length = sizeof(prefix_next) + 13;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, 0, &major, &minor);
    is_int(TOKEN_OK, status, "third token sent okay");
    memcpy(buffer, prefix_last, sizeof(prefix_last));
    memcpy(buffer + sizeof(prefix_last), data + 19, sizeof(data) - 19);
    token.value = buffer;
    token.length = sizeof(prefix_next) + sizeof(data) - 19;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, 0, &major, &minor);
    is_int(TOKEN_OK, status, "fourth token sent okay");
    r->ready = 1;
    output = remctl_output(r);
    ok(output != NULL, "got output");
    if (output == NULL)
        ok_block(2, false, "got output");
    else {
        is_int(REMCTL_OUT_STATUS, output->type, "...of type status");
        is_int(2, output->status, "...with correct status");
    }
    remctl_close(r);

    return 0;
}
