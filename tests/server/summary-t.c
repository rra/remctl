/*
 * Test suite for the server using the summary command.
 *
 * Written by Jon Robertson <jonrober@stanford.edu>
 * Copyright 2016 Russ Allbery <eagle@eyrie.org>
 * Copyright 2012, 2013, 2014
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
#include <tests/tap/process.h>
#include <tests/tap/remctl.h>


int
main(void)
{
    struct kerberos_config *config;
    struct remctl_result *result;
    struct process *remctld;
    const char *expected;
    const char *test[] = { "help", NULL };

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);

    plan(12);

    /* Run the tests with summaries. */
    remctld = remctld_start(config, "data/conf-simple", NULL);
    result = remctl("localhost", 14373, config->principal, test);
    ok(result != NULL, "summary command works");
    if (result == NULL)
        bail("remctl returned NULL");
    is_int(0, result->status, "...with correct status");
    is_int(0, result->stderr_len, "...and no stderr");
    is_int(32, result->stdout_len, "...and correct stdout_len");
    if (result->stdout_buf == NULL)
        ok(0, "...and correct data");
    else {
        expected = "summary text\nsubcommand summary\n";
        ok(memcmp(expected, result->stdout_buf, 32) == 0,
           "...and correct data");
    }
    is_string(NULL, result->error, "...and no error");
    remctl_result_free(result);
    process_stop(remctld);

    /* Run the tests with the no-summary configuration. */
    remctld = remctld_start(config, "data/conf-nosummary", NULL);
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
    process_stop(remctld);

    return 0;
}
