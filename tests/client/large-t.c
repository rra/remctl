/*
 * Test suite for over-large commands.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2007, 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/concat.h>
#include <util/protocol.h>
#include <util/xmalloc.h>


int
main(void)
{
    struct kerberos_config *krbconf;
    char *config, *path;
    pid_t remctld;
    struct remctl *r;
    struct remctl_output *output;
    struct iovec command[7];

    if (chdir(getenv("SOURCE")) < 0)
        bail("can't chdir to SOURCE");
    krbconf = kerberos_setup();
    if (krbconf->keytab_principal == NULL)
        skip_all("Kerberos tests not configured");
    plan(6);
    config = concatpath(getenv("SOURCE"), "data/conf-simple");
    path = concatpath(getenv("BUILD"), "../server/remctld");
    remctld = remctld_start(path, krbconf, config, NULL);

    command[0].iov_len = strlen("test");
    command[0].iov_base = (char *) "test";
    command[1].iov_len = strlen("noauth");
    command[1].iov_base = (char *) "noauth";
    command[2].iov_len = TOKEN_MAX_DATA - 31;
    command[2].iov_base = xmalloc(command[2].iov_len);
    memset(command[2].iov_base, 'A', command[2].iov_len);
    command[3].iov_len = TOKEN_MAX_DATA;
    command[3].iov_base = xmalloc(command[3].iov_len);
    memset(command[3].iov_base, 'B', command[3].iov_len);
    command[4].iov_len = TOKEN_MAX_DATA - 20;
    command[4].iov_base = xmalloc(command[4].iov_len);
    memset(command[4].iov_base, 'C', command[4].iov_len);
    command[5].iov_len = 1;
    command[5].iov_base = (char *) "D";
    command[6].iov_len = 0;
    command[6].iov_base = (char *) "";

    r = remctl_new();
    ok(r != NULL, "remctl_new");
    ok(remctl_open(r, "localhost", 14373, krbconf->keytab_principal),
       "remctl_open");
    ok(remctl_commandv(r, command, 7), "sending extra large command");
    output = remctl_output(r);
    printf("\n");
    ok(output != NULL, "...and got a response");
    if (output != NULL) {
        is_int(REMCTL_OUT_ERROR, output->type, "...with the right type");
        is_int(ERROR_ACCESS, output->error, "...and the right error");
    } else {
        ok(0, "...with the right type");
        ok(0, "...and the right error");
    }
    remctl_close(r);

    remctld_stop(remctld);
    return 0;
}
