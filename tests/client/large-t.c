/* $Id$ */
/* Test suite for over-large commands. */

/* Written by Russ Allbery <rra@stanford.edu>
   Copyright 2007 Board of Trustees, Leland Stanford Jr. University
   See README for licensing terms. */

#include <config.h>
#include <system.h>

#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>


int
main(void)
{
    char *principal;
    pid_t remctld;
    struct remctl *r;
    struct remctl_output *output;
    struct iovec command[5];
    struct timeval tv;

    test_init(6);

    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 6, "Kerberos tests not configured");
    } else {
        remctld = spawn_remctld(principal);
        if (remctld <= 0)
            die("cannot spawn remctld");

        command[0].iov_len = strlen("test");
        command[0].iov_base = (char *) "test";
        command[1].iov_len = strlen("noauth");
        command[1].iov_base = (char *) "noauth";
        command[2].iov_len = 64 * 1024 - 27;
        command[2].iov_base = xmalloc(command[2].iov_len);
        memset(command[2].iov_base, 'A', command[2].iov_len);
        command[3].iov_len = 64 * 1024 - 3;
        command[3].iov_base = xmalloc(command[3].iov_len);
        memset(command[3].iov_base, 'B', command[3].iov_len);
        command[4].iov_len = 0;
        command[4].iov_base = (char *) "";

        r = remctl_new();
        ok(1, r != NULL);
        ok(2, remctl_open(r, "localhost", 14444, principal));
        ok(3, remctl_commandv(r, command, 5));
        output = remctl_output(r);
        ok(4, output != NULL);
        if (output != NULL) {
            ok_int(5, REMCTL_OUT_ERROR, output->type);
            ok_int(6, ERROR_ACCESS, output->error);
        } else {
            ok(5, 0);
            ok(6, 0);
        }
        remctl_close(r);

        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        select(0, NULL, NULL, NULL, &tv);
        if (waitpid(remctld, NULL, WNOHANG) == 0) {
            kill(remctld, SIGTERM);
            waitpid(remctld, NULL, 0);
        }
    }
    unlink("data/test.cache");
    unlink("data/pid");
    exit(0);
}
