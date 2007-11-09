/* $Id$ */
/* Test suite for errors returned by the server. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2006, 2007 Board of Trustees, Leland Stanford Jr. University
   See README for licensing terms. */

#include <config.h>
#include <system.h>

#include <signal.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

/* Run the given command and return the error code from the server, or
   ERROR_INTERNAL if the command unexpectedly succeeded or we didn't get an
   error code. */
static int
test_error(struct remctl *r, const char *arg)
{
    struct remctl_output *output;
    const char *command[] = { "test", NULL, NULL };

    command[1] = arg;
    if (!remctl_command(r, command)) {
        warn("remctl error %s", remctl_error(r));
        return ERROR_INTERNAL;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            warn("test %s returned output: %.*s", arg, (int) output->length,
                 output->data);
            break;
        case REMCTL_OUT_STATUS:
            warn("test %s returned status %d", arg, output->status);
            return ERROR_INTERNAL;
        case REMCTL_OUT_ERROR:
            return output->error;
        case REMCTL_OUT_DONE:
            warn("unexpected done token");
            return ERROR_INTERNAL;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
    return ERROR_INTERNAL;
}


/* Try to send a command with 10K arguments to the server.  This should result
   in ERROR_TOOMANY_ARGS given the current server limits. */
static int
test_excess_args(struct remctl *r)
{
    struct remctl_output *output;
    const char **command;
    size_t i;

    command = xmalloc((10 * 1024 + 3) * sizeof(const char *));
    command[0] = "test";
    command[1] = "echo";
    for (i = 2; i < (10 * 1024) + 2; i++)
        command[i] = "a";
    command[10 * 1024 + 2] = NULL;
    if (!remctl_command(r, command)) {
        warn("remctl error %s", remctl_error(r));
        return ERROR_INTERNAL;
    }
    free(command);
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            warn("test echo returned output: %.*s", (int) output->length,
                 output->data);
            break;
        case REMCTL_OUT_STATUS:
            warn("test echo returned status %d", output->status);
            return ERROR_INTERNAL;
        case REMCTL_OUT_ERROR:
            return output->error;
        case REMCTL_OUT_DONE:
            warn("unexpected done token");
            return ERROR_INTERNAL;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
    return ERROR_INTERNAL;
}


int
main(void)
{
    char *principal;
    struct remctl *r;
    pid_t remctld;
    int status;

    test_init(4);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 4, "Kerberos tests not configured");
        return 0;
    }

    /* Spawn the remctld server. */
    remctld = spawn_remctld(principal);
    if (remctld <= 0)
        die("cannot spawn remctld");

    /* Run the tests. */
    r = remctl_new();
    if (!remctl_open(r, "localhost", 14444, principal))
        die("cannot contact remctld");
    status = test_error(r, "bad-command");
    ok_int(1, ERROR_UNKNOWN_COMMAND, status);
    status = test_error(r, "noauth");
    ok_int(2, ERROR_ACCESS, status);
    status = test_excess_args(r);
    ok_int(3, ERROR_TOOMANY_ARGS, status);
    status = test_error(r, NULL);
    ok_int(4, ERROR_UNKNOWN_COMMAND, status);
    remctl_close(r);

    kill(remctld, SIGTERM);
    waitpid(remctld, NULL, 0);
    unlink("data/pid");

    return 0;
}
