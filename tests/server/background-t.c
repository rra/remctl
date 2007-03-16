/* $Id$ */
/* Test suite for behavior with backgrounded processes. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2007 Board of Trustees, Leland Stanford Jr. University
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
    static const char *command[] = { "test", "background", NULL };
    struct remctl_result *result;
    pid_t remctld;
    unsigned long child;
    FILE *pidfile;

    test_init(6);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 6, "Kerberos tests not configured");
        return 0;
    }

    /* Spawn the remctld server. */
    remctld = spawn_remctld(principal);
    if (remctld <= 0)
        die("cannot spawn remctld");

    /* Run the test. */
    result = remctl("localhost", 14444, principal, command);
    ok(1, result != NULL);
    if (result == NULL)
        ok_block(2, 5, 0);
    else {
        ok_int(2, 0, result->status);
        ok_int(3, 7, result->stdout_len);
        ok(4, memcmp("Parent\n", result->stdout_buf, 7) == 0);
        ok_int(5, 0, result->stderr_len);
        ok(6, result->error == NULL);
    }
    remctl_result_free(result);

    /* Now that we're finished testing, stop the backgrounded process since
       otherwise it blocks runtests from finishing. */
    pidfile = fopen("data/cmd-background.pid", "r");
    if (pidfile != NULL) {
        child = 0;
        fscanf(pidfile, "%lu", &child);
        if (child > 1)
            kill(child, SIGTERM);
    }
    unlink("data/cmd-background.pid");

    kill(remctld, SIGTERM);
    waitpid(remctld, NULL, 0);
    unlink("data/pid");

    return 0;
}
