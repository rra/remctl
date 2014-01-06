/*
 * Test suite for the server connection negotiation code.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2007, 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/gssapi.h>
#include <portable/socket.h>

#include <sys/wait.h>

#include <server/internal.h>
#include <tests/tap/basic.h>
#include <tests/tap/kerberos.h>
#include <util/messages.h>
#include <util/tokens.h>


/*
 * The die handler function set by make_connection so that it will exit with
 * _exit and not exit on errors and not run the cleanup functions.
 */
static int
exit_child(void)
{
    _exit(1);
}


/*
 * Open a new connection to a server, taking the protocol version and
 * principal to use.  1 indicates protocol version 1 the whole way, 2
 * indicates version 2 from the start, and 0 starts with version 2, goes back
 * to version 1, and then goes to version 2.
 */
static void
make_connection(int protocol, const char *principal)
{
    struct sockaddr_in saddr;
    socket_type fd;
    int flags;
    gss_buffer_desc send_tok, recv_tok, name_buffer, *token_ptr;
    gss_buffer_desc empty_token = { 0, (void *) "" };
    gss_name_t name;
    gss_ctx_id_t gss_context;
    OM_uint32 major, minor, init_minor, gss_flags;
    static const OM_uint32 req_gss_flags
        = (GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_CONF_FLAG
           | GSS_C_INTEG_FLAG);

    /* Set up the exit handler so that we don't call exit. */
    message_fatal_cleanup = exit_child;

    /* Connect. */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(14373);
    saddr.sin_addr.s_addr = INADDR_ANY;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
        sysdie("error creating socket");
    if (connect(fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysdie("error connecting");

    /* Import the name into target_name. */
    name_buffer.value = (char *) principal;
    name_buffer.length = strlen(principal) + 1;
    major = gss_import_name(&minor, &name_buffer, GSS_C_NT_USER_NAME, &name);
    if (major != GSS_S_COMPLETE)
        die("cannot import name");

    /* Send the initial negotiation token. */
    flags = TOKEN_NOOP | TOKEN_CONTEXT_NEXT;
    if (protocol == 0 || protocol > 1)
        flags |= TOKEN_PROTOCOL;
    if (token_send(fd, flags, &empty_token, 0) != TOKEN_OK)
        sysdie("failure sending token");

    /* Perform the context-establishment loop. */
    token_ptr = GSS_C_NO_BUFFER;
    gss_context = GSS_C_NO_CONTEXT;
    do {
        major = gss_init_sec_context(&init_minor, GSS_C_NO_CREDENTIAL, 
                    &gss_context, name, (const gss_OID) GSS_KRB5_MECHANISM,
                    req_gss_flags, 0, NULL, token_ptr, NULL, &send_tok,
                    &gss_flags, NULL);
        if (token_ptr != GSS_C_NO_BUFFER)
            gss_release_buffer(&minor, &recv_tok);
        if (send_tok.length != 0) {
            flags = TOKEN_CONTEXT;
            if (protocol > 1)
                flags |= TOKEN_PROTOCOL;
            if (protocol == 0)
                protocol = 2;
            if (token_send(fd, flags, &send_tok, 0) != TOKEN_OK)
                sysdie("failure sending token");
        }
        gss_release_buffer(&minor, &send_tok);
        if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED)
            die("failure initializing context");
        if (major == GSS_S_CONTINUE_NEEDED) {
            if (token_recv(fd, &flags, &recv_tok, 64 * 1024, 0) != TOKEN_OK)
                sysdie("failure receiving token");
            token_ptr = &recv_tok;
        }
    } while (major == GSS_S_CONTINUE_NEEDED);

    /* All done. */
    gss_delete_sec_context(&minor, &gss_context, GSS_C_NO_BUFFER);
    socket_close(fd);
    _exit(0);
}


int
main(void)
{
    struct kerberos_config *config;
    socket_type s, fd;
    int protocol;
    pid_t child;
    struct sockaddr_in saddr;
    struct client *client;
    int on = 1;
    const void *onaddr = &on;

    /* Unless we have Kerberos available, we can't really do anything. */
    config = kerberos_setup(TAP_KRB_NEEDS_KEYTAB);
    plan(2 * 3);

    /* Set up address to which we're going to bind and start listening.. */
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(14373);
    saddr.sin_addr.s_addr = INADDR_ANY;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET)
        sysbail("error creating socket");
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, onaddr, sizeof(on));
    if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
        sysbail("error binding socket");
    if (listen(s, 1) < 0)
        sysbail("error listening to socket");

    /*
     * We're going to try this three times, for each of the three possible
     * different protocol negotiation behaviors that accept_connection can
     * test.  Each time, we're going to check that we got a context and that
     * we negotiated the appropriate protocol.
     */
    for (protocol = 0; protocol <= 2; protocol++) {
        child = fork();
        if (child < 0)
            sysbail("cannot fork");
        else if (child == 0)
            make_connection(protocol, config->principal);
        alarm(1);
        fd = accept(s, NULL, 0);
        if (fd == INVALID_SOCKET)
            sysbail("error accepting connection");
        alarm(0);
        client = server_new_client(fd, GSS_C_NO_CREDENTIAL);
        ok(client != NULL, "accept client with protocol %d", protocol);
        if (client == NULL)
            ok(0, "negotiated right protocol");
        else
            is_int((protocol < 2) ? 1 : 2, client->protocol,
                   "negotiated right protocol");
        server_free_client(client);
        waitpid(child, NULL, 0);
    }
    socket_close(s);
    return 0;
}
