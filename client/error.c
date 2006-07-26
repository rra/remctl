/*  $Id$
**
**  Error handling for the remctl client library.
**
**  Written by Russ Allbery <rra@stanford.edu>
**
**  A set of helper routines to do error handling inside the remctl client
**  library.  These mostly involve setting the error parameter in the remctl
**  struct to something appropriate so that the next call to remctl_error will
**  return the appropriate details.
*/

#include <config.h>
#include <system.h>

#include <errno.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <client/internal.h>
#include <util/util.h>

/*
**  Internal function to set the error message, freeing an old error message
**  if one is present.
*/
void
_remctl_set_error(struct remctl *r, const char *format, ...)
{
    va_list args;
    int status;

    if (r->error != NULL)
        free(r->error);
    va_start(args, format);
    status = vasprintf(&r->error, format, args);
    va_end(args);

    /* If vasprintf fails, there isn't much we can do, but make sure that at
       least the error is in a consistent state. */
    if (status < 0)
        r->error = NULL;
}


/*
**  Internal function to set the remctl error message from a GSS-API error
**  message.  Uses gss_display_status to get the internal error message.
*/
void
_remctl_gssapi_error(struct remctl *r, const char *error, OM_uint32 major,
                     OM_uint32 minor)
{
    char *string, *old;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx, status;

    if (r->error != NULL)
        free(r->error);
    string = NULL;
    msg_ctx = 0;
    do {
        gss_display_status(&status, major, GSS_C_GSS_CODE, GSS_C_NULL_OID,
                           &msg_ctx, &msg);
        if (string != NULL) {
            old = string;
            string = concat(string, ", ", msg.value, (char *) 0);
            free(old);
        } else {
            string = concat("GSS-API error ", error, ": ", msg.value,
                           (char *) 0);
        }
    } while (msg_ctx != 0);
    if (minor != 0) {
        msg_ctx = 0;
        do {
            gss_display_status(&status, minor, GSS_C_MECH_CODE,
                               GSS_C_NULL_OID, &msg_ctx, &msg);
            old = string;
            string = concat(string, ", ", msg.value, (char *) 0);
            free(old);
        } while (msg_ctx != 0);
    }
    r->error = string;
}


/*
**  Internal function to set the remctl error message from a token error.
**  Handles the various token failure codes from the token_send and token_recv
**  functions and their *_priv counterparts.
*/
void
_remctl_token_error(struct remctl *r, const char *error, int status,
                    OM_uint32 major, OM_uint32 minor)
{
    switch (status) {
    case TOKEN_OK:
        _remctl_set_error(r, "error %s", error);
        break;
    case TOKEN_FAIL_SYSTEM:
        _remctl_set_error(r, "error %s: %s", error, strerror(errno));
        break;
    case TOKEN_FAIL_INVALID:
        _remctl_set_error(r, "error %s: invalid token format", error);
        break;
    case TOKEN_FAIL_LARGE:
        _remctl_set_error(r, "error %s: token too larger", error);
        break;
    case TOKEN_FAIL_EOF:
        _remctl_set_error(r, "error %s: unexpected end of file", error);
        break;
    case TOKEN_FAIL_GSSAPI:
        _remctl_gssapi_error(r, error, major, minor);
        break;
    default:
        _remctl_set_error(r, "error %s: unknown error", error);
        break;
    }
}
