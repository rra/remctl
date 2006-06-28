/* $Id$ */
/* Test suite for the client connection negotiation code. */

#include <config.h>
#include <system.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* Handle compatibility to older versions of MIT Kerberos. */
#ifndef HAVE_GSS_RFC_OIDS
# define GSS_C_NT_USER_NAME gss_nt_user_name
#endif

/* Heimdal provides a nice #define for this. */
#if !HAVE_DECL_GSS_KRB5_MECHANISM
# include <gssapi/gssapi_krb5.h>
# define GSS_KRB5_MECHANISM gss_mech_krb5
#endif

#include <client/internal.h>
#include <client/remctl.h>
#include <tests/libtest.h>
#include <util/util.h>

/* Create a socket, accept a single connection, try to establish a context,
   and then close the connection and exit.  We run this in a subprocess to
   provide the foil against which to test our connection negotiation.  Takes a
   flag saying what protocol version to use.  1 indicates protocol version 1
   the whole way, 2 indicates version 2 from the start, and 0 starts with
   version 1 and then goes back to version 2.  Also takes the PID of the
   parent process so that we can signal our parent when we're ready to
   proceed. */
static void
accept_connection(int protocol)
{
    struct sockaddr_in saddr;
    int s, conn, fd;
    int on = 1;
    int flags, wanted_flags;
    gss_buffer_desc send_tok, recv_tok;
    OM_uint32 major, minor, ret_flags;
    gss_ctx_id_t context;
    gss_name_t client;
    gss_OID doid;

    /* Create the socket and accept the connection. */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(14444);
    saddr.sin_addr.s_addr = INADDR_ANY;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        sysdie("error creating socket");
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
    if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysdie("error binding socket");
    if (listen(s, 1) < 0)
        sysdie("error listening to socket");
    fd = open("data/pid", O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        sysdie("cannot create sentinal");
    close(fd);
    conn = accept(s, NULL, 0);
    if (conn < 0)
        sysdie("error accepting connection");

    /* Now do the context negotiation. */
    if (token_recv(conn, &flags, &recv_tok, 64 * 1024) != TOKEN_OK)
        die("cannot recv initial token");
    if (flags != (TOKEN_NOOP | TOKEN_CONTEXT_NEXT | TOKEN_PROTOCOL))
        die("bad flags on initial token");
    wanted_flags = TOKEN_CONTEXT | TOKEN_PROTOCOL;
    context = GSS_C_NO_CONTEXT;
    do {
        if (token_recv(conn, &flags, &recv_tok, 64 * 1024) != TOKEN_OK)
            die("cannot recv subsequent token");
        if (flags != wanted_flags)
            die("bad flags on subsequent token");
        major = gss_accept_sec_context(&minor, &context, GSS_C_NO_CREDENTIAL,
                       &recv_tok, GSS_C_NO_CHANNEL_BINDINGS, &client, &doid,
                       &send_tok, &ret_flags, NULL, NULL);
        if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED)
            die("GSSAPI failure: %ld %ld\n", (long) major, (long) minor);
        gss_release_buffer(&minor, &recv_tok);
        if (send_tok.length != 0) {
            flags = TOKEN_CONTEXT;
            if (protocol == 2)
                flags |= TOKEN_PROTOCOL;
            else if (protocol == 0)
                protocol = 2;
            if (token_send(conn, flags, &send_tok) != TOKEN_OK)
                die("cannot send subsequent token");
            gss_release_buffer(&minor, &send_tok);
        }
    } while (major == GSS_S_CONTINUE_NEEDED);

    /* All done.  Don't bother cleaning up, just exit. */
    exit(0);
}

int
main(void)
{
    char *principal;
    struct remctl *r;
    int protocol, n;
    pid_t child;
    struct timeval tv;

    test_init(5 * 3);

    /* Unless we have Kerberos available, we can't really do anything. */
    principal = kerberos_setup();
    if (principal == NULL) {
        skip_block(1, 6, "Kerberos tests not configured");
        return 0;
    }
    putenv("KRB5_KTNAME=data/test.keytab");

    /* We're going to try this three times, for each of the three possible
       different protocol negotiation behaviors that accept_connection can
       test.  Each time, we're going to check that we got a context and that
       we negotiated the appropriate protocol. */
    n = 1;
    for (protocol = 0; protocol <= 2; protocol++) {
        r = remctl_new();
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0)
            accept_connection(protocol);
        alarm(1);
        while (access("data/pid", F_OK) < 0) {
            tv.tv_sec = 0;
            tv.tv_usec = 50000;
            select(0, NULL, NULL, NULL, &tv);
        }
        alarm(0);
        if (!remctl_open(r, "127.0.0.1", 14444, principal)) {
            warn("open error: %s", remctl_error(r));
            ok_block(n, 5, 0);
            n += 5;
        } else {
            ok(n++, 1);
            ok_int(n++, (protocol < 2) ? 1 : 2, r->protocol);
            ok_string(n++, r->host, "127.0.0.1");
            ok_int(n++, r->port, 14444);
            ok_string(n++, r->principal, principal);
        }
        remctl_close(r);
        waitpid(child, NULL, 0);
        unlink("data/pid");
    }

    return 0;
}
