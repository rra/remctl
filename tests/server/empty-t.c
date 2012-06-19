/*
 * Test suite for user handling of an empty configuration.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <client/remctl.h>
#include <util/protocol.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <tests/tap/string.h>


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    struct remctl_output *output;
    char *tmpdir, *confpath;
    FILE *conf;
    const char *test[] = { "test", "test", NULL };

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);

    /* Write out our empty configuration file. */
    tmpdir = test_tmpdir();
    basprintf(&confpath, "%s/conf-empty", tmpdir);
    conf = fopen(confpath, "w");
    if (conf == NULL)
        sysbail("cannot create %s", confpath);
    fclose(conf);

    /* Now we can start remctl with our temporary configuration file. */
    remctld_start(config, "tmp/conf-empty", NULL);

    plan(7);

    /* Test that we get a valid UNKNOWN_COMMAND error. */
    r = remctl_new();
    ok(remctl_open(r, "localhost", 14373, config->principal), "remctl_open");
    ok(remctl_command(r, test), "remctl_command");
    output = remctl_output(r);
    ok(output != NULL, "first output token is not null");
    if (output == NULL)
        ok_block(4, 0, "...and has correct content");
    else {
        is_int(REMCTL_OUT_ERROR, output->type, "...and is an error");
        is_int(15, output->length, "...and is right length");
        if (output->data == NULL)
            ok(0, "...and has the right error message");
        else
            ok(memcmp("Unknown command", output->data, 15) == 0,
               "...and has the right error message");
        is_int(ERROR_UNKNOWN_COMMAND, output->error, "...and error number");
    }
    remctl_close(r);
    unlink(confpath);
    free(confpath);
    test_tmpdir_free(tmpdir);
    return 0;
}
