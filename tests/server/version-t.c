/* $Id$ */
/* Test suite for version negotiation in the server. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2006, 2007 Board of Trustees, Leland Stanford Jr. University
   See README for licensing terms. */

#include <config.h>
#include <system.h>
#include <portable/gssapi.h>

#include <signal.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

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
    char *principal;
    struct remctl *r;
    pid_t remctld;
    OM_uint32 major, minor;
    int flags, status;
    gss_buffer_desc tok;

    test_init(8);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 8, "Kerberos tests not configured");
        return 0;
    }

    /* Spawn the remctld server. */
    remctld = spawn_remctld(principal);
    if (remctld <= 0)
        die("cannot spawn remctld");

    /* Open the connection to the site. */
    r = remctl_new();
    ok(1, r != NULL);
    ok(2, remctl_open(r, "localhost", 14444, principal));

    /* Send the command token. */
    tok.length = sizeof(token);
    tok.value = (char *) token;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &tok, &major, &minor);
    if (status != TOKEN_OK)
        die("cannot send token");

    /* Accept the remote token. */
    status = token_recv_priv(r->fd, r->context, &flags, &tok, 1024 * 64,
                             &major, &minor);
    ok_int(3, TOKEN_OK, status);
    ok_int(4, TOKEN_DATA | TOKEN_PROTOCOL, flags);
    ok_int(5, 3, tok.length);
    ok_int(6, 2, ((char *) tok.value)[0]);
    ok_int(7, MESSAGE_VERSION, ((char *) tok.value)[1]);
    ok_int(8, 2, ((char *) tok.value)[0]);

    /* Close things out. */
    remctl_close(r);
    kill(remctld, SIGTERM);
    waitpid(remctld, NULL, 0);
    unlink("data/pid");

    return 0;
}
