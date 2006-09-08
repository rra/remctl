/* $Id$ */
/* Test suite for environment variables set by the server. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2006 Board of Trustees, Leland Stanford Jr. University
   See README for licensing terms. */

#include <config.h>
#include <system.h>

#include <signal.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

/* Run the remote env command with the given variable and return the value
   from the server or NULL if there was an error. */
static char *
test_env(struct remctl *r, const char *variable)
{
    struct remctl_output *output;
    char *value = NULL;
    const char *command[] = { "test", "env", NULL, NULL };

    command[2] = variable;
    if (!remctl_command(r, command)) {
        warn("remctl error %s", remctl_error(r));
        return NULL;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            value = xstrndup(output->data, output->length);
            break;
        case REMCTL_OUT_STATUS:
            if (output->status != 0) {
                if (value != NULL)
                    free(value);
                warn("test env returned status %d", output->status);
                return NULL;
            }
            if (value == NULL)
                value = xstrdup("");
            return value;
        case REMCTL_OUT_ERROR:
            if (value != NULL)
                free(value);
            warn("test env returned error: %.*s", (int) output->length,
                 output->data);
            return NULL;
        case REMCTL_OUT_DONE:
            if (value != NULL)
                free(value);
            warn("unexpected done token");
            return NULL;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
    return value;
}


int
main(void)
{
    char *principal, *expected, *value;
    struct remctl *r;
    pid_t remctld;

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
    expected = concat(principal, "\n", NULL);
    value = test_env(r, "REMUSER");
    ok_string(1, expected, value);
    free(value);
    value = test_env(r, "REMOTE_USER");
    ok_string(2, expected, value);
    free(value);
    value = test_env(r, "REMOTE_ADDR");
    ok_string(3, "127.0.0.1\n", value);
    free(value);
    value = test_env(r, "REMOTE_HOST");
    ok(4, strcmp(value, "\n") == 0 || strstr(value, "localhost") != NULL);
    free(value);
    remctl_close(r);

    kill(remctld, SIGTERM);
    waitpid(remctld, NULL, 0);
    unlink("data/pid");

    return 0;
}
