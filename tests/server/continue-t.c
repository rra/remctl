/* $Id$
 *
 * Test suite for continued commands.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006 Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <signal.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>


int
main(void)
{
    char *principal;
    struct remctl *r;
    struct remctl_output *output;
    pid_t remctld;
    static const char prefix_first[] = { 2, MESSAGE_COMMAND, 1, 1 };
    static const char prefix_next[] = { 2, MESSAGE_COMMAND, 1, 2 };
    static const char prefix_last[] = { 2, MESSAGE_COMMAND, 1, 3 };
    static const char data[] = {
        0, 0, 0, 3,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's',
        0, 0, 0, 1, '2'
    };
    char buffer[BUFSIZ];
    gss_buffer_desc token;
    OM_uint32 major, minor;
    int status;

    test_init(9);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 9, "Kerberos tests not configured");
        return 0;
    }

    /* Spawn the remctld server. */
    remctld = spawn_remctld(principal);
    if (remctld <= 0)
        die("cannot spawn remctld");

    /* Open a connection. */
    r = remctl_new();
    ok(1, r != NULL);
    ok(2, remctl_open(r, "localhost", 14444, principal));

    /* Send the command broken in the middle of protocol elements. */
    token.value = buffer;
    memcpy(buffer, prefix_first, sizeof(prefix_first));
    memcpy(buffer + sizeof(prefix_first), data, 2);
    token.length = sizeof(prefix_first) + 2;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, &major, &minor);
    ok_int(3, TOKEN_OK, status);
    memcpy(buffer, prefix_next, sizeof(prefix_next));
    memcpy(buffer + sizeof(prefix_next), data + 2, 4);
    token.length = sizeof(prefix_next) + 4;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, &major, &minor);
    ok_int(4, TOKEN_OK, status);
    memcpy(buffer, prefix_next, sizeof(prefix_next));
    memcpy(buffer + sizeof(prefix_next), data + 6, 13);
    token.length = sizeof(prefix_next) + 13;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, &major, &minor);
    ok_int(5, TOKEN_OK, status);
    memcpy(buffer, prefix_last, sizeof(prefix_last));
    memcpy(buffer + sizeof(prefix_last), data + 19, sizeof(data) - 19);
    token.length = sizeof(prefix_next) + sizeof(data) - 19;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, &major, &minor);
    ok_int(6, TOKEN_OK, status);
    r->ready = 1;
    output = remctl_output(r);
    ok(7, output != NULL);
    ok_int(8, REMCTL_OUT_STATUS, output->type);
    ok_int(9, 2, output->status);
    remctl_close(r);

    kill(remctld, SIGTERM);
    waitpid(remctld, NULL, 0);
    unlink("data/pid");

    return 0;
}
