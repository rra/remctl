/*
 * Internal support functions for the remctl client library.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on prior work by Anton Ushakov
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2002-2010, 2012-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CLIENT_INTERNAL_H
#define CLIENT_INTERNAL_H 1

#include <config.h>
#include <portable/gssapi.h>
#ifdef HAVE_KRB5
# include <portable/krb5.h>
#endif
#include <portable/macros.h>
#include <portable/socket.h>
#include <portable/stdbool.h>
#include <sys/types.h>

/* Forward declaration to avoid unnecessary includes. */
struct iovec;

/* Private structure that holds the details of an open remctl connection. */
struct remctl {
    const char *host;           /* From remctl_open, stored here because */
    unsigned short port;        /*   remctl v1 requires opening a new    */
    const char *principal;      /*   connection for each command.        */
    int protocol;               /* Protocol version. */
    char *source;               /* Source address for connection. */
    time_t timeout;
    char *ccache;               /* Path to client ticket cache. */
    socket_type fd;
    gss_ctx_id_t context;
    char *error;
    struct remctl_output *output;
    int status;
    bool ready;                 /* If true, we are expecting server output. */

    /* Used to hold state for remctl_set_ccache. */
#ifdef HAVE_KRB5
    krb5_context krb_ctx;
    krb5_ccache krb_ccache;
#endif
};

BEGIN_DECLS

/* Internal functions should all default to hidden visibility. */
#pragma GCC visibility push(hidden)

/* Helper functions to set errors. */
void internal_set_error(struct remctl *, const char *, ...)
    __attribute__((__format__(printf, 2, 3), __nonnull__));
void internal_gssapi_error(struct remctl *, const char *error,
                           OM_uint32 major, OM_uint32 minor);
#ifdef HAVE_KRB5
void internal_krb5_error(struct remctl *, const char *error,
                         krb5_error_code code);
#endif
void internal_token_error(struct remctl *, const char *error, int status,
                          OM_uint32 major, OM_uint32 minor);

/* Wipe and free the output token. */
void internal_output_wipe(struct remctl_output *);

/* Establish a network connection */
socket_type internal_connect(struct remctl *, const char *, unsigned short);

/* General connection opening and negotiation function. */
bool internal_open(struct remctl *, const char *host, const char *principal);

/* Send a protocol v1 command. */
bool internal_v1_commandv(struct remctl *, const struct iovec *command,
                          size_t count);

/* Read a protocol v1 response. */
struct remctl_output *internal_v1_output(struct remctl *);

/* Send a protocol v2 command. */
bool internal_v2_commandv(struct remctl *, const struct iovec *command,
                          size_t count);

/* Send a protocol v3 NOOP command. */
bool internal_noop(struct remctl *);

/* Send a protocol v2 QUIT command. */
bool internal_v2_quit(struct remctl *);

/* Read a protocol v2 response. */
struct remctl_output *internal_v2_output(struct remctl *);

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !CLIENT_INTERNAL_H */
