/*
 * Test suite for address binding in the server.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/system.h>

#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/process.h>
#include <tests/tap/remctl.h>
#include <util/network.h>


/*
 * Test whether IPv6 is available.
 */
static bool
have_ipv6(void)
{
    socket_type fd;

    fd = network_bind_ipv6(SOCK_STREAM, "::1", 14373);
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
 * Test whether we have any IPv6 addresses.  This is used to decide
 * whether to expect the server to bind IPv6 addresses by default.
 */
static bool
have_ipv6_addr(void)
{
    struct addrinfo hints, *addrs, *addr;
    int error;
    char service[16];
    bool result = false;

    /* Do the query to find all the available addresses. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%d", 14373);
    error = getaddrinfo(NULL, service, &hints, &addrs);
    if (error < 0)
        return false;
    for (addr = addrs; addr != NULL; addr = addr->ai_next) {
        if (addr->ai_family == AF_INET6)
            result = true;
    }
    freeaddrinfo(addrs);
    return result;
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
        diag("remctl error %s", remctl_error(r));
        ok_block(0, 3, "... command failed");
        return;
    }
    do {
        output = remctl_output(r);
        switch (output->type) {
        case REMCTL_OUT_OUTPUT:
            is_int(strlen(addr) + 1, output->length, "... length ok");
            seen = bstrndup(output->data, output->length - 1);
            is_string(addr, seen, "... REMOTE_ADDR correct");
            free(seen);
            break;
        case REMCTL_OUT_STATUS:
            is_int(0, output->status, "... status ok");
            break;
        case REMCTL_OUT_ERROR:
            diag("test env returned error: %.*s", (int) output->length,
                 output->data);
            ok_block(0, 3, "... error received");
            break;
        case REMCTL_OUT_DONE:
            diag("unexpected done token");
            break;
        }
    } while (output->type == REMCTL_OUT_OUTPUT);
}


int
main(void)
{
    struct kerberos_config *config;
    struct remctl *r;
    struct process *remctld;
#ifdef HAVE_INET6
    bool ipv6 = have_ipv6();
#endif

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);

    /* Initialize our testing. */
    plan(26);

    /* Test connecting to IPv4 and IPv6 with default bind. */
    remctld = remctld_start(config, "data/conf-simple", NULL);
    r = remctl_new();
    ok(remctl_open(r, "127.0.0.1", 14373, config->principal),
       "Connect to 127.0.0.1");
    test_command(r, "127.0.0.1");
    remctl_close(r);
#ifdef HAVE_INET6
    if (ipv6 && have_ipv6_addr()) {
        r = remctl_new();
        ok(remctl_open(r, "::1", 14373, config->principal), "Connect to ::1");
        test_command(r, "::1");
        remctl_close(r);
    } else {
        skip_block(4, "IPv6 not supported");
    }
#else
    skip_block(4, "IPv6 not supported");
#endif
    process_stop(remctld);

    /* Try binding to only IPv4. */
    remctld = remctld_start(config, "data/conf-simple", "-b", "127.0.0.1",
                            NULL);
    r = remctl_new();
    ok(remctl_open(r, "127.0.0.1", 14373, config->principal),
       "Connect to 127.0.0.1 when bound to that address");
    test_command(r, "127.0.0.1");
    remctl_close(r);
#ifdef HAVE_INET6
    if (ipv6) {
        r = remctl_new();
        ok(!remctl_open(r, "::1", 14373, config->principal),
           "Cannot connect to ::1 when only bound to 127.0.0.1");
        remctl_close(r);
    } else {
        skip("IPv6 not supported");
    }
#else
    skip("IPv6 not supported");
#endif
    process_stop(remctld);

    /* Try binding to only IPv6. */
#ifdef HAVE_INET6
    if (ipv6) {
        remctld = remctld_start(config, "data/conf-simple", "-b", "::1", NULL);
        r = remctl_new();
        ok(!remctl_open(r, "127.0.0.1", 14373, config->principal),
           "Cannot connect to 127.0.0.1 when only bound to ::1");
        remctl_close(r);
        r = remctl_new();
        ok(remctl_open(r, "::1", 14373, config->principal),
           "Connect to ::1 when bound only to it");
        test_command(r, "::1");
        remctl_close(r);
        process_stop(remctld);
    } else {
        skip_block(5, "IPv6 not supported");
    }
#else
    skip_block(5, "IPv6 not supported");
#endif

    /* Try binding explicitly to local IPv4 and IPv6 addresses. */
#ifdef HAVE_INET6
    if (ipv6) {
        remctld = remctld_start(config, "data/conf-simple", "-b", "127.0.0.1",
                                "-b", "::1", NULL);
        r = remctl_new();
        ok(remctl_open(r, "127.0.0.1", 14373, config->principal),
           "Connect to 127.0.0.1 when bound to both local addresses");
        test_command(r, "127.0.0.1");
        remctl_close(r);
        r = remctl_new();
        ok(remctl_open(r, "::1", 14373, config->principal),
           "Connect to ::1 when bound to both local addresses");
        test_command(r, "::1");
        remctl_close(r);
        process_stop(remctld);
    } else {
        skip_block(8, "IPv6 not supported");
    }
#else
    skip_block(8, "IPv6 not supported");
#endif
    
    return 0;
}
