/*
 * Test suite for the high-level remctl library API.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2019 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2007, 2009-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <sys/uio.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/network.h>
#include <util/protocol.h>


/*
 * Run a single command and verify that the results are as expected.  This is
 * used to test a successful connection after one of the varient versions of
 * remctl_open.
 */
static void
test_command(struct remctl *r)
{
    struct remctl_output *output = NULL;
    static const char *test[] = {"test", "test", NULL};

    /* Send a successful command. */
    if (r == NULL)
        ok_block(3, 0, "...remctl_open failed");
    else {
        ok(remctl_command(r, test), "remctl_command");
        is_string("no error", remctl_error(r), "...still no error");
        output = remctl_output(r);
        ok(output != NULL, "first output token is not null");
    }
    if (output == NULL)
        ok_block(4, 0, "...and has correct content");
    else {
        is_int(REMCTL_OUT_OUTPUT, output->type, "...and is right type");
        is_int(12, output->length, "...and is right length");
        if (output->data == NULL)
            ok(0, "...and is right data");
        else
            ok(memcmp("hello world\n", output->data, 11) == 0,
               "...and is right data");
        is_int(1, output->stream, "...and is right stream");
    }
    if (r == NULL)
        ok_block(3, false, "...remctl_open failed");
    else {
        output = remctl_output(r);
        ok(output != NULL, "second output token is not null");
        if (output == NULL)
            ok_block(2, false, "second output token is not null");
        else {
            is_int(REMCTL_OUT_STATUS, output->type, "...and is right type");
            is_int(0, output->status, "...and is right status");
        }
    }
}


/*
 * Takes the principal and the protocol version and runs a set of tests.  Due
 * to the compatibility layer, we should be able to run the same commands
 * regardless of the protocol (we're not testing any of the v2-specific
 * features here).
 */
static void
do_tests(const char *principal, int protocol)
{
    struct remctl *r;
    struct iovec *command;
    struct remctl_output *output;
    static const char *error[] = {"test", "bad-command", NULL};
    static const char *no_service[] = {"all", NULL};

    /* Open the connection. */
    r = remctl_new();
    ok(r != NULL, "protocol %d: remctl_new", protocol);
    if (r == NULL)
        bail("remctl_new returned NULL");
    is_string("no error", remctl_error(r), "remctl_error with no error");
    r->protocol = protocol;
    ok(remctl_open(r, "localhost", 14373, principal), "remctl_open");
    is_string("no error", remctl_error(r), "...still no error");

    /* Send NOOP for protocol version 2 and higher. */
    if (protocol == 2) {
        ok(remctl_noop(r), "remctl_noop");
        is_string("no error", remctl_error(r), "...still no error");
    } else {
        ok(!remctl_noop(r), "remctl_noop fails");
        is_string("NOOP message not supported", remctl_error(r),
                  "...with correct error");
    }

    /* Send a successful command. */
    test_command(r);

    /* Send a successful command with remctl_commandv. */
    command = bcalloc(2, sizeof(struct iovec));
    command[0].iov_base = (char *) "test";
    command[0].iov_len = 4;
    command[1].iov_base = (char *) "test";
    command[1].iov_len = 4;
    ok(remctl_commandv(r, command, 2), "remctl_commandv");
    is_string("no error", remctl_error(r), "...still no error");
    output = remctl_output(r);
    ok(output != NULL, "first output token is not null");
    if (output == NULL)
        ok_block(4, false, "first output token is not null");
    else {
        is_int(REMCTL_OUT_OUTPUT, output->type, "...and is right type");
        is_int(12, output->length, "...and is right length");
        if (output->data == NULL)
            ok(0, "...and is right data");
        else
            ok(memcmp("hello world\n", output->data, 11) == 0,
               "...and is right data");
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
    free(command);

    /* Send a failing command. */
    ok(remctl_command(r, error), "remctl_command of error command");
    is_string("no error", remctl_error(r), "...no error on send");
    output = remctl_output(r);
    ok(output != NULL, "first output token is not null");
    if (output == NULL)
        ok_block(protocol == 1 ? 7 : 4, false,
                 "first output token is not null");
    else if (protocol == 1) {
        is_int(REMCTL_OUT_OUTPUT, output->type,
               "...and is right protocol 1 type");
        is_int(16, output->length, "...and is right length");
        if (output->data == NULL)
            ok(0, "...and has the right error message");
        else
            ok(memcmp("Unknown command\n", output->data, 16) == 0,
               "...and has the right error message");
        is_int(1, output->stream, "...and is right stream");
        output = remctl_output(r);
        ok(output != NULL, "second output token is not null");
        if (output == NULL)
            ok_block(2, false, "second output token is not null");
        else {
            is_int(REMCTL_OUT_STATUS, output->type, "...and is right type");
            is_int(-1, output->status, "...and is right status");
        }
    } else {
        is_int(REMCTL_OUT_ERROR, output->type,
               "...and is right protocol 2 type");
        is_int(15, output->length, "...and is right length");
        if (output->data == NULL)
            ok(0, "...and has the right error message");
        else
            ok(memcmp("Unknown command", output->data, 15) == 0,
               "...and has the right error message");
        is_int(ERROR_UNKNOWN_COMMAND, output->error, "...and error number");
    }

    /* Send a command with no service. */
    ok(remctl_command(r, no_service), "remctl_command with no service");
    is_string("no error", remctl_error(r), "...and no error");
    output = remctl_output(r);
    ok(output != NULL, "...and non-null output token");
    if (output == NULL)
        ok_block(4, false, "...and non-null output token");
    else {
        is_int(REMCTL_OUT_OUTPUT, output->type, "...of correct type");
        is_int(12, output->length, "...and length");
        if (output->data == NULL)
            ok(0, "...and data");
        else
            ok(memcmp("hello world\n", output->data, 11) == 0, "...and data");
        is_int(1, output->stream, "...and stream");
    }
    output = remctl_output(r);
    ok(output != NULL, "...and non-null second token");
    if (output == NULL)
        ok_block(2, false, "...and non-null second token");
    else {
        is_int(REMCTL_OUT_STATUS, output->type, "...of right type");
        is_int(0, output->status, "...and status");
    }

    /* All done. */
    remctl_close(r);
    ok(1, "remctl_close didn't explode");
}


int
main(void)
{
    struct kerberos_config *config;
    struct remctl_result *result;
    struct remctl *r;
    struct addrinfo ai;
    struct sockaddr_in sin;
    int fd;
    char *message, *p;
    static const char *test[] = {"test", "test", NULL};
    static const char *error[] = {"test", "bad-command", NULL};

    /* Set up Kerberos and remctld. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", (char *) 0);

    plan(141);

    /* Run the basic protocol tests. */
    do_tests(config->principal, 1);
    do_tests(config->principal, 2);

    /*
     * We don't have a way of forcing the simple protocol to use a particular
     * protocol, so we always do it via protocol v2.  But if the above worked
     * with protocol v1, and this wrapper works with v2, everything should
     * have gotten tested.
     */
    result = remctl("localhost", 14373, config->principal, test);
    ok(result != NULL, "basic remctl API works");
    if (result == NULL)
        bail("remctl returned NULL");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(12, result->stdout_len, "...and correct stdout_len");
    if (result->stdout_buf == NULL)
        ok(0, "...and correct data");
    else
        ok(memcmp("hello world\n", result->stdout_buf, 11) == 0,
           "...and correct data");
    ok(result->error == NULL, "...and no error");
    remctl_result_free(result);
    result = remctl("localhost", 14373, config->principal, error);
    ok(result != NULL, "remctl API with error works");
    if (result == NULL)
        bail("remctl returned NULL");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stdout_len, "...and no stdout");
    is_int(0, result->stderr_len, "...and no stderr");
    if (result->error == NULL)
        ok(0, "...and the right error string");
    else
        is_string("Unknown command", result->error,
                  "...and the right error string");
    remctl_result_free(result);

    /* Test opening a connection from a struct sockaddr. */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(14373);
    sin.sin_addr.s_addr = htonl(0x7f000001UL);
    r = remctl_new();
    ok(remctl_open_sockaddr(r, NULL, (const struct sockaddr *) &sin,
                            sizeof(sin), config->principal),
       "remctl_open_sockaddr works");
    is_string("no error", remctl_error(r), "...with no error");
    test_command(r);
    remctl_close(r);

    /* Likewise from a struct addrinfo. */
    memset(&ai, 0, sizeof(ai));
    ai.ai_family = AF_INET;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_protocol = IPPROTO_TCP;
    ai.ai_addrlen = sizeof(sin);
    ai.ai_addr = (struct sockaddr *) &sin;
    r = remctl_new();
    ok(remctl_open_addrinfo(r, NULL, &ai, config->principal),
       "remctl_open_addrinfo works");
    is_string("no error", remctl_error(r), "...with no error");
    test_command(r);
    remctl_close(r);

    /* Likewise from an already-open connection. */
    fd = network_connect(&ai, NULL, 0);
    if (fd == INVALID_SOCKET)
        sysbail("cannot connect to localhost");
    r = remctl_new();
    ok(remctl_open_fd(r, NULL, fd, config->principal), "remctl_open_fd works");
    is_string("no error", remctl_error(r), "...with no error");
    test_command(r);
    remctl_close(r);

    /*
     * Test the error message when doing port fallback to the legacy port and
     * the connection fails.  The reported port should be the current port
     * even though we try the legacy port second.  The IP address here is
     * reserved for documentation purposes by RFC 5737 and should therefore
     * never be running a remctl listener.
     *
     * We strip the system call error off of the error string before we check
     * it since the translation may vary by system.
     */
    r = remctl_new();
    ok(remctl_set_timeout(r, 1), "set one second timeout");
    ok(!remctl_open(r, "192.0.2.1", 0, config->principal),
       "remctl_open for 192.0.2.1 fails");
    message = bstrdup(remctl_error(r));
    p = strrchr(message, ':');
    if (p != NULL)
        *p = '\0';
    is_string("cannot connect to 192.0.2.1 (port 4373)", message,
              "...with correct error");
    free(message);
    remctl_close(r);

    return 0;
}
