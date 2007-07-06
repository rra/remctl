/* $Id$ */
/* Test suite for malformed commands. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2007 Board of Trustees, Leland Stanford Jr. University
   See README for licensing terms. */

#include <config.h>
#include <system.h>

#include <signal.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

/* Send an invalid token to the given remctl connection and verify that it
   returns the specified error code and string. */
static int
test_bad_token(int n, const char *principal, const char *data, size_t length,
               enum error_codes code, const char *message)
{
    struct remctl *r;
    struct remctl_output *output;
    gss_buffer_desc token;
    OM_uint32 major, minor;
    int status;

    /* We have to open a new connection for every test since some errors close
       the connection. */
    r = remctl_new();
    ok(n++, r != NULL);
    ok(n++, remctl_open(r, "localhost", 14444, principal));
    token.value = (void *) data;
    token.length = length;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, &major, &minor);
    ok_int(n++, TOKEN_OK, status);
    if (status != TOKEN_OK) {
        remctl_close(r);
        ok_block(n, 5, 0);
        return n + 5;
    }
    r->ready = 1;
    output = remctl_output(r);
    ok(n++, output != NULL);
    if (output == NULL) {
        remctl_close(r);
        ok_block(n, 4, 0);
        return n + 4;
    }
    ok_int(n++, REMCTL_OUT_ERROR, output->type);
    ok_int(n++, code, output->error);
    ok_int(n++, strlen(message), output->length);
    if (output->length <= strlen(message))
        ok(n++, memcmp(output->data, message, output->length) == 0);
    else
        ok(n++, 0);
    remctl_close(r);
    return n;
}

/* Send a chunk of data to the given remctl connection and verify that it
   returns a bad command error token.  Returns the next test number. */
static int
test_bad_command(int n, const char *principal, const char *data, size_t length)
{
    char buffer[BUFSIZ];
    size_t buflen;
    static const char prefix[] = { 2, MESSAGE_COMMAND, 1, 0 };

    memcpy(buffer, prefix, sizeof(prefix));
    memcpy(buffer + sizeof(prefix), data, length);
    buflen = sizeof(prefix) + length;
    return test_bad_token(n, principal, buffer, buflen, ERROR_BAD_COMMAND,
                          "Invalid command token");
}

int
main(void)
{
    char *principal;
    pid_t remctld;
    int n;
    static const char token_message[] = {
        2, 47
    };
    static const char token_continue[] = {
        2, MESSAGE_COMMAND, 1, 4,
        0, 0, 0, 2,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's'
    };
    static const char data_argv0[] = {
        0, 0, 0, 0
    };
    static const char data_trunc[] = {
        0, 0, 0, 2,
        0, 0, 0, 1, 't'
    };
    static const char data_trunc_arg[] = {
        0, 0, 0, 2,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u'
    };
    static const char data_short[] = {
        0, 0, 0, 3,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's'
    };
    static const char data_long[] = {
        0, 0, 0, 2,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's',
        0, 0, 0, 1, '2'
    };
    static const char data_extra[] = {
        0, 0, 0, 2,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 5, 's', 't', 'a', 't', 'u', 's'
    };

    test_init(8 * 8);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 8 * 8, "Kerberos tests not configured");
        return 0;
    }

    /* Spawn the remctld server. */
    remctld = spawn_remctld(principal);
    if (remctld <= 0)
        die("cannot spawn remctld");

    /* Test basic token errors. */
    n = 1;
    n = test_bad_token(n, principal, token_message, sizeof(token_message),
                       ERROR_UNKNOWN_MESSAGE, "Unknown message");
    n = test_bad_token(n, principal, token_continue, sizeof(token_continue),
                       ERROR_BAD_COMMAND, "Invalid command token");

    /* Test a bunch of malformatted commands. */
    n = test_bad_command(n, principal, data_argv0, sizeof(data_argv0));
    n = test_bad_command(n, principal, data_trunc, sizeof(data_trunc));
    n = test_bad_command(n, principal, data_trunc_arg, sizeof(data_trunc_arg));
    n = test_bad_command(n, principal, data_short, sizeof(data_short));
    n = test_bad_command(n, principal, data_long, sizeof(data_long));
    n = test_bad_command(n, principal, data_extra, sizeof(data_extra));

    kill(remctld, SIGTERM);
    waitpid(remctld, NULL, 0);
    unlink("data/pid");

    return 0;
}
