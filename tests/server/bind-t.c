/*
 * Test suite for address binding in the server.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>

#include <signal.h>
#include <sys/wait.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <util/concat.h>
#include <util/messages.h>
#include <util/network.h>


/*
 * Test whether IPv6 is available.
 */
static bool
have_ipv6(void)
{
    socket_type fd;

    fd = network_bind_ipv6("::1", 14373);
    if (fd == INVALID_SOCKET) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT
            || errno == EADDRNOTAVAIL)
            return false;
        return true;
    }
    close(fd);
    return true;
}


/*
 * Run the remote test command and confirm the output is correct.
 */
static void
test_command(struct remctl *r)
{
    struct remctl_output *output;
    const char *command[] = { "test", "test", NULL };

    if (!remctl_command(r, command)) {
        notice("# remctl error %s", remctl_error(r));
        ok_block(0, 3, "... command failed");
        return;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            is_int(strlen("hello world\n"), output->length, "... length ok");
            is_int(0, memcmp("hello world\n", output->data, output->length),
                   "... data ok");
            break;
        case REMCTL_OUT_STATUS:
            is_int(0, output->status, "... status ok");
            break;
        case REMCTL_OUT_ERROR:
            notice("# test env returned error: %.*s", (int) output->length,
                   output->data);
            ok_block(0, 3, "... error received");
            break;
        case REMCTL_OUT_DONE:
            notice("# unexpected done token");
            break;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
}


int
main(void)
{
    char *principal, *config, *path;
    struct remctl *r;
    pid_t remctld;

    /* Unless we have Kerberos available, we can't really do anything. */
    if (chdir(getenv("SOURCE")) < 0)
        bail("can't chdir to SOURCE");
    principal = kerberos_setup();
    if (principal == NULL)
        skip_all("Kerberos tests not configured");
    config = concatpath(getenv("SOURCE"), "data/conf-simple");
    path = concatpath(getenv("BUILD"), "../server/remctld");
    remctld = remctld_start(path, principal, config);

    /* Run the tests. */
    plan(4 * 2);
    r = remctl_new();
    ok(remctl_open(r, "127.0.0.1", 14373, principal), "Connect to 127.0.0.1");
    test_command(r);
    remctl_close(r);
#ifdef HAVE_INET6
    if (have_ipv6()) {
        r = remctl_new();
        ok(remctl_open(r, "::1", 14373, principal), "Connect to ::1");
        test_command(r);
        remctl_close(r);
    } else {
        skip_block(4, "IPv6 not supported");
    }
#else
    skip_block(4, "IPv6 not supported");
#endif

    remctld_stop(remctld);
    kerberos_cleanup();
    return 0;
}
