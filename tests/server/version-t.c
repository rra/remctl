/*
 * Test suite for version negotiation in the server.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/gssapi.h>

#include <signal.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/gss-tokens.h>
#include <util/protocol.h>

/* A command token to run test test. */
static const char token[] = {
    9, 1, 0, 0,
    0, 0, 0, 2,
    0, 0, 0, 4, 't', 'e', 's', 't',
    0, 0, 0, 4, 't', 'e', 's', 't'
};


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    OM_uint32 major, minor;
    int flags, status;
    gss_buffer_desc tok;
    struct sigaction sa;

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(10);

    /* Ignore SIGPIPE signals so that we get errors from write. */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) < 0)
        sysbail("cannot set SIGPIPE handler");

    /* Open the connection to the site. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");

    /* Send the command token. */
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
    is_int(3, tok.length, "token had correct length");
    is_int(2, ((char *) tok.value)[0], "protocol version is 2");
    is_int(MESSAGE_VERSION, ((char *) tok.value)[1], "message version code");
    is_int(3, ((char *) tok.value)[2], "highest supported version is 3");

    /*
     * Send the token again and get another response to ensure that the server
     * hadn't closed the connection.
     */
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &tok, 0, &major, &minor);
    is_int(TOKEN_OK, status, "connection is still open");
    if (status == TOKEN_OK) {
        status = token_recv_priv(r->fd, r->context, &flags, &tok, 1024 * 64,
                                 0, &major, &minor);
        is_int(TOKEN_OK, status, "received token correctly");
    } else {
        ok(false, "unable to get reply to token");
    }

    /* Close things out. */
    remctl_close(r);
    return 0;
}
