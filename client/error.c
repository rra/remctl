/*
 * Error handling for the remctl client library.
 *
 * A set of helper routines to do error handling inside the remctl client
 * library.  These mostly involve setting the error parameter in the remctl
 * struct to something appropriate so that the next call to remctl_error will
 * return the appropriate details.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2008, 2010, 2013-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/gssapi.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <errno.h>

#include <client/internal.h>
#include <util/gss-errors.h>
#include <util/tokens.h>

/*
 * Internal function to set the error message, freeing an old error message if
 * one is present.
 */
void
internal_set_error(struct remctl *r, const char *format, ...)
{
    va_list args;
    int status;

    free(r->error);
    va_start(args, format);
    status = vasprintf(&r->error, format, args);
    va_end(args);

    /*
     * If vasprintf fails, there isn't much we can do, but make sure that at
     * least the error is in a consistent state.
     */
    if (status < 0)
        r->error = NULL;
}


/*
 * Internal function to set the remctl error message from a GSS-API error
 * message.
 */
void
internal_gssapi_error(struct remctl *r, const char *error, OM_uint32 major,
                      OM_uint32 minor)
{
    free(r->error);
    r->error = gssapi_error_string(error, major, minor);
}


/*
 * Internal function to set the remctl error message from a Kerberos error
 * message.
 */
#ifdef HAVE_KRB5
void
internal_krb5_error(struct remctl *r, const char *error, krb5_error_code code)
{
    const char *message;

    if (r->krb_ctx == NULL)
        internal_set_error(r, "error %s: cannot create Kerberos context",
                           error);
    message = krb5_get_error_message(r->krb_ctx, code);
    internal_set_error(r, "error %s: %s", error, message);
    krb5_free_error_message(r->krb_ctx, message);
}
#endif


/*
 * Internal function to set the remctl error message from a token error.
 * Handles the various token failure codes from the token_send and token_recv
 * functions and their *_priv counterparts.
 */
void
internal_token_error(struct remctl *r, const char *error, int status,
                     OM_uint32 major, OM_uint32 minor)
{
    const char *message;

    switch (status) {
    case TOKEN_OK:
        internal_set_error(r, "error %s", error);
        break;
    case TOKEN_FAIL_SYSTEM:
        internal_set_error(r, "error %s: %s", error, strerror(errno));
        break;
    case TOKEN_FAIL_SOCKET:
        message = socket_strerror(socket_errno);
        internal_set_error(r, "error %s: %s", error, message);
        break;
    case TOKEN_FAIL_INVALID:
        internal_set_error(r, "error %s: invalid token format", error);
        break;
    case TOKEN_FAIL_LARGE:
        internal_set_error(r, "error %s: token too larger", error);
        break;
    case TOKEN_FAIL_EOF:
        internal_set_error(r, "error %s: unexpected end of file", error);
        break;
    case TOKEN_FAIL_GSSAPI:
        internal_gssapi_error(r, error, major, minor);
        break;
    case TOKEN_FAIL_TIMEOUT:
        internal_set_error(r, "error %s: timed out", error);
        break;
    default:
        internal_set_error(r, "error %s: unknown error", error);
        break;
    }
}
