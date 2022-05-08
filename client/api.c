/*
 * Entry points for the remctl library API.
 *
 * All of the high-level entry points for the remctl library API, defined in
 * remctl.h, are found here.  The detailed implementation of these APIs are
 * in other source files.
 *
 * All public functions defined here should use int to return boolean values.
 * We don't try to set up the portability glue to use bool in our public
 * headers.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2018-2020, 2022 Russ Allbery <eagle@eyrie.org>
 * Copyright 2002-2009, 2011-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/gssapi.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/socket.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <errno.h>
#include <time.h>

#include <client/internal.h>
#include <client/remctl.h>
#include <util/macros.h>
#include <util/network.h>


/*
 * Handle an internal failure for the simplified interface.  We try to grab
 * the remctl error and put it into the error field in the remctl result, but
 * if that fails, we free the remctl result and return NULL to indicate a
 * fatal error.
 */
static struct remctl_result *
internal_fail(struct remctl *r, struct remctl_result *result)
{
    const char *error;

    error = remctl_error(r);
    if (error == NULL) {
        remctl_close(r);
        remctl_result_free(result);
        return NULL;
    } else {
        result->error = strdup(error);
        remctl_close(r);
        if (result->error != NULL)
            return result;
        else {
            free(result);
            return NULL;
        }
    }
}


/*
 * Given a struct remctl_result into which we're accumulating output and a
 * struct remctl_output that contains a fragment of output, append the output
 * to the appropriate slot in the result.  This is not particularly efficient.
 * Returns false if something fails and tries to set result->error; if we
 * can't even do that, make sure it's set to NULL.
 */
static bool
internal_output_append(struct remctl_result *result,
                       struct remctl_output *output)
{
    char **buffer = NULL;
    size_t *length = NULL;
    char *old, *newbuf;
    size_t oldlen, newlen;
    int status;

    if (output->type == REMCTL_OUT_ERROR)
        buffer = &result->error;
    else if (output->type == REMCTL_OUT_OUTPUT && output->stream == 1) {
        buffer = &result->stdout_buf;
        length = &result->stdout_len;
    } else if (output->type == REMCTL_OUT_OUTPUT && output->stream == 2) {
        buffer = &result->stderr_buf;
        length = &result->stderr_len;
    } else if (output->type == REMCTL_OUT_OUTPUT) {
        free(result->error);
        status =
            asprintf(&result->error, "bad output stream %d", output->stream);
        if (status < 0)
            result->error = NULL;
        return false;
    } else {
        free(result->error);
        result->error = strdup("internal error: bad output type");
        return false;
    }

    /* We've done our setup, so now we can do the actual manipulation. */
    old = *buffer;
    if (length != NULL)
        oldlen = *length;
    else if (old == NULL)
        oldlen = 0;
    else
        oldlen = strlen(old);
    newlen = oldlen + output->length;
    if (output->type == REMCTL_OUT_ERROR)
        newlen++;
    newbuf = realloc(*buffer, newlen);
    if (newbuf == NULL) {
        free(result->error);
        result->error = strdup("cannot allocate memory");
        return false;
    }
    *buffer = newbuf;
    if (length != NULL)
        *length = newlen;
    memcpy(*buffer + oldlen, output->data, output->length);
    if (output->type == REMCTL_OUT_ERROR)
        (*buffer)[newlen - 1] = '\0';
    return true;
}


/*
 * The simplified interface.  Given a host, a port, and a command (as a
 * null-terminated argv-style vector), run the command on that host and port
 * and return a struct remctl_result.  The result should be freed with
 * remctl_result_free.
 */
struct remctl_result *
remctl(const char *host, unsigned short port, const char *principal,
       const char **command)
{
    struct remctl *r = NULL;
    struct remctl_result *result;
    enum remctl_output_type type;

    result = calloc(1, sizeof(struct remctl_result));
    if (result == NULL)
        return NULL;
    r = remctl_new();
    if (r == NULL) {
        free(result);
        return NULL;
    }
    if (!remctl_open(r, host, port, principal))
        return internal_fail(r, result);
    if (!remctl_command(r, command))
        return internal_fail(r, result);
    do {
        struct remctl_output *output;

        output = remctl_output(r);
        if (output == NULL)
            return internal_fail(r, result);
        type = output->type;
        if (type == REMCTL_OUT_OUTPUT || type == REMCTL_OUT_ERROR) {
            if (!internal_output_append(result, output)) {
                if (result->error != NULL)
                    return result;
                else {
                    free(result);
                    return NULL;
                }
            }
        } else if (type == REMCTL_OUT_STATUS) {
            result->status = output->status;
        } else {
            remctl_close(r);
            return result;
        }
    } while (type == REMCTL_OUT_OUTPUT);
    remctl_close(r);
    return result;
}


/*
 * Free a struct remctl_result returned by remctl.
 */
void
remctl_result_free(struct remctl_result *result)
{
    if (result == NULL)
        return;
    free(result->error);
    free(result->stdout_buf);
    free(result->stderr_buf);
    free(result);
}


/*
 * Create a new remctl object.  Don't attempt to connect here; we do that
 * separately so that we can still use the object to store error messages
 * from connection failures.  Return NULL on memory allocation failures or
 * socket initialization failures (Windows).
 */
struct remctl *
remctl_new(void)
{
    struct remctl *r;

    if (!socket_init())
        return NULL;
    r = calloc(1, sizeof(struct remctl));
    if (r == NULL)
        return NULL;
    r->fd = INVALID_SOCKET;
    r->context = GSS_C_NO_CONTEXT;
    return r;
}


/*
 * Set the Kerberos credential cache for client connections.  Takes a string
 * representing the Kerberos credential cache name (the format may vary based
 * on the underlying Kerberos implementation).  When the GSS-API context is
 * created for a client connection in a subsequent remctl_open, this will be
 * set as the Kerberos credential cache.  Returns true on success and false on
 * failure.
 *
 * Callers should be prepared for failure for GSS-API implementations that do
 * not support setting the Kerberos ticket cache.  A reasonable fallback is to
 * set the KRB5CCNAME environment variable.
 *
 * If Kerberos libraries and the gss_krb5_import_cred function are available,
 * this will be per-context.  Otherwise, be aware that this function sets the
 * Kerberos credential cache globally for all uses of GSS-API by that process.
 */
#if defined(HAVE_KRB5) && defined(HAVE_GSS_KRB5_IMPORT_CRED)
int
remctl_set_ccache(struct remctl *r, const char *ccache)
{
    char *copy;

    copy = strdup(ccache);
    if (copy == NULL) {
        internal_set_error(r, "cannot allocate memory: %s", strerror(errno));
        return 0;
    }
    free(r->ccache);
    r->ccache = copy;
    return 1;
}
#elif defined(HAVE_GSS_KRB5_CCACHE_NAME)
int
remctl_set_ccache(struct remctl *r, const char *ccache)
{
    OM_uint32 major, minor;

    major = gss_krb5_ccache_name(&minor, ccache, NULL);
    if (major != GSS_S_COMPLETE) {
        internal_gssapi_error(r, "cannot set credential cache", major, minor);
        return 0;
    }
    return 1;
}
#else  /* !HAVE_GSS_KRB5_CCACHE_NAME */
int
remctl_set_ccache(struct remctl *r, const char *ccache UNUSED)
{
    internal_set_error(r, "setting credential cache not supported");
    return 0;
}
#endif /* !HAVE_GSS_KRB5_CCACHE_NAME */


/*
 * Set the source address for client connections.  Takes a string, which may
 * be NULL to use whatever the default source address is.  The string will be
 * parsed as an IPv4 or IPv6 address, and only connections over the
 * corresponding protocol will be attempted.  Returns true on success and
 * false on failure to allocate memory.
 */
int
remctl_set_source_ip(struct remctl *r, const char *source)
{
    char *copy;

    copy = strdup(source);
    if (copy == NULL) {
        internal_set_error(r, "cannot allocate memory: %s", strerror(errno));
        return 0;
    }
    free(r->source);
    r->source = copy;
    return 1;
}


/*
 * Set the network timeout in seconds, which may be 0 to not use any timeout
 * (the default).  Returns true on success, false on an invalid timeout, such
 * as a negative value.
 */
int
remctl_set_timeout(struct remctl *r, time_t timeout)
{
    if (timeout < 0) {
        internal_set_error(r, "invalid timeout %ld", (long) timeout);
        return 0;
    }
    r->timeout = timeout;
    return 1;
}


static void
internal_reset(struct remctl *r)
{
    if (r->fd != -1) {
        if (r->protocol > 1)
            internal_v2_quit(r);
        socket_close(r->fd);
    }
    free(r->error);
    r->error = NULL;
    if (r->output != NULL) {
        internal_output_wipe(r->output);
        free(r->output);
        r->output = NULL;
    }
}


/*
 * Open a new persistant remctl connection to a server, given the host, port,
 * and principal.  Returns true on success and false on failure.
 */
int
remctl_open(struct remctl *r, const char *host, unsigned short port,
            const char *principal)
{
    bool port_fallback = false;
    socket_type fd = INVALID_SOCKET;
    char *old_error;

    /* Reset and reconfigure the client object. */
    internal_reset(r);
    r->host = host;
    r->port = port;
    r->principal = principal;

    /*
     * If port is 0, default to trying the standard port and then falling back
     * on the old port.
     */
    if (port == 0) {
        port = REMCTL_PORT;
        port_fallback = true;
    }

    /*
     * Make the network connection.  If we're doing a fallback to the legacy
     * port, preserve the error message from the initial connect and report
     * it by preference to the error message for the legacy connect.
     */
    fd = internal_connect(r, host, port);
    if (fd == INVALID_SOCKET && port_fallback) {
        old_error = r->error;
        r->error = NULL;
        fd = internal_connect(r, host, REMCTL_PORT_OLD);
        if (fd == INVALID_SOCKET) {
            free(r->error);
            r->error = old_error;
        } else {
            free(old_error);
        }
    }
    if (fd == INVALID_SOCKET)
        return false;
    r->fd = fd;
    return internal_open(r, host, principal);
}


/*
 * Open a new remctl connection to a server, given the hostname, address
 * information, and principal.  At least one of host or principal is required.
 * Returns true on success and false on failure.
 */
int
remctl_open_addrinfo(struct remctl *r, const char *host,
                     const struct addrinfo *ai, const char *principal)
{
    socket_type fd;

    internal_reset(r);
    r->host = NULL;
    r->port = 0;
    r->principal = principal;

    /* Make the network connection. */
    fd = network_connect(ai, r->source, r->timeout);
    if (fd == INVALID_SOCKET) {
        internal_set_error(r, "cannot connect: %s",
                           socket_strerror(socket_errno));
        return false;
    }
    r->fd = fd;
    return internal_open(r, host, principal);
}


/*
 * Open a new remctl connection to a server, given the hostname, socket
 * address, and principal.  At least one of host or principal is required.
 * Returns true on success and false on failure.
 */
int
remctl_open_sockaddr(struct remctl *r, const char *host,
                     const struct sockaddr *addr, int addrlen,
                     const char *principal)
{
    struct addrinfo ai;

    memset(&ai, 0, sizeof(ai));
    ai.ai_family = addr->sa_family;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_protocol = IPPROTO_TCP;
    ai.ai_addrlen = addrlen;
    ai.ai_addr = (struct sockaddr *) addr;
    return remctl_open_addrinfo(r, host, &ai, principal);
}


/*
 * Open a new remctl connection to a server, given the hostname, socket,
 * and principal.  At least one of host or principal is required.
 * Returns true on success and false on failure.
 */
int
remctl_open_fd(struct remctl *r, const char *host, socket_type fd,
               const char *principal)
{
    internal_reset(r);
    r->host = NULL;
    r->port = 0;
    r->principal = principal;
    r->fd = fd;
    return internal_open(r, host, principal);
}


/*
 * Close a persistant remctl connection.
 */
void
remctl_close(struct remctl *r)
{
    OM_uint32 minor;

    /* Allow the passed struct to already be NULL. */
    if (r == NULL)
        return;

    /* If we have an open connection, shut it down. */
    if (r->protocol > 1 && r->fd != -1)
        internal_v2_quit(r);
    if (r->fd != INVALID_SOCKET) {
        shutdown(r->fd, SHUT_RDWR);
        socket_close(r->fd);
    }
    if (r->context != GSS_C_NO_CONTEXT) {
        gss_delete_sec_context(&minor, &r->context, GSS_C_NO_BUFFER);
    }

    /* If we have a registered ticket cache, free those resources. */
#ifdef HAVE_KRB5
    if (r->krb_ctx != NULL) {
        if (r->krb_ccache != NULL)
            krb5_cc_close(r->krb_ctx, r->krb_ccache);
        krb5_free_context(r->krb_ctx);
    }
#endif

    /* Free remaining resources. */
    free(r->source);
    free(r->ccache);
    free(r->error);
    if (r->output != NULL) {
        free(r->output->data);
        free(r->output);
    }
    free(r);

    /*
     * Always shut down the Windows socket library since we initialized it in
     * remctl_new.
     */
    socket_shutdown();
}


/*
 * Internal function to reopen the connection if it was closed and verify that
 * we have an open connection, and reset the error message.  Used by
 * remctl_commandv and remctl_noop.  Returns true on success and false on
 * failure.
 */
static bool
internal_reopen(struct remctl *r)
{
    if (r->fd == INVALID_SOCKET) {
        if (r->host == NULL) {
            internal_set_error(r, "no connection open");
            return false;
        }
        if (!remctl_open(r, r->host, r->port, r->principal))
            return false;
    }
    free(r->error);
    r->error = NULL;
    return true;
}


/*
 * Send a complete remote command.  Returns true on success, false on failure.
 * On failure, use remctl_error to get the error.  command is a
 * NULL-terminated array of nul-terminated strings.  finished is a boolean
 * flag that should be false when the command is not yet complete and true
 * when this is the final (or only) segment.
 *
 * Implement in terms of remctl_commandv.
 */
int
remctl_command(struct remctl *r, const char **command)
{
    struct iovec *vector;
    size_t count, i;
    int status;

    for (count = 0; command[count] != NULL; count++)
        ;
    if (count == 0) {
        internal_set_error(r, "cannot send empty command");
        return 0;
    }
    vector = calloc(count, sizeof(struct iovec));
    if (vector == NULL) {
        internal_set_error(r, "cannot allocate memory: %s", strerror(errno));
        return 0;
    }
    for (i = 0; i < count; i++) {
        vector[i].iov_base = (void *) command[i];
        vector[i].iov_len = strlen(command[i]);
    }
    status = remctl_commandv(r, vector, count);
    free(vector);
    return status;
}


/*
 * Same as remctl_command, but take the command as an array of struct iovecs
 * instead.  Use this form for binary data.
 */
int
remctl_commandv(struct remctl *r, const struct iovec *command, size_t count)
{
    if (!internal_reopen(r))
        return 0;
    if (r->protocol == 1)
        return internal_v1_commandv(r, command, count);
    else
        return internal_v2_commandv(r, command, count);
}


/*
 * Send a NOOP command, or return an error if we're using too old of a
 * protocol version.  Returns true on success, false on failure.  On failure,
 * use remctl_error to get the error.
 */
int
remctl_noop(struct remctl *r)
{
    if (!internal_reopen(r))
        return 0;
    if (r->protocol == 1) {
        internal_set_error(r, "NOOP message not supported");
        return 0;
    }
    return internal_noop(r);
}


/*
 * Helper function for remctl_output implementations.  Free and reset the
 * elements of the output struct, but don't free the output struct itself.
 */
void
internal_output_wipe(struct remctl_output *output)
{
    if (output == NULL)
        return;
    free(output->data);
    memset(output, 0, sizeof(*output));
    output->type = REMCTL_OUT_DONE;
}


/*
 * Retrieve output from the remote server.  Each call to this function on the
 * same connection invalidates the previous returned remctl_output struct.
 *
 * This function will return zero or more REMCTL_OUT_OUTPUT types followed by
 * a REMCTL_OUT_STATUS type, *or* a REMCTL_OUT_ERROR type.  In either case,
 * any subsequent call before sending a new command will return
 * REMCTL_OUT_DONE.  If the function returns NULL, an internal error occurred;
 * call remctl_error to retrieve the error message.
 *
 * The remctl_output struct should *not* be freed by the caller.  It will be
 * invalidated after another call to remctl_output or to remctl_close on the
 * same connection.
 */
struct remctl_output *
remctl_output(struct remctl *r)
{
    if (r->fd == INVALID_SOCKET && (r->protocol != 1 || r->host == NULL)) {
        internal_set_error(r, "no connection open");
        return NULL;
    }
    free(r->error);
    r->error = NULL;
    if (r->protocol == 1)
        return internal_v1_output(r);
    else
        return internal_v2_output(r);
}


/*
 * Returns the internal error message after a failure or "no error" if the
 * last command completed successfully.  This should generally only be called
 * after a failure.
 */
const char *
remctl_error(struct remctl *r)
{
    return (r->error != NULL) ? r->error : "no error";
}
