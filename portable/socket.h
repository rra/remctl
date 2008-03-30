/*  $Id$
**
**  Portability wrapper around <sys/socket.h> and friends.
**
**  This header file is the equivalent of:
**
**      #include <arpa/inet.h>
**      #include <netinet/in.h>
**      #include <netdb.h>
**      #include <sys/socket.h>
**
**  but also cleans up various messes, mostly related to IPv6 support.  It
**  ensures that inet_aton, inet_ntoa, and inet_ntop are available and
**  properly prototyped.
**
**  Copyright 2008 Board of Trustees, Leland Stanford Jr. University
**  Copyright (c) 2004, 2005, 2006, 2007
**      by Internet Systems Consortium, Inc. ("ISC")
**  Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
**      2002, 2003 by The Internet Software Consortium and Rich Salz
**
**  This code is derived from software contributed to the Internet Software
**  Consortium by Rich Salz.
**
**  Permission to use, copy, modify, and distribute this software for any
**  purpose with or without fee is hereby granted, provided that the above
**  copyright notice and this permission notice appear in all copies.
**
**  THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
**  REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
**  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
**  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
**  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
**  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef PORTABLE_SOCKET_H
#define PORTABLE_SOCKET_H 1

#include <config.h>

#include <errno.h>
#include <sys/types.h>

/* BSDI needs <netinet/in.h> before <arpa/inet.h>. */
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <sys/socket.h>
#endif

/* Pick up definitions of getaddrinfo and getnameinfo if not otherwise
   available. */
#include <portable/getaddrinfo.h>
#include <portable/getnameinfo.h>

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

/* Provide prototypes for inet_aton and inet_ntoa if not prototyped in the
   system header files since they're occasionally available without proper
   prototypes. */
#if !HAVE_DECL_INET_ATON
extern int              inet_aton(const char *, struct in_addr *);
#endif
#if !HAVE_DECL_INET_NTOA
extern const char *     inet_ntoa(const struct in_addr);
#endif
#if !HAVE_INET_NTOP
extern const char *     inet_ntop(int, const void *, char *, socklen_t);
#endif

/* Used for portability to Windows, which requires different functions be
   called to close sockets, send data to or read from sockets, and get socket
   errors than the regular functions and variables.  socket_init() must be
   called before socket functions are used and socket_shutdown() at the end
   of the program. */
#ifdef _WIN32
int socket_init(void);
# define socket_shutdown()      WSACleanup()
# define socket_close(fd)       closesocket(fd)
# define socket_errno           WSAGetLastError()
# define socket_set_errno(e)    WSASetLastError(e)
# define socket_read(fd, b, s)  recv((fd), (b), (s), 0)
# define socket_write(fd, b, s) send((fd), (b), (s), 0)
#else
# define socket_init()          1
# define socket_shutdown()      /* empty */
# define socket_close(fd)       close(fd)
# define socket_errno           errno
# define socket_set_errno(e)    errno = (e)
# define socket_strerror(e)     strerror(e)
# define socket_read(fd, b, s)  read((fd), (b), (s))
# define socket_write(fd, b, s) write((fd), (b), (s))
#endif

/* Some systems don't define INADDR_LOOPBACK. */
#ifndef INADDR_LOOPBACK
# define INADDR_LOOPBACK 0x7f000001UL
#endif

/* Defined by RFC 2553, used to store a generic address.  Note that this
   doesn't do the alignment mangling that RFC 2553 does; it's not clear if
   that should be added.... */
#if !HAVE_STRUCT_SOCKADDR_STORAGE
# if HAVE_STRUCT_SOCKADDR_SA_LEN
struct sockaddr_storage {
    unsigned char ss_len;
    unsigned char ss_family;
    unsigned char __padding[128 - 2];
};
# else
struct sockaddr_storage {
    unsigned short ss_family;
    unsigned char __padding[128 - 2];
};
# endif
#endif

/* Use convenient, non-uglified names for the fields since we use them quite a
   bit in code. */
#if HAVE_STRUCT_SOCKADDR_STORAGE && !HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
# define ss_family __ss_family
# define ss_len    __ss_len
#endif

/* Fix IN6_ARE_ADDR_EQUAL if required. */
#ifdef HAVE_BROKEN_IN6_ARE_ADDR_EQUAL
# undef IN6_ARE_ADDR_EQUAL
# define IN6_ARE_ADDR_EQUAL(a, b) \
    (memcmp((a), (b), sizeof(struct in6_addr)) == 0)
#endif

/* Define an SA_LEN macro that gives us the length of a sockaddr. */
#if !HAVE_SA_LEN
# if HAVE_STRUCT_SOCKADDR_SA_LEN
#  define SA_LEN(s)     ((s)->sa_len)
# else
/* Hack courtesy of the USAGI project. */
#  if HAVE_INET6
#   define SA_LEN(s) \
    ((((const struct sockaddr *)(s))->sa_family == AF_INET6)            \
        ? sizeof(struct sockaddr_in6)                                   \
        : ((((const struct sockaddr *)(s))->sa_family == AF_INET)       \
            ? sizeof(struct sockaddr_in)                                \
            : sizeof(struct sockaddr)))
#  else
#   define SA_LEN(s) \
    ((((const struct sockaddr *)(s))->sa_family == AF_INET)             \
        ? sizeof(struct sockaddr_in)                                    \
        : sizeof(struct sockaddr))
#  endif
# endif /* HAVE_SOCKADDR_LEN */
#endif /* !HAVE_SA_LEN_MACRO */

/* POSIX requires AI_ADDRCONFIG and AI_NUMERICSERV, but some implementations
   don't have them yet.  It's only used in a bitwise OR of flags, so defining
   them to 0 makes them harmlessly go away. */
#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif
#ifndef AI_NUMERICSERV
# define AI_NUMERICSERV 0
#endif

/* Constants required by the IPv6 API.  The buffer size required to hold any
   nul-terminated text representation of the given address type. */
#ifndef INET_ADDRSTRLEN
# define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
# define INET6_ADDRSTRLEN 46
#endif

/* This is one of the defined error codes from inet_ntop, but it may not be
   available on systems too old to have that function. */
#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT EDOM
#endif

END_DECLS

#endif /* !PORTABLE_SOCKET_H */
