/*
 * Test suite for the client connection negotiation code.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2007, 2009-2010, 2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/gssapi.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <fcntl.h>
#ifdef HAVE_SYS_SELECT_H
#    include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/wait.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <tests/tap/remctl.h>
#include <tests/tap/string.h>
#include <util/messages.h>
#include <util/tokens.h>


/*
 * Create a socket, accept a single connection, try to establish a context,
 * and then close the connection and exit.  We run this in a subprocess to
 * provide the foil against which to test our connection negotiation.  Takes a
 * flag saying what protocol version to use.  1 indicates protocol version 1
 * the whole way, 2 indicates version 2 from the start, and 0 starts with
 * version 1 and then goes back to version 2.
 */
static void
accept_connection(const char *pidfile, int protocol)
{
    struct sockaddr_in saddr;
    socket_type s, conn;
    int fd;
    int on = 1;
    const void *onaddr = &on;
    int flags, wanted_flags;
    gss_buffer_desc send_tok, recv_tok;
    OM_uint32 major, minor, ret_flags;
    gss_ctx_id_t context;
    gss_name_t client;
    gss_OID doid;

    /* Create the socket and accept the connection. */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(14373);
    saddr.sin_addr.s_addr = INADDR_ANY;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
        sysdie("error creating socket");
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, onaddr, sizeof(on));
    if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysdie("error binding socket");
    if (listen(s, 1) < 0)
        sysdie("error listening to socket");
    fd = open(pidfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        sysdie("cannot create sentinal");
    close(fd);
    conn = accept(s, NULL, 0);
    if (conn == INVALID_SOCKET)
        sysdie("error accepting connection");

    /* Now do the context negotiation. */
    if (token_recv(conn, &flags, &recv_tok, 64 * 1024, 0) != TOKEN_OK)
        die("cannot recv initial token");
    if (flags != (TOKEN_NOOP | TOKEN_CONTEXT_NEXT | TOKEN_PROTOCOL))
        die("bad flags on initial token");
    wanted_flags = TOKEN_CONTEXT | TOKEN_PROTOCOL;
    context = GSS_C_NO_CONTEXT;
    do {
        if (token_recv(conn, &flags, &recv_tok, 64 * 1024, 0) != TOKEN_OK)
            die("cannot recv subsequent token");
        if (flags != wanted_flags)
            die("bad flags on subsequent token");
        major = gss_accept_sec_context(&minor, &context, GSS_C_NO_CREDENTIAL,
                                       &recv_tok, GSS_C_NO_CHANNEL_BINDINGS,
                                       &client, &doid, &send_tok, &ret_flags,
                                       NULL, NULL);
        if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED)
            die("GSS-API failure: %ld %ld\n", (long) major, (long) minor);
        gss_release_buffer(&minor, &recv_tok);
        if (send_tok.length != 0) {
            flags = TOKEN_CONTEXT;
            if (protocol == 2)
                flags |= TOKEN_PROTOCOL;
            else if (protocol == 0)
                protocol = 2;
            if (token_send(conn, flags, &send_tok, 0) != TOKEN_OK)
                die("cannot send subsequent token");
            gss_release_buffer(&minor, &send_tok);
        }
    } while (major == GSS_S_CONTINUE_NEEDED);

    /* All done.  Clean up memory. */
    gss_release_name(&minor, &client);
    gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
    socket_close(conn);
    socket_close(s);
}


int
main(void)
{
    struct kerberos_config *config;
    char *p, *path, *pidfile;
    const char *error;
    struct remctl *r;
    int protocol;
    pid_t child;
    struct timeval tv;

    plan(5 * 3 + 3);

    /*
     * Now, check that the right thing happens when we try to connect to a
     * port where nothing is listening.  Modifying the returned error is not
     * actually allowed, but we know enough about the internals to know that
     * we can get away with it.
     */
    r = remctl_new();
    ok(!remctl_open(r, "127.0.0.1", 14445, NULL),
       "correct connection failure");
    error = remctl_error(r);
    ok(error != NULL, "...with error");
    if (error != NULL && strchr(error, ':') != NULL) {
        p = strchr(error, ':');
        *p = '\0';
        is_string("cannot connect to 127.0.0.1 (port 14445)", error,
                  "...and the correct error");
    } else {
        ok(0, "...and the correct error");
    }
    remctl_close(r);

    /* Unless we have Kerberos available, we can't really do anything else. */
    config = kerberos_setup(TAP_KRB_NEEDS_NONE);
    if (config->principal == NULL) {
        skip_block(5 * 3, "Kerberos tests not configured");
        return 0;
    }

    /*
     * We're going to try this three times, for each of the three possible
     * different protocol negotiation behaviors that accept_connection can
     * test.  Each time, we're going to check that we got a context and that
     * we negotiated the appropriate protocol.
     */
    path = test_tmpdir();
    basprintf(&pidfile, "%s/pid", path);
    for (protocol = 0; protocol <= 2; protocol++) {
        child = fork();
        if (child < 0)
            sysbail("cannot fork");
        else if (child == 0) {
            test_tmpdir_free(path);
            accept_connection(pidfile, protocol);
            free(pidfile);
            exit(0);
        }
        alarm(1);
        while (access(pidfile, F_OK) < 0) {
            tv.tv_sec = 0;
            tv.tv_usec = 50000;
            select(0, NULL, NULL, NULL, &tv);
        }
        alarm(0);
        r = remctl_new();
        if (!remctl_open(r, "127.0.0.1", 14373, config->principal)) {
            notice("# open error: %s", remctl_error(r));
            ok_block(5, 0, "protocol %d", protocol);
        } else {
            ok(1, "remctl_open with protocol %d", protocol);
            is_int((protocol < 2) ? 1 : 2, r->protocol,
                   "negotiated correct protocol");
            is_string(r->host, "127.0.0.1", "host is correct");
            is_int(r->port, 14373, "port is correct");
            is_string(r->principal, config->principal, "principal is correct");
        }
        remctl_close(r);
        waitpid(child, NULL, 0);
        unlink(pidfile);
    }
    free(pidfile);
    test_tmpdir_free(path);

    return 0;
}
