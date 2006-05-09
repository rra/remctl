/*  $Id$
**
**  Internal support functions for the remctl client library.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Based on prior work by Anton Ushakov <antonu@stanford.edu>
*/

#ifndef CLIENT_INTERNAL_H
#define CLIENT_INTERNAL_H 1

#include <config.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* Private structure that holds the details of an open remctl connection. */
struct remctl {
    int protocol;
    int fd;
    gss_ctx_id_t context;
    char *error;
    struct remctl_output *output;
    int status;
};

/* Token types and flags. */
#define TOKEN_NOOP              (1 << 0)
#define TOKEN_CONTEXT           (1 << 1)
#define TOKEN_DATA              (1 << 2)
#define TOKEN_MIC               (1 << 3)
#define TOKEN_CONTEXT_NEXT      (1 << 4)
#define TOKEN_SEND_MIC          (1 << 5)
#define TOKEN_PROTOCOL          (1 << 6)

/* Helper functions to set errors. */
void _remctl_set_error(struct remctl *, const char *, ...);
void _remctl_gssapi_error(struct remctl *, const char *error,
                          OM_uint32 major, OM_uint32 minor);
void _remctl_token_error(struct remctl *, const char *error, int status,
                         OM_uint32 major, OM_uint32 minor);


#endif /* !CLIENT_INTERNAL_H */
