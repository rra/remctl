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

#include <signal.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/concat.h>
#include <util/gss-tokens.h>
#include <util/protocol.h>

/* The no-op token. */
static const char token[] = { 3, 7 };


int
main(void)
{
    struct kerberos_config *krbconf;
    char *config, *path;
    struct remctl *r;
    pid_t remctld;
    OM_uint32 major, minor;
    int flags, status;
    gss_buffer_desc tok;

    /* Unless we have Kerberos available, we can't really do anything. */
    if (chdir(getenv("SOURCE")) < 0)
        bail("can't chdir to SOURCE");
    krbconf = kerberos_setup();
    if (krbconf->keytab_principal == NULL)
        skip_all("Kerberos tests not configured");
    config = concatpath(getenv("SOURCE"), "data/conf-simple");
    path = concatpath(getenv("BUILD"), "../server/remctld");
    remctld = remctld_start(path, krbconf, config, NULL);

    plan(7);

    /* Open the connection to the site. */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_open(r, "localhost", 14373, krbconf->keytab_principal),
       "remctl_open");

    /* Send the no-op token. */
    tok.length = sizeof(token);
    tok.value = (char *) token;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &tok, &major, &minor);
    if (status != TOKEN_OK)
        bail("cannot send token");

    /* Accept the remote token. */
    status = token_recv_priv(r->fd, r->context, &flags, &tok, 1024 * 64,
                             &major, &minor);
    is_int(TOKEN_OK, status, "received token correctly");
    is_int(TOKEN_DATA | TOKEN_PROTOCOL, flags, "token had correct flags");
    is_int(2, tok.length, "token had correct length");
    is_int(3, ((char *) tok.value)[0], "protocol version is 3");
    is_int(MESSAGE_NOOP, ((char *) tok.value)[1], "no-op message");

    /* Close things out. */
    remctl_close(r);

    remctld_stop(remctld);
    return 0;
}
