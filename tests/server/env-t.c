/*
 * Test suite for environment variables set by the server.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <tests/tap/string.h>


/*
 * Run the remote env command with the given variable and return the value
 * from the server or NULL if there was an error.
 */
static char *
test_env(struct remctl *r, const char *variable)
{
    struct remctl_output *output;
    char *value = NULL;
    const char *command[] = { "test", "env", NULL, NULL };

    command[2] = variable;
    if (!remctl_command(r, command)) {
        diag("remctl error %s", remctl_error(r));
        return NULL;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            value = bstrndup(output->data, output->length);
            break;
        case REMCTL_OUT_STATUS:
            if (output->status != 0) {
                if (value != NULL)
                    free(value);
                diag("test env returned status %d", output->status);
                return NULL;
            }
            if (value == NULL)
                value = bstrdup("");
            return value;
        case REMCTL_OUT_ERROR:
            if (value != NULL)
                free(value);
            diag("test env returned error: %.*s", (int) output->length,
                 output->data);
            return NULL;
        case REMCTL_OUT_DONE:
            if (value != NULL)
                free(value);
            diag("unexpected done token");
            return NULL;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
    return value;
}


int
main(void)
{
    struct kerberos_config *krbconf;
    char *expected, *value;
    struct remctl *r;

    /* Unless we have Kerberos available, we can't really do anything. */
    krbconf = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(krbconf, "data/conf-simple", NULL);

    plan(4);

    /* Run the tests. */
    r = remctl_new();
    if (!remctl_open(r, "localhost", 14373, krbconf->keytab_principal))
        bail("cannot contact remctld");
    basprintf(&expected, "%s\n", krbconf->keytab_principal);
    value = test_env(r, "REMUSER");
    is_string(expected, value, "value for REMUSER");
    free(value);
    value = test_env(r, "REMOTE_USER");
    is_string(expected, value, "value for REMOTE_USER");
    free(value);
    value = test_env(r, "REMOTE_ADDR");
    is_string("127.0.0.1\n", value, "value for REMOTE_ADDR");
    free(value);
    value = test_env(r, "REMOTE_HOST");
    ok(strcmp(value, "\n") == 0 || strstr(value, "localhost") != NULL,
       "value for REMOTE_HOST");
    free(value);
    remctl_close(r);

    return 0;
}
