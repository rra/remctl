/*
 * Test suite for the server using the summary command.
 *
 * Written by Jon Robertson <jonrober@stanford.edu>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>


int
main(void)
{
    struct kerberos_config *config;
    struct remctl_result *result;
    const char *helptest[]    = { "help", "test-summary", NULL };
    const char *subhelptest[] = { "help", "test-summary", "subhelp", NULL };
    const char *nohelptest[]  = { "help", "test", "test", NULL };
    const char *badcommand1[] = { "help", "test", NULL };
    const char *badcommand2[] = { "help", "invalid", "invalid", NULL };

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(30);

    /* Run the tests. */
    result = remctl("localhost", 14373, config->principal, helptest);
    ok(result != NULL, "help command works");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(10, result->stdout_len, "...and correct stdout_len");
    ok(memcmp("help text\n", result->stdout_buf, 10) == 0,
       "...and correct data");
    ok(result->error == NULL, "...and no error");
    remctl_result_free(result);

    result = remctl("localhost", 14373, config->principal, subhelptest);
    ok(result != NULL, "help with subcommand works");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(13, result->stdout_len, "...and correct stdout_len");
    ok(memcmp("subhelp text\n", result->stdout_buf, 13) == 0,
       "...and correct data");
    ok(result->error == NULL, "...and no error");
    remctl_result_free(result);

    result = remctl("localhost", 14373, config->principal, nohelptest);
    ok(result != NULL, "help for command without help does not work");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(0, result->stdout_len, "...and no stdout");
    ok(result->error != NULL, "...and error");
    is_string("No help defined for command", result->error,
              "...and correct error text");
    remctl_result_free(result);

    result = remctl("localhost", 14373, config->principal, badcommand1);
    ok(result != NULL, "help for command with non-matching subcommand");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(0, result->stdout_len, "...and no stdout");
    ok(result->error != NULL, "...and error");
    is_string("Unknown command", result->error, "...and correct error text");
    remctl_result_free(result);

    result = remctl("localhost", 14373, config->principal, badcommand2);
    ok(result != NULL, "help for unknown command");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(0, result->stdout_len, "...and no stdout");
    ok(result->error != NULL, "...and error");
    is_string("Unknown command", result->error, "...and correct error text");
    remctl_result_free(result);

    return 0;
}
