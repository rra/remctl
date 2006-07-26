/* $Id$ */
/* Fake token_send and token_recv functions for testing. */

#include <config.h>
#include <system.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <util/util.h>

/* __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
   could you use the __format__ form of the attributes, which is what we use
   (to avoid confusion with other macros). */
#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __attribute__(spec)   /* empty */
# endif
#endif

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED  __attribute__((__unused__))

enum token_status fake_token_send(int, int, gss_buffer_t);
enum token_status fake_token_recv(int, int *, gss_buffer_t, size_t);

/* The token and flags are actually read from or written to these
   variables. */
char send_buffer[2048];
char recv_buffer[2048];
size_t send_length;
size_t recv_length;
int send_flags;
int recv_flags;

/* Accept a token write request and store it into the buffer. */
enum token_status
fake_token_send(int fd UNUSED, int flags, gss_buffer_t tok)
{
    if (tok->length > sizeof(send_buffer))
        return TOKEN_FAIL_SYSTEM;
    send_flags = flags;
    send_length = tok->length;
    memcpy(send_buffer, tok->value, tok->length);
    return TOKEN_OK;
}

/* Receive a token from the stored buffer and return it. */
enum token_status
fake_token_recv(int fd UNUSED, int *flags, gss_buffer_t tok, size_t max)
{
    if (recv_length > max)
        return TOKEN_FAIL_LARGE;
    tok->value = malloc(recv_length);
    if (tok->value == NULL)
        return TOKEN_FAIL_SYSTEM;
    memcpy(tok->value, recv_buffer, recv_length);
    tok->length = recv_length;
    *flags = recv_flags;
    return TOKEN_OK;
}
