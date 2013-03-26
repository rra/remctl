/*
 * Test suite for the server using the summary command.
 *
 * Written by Jon Robertson <jonrober@stanford.edu>
 * Copyright 2012, 2013
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
    const char *test[] = { "help", NULL };

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    remctld_start(config, "data/conf-simple", NULL);

    plan(12);

    /* Run the tests. */
    result = remctl("localhost", 14373, config->principal, test);
    ok(result != NULL, "summary command works");
    if (result == NULL)
        bail("remctl returned NULL");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(13, result->stdout_len, "...and correct stdout_len");
    if (result->stdout_buf == NULL)
        ok(0, "...and correct data");
    else
        ok(memcmp("summary text\n", result->stdout_buf, 13) == 0,
           "...and correct data");
    is_string(NULL, result->error, "...and no error");
    remctl_result_free(result);
    remctld_stop();

    /* Run the tests. */
    remctld_start(config, "data/conf-nosummary", NULL);
    result = remctl("localhost", 14373, config->principal, test);
    ok(result != NULL, "summary command works");
    if (result == NULL)
        bail("remctl returned NULL");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(0, result->stdout_len, "...and no stdout");
    ok(result->error != NULL, "...and error");
    is_string("Unknown command", result->error, "...and correct error text");
    remctl_result_free(result);

    return 0;
}
