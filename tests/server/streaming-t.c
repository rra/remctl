/* $Id$ */
/* Test suite for streaming data from the server. */

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

int
main(void)
{
    char *principal;
    struct remctl *r;
    struct remctl_output *output;
    pid_t remctld;
    const char *command[] = { "test", "streaming", NULL };

    test_init(32);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 32, "Kerberos tests not configured");
        return 0;
    }

    /* Spawn the remctld server. */
    remctld = spawn_remctld(principal);
    if (remctld <= 0)
        die("cannot spawn remctld");

    /* First, version 2. */
    r = remctl_new();
    ok(1, r != NULL);
    ok(2, remctl_open(r, "localhost", 14444, principal));
    ok(3, remctl_command(r, command, 1));
    output = remctl_output(r);
    ok(4, output != NULL);
    ok_int(5, REMCTL_OUT_OUTPUT, output->type);
    ok_int(6, 23, output->length);
    if (output->data == NULL)
        ok(7, 0);
    else
        ok(7, memcmp("This is the first line\n", output->data, 23) == 0);
    ok_int(8, 1, output->stream);
    output = remctl_output(r);
    ok(9, output != NULL);
    ok_int(10, REMCTL_OUT_OUTPUT, output->type);
    ok_int(11, 24, output->length);
    if (output->data == NULL)
        ok(12, 0);
    else
        ok(12, memcmp("This is the second line\n", output->data, 24) == 0);
    ok_int(13, 2, output->stream);
    output = remctl_output(r);
    ok(14, output != NULL);
    ok_int(15, REMCTL_OUT_OUTPUT, output->type);
    ok_int(16, 23, output->length);
    if (output->data == NULL)
        ok(17, 0);
    else
        ok(17, memcmp("This is the third line\n", output->data, 23) == 0);
    ok_int(18, 1, output->stream);
    output = remctl_output(r);
    ok(19, output != NULL);
    ok_int(20, REMCTL_OUT_STATUS, output->type);
    ok_int(21, 0, output->status);
    remctl_close(r);

    /* Now, version 1. */
    r = remctl_new();
    r->protocol = 1;
    ok(22, r != NULL);
    ok(23, remctl_open(r, "localhost", 14444, principal));
    ok(24, remctl_command(r, command, 1));
    output = remctl_output(r);
    ok(25, output != NULL);
    ok_int(26, REMCTL_OUT_OUTPUT, output->type);
    ok_int(27, 70, output->length);
    if (output->data == NULL)
        ok(28, 0);
    else
        ok(28, memcmp("This is the first line\nThis is the second line\n"
                      "This is the third line\n", output->data, 70) == 0);
    ok_int(29, 1, output->stream);
    output = remctl_output(r);
    ok(30, output != NULL);
    ok_int(31, REMCTL_OUT_STATUS, output->type);
    ok_int(32, 0, output->status);
    remctl_close(r);

    kill(remctld, SIGTERM);
    waitpid(remctld, NULL, 0);
    unlink("data/pid");

    return 0;
}
