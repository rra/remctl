/*
 * Test suite for no-op messages in the server.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2011, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/gssapi.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/gss-tokens.h>
#include <util/protocol.h>

/* The no-op token. */
static const char token[] = { 3, 7 };


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    OM_uint32 major, minor;
    int flags, status;
    gss_buffer_desc tok;

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(7);

    /* Open the connection to the site. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");

    /* Send the no-op token. */
    tok.length = sizeof(token);
    tok.value = (char *) token;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &tok, 0, &major, &minor);
    if (status != TOKEN_OK)
        bail("cannot send token");

    /* Accept the remote token. */
    status = token_recv_priv(r->fd, r->context, &flags, &tok, 1024 * 64, 0,
                             &major, &minor);
    is_int(TOKEN_OK, status, "received token correctly");
    is_int(TOKEN_DATA | TOKEN_PROTOCOL, flags, "token had correct flags");
    is_int(2, tok.length, "token had correct length");
    is_int(3, ((char *) tok.value)[0], "protocol version is 3");
    is_int(MESSAGE_NOOP, ((char *) tok.value)[1], "no-op message");

    /* Close things out. */
    remctl_close(r);
    return 0;
}
