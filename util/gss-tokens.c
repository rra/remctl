/*  $Id$
**
**  GSS token handling routines.
**
**  Higher-level wrappers around the low-level token handling routines that
**  apply integrity and privacy protection to the token data before sending.
**  token_send_priv and token_recv_priv are similar to token_send and
**  token_recv except that they also take a GSS-API context and a GSS-API
**  major and minor status to report errors.
*/

#include <config.h>
#include <system.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <util/util.h>

/* If we're running the test suite, call testing versions of the token
   functions. */
#if TESTING
# define token_send fake_token_send
# define token_recv fake_token_recv
enum token_status token_send(int, int, gss_buffer_t);
enum token_status token_recv(int, int *, gss_buffer_t, size_t);
#endif


/*
**  Wraps, encrypts, and sends a data payload token.  Takes the file
**  descriptor to send to, the GSS-API context, the flags to send with the
**  token, the token, and the status variables.  Returns TOKEN_OK on success
**  and TOKEN_FAIL_SYSTEM or TOKEN_FAIL_GSSAPI on failure.  If the latter is
**  returned, the major and minor status variables will be set to something
**  useful.
*/
enum token_status
token_send_priv(int fd, gss_ctx_id_t ctx, int flags, gss_buffer_t tok,
                OM_uint32 *major, OM_uint32 *minor)
{
    gss_buffer_desc out;
    int state;
    enum token_status status;

    *major = gss_wrap(minor, ctx, 1, GSS_C_QOP_DEFAULT, tok, &state, &out);
    if (*major != GSS_S_COMPLETE)
        return TOKEN_FAIL_GSSAPI;
    status = token_send(fd, flags, &out);
    gss_release_buffer(minor, &out);
    return status;
}


/*
**  Receives and unwraps a data payload token.  Takes the file descriptor,
**  GSS-API context, a pointer into which to storge the flags, a buffer for
**  the message, and a place to put GSS-API major and minor status.  Returns
**  TOKEN_OK on success or one of the TOKEN_FAIL_* statuses on failure.
*/
enum token_status
token_recv_priv(int fd, gss_ctx_id_t ctx, int *flags, gss_buffer_t tok,
                size_t max, OM_uint32 *major, OM_uint32 *minor)
{
    gss_buffer_desc in;
    int state;
    enum token_status status;

    status = token_recv(fd, flags, &in, max);
    if (status != TOKEN_OK)
        return status;
    *major = gss_unwrap(minor, ctx, &in, tok, &state, NULL);
    if (*major != GSS_S_COMPLETE)
        return TOKEN_FAIL_GSSAPI;
    return TOKEN_OK;
}
