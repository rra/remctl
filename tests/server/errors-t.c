/*
 * Test suite for errors returned by the server.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2007, 2009-2010, 2012, 2014
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
#include <util/protocol.h>


/*
 * Run the given command and return the error code from the server, or
 * ERROR_INTERNAL if the command unexpectedly succeeded or we didn't get an
 * error code.
 */
static int
test_error(struct remctl *r, const char *arg)
{
    struct remctl_output *output;
    const char *command[] = {"test", NULL, NULL};

    command[1] = arg;
    if (arg == NULL)
        arg = "(null)";
    if (!remctl_command(r, command)) {
        diag("remctl error %s", remctl_error(r));
        return ERROR_INTERNAL;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            diag("test %s returned output: %.*s", arg, (int) output->length,
                 output->data);
            break;
        case REMCTL_OUT_STATUS:
            diag("test %s returned status %d", arg, output->status);
            return ERROR_INTERNAL;
        case REMCTL_OUT_ERROR:
            return output->error;
        case REMCTL_OUT_DONE:
            diag("unexpected done token");
            return ERROR_INTERNAL;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
    return ERROR_INTERNAL;
}


/*
 * Try to send a command with 10K arguments to the server.  This should result
 * in ERROR_TOOMANY_ARGS given the current server limits.
 */
static int
test_excess_args(struct remctl *r)
{
    struct remctl_output *output;
    const char **command;
    size_t i;

    command = bcalloc(10 * 1024 + 3, sizeof(const char *));
    command[0] = "test";
    command[1] = "echo";
    for (i = 2; i < (10 * 1024) + 2; i++)
        command[i] = "a";
    command[10 * 1024 + 2] = NULL;
    if (!remctl_command(r, command)) {
        diag("remctl error %s", remctl_error(r));
        return ERROR_INTERNAL;
    }
    free(command);
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            diag("test echo returned output: %.*s", (int) output->length,
                 output->data);
            break;
        case REMCTL_OUT_STATUS:
            diag("test echo returned status %d", output->status);
            return ERROR_INTERNAL;
        case REMCTL_OUT_ERROR:
            return output->error;
        case REMCTL_OUT_DONE:
            diag("unexpected done token");
            return ERROR_INTERNAL;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
    return ERROR_INTERNAL;
}


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    int status;

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(4);

    /* Run the tests. */
    r = remctl_new();
    if (!remctl_open(r, "localhost", 14373, config->principal))
        bail("cannot contact remctld");
    status = test_error(r, "bad-command");
    is_int(ERROR_UNKNOWN_COMMAND, status, "unknown command");
    status = test_error(r, "noauth");
    is_int(ERROR_ACCESS, status, "access denied");
    status = test_excess_args(r);
    is_int(ERROR_TOOMANY_ARGS, status, "too many arguments");
    status = test_error(r, NULL);
    is_int(ERROR_UNKNOWN_COMMAND, status, "unknown command");
    remctl_close(r);

    return 0;
}
