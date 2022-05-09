/*
 * Test suite for malformed commands.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2007, 2009-2010, 2012-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/gss-tokens.h>
#include <util/protocol.h>


/*
 * Send an invalid token to the given remctl connection and verify that it
 * returns the specified error code and string.
 */
static void
test_bad_token(const struct kerberos_config *config, const char *data,
               size_t length, enum error_codes code, const char *message,
               const char *description)
{
    struct remctl *r;
    struct remctl_output *output;
    gss_buffer_desc token;
    OM_uint32 major, minor;
    int status;

    /*
     * We have to open a new connection for every test since some errors close
     * the connection.
     */
    r = remctl_new();
    ok(r != NULL, "remctl_new");
    if (r == NULL)
        bail("remctl_new returned NULL");
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");
    token.value = (void *) data;
    token.length = length;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, 0, &major, &minor);
    is_int(TOKEN_OK, status, "sent token for %s", description);
    if (status != TOKEN_OK) {
        remctl_close(r);
        ok_block(5, 0, "testing for %s", description);
    }
    r->ready = 1;
    output = remctl_output(r);
    ok(output != NULL, "output is not null");
    if (output == NULL)
        ok_block(4, false, "testing for %s", description);
    else {
        is_int(REMCTL_OUT_ERROR, output->type, "error type");
        is_int(code, output->error, "right error code for %s", description);
        is_int(strlen(message), output->length, "right length for %s",
               description);
        if (output->length <= strlen(message))
            ok(memcmp(output->data, message, output->length) == 0,
               "right data for %s", description);
        else
            ok(0, "right data for %s", description);
    }
    remctl_close(r);
}


/*
 * Send a chunk of data to the given remctl connection and verify that it
 * returns a bad command error token.
 */
static void
test_bad_command(const struct kerberos_config *config, const char *data,
                 size_t length, const char *description)
{
    char buffer[BUFSIZ];
    size_t buflen;
    static const char prefix[] = {2, MESSAGE_COMMAND, 1, 0};

    memcpy(buffer, prefix, sizeof(prefix));
    memcpy(buffer + sizeof(prefix), data, length);
    buflen = sizeof(prefix) + length;
    test_bad_token(config, buffer, buflen, ERROR_BAD_COMMAND,
                   "Invalid command token", description);
}


int
main(void)
{
    struct kerberos_config *config;
    static const char token_message[] = {2, 47};
    /* clang-format off */
    static const char token_continue[] = {
        2, MESSAGE_COMMAND, 1, 4,
        0, 0, 0, 2,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's'
    };
    static const char token_argv0[] = {
        2, MESSAGE_COMMAND, 1, 0,
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
    static const char data_nul_command[] = {
        0, 0, 0, 2,
        0, 0, 0, 4, 't', '\0','s', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's'
    };
    static const char data_nul_sub[] = {
        0, 0, 0, 2,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', '\0'
    };
    static const char data_nul_argument[] = {
        0, 0, 0, 3,
        0, 0, 0, 4, 't', 'e', 's', 't',
        0, 0, 0, 6, 's', 't', 'a', 't', 'u', 's',
        0, 0, 0, 1, '\0'
    };
    /* clang-format on */

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(11 * 8);

    /* Test basic token errors. */
    test_bad_token(config, token_message, sizeof(token_message),
                   ERROR_UNKNOWN_MESSAGE, "Unknown message",
                   "unknown message");
    test_bad_token(config, token_continue, sizeof(token_continue),
                   ERROR_BAD_COMMAND, "Invalid command token",
                   "bad command token");
    test_bad_token(config, token_argv0, sizeof(token_argv0),
                   ERROR_UNKNOWN_COMMAND, "Unknown command", "empty command");

    /* Test a bunch of malformatted commands. */
    test_bad_command(config, data_trunc, sizeof(data_trunc),
                     "truncated command");
    test_bad_command(config, data_trunc_arg, sizeof(data_trunc_arg),
                     "truncated argument");
    test_bad_command(config, data_short, sizeof(data_short),
                     "missing argument");
    test_bad_command(config, data_long, sizeof(data_long), "extra argument");
    test_bad_command(config, data_extra, sizeof(data_extra),
                     "extra trailing garbage");
    test_bad_command(config, data_nul_command, sizeof(data_nul_command),
                     "nul in command");
    test_bad_command(config, data_nul_sub, sizeof(data_nul_sub),
                     "nul in subcommand");
    test_bad_command(config, data_nul_argument, sizeof(data_nul_argument),
                     "nul in argument");

    return 0;
}
