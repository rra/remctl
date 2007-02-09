/* $Id$ */
/* network test suite. */

/* Copyright (c) 2004, 2005, 2006, 2007
       by Internet Systems Consortium, Inc. ("ISC")
   Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
       2002, 2003 by The Internet Software Consortium and Rich Salz

   This code is derived from software contributed to the Internet Software
   Consortium by Rich Salz.

   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
   SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <config.h>
#include <system.h>
#include <portable/socket.h>

#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

#include <tests/libtest.h>
#include <util/util.h>

/* Set this globally to 0 if IPv6 is available but doesn't work. */
static int ipv6 = 1;

/* The server portion of the test.  Listens to a socket and accepts a
   connection, making sure what is printed on that connection matches what the
   client is supposed to print. */
static int
listener(int fd, int n)
{
    int client;
    FILE *out;
    char buffer[512];

    client = accept(fd, NULL, NULL);
    close(fd);
    if (client < 0) {
        syswarn("cannot accept connection from socket");
        ok_block(n, 2, 0);
        return n + 2;
    }
    ok(n++, 1);
    out = fdopen(client, "r");
    if (fgets(buffer, sizeof(buffer), out) == NULL) {
        syswarn("cannot read from socket");
        ok(n++, 0);
        return n;
    }
    ok_string(n++, "socket test\r\n", buffer);
    fclose(out);
    return n;
}

/* Connect to the given host on port 11119 and send a constant string to a
   socket, used to do the client side of the testing.  Takes the source
   address as well to pass into network_connect_host. */
static void
client(const char *host, const char *source)
{
    int fd;
    FILE *out;

    fd = network_connect_host(host, 11119, source);
    if (fd < 0)
        sysdie("connect failed");
    out = fdopen(fd, "w");
    if (out == NULL)
        sysdie("fdopen failed");
    fputs("socket test\r\n", out);
    fclose(out);
    _exit(0);
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv4.  Takes an optional source address to use for client
   connections. */
static int
test_ipv4(int n, const char *source)
{
    int fd;
    pid_t child;

    fd = network_bind_ipv4("127.0.0.1", 11119);
    if (fd < 0)
        sysdie("cannot create or bind socket");
    if (listen(fd, 1) < 0) {
        syswarn("cannot listen to socket");
        ok(n++, 0);
        ok(n++, 0);
        ok(n++, 0);
    } else {
        ok(n++, 1);
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0)
            client("127.0.0.1", source);
        else {
            n = listener(fd, n);
            waitpid(child, NULL, 0);
        }
    }
    return n;
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv6.  Takes an optional source address to use for client
   connections. */
#ifdef HAVE_INET6
static int
test_ipv6(int n, const char *source)
{
    int fd;
    pid_t child;

    fd = network_bind_ipv6("::1", 11119);
    if (fd < 0) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT
            || errno == EADDRNOTAVAIL) {
            ipv6 = 0;
            skip_block(n, 3, "IPv6 not supported");
            return n + 3;
        } else
            sysdie("cannot create socket");
    }
    if (listen(fd, 1) < 0) {
        syswarn("cannot listen to socket");
        ok_block(n, 3, 0);
        n += 3;
    } else {
        ok(n++, 1);
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0)
            client("::1", source);
        else {
            n = listener(fd, n);
            waitpid(child, NULL, 0);
        }
    }
    return n;
}
#else /* !HAVE_INET6 */
static int
test_ipv6(int n, const char *source UNUSED)
{
    skip_block(n, 3, "IPv6 not supported");
    return n + 3;
}
#endif /* !HAVE_INET6 */

/* Bring up a server on port 11119 on all addresses and try connecting to it
   via all of the available protocols.  Takes an optional source address to
   use for client connections. */
static int
test_all(int n, const char *source_ipv4, const char *source_ipv6)
{
    int *fds, count, fd, i;
    pid_t child;
    struct sockaddr_storage saddr;
    size_t size = sizeof(saddr);

    network_bind_all(11119, &fds, &count);
    if (count == 0)
        sysdie("cannot create or bind socket");
    if (count > 2) {
        warn("got more than two sockets, using just the first two");
        count = 2;
    }
    for (i = 0; i < count; i++) {
        fd = fds[i];
        if (listen(fd, 1) < 0) {
            syswarn("cannot listen to socket %d", fd);
            ok_block(n, 3, 0);
            n += 3;
        } else {
            ok(n++, 1);
            child = fork();
            if (child < 0)
                sysdie("cannot fork");
            else if (child == 0) {
                if (getsockname(fd, (struct sockaddr *) &saddr, &size) < 0)
                    sysdie("cannot getsockname");
                if (saddr.ss_family == AF_INET)
                    client("127.0.0.1", source_ipv4);
                else if (saddr.ss_family == AF_INET6)
                    client("::1", source_ipv6);
                else {
                    warn("unknown socket family %d", saddr.ss_family);
                    skip_block(n, 2, "unknown socket family");
                    n += 2;
                }
                size = sizeof(saddr);
            } else {
                n = listener(fd, n);
                waitpid(child, NULL, 0);
            }
        }
    }
    if (count == 1) {
        skip_block(n, 3, "only one listening socket");
        n += 3;
    }
    return n;
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv4 using network_connect_sockaddr.  Takes an optional source
   address to use for client connections. */
static int
test_create_ipv4(int n, const char *source)
{
    int fd;
    pid_t child;

    fd = network_bind_ipv4("127.0.0.1", 11119);
    if (fd < 0)
        sysdie("cannot create or bind socket");
    if (listen(fd, 1) < 0) {
        syswarn("cannot listen to socket");
        ok(n++, 0);
        ok(n++, 0);
        ok(n++, 0);
    } else {
        ok(n++, 1);
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0) {
            struct sockaddr_in sin;
            FILE *out;

            fd = network_client_create(PF_INET, SOCK_STREAM, source);
            if (fd < 0)
                _exit(1);
            sin.sin_family = AF_INET;
            sin.sin_port = htons(11119);
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
                _exit(1);
            out = fdopen(fd, "w");
            if (out == NULL)
                _exit(1);
            fputs("socket test\r\n", out);
            fclose(out);
            _exit(0);
        } else {
            n = listener(fd, n);
            waitpid(child, NULL, 0);
        }
    }
    return n;
}

/* Tests network_addr_compare.  Takes the test number, the expected result,
   the two addresses, and the mask. */
static void
ok_addr(int n, int expected, const char *a, const char *b, const char *mask)
{
    if (expected)
        ok(n, network_addr_match(a, b, mask));
    else
        ok(n, !network_addr_match(a, b, mask));
}

int
main(void)
{
    int n, status;
    struct addrinfo *ai, *ai4;
    struct addrinfo hints;
    char addr[INET6_ADDRSTRLEN];
    static const char *port = "119";

#ifdef HAVE_INET6
    struct addrinfo *ai6;
    char *p;
    static const char *ipv6_addr = "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210";
#endif

    test_init(117);

    /* If IPv6 support appears to be available but doesn't work, we have to
       skip the test_all tests since they'll create a socket that we then
       can't connect to.  This is the case on Solaris 8 without IPv6
       configured. */
    n = test_ipv4(1, NULL);                     /* Tests  1-3.  */
    n = test_ipv6(n, NULL);                     /* Tests  4-6.  */
    if (ipv6)
        n = test_all(n, NULL, NULL);            /* Tests  7-12. */
    else {
        skip_block(n, 6, "IPv6 not configured");
        n += 6;
    }
    n = test_create_ipv4(n, NULL);              /* Tests 13-15. */

    /* This won't make a difference for loopback connections. */
    n = test_ipv4(n, "127.0.0.1");              /* Tests 16-18. */
    n = test_ipv6(n, "::1");                    /* Tests 19-21. */
    if (ipv6)
        n = test_all(n, "127.0.0.1", "::1");    /* Tests 22-27. */
    else {
        skip_block(n, 6, "IPv6 not configured");
        n += 6;
    }
    n = test_create_ipv4(n, "127.0.0.1");       /* Tests 28-30. */

    /* Skip a block of tests that are only applicable to INN. */
    ok_block(n, 30, 1);
    n += 30;

    /* Now, test network_sockaddr_sprint, network_sockaddr_equal, and
       network_sockaddr_port. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;
    status = getaddrinfo("127.0.0.1", port, &hints, &ai4);
    if (status != 0)
        die("getaddrinfo on 127.0.0.1 failed: %s", gai_strerror(status));
    ok(61, network_sockaddr_sprint(addr, sizeof(addr), ai4->ai_addr));
    ok_string(62, "127.0.0.1", addr);
    ok_int(63, 119, network_sockaddr_port(ai4->ai_addr));
    ok(64, network_sockaddr_equal(ai4->ai_addr, ai4->ai_addr));
    status = getaddrinfo("127.0.0.2", NULL, &hints, &ai);
    if (status != 0)
        die("getaddrinfo on 127.0.0.2 failed: %s", gai_strerror(status));
    ok(65, !network_sockaddr_equal(ai->ai_addr, ai4->ai_addr));
    ok(66, !network_sockaddr_equal(ai4->ai_addr, ai->ai_addr));

    /* The same for IPv6. */
#ifdef HAVE_INET6
    status = getaddrinfo(ipv6_addr, port, &hints, &ai6);
    if (status != 0)
        sysdie("getaddr on %s failed", ipv6_addr);
    ok(67, network_sockaddr_sprint(addr, sizeof(addr), ai6->ai_addr));
    for (p = addr; *p != '\0'; p++)
        if (islower((unsigned char) *p))
            *p = toupper((unsigned char) *p);
    ok_string(68, ipv6_addr, addr);
    ok_int(69, 119, network_sockaddr_port(ai6->ai_addr));
    ok(70, network_sockaddr_equal(ai6->ai_addr, ai6->ai_addr));
    ok(71, !network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr));
    ok(72, !network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr));

    /* Test IPv4 mapped addresses. */
    status = getaddrinfo("::ffff:7f00:1", NULL, &hints, &ai6);
    if (status != 0)
        sysdie("getaddr on ::ffff:7f00:1 failed");
    ok(73, network_sockaddr_sprint(addr, sizeof(addr), ai6->ai_addr));
    ok_string(74, "127.0.0.1", addr);
    ok(75, network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr));
    ok(76, network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr));
    ok(77, !network_sockaddr_equal(ai->ai_addr, ai6->ai_addr));
    ok(78, !network_sockaddr_equal(ai6->ai_addr, ai->ai_addr));
    freeaddrinfo(ai6);
#else
    skip_block(67, 12, "IPv6 not supported");
#endif

    /* Check the domains of functions and their error handling. */
    ai4->ai_addr->sa_family = AF_UNIX;
    ok(79, !network_sockaddr_equal(ai4->ai_addr, ai4->ai_addr));
    ok_int(80, 0, network_sockaddr_port(ai4->ai_addr));

    /* Tests for network_addr_compare. */
    ok_addr( 81, 1, "127.0.0.1", "127.0.0.1",   NULL);
    ok_addr( 82, 0, "127.0.0.1", "127.0.0.2",   NULL);
    ok_addr( 83, 1, "127.0.0.1", "127.0.0.0",   "31");
    ok_addr( 84, 0, "127.0.0.1", "127.0.0.0",   "32");
    ok_addr( 85, 0, "127.0.0.1", "127.0.0.0",   "255.255.255.255");
    ok_addr( 86, 1, "127.0.0.1", "127.0.0.0",   "255.255.255.254");
    ok_addr( 87, 1, "10.10.4.5", "10.10.4.255", "24");
    ok_addr( 88, 0, "10.10.4.5", "10.10.4.255", "25");
    ok_addr( 89, 1, "10.10.4.5", "10.10.4.255", "255.255.255.0");
    ok_addr( 90, 0, "10.10.4.5", "10.10.4.255", "255.255.255.128");
    ok_addr( 91, 0, "129.0.0.0", "1.0.0.0",     "1");
    ok_addr( 92, 1, "129.0.0.0", "1.0.0.0",     "0");
    ok_addr( 93, 1, "129.0.0.0", "1.0.0.0",     "0.0.0.0");

    /* Try some IPv6 addresses. */
#ifdef HAVE_INET6
    ok_addr( 94, 1, ipv6_addr,   ipv6_addr,     NULL);
    ok_addr( 95, 1, ipv6_addr,   ipv6_addr,     "128");
    ok_addr( 96, 1, ipv6_addr,   ipv6_addr,     "60");
    ok_addr( 97, 1, "::127",     "0:0::127",    "128");
    ok_addr( 98, 1, "::127",     "0:0::128",    "120");
    ok_addr( 99, 0, "::127",     "0:0::128",    "128");
    ok_addr(100, 0, "::7fff",    "0:0::8000",   "113");
    ok_addr(101, 1, "::7fff",    "0:0::8000",   "112");
    ok_addr(102, 0, "::3:ffff",  "::2:ffff",    "120");
    ok_addr(103, 0, "::3:ffff",  "::2:ffff",    "119");
    ok_addr(104, 0, "ffff::1",   "7fff::1",     "1");
    ok_addr(105, 1, "ffff::1",   "7fff::1",     "0");
    ok_addr(106, 0, "fffg::1",   "fffg::1",     NULL);
    ok_addr(107, 0, "ffff::1",   "7fff::1",     "-1");
    ok_addr(108, 0, "ffff::1",   "ffff::1",     "-1");
    ok_addr(109, 0, "ffff::1",   "ffff::1",     "129");
#else
    skip_block(94, 16, "IPv6 not supported");
#endif

    /* Test some invalid addresses. */
    ok_addr(110, 0, "fred",      "fred",        NULL);
    ok_addr(111, 0, "",          "",            NULL);
    ok_addr(112, 0, "",          "",            "0");
    ok_addr(113, 0, "127.0.0.1", "127.0.0.1",   "pete");
    ok_addr(114, 0, "127.0.0.1", "127.0.0.1",   "1p");
    ok_addr(115, 0, "127.0.0.1", "127.0.0.1",   "1p");
    ok_addr(116, 0, "127.0.0.1", "127.0.0.1",   "-1");
    ok_addr(117, 0, "127.0.0.1", "127.0.0.1",   "33");

    return 0;
}
