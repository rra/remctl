/*
 * Shared GSS-API error handling code.
 *
 * Helper functions to interpret GSS-API errors that can be shared between the
 * client and the server.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2007, 2010
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/gssapi.h>
#include <portable/system.h>

#include <util/gss-errors.h>


/*
 * Turn a GSS-API error code pair into a human-readable string, prefixed with
 * "GSS-API error" and the provided string.  Uses gss_display_status to get
 * the internal error message.  Returns a newly allocated string that the
 * caller must free.
 *
 * Don't use xmalloc here, since this may be inside the client library and die
 * inside a library isn't friendly.
 */
char *
gssapi_error_string(const char *prefix, OM_uint32 major, OM_uint32 minor)
{
    char *string, *old;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx, status, dummy;

    string = NULL;
    msg_ctx = 0;
    do {
        gss_display_status(&status, major, GSS_C_GSS_CODE,
                           (const gss_OID) GSS_KRB5_MECHANISM, &msg_ctx, &msg);
        if (string == NULL) {
            if (asprintf(&string, "GSS-API error %s: %s", prefix,
                         (char *) msg.value)
                < 0)
                string = NULL;
        } else {
            old = string;
            if (asprintf(&string, "%s, %s", old, (char *) msg.value) < 0)
                string = old;
            else
                free(old);
        }
        gss_release_buffer(&dummy, &msg);
    } while (msg_ctx != 0);
    if (minor != 0) {
        msg_ctx = 0;
        do {
            gss_display_status(&status, minor, GSS_C_MECH_CODE,
                               (const gss_OID) GSS_KRB5_MECHANISM, &msg_ctx,
                               &msg);
            old = string;
            if (asprintf(&string, "%s, %s", old, (char *) msg.value) < 0)
                string = old;
            else
                free(old);
            gss_release_buffer(&dummy, &msg);
        } while (msg_ctx != 0);
    }
    return string;
}
