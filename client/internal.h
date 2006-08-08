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

#include <sys/uio.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* BEGIN_DECLS is used at the beginning of declarations so that C++
   compilers don't mangle their names.  END_DECLS is used at the end. */
#undef BEGIN_DECLS
#undef END_DECLS
#ifdef __cplusplus
# define BEGIN_DECLS    extern "C" {
# define END_DECLS      }
#else
# define BEGIN_DECLS    /* empty */
# define END_DECLS      /* empty */
#endif

BEGIN_DECLS

/* Private structure that holds the details of an open remctl connection. */
struct remctl {
    const char *host;           /* From remctl_open, stored here because */
    unsigned short port;        /*   remctl v1 requires opening a new    */
    const char *principal;      /*   connection for each command.        */
    int protocol;
    int fd;
    gss_ctx_id_t context;
    char *error;
    struct remctl_output *output;
    int status;
    int ready;                  /* If true, we are expecting server output. */
};

/* Helper functions to set errors. */
void internal_set_error(struct remctl *, const char *, ...);
void internal_gssapi_error(struct remctl *, const char *error,
                           OM_uint32 major, OM_uint32 minor);
void internal_token_error(struct remctl *, const char *error, int status,
                          OM_uint32 major, OM_uint32 minor);

/* Other helper functions. */
void internal_output_wipe(struct remctl_output *);

/* General connection opening and negotiation function. */
int internal_open(struct remctl *, const char *host, unsigned short port,
                  const char *principal);

/* Protocol one functions. */
int internal_v1_commandv(struct remctl *, const struct iovec *command,
                         size_t count, int finished);
struct remctl_output *internal_v1_output(struct remctl *r);

/* Protocol two functions. */
int internal_v2_commandv(struct remctl *, const struct iovec *command,
                         size_t count, int finished);
int internal_v2_quit(struct remctl *);
struct remctl_output *internal_v2_output(struct remctl *r);

END_DECLS

#endif /* !CLIENT_INTERNAL_H */
