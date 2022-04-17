/*
 * Public interface to remctl client library.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on prior work by Anton Ushakov
 * Copyright 2019 Russ Allbery <eagle@eyrie.org>
 * Copyright 2002-2008, 2011-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef REMCTL_H
#define REMCTL_H 1

#include <sys/types.h> /* size_t */
#include <time.h>      /* time_t */

/*
 * __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
 * could you use the __format__ form of the attributes, which is what we use
 * (to avoid confusion with other macros), and only with gcc 2.96 can you use
 * the attribute __malloc__.  2.96 is very old, so don't bother trying to get
 * the other attributes to work with GCC versions between 2.7 and 2.96.
 */
#ifndef __attribute__
#    if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#        define __attribute__(spec) /* empty */
#    endif
#endif

/*
 * LLVM and Clang pretend to be GCC but don't support all of the __attribute__
 * settings that GCC does.  For them, suppress warnings about unknown
 * attributes on declarations.  This unfortunately will affect the entire
 * compilation context, but there's no push and pop available.
 */
#if !defined(__attribute__) && (defined(__llvm__) || defined(__clang__))
#    pragma GCC diagnostic ignored "-Wattributes"
#endif

/*
 * Normally we treat iovec as an opaque struct and clients who want to use the
 * iovec interface need to include <sys/uio.h> themselves.  However, Windows
 * doesn't provide this struct, so we define it for Windows.  It will already
 * be defined by remctl's internal build system, so deal with that.
 */
#if defined(_WIN32) && !defined(PORTABLE_UIO_H)
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#else
struct iovec;
#endif

/* Forward declarations of socket structs for alternate remctl_open APIs. */
struct addrinfo;
struct sockaddr;

/*
 * BEGIN_DECLS is used at the beginning of declarations so that C++
 * compilers don't mangle their names.  END_DECLS is used at the end.
 */
#undef BEGIN_DECLS
#undef END_DECLS
#ifdef __cplusplus
#    define BEGIN_DECLS extern "C" {
#    define END_DECLS   }
#else
#    define BEGIN_DECLS /* empty */
#    define END_DECLS   /* empty */
#endif

/* The standard remctl port and the legacy port used before 2.11. */
#define REMCTL_PORT     4373
#define REMCTL_PORT_OLD 4444

/* The standard remctl service name for /etc/services. */
#define REMCTL_SERVICE  "remctl"

/* Used to hold the return from a simple remctl call. */
struct remctl_result {
    char *error;       /* remctl error if non-NULL. */
    char *stdout_buf;  /* Standard output. */
    size_t stdout_len; /* Length of standard output. */
    char *stderr_buf;  /* Standard error. */
    size_t stderr_len; /* Length of standard error. */
    int status;        /* Exit status of remote command. */
};

/* The type of a remctl_output struct. */
enum remctl_output_type {
    REMCTL_OUT_OUTPUT,
    REMCTL_OUT_STATUS,
    REMCTL_OUT_ERROR,
    REMCTL_OUT_DONE
};

/* Used to return incremental output from a persistant connection. */
struct remctl_output {
    enum remctl_output_type type;
    char *data;
    size_t length;
    int stream; /* 1 == stdout, 2 == stderr */
    int status; /* Exit status of remote command. */
    int error;  /* Remote error code. */
};

/* Opaque struct representing an open remctl connection. */
struct remctl;

BEGIN_DECLS

/*
 * First, the simple interface.  Given a host, a port (may be 0 to use
 * REMCTL_PORT with fallback to REMCTL_PORT_OLD), the principal to
 * authenticate as (may be NULL to use host/<host>), and a command (as a
 * null-terminated argv-style vector), run the command on that host and port
 * and return a struct remctl_result.  The result should be freed with
 * remctl_result_free.
 */
struct remctl_result *remctl(const char *host, unsigned short port,
                             const char *principal, const char **command)
    __attribute__((__nonnull__(1, 4), __malloc__));
void remctl_result_free(struct remctl_result *);

/*
 * Now, the more complex persistant interface.  The basic housekeeping
 * functions.  port may be 0, in which case REMCTL_PORT is used with fallback
 * to REMCTL_PORT_OLD.  principal may be NULL, in which case host/<host> is
 * used (with no transformations applied to host at all).
 */
struct remctl *remctl_new(void) __attribute__((__malloc__));
int remctl_open(struct remctl *, const char *host, unsigned short port,
                const char *principal) __attribute__((__nonnull__(1, 2)));
void remctl_close(struct remctl *);

/*
 * Some alternate interfaces for opening connections.  In each of these,
 * principal may be NULL, in which case host/<host> is used, as for the basic
 * remctl_open.  If principal is not NULL, then host is not used and may be
 * NULL.  Connections established using these interfaces do not support
 * automatic connection reopening, which means they cannot be used to send
 * more than one command to a v1 server.
 */
int remctl_open_addrinfo(struct remctl *r, const char *host,
                         const struct addrinfo *ai, const char *principal)
    __attribute__((__nonnull__(1, 3)));
int remctl_open_sockaddr(struct remctl *r, const char *host,
                         const struct sockaddr *addr, int addrlen,
                         const char *principal)
    __attribute__((__nonnull__(1, 3)));

/* Windows uses a SOCKET type for sockets instead of an integer. */
#ifdef _WIN32
int remctl_open_fd(struct remctl *r, const char *host, SOCKET fd,
                   const char *principal) __attribute__((__nonnull__(1)));
#else
int remctl_open_fd(struct remctl *r, const char *host, int fd,
                   const char *principal) __attribute__((__nonnull__(1)));
#endif


/*
 * Set the Kerberos credential cache for client connections.  This must be
 * called before remctl_open.  Takes a string representing the Kerberos
 * credential cache name (the format may vary based on the underlying Kerberos
 * implementation).  Returns true on success and false on failure.
 *
 * Callers should be prepared for failure for GSS-API implementations that do
 * not support setting the Kerberos ticket cache.  A reasonable fallback is to
 * set the KRB5CCNAME environment variable.
 *
 * Be aware that this function sets the Kerberos credential cache globally for
 * all uses of GSS-API by that process.  The GSS-API does not provide a way of
 * setting it only for one particular GSS-API context.
 */
int remctl_set_ccache(struct remctl *, const char *)
    __attribute__((__nonnull__));

/*
 * Set the source address for connections.  If remctl_set_source_ip is called
 * before remctl_open, the IP address passed into remctl_set_source_ip will be
 * used as the source address.  This may be NULL to use the default system
 * source address; otherwise, it should be either an IPv4 or an IPv6 address.
 * Returns true on success, false on failure.  On failure, use remctl_error to
 * get the error.
 */
int remctl_set_source_ip(struct remctl *, const char *)
    __attribute__((__nonnull__));

/*
 * Set the network timeout, which may be 0 to not use any timeout (the
 * default).  If remctl_set_timeout is called before remctl_open, that timeout
 * (in seconds) will be used for each network action, both connect and packet
 * reads and writes.  Returns true on success, false on failure (only possible
 * with an invalid timeout, such as a negative value).  On failure, use
 * remctl_error to get the error.
 */
int remctl_set_timeout(struct remctl *, time_t) __attribute__((__nonnull__));

/*
 * Send a complete remote command.  Returns true on success, false on failure.
 * On failure, use remctl_error to get the error.  There are two forms of this
 * function; remctl_command takes a NULL-terminated array of nul-terminated
 * strings and remctl_commandv takes an array of struct iovecs of length
 * count.  The latter form should be used for binary data.
 */
int remctl_command(struct remctl *, const char **command)
    __attribute__((__nonnull__));
int remctl_commandv(struct remctl *, const struct iovec *, size_t count)
    __attribute__((__nonnull__));

/*
 * Send a NOOP message to the server and read the NOOP reply.  This is
 * normally used to keep a connection alive (through a firewall with timeouts,
 * for example) while awaiting subsequent commands.  Returns true on success
 * and false on failure.  On failure, use remctl_error to get the error.
 *
 * This is a protocol version 3 message and requires a server that supports
 * it, so the caller should be prepared to handle an error return and fall
 * back on reopening the connection when necessary.
 */
int remctl_noop(struct remctl *) __attribute__((__nonnull__));

/*
 * Retrieve output from the remote server.  Each call to this function on the
 * same connection invalidates the previous returned remctl_output struct, so
 * copy any data that should be persistant before calling this function again.
 *
 * This function will return zero or more REMCTL_OUT_OUTPUT types followed by
 * a REMCTL_OUT_STATUS type, *or* a REMCTL_OUT_ERROR type.  In either case,
 * any subsequent call before sending a new command will return
 * REMCTL_OUT_DONE.  If the function returns NULL, an internal error occurred;
 * call remctl_error to retrieve the error message.
 *
 * The remctl_output struct should *not* be freed by the caller.  It will be
 * invalidated after another call to remctl_output or to remctl_close on the
 * same connection.
 */
struct remctl_output *remctl_output(struct remctl *)
    __attribute__((__nonnull__));

/*
 * Call remctl_error after an error return to retrieve the internal error
 * message.  The returned error string will be invalidated by any subsequent
 * call to a remctl library function.
 */
const char *remctl_error(struct remctl *) __attribute__((__nonnull__));

END_DECLS

#endif /* !REMCTL_H */
