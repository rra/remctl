/*  $Id$
**
**  Utility functions for remctl.
**
**  Written by Anton Ushakov <antonu@stanford.edu>
**
**  Defines various utility functions used by both remctl and remctld to
**  handle the GSSAPI conversation, to encode and decode tokens, to read the
**  output from programs, to report GSSAPI problems, and similar purposes.
*/

#ifndef GSS_UTILS_H
#define GSS_UTILS_H 1

#include "config.h"

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

/* Send or receive a message via GSSAPI. */
int gss_sendmsg(gss_ctx_id_t, int flags, char *msg, OM_uint32 msglength);
int gss_recvmsg(gss_ctx_id_t, int *flags, char **msg, OM_uint32 *msglength);

/* Display GSSAPI information. */
void display_status(const char *msg, OM_uint32 maj_stat, OM_uint32 min_stat);
void display_ctx_flags(OM_uint32 flags);

/* Decode and display a token. */
void print_token(gss_buffer_t tok);

/* Used as the default max buffer for the argv passed into the server, and for 
   the return message from the server. */
#define MAXBUFFER               64000  

/* The maximum size of argc passed to the server. */
#define MAXCMDARGS              64

/* Maximum encrypted token size.  I notices it was roughly 6 - 7 times the
   clear text, so on the safe size, it's 10 * MAXBUFFER */
#define MAXENCRYPT              640000

#endif /* !GSS_UTILS_H */
