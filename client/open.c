/*
 * Open a connection to a remote server.
 *
 * This is the client implementation of opening a connection to a remote
 * server and doing the initial GSS-API negotiation.  This function is shared
 * between the v1 and v2 implementations.  One of the things it establishes is
 * what protocol is being used.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Based on work by Anton Ushakov
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/gssapi.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <errno.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <util/network.h>
#include <util/protocol.h>
#include <util/tokens.h>


/*
 * Given the remctl object (for error reporting), host, and port, attempt a
 * network connection.  Returns the file descriptor if successful or
 * INVALID_SOCKET on failure.
 */
socket_type
internal_connect(struct remctl *r, const char *host, unsigned short port)
{
    struct addrinfo hints, *ai;
    char portbuf[16];
    int status;
    socket_type fd;

    /*
     * Look up the remote host and open a TCP connection.  Call getaddrinfo
     * and network_connect instead of network_connect_host so that we can
     * report the complete error on host resolution.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portbuf, sizeof(portbuf), "%hu", port);
    status = getaddrinfo(host, portbuf, &hints, &ai);
    if (status != 0) {
        internal_set_error(r, "unknown host %s: %s", host,
                           gai_strerror(status));
        return INVALID_SOCKET;
    }
    fd = network_connect(ai, r->source, r->timeout);
    freeaddrinfo(ai);
    if (fd == INVALID_SOCKET) {
        internal_set_error(r, "cannot connect to %s (port %hu): %s", host,
                           port, socket_strerror(socket_errno));
        return INVALID_SOCKET;
    }
    return fd;
}


/*
 * Given the remctl struct, the host to connect to, the principal name (which
 * may be NULL to use the default), and a pointer to a gss_name_t, import that
 * principal into a GSS-API name.  We want to use a host-based name if
 * possible since that will trigger domain to realm mapping and name
 * canonicalization if desired, but given an arbitrary Kerberos principal, we
 * don't know whether it's host-based or not.  Therefore, if the principal was
 * specified explicitly, always just use it.
 *
 * Returns true on success and false on failure.
 */
static bool
internal_import_name(struct remctl *r, const char *host,
                     const char *principal, gss_name_t *name)
{
    gss_buffer_desc name_buffer;
    char *defprinc = NULL;
    OM_uint32 major, minor;
    gss_OID oid;

    /*
     * If principal is NULL, use host@<host>.  Don't use xmalloc here since it
     * dies on failure and that's rude for a library.
     */
    if (principal == NULL) {
        if (asprintf(&defprinc, "host@%s", host) < 0) {
            internal_set_error(r, "cannot allocate memory: %s",
                               strerror(errno));
            return false;
        }
        principal = defprinc;
    }

    /*
     * Import the name.  If principal was null, we use a host-based OID;
     * otherwise, specify that the name is a Kerberos principal.
     */
    name_buffer.value = (char *) principal;
    name_buffer.length = strlen(principal) + 1;
    if (defprinc == NULL)
        oid = GSS_C_NT_USER_NAME;
    else
        oid = GSS_C_NT_HOSTBASED_SERVICE;
    major = gss_import_name(&minor, &name_buffer, oid, name);
    if (defprinc != NULL)
        free(defprinc);
    if (major != GSS_S_COMPLETE) {
        internal_gssapi_error(r, "parsing name", major, minor);
        return false;
    }
    return true;
}


/*
 * Import the client credentials from a designated Kerberos ticket cache.
 *
 * This code is used if we have Kerberos libraries available and the GSS-API
 * implementation supports gss_krb5_import_cred.  In that case, we can tell
 * GSS-API which ticket cache to use.  Otherwise, we have to either set a
 * global GSS-API variable with gss_krb5_ccache_name or just use whatever the
 * default is.  The other cases are handled in remctl_set_ccache.
 */
#ifdef HAVE_GSS_KRB5_IMPORT_CRED
static bool
internal_set_cred(struct remctl *r, gss_cred_id_t *gss_cred)
{
    krb5_error_code code;
    OM_uint32 major, minor;

    if (r->krb_ctx == NULL) {
        code = krb5_init_context(&r->krb_ctx);
        if (code != 0) {
            internal_krb5_error(r, "opening ticket cache", code);
            return false;
        }
    }
    code = krb5_cc_resolve(r->krb_ctx, r->ccache, &r->krb_ccache);
    if (code != 0) {
        internal_krb5_error(r, "opening ticket cache", code);
        return false;
    }
    major = gss_krb5_import_cred(&minor, r->krb_ccache, NULL, NULL, gss_cred);
    if (major != GSS_S_COMPLETE) {
        internal_gssapi_error(r, "importing ticket cache", major, minor);
        return false;
    }
    return true;
}
#else /* !HAVE_GSS_KRB5_IMPORT_CRED */
static bool
internal_set_cred(struct remctl *r UNUSED, gss_cred_id_t *gss_cred UNUSED)
{
    return false;
}
#endif /* !HAVE_GSS_KRB5_IMPORT_CRED */


/*
 * Open a new connection to a server.  Returns true on success, false on
 * failure.  On failure, sets the error message appropriately.
 */
bool
internal_open(struct remctl *r, const char *host, const char *principal)
{
    int status, flags;
    gss_buffer_desc send_tok, recv_tok, *token_ptr;
    gss_buffer_desc empty_token = { 0, (void *) "" };
    gss_name_t name = GSS_C_NO_NAME;
    gss_cred_id_t gss_cred = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t gss_context = GSS_C_NO_CONTEXT;
    OM_uint32 major, minor, init_minor, gss_flags;
    static const OM_uint32 wanted_gss_flags
        = (GSS_C_MUTUAL_FLAG | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG
           | GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG);
    static const OM_uint32 req_gss_flags
        = (GSS_C_MUTUAL_FLAG | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG);

    /* Import the name. */
    if (!internal_import_name(r, host, principal, &name))
        goto fail;

    /* If the user has specified a Kerberos ticket cache, import it. */
    if (r->ccache != NULL)
        if (!internal_set_cred(r, &gss_cred))
            goto fail;

    /*
     * Default to protocol version two, but if some other protocol is already
     * set in the remctl struct, don't override.  This facility is used only
     * for testing currently.
     */
    if (r->protocol == 0)
        r->protocol = 2;

    /* Send the initial negotiation token. */
    status = token_send(r->fd, TOKEN_NOOP | TOKEN_CONTEXT_NEXT | TOKEN_PROTOCOL,
                        &empty_token, r->timeout);
    if (status != TOKEN_OK) {
        internal_token_error(r, "sending initial token", status, 0, 0);
        goto fail;
    }

    /*
     * Perform the context-establishment loop.
     *
     * On each pass through the loop, token_ptr points to the token to send to
     * the server (or GSS_C_NO_BUFFER on the first pass).  Every generated
     * token is stored in send_tok which is then transmitted to the server;
     * every received token is stored in recv_tok, which token_ptr is then set
     * to, to be processed by the next call to gss_init_sec_context.
     *
     * GSS-API guarantees that send_tok's length will be non-zero if and only
     * if the server is expecting another token from us, and that
     * gss_init_sec_context returns GSS_S_CONTINUE_NEEDED if and only if the
     * server has another token to send us.
     *
     * We start with the assumption that we're going to do protocol v2, but if
     * the server ever drops TOKEN_PROTOCOL from the response, we fall back to
     * v1.
     */
    token_ptr = GSS_C_NO_BUFFER;
    do {
        major = gss_init_sec_context(&init_minor, gss_cred, &gss_context,
                    name, (const gss_OID) GSS_KRB5_MECHANISM, wanted_gss_flags,
                    0, NULL, token_ptr, NULL, &send_tok, &gss_flags, NULL);
        if (token_ptr != GSS_C_NO_BUFFER)
            free(recv_tok.value);

        /* If we have anything more to say, send it. */
        if (send_tok.length != 0) {
            flags = TOKEN_CONTEXT;
            if (r->protocol > 1)
                flags |= TOKEN_PROTOCOL;
            status = token_send(r->fd, flags, &send_tok, r->timeout);
            if (status != TOKEN_OK) {
                internal_token_error(r, "sending token", status, 0, 0);
                gss_release_buffer(&minor, &send_tok);
                goto fail;
            }
        }
        gss_release_buffer(&minor, &send_tok);

        /* On error, report the error and abort. */
        if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED) {
            internal_gssapi_error(r, "initializing context", major,
                                  init_minor);
            goto fail;
        }

        /* If we're still expecting more, retrieve it. */
        if (major == GSS_S_CONTINUE_NEEDED) {
            status = token_recv(r->fd, &flags, &recv_tok, TOKEN_MAX_LENGTH,
                                r->timeout);
            if (status != TOKEN_OK) {
                internal_token_error(r, "receiving token", status, major,
                                     minor);
                goto fail;
            }
            if (r->protocol > 1 && (flags & TOKEN_PROTOCOL) != TOKEN_PROTOCOL)
                r->protocol = 1;
            token_ptr = &recv_tok;
        }
    } while (major == GSS_S_CONTINUE_NEEDED);

    /*
     * If the flags we get back from the server are bad and we're doing
     * protocol v2, report an error and abort.  This must be done after
     * establishing the context, since Heimdal doesn't report all flags until
     * context negotiation is complete.
     */
    if (r->protocol > 1 && (gss_flags & req_gss_flags) != req_gss_flags) {
        internal_set_error(r, "server did not negotiate acceptable GSS-API"
                           " flags");
        goto fail;
    }

    /* Success.  Set the context in the struct remctl object. */
    r->context = gss_context;
    r->ready = 0;
    gss_release_name(&minor, &name);
    return true;

fail:
    socket_close(r->fd);
    r->fd = INVALID_SOCKET;
    if (name != GSS_C_NO_NAME)
        gss_release_name(&minor, &name);
    if (gss_context != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor, &gss_context, GSS_C_NO_BUFFER);
    return false;
}
