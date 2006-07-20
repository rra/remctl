/*  $Id$
**
**  Logging and error handling for the remctld server.
**
**  A set of helper routines to do error handling and other logging for
**  remctld, mostly wrappers around warn and die which will send errors to the
**  right place.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Copyright 2006 Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>

#include <errno.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <server/internal.h>
#include <util/util.h>

/*
**  Report a GSS-API failure using warn.  Uses gss_display_status to get the
**  internal error message.
*/
void
warn_gssapi(const char *error, OM_uint32 major, OM_uint32 minor)
{
    char *string, *old;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx, status;

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
    warn("%s", string);
    free(string);
}


/*
**  Report a token error using warn.
*/
void
warn_token(const char *error, int status, OM_uint32 major, OM_uint32 minor)
{
    switch (status) {
    case TOKEN_OK:
        warn("error %s", error);
        break;
    case TOKEN_FAIL_SYSTEM:
        syswarn("error %s: %s", error);
        break;
    case TOKEN_FAIL_INVALID:
        warn("error %s: invalid token format", error);
        break;
    case TOKEN_FAIL_LARGE:
        warn("error %s: token too larger", error);
        break;
    case TOKEN_FAIL_GSSAPI:
        warn_gssapi(error, major, minor);
        break;
    default:
        warn("error %s: unknown error", error);
        break;
    }
}


/*
**  Log a command.  Takes the argument vector, the configuration line that
**  matched the command, and the principal running the command.
*/
void
server_log_command(struct vector *argv, struct confline *cline,
                   const char *user)
{
    char *command;
    unsigned int i, j;
    struct cvector *masked;
    const char *arg;

    if (cline == NULL || cline->logmask == NULL)
        command = vector_join(argv, " ");
    else {
        masked = cvector_new();
        for (i = 0; i < argv->count; i++) {
            arg = argv->strings[i];
            for (j = 0; j < cline->logmask->count; j++) {
                if (atoi(cline->logmask->strings[j]) == (int) i) {
                    arg = "**MASKED**";
                    break;
                }
            }
            cvector_add(masked, arg);
        }
        command = cvector_join(masked, " ");
        cvector_free(masked);
    }
    notice("COMMAND from %s: %s", user, command);
    free(command);
}
