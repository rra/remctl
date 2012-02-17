/*
 * Test suite for address binding in the server.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2011, 2012
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
#include <util/xmalloc.h>


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
 * Run the remote test command and confirm the output is correct.  Takes the
 * string to expect the REMOTE_ADDR environment variable to be set to.
 */
static void
test_command(struct remctl *r, const char *addr)
{
    struct remctl_output *output;
    char *seen;
    const char *command[] = { "test", "env", "REMOTE_ADDR", NULL };

    if (!remctl_command(r, command)) {
        notice("# remctl error %s", remctl_error(r));
        ok_block(0, 3, "... command failed");
        return;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            is_int(strlen(addr) + 1, output->length, "... length ok");
            seen = xmalloc(output->length);
            memcpy(seen, output->data, output->length);
            seen[output->length - 1] = '\0';
            is_string(addr, seen, "... REMOTE_ADDR correct");
            free(seen);
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
    struct kerberos_config *krbconf;
    struct remctl *r;
#ifdef HAVE_INET6
    bool ipv6 = false;
#endif

    /* Unless we have Kerberos available, we can't really do anything. */
    if (chdir(getenv("SOURCE")) < 0)
        bail("can't chdir to SOURCE");
    krbconf = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);

    /* Initialize our testing. */
    ipv6 = have_ipv6();
    plan(26);

    /* Test connecting to IPv4 and IPv6 with default bind. */
    remctld_start(krbconf, "data/conf-simple", NULL);
    r = remctl_new();
    ok(remctl_open(r, "127.0.0.1", 14373, krbconf->keytab_principal),
       "Connect to 127.0.0.1");
    test_command(r, "127.0.0.1");
    remctl_close(r);
#ifdef HAVE_INET6
    if (ipv6) {
        r = remctl_new();
        ok(remctl_open(r, "::1", 14373, krbconf->keytab_principal),
           "Connect to ::1");
        test_command(r, "::1");
        remctl_close(r);
    } else {
        skip_block(4, "IPv6 not supported");
    }
#else
    skip_block(4, "IPv6 not supported");
#endif
    remctld_stop();

    /* Try binding to only IPv4. */
    remctld_start(krbconf, "data/conf-simple", "-b", "127.0.0.1", NULL);
    r = remctl_new();
    ok(remctl_open(r, "127.0.0.1", 14373, krbconf->keytab_principal),
       "Connect to 127.0.0.1 when bound to that address");
    test_command(r, "127.0.0.1");
    remctl_close(r);
#ifdef HAVE_INET6
    if (ipv6) {
        r = remctl_new();
        ok(!remctl_open(r, "::1", 14373, krbconf->keytab_principal),
           "Cannot connect to ::1 when only bound to 127.0.0.1");
        remctl_close(r);
    } else {
        skip("IPv6 not supported");
    }
#else
    skip("IPv6 not supported");
#endif
    remctld_stop();

    /* Try binding to only IPv6. */
#ifdef HAVE_INET6
    if (ipv6) {
        remctld_start(krbconf, "data/conf-simple", "-b", "::1", NULL);
        r = remctl_new();
        ok(!remctl_open(r, "127.0.0.1", 14373, krbconf->keytab_principal),
           "Cannot connect to 127.0.0.1 when only bound to ::1");
        remctl_close(r);
        r = remctl_new();
        ok(remctl_open(r, "::1", 14373, krbconf->keytab_principal),
           "Connect to ::1 when bound only to it");
        test_command(r, "::1");
        remctl_close(r);
        remctld_stop();
    } else {
        skip_block(5, "IPv6 not supported");
    }
#else
    skip_block(5, "IPv6 not supported");
#endif

    /* Try binding explicitly to local IPv4 and IPv6 addresses. */
#ifdef HAVE_INET6
    if (ipv6) {
        remctld_start(krbconf, "data/conf-simple", "-b", "127.0.0.1", "-b",
                      "::1", NULL);
        r = remctl_new();
        ok(remctl_open(r, "127.0.0.1", 14373, krbconf->keytab_principal),
           "Connect to 127.0.0.1 when bound to both local addresses");
        test_command(r, "127.0.0.1");
        remctl_close(r);
        r = remctl_new();
        ok(remctl_open(r, "::1", 14373, krbconf->keytab_principal),
           "Connect to ::1 when bound to both local addresses");
        test_command(r, "::1");
        remctl_close(r);
        remctld_stop();
    } else {
        skip_block(8, "IPv6 not supported");
    }
#else
    skip_block(8, "IPv6 not supported");
#endif
    
    return 0;
}
