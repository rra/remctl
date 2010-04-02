/*
 * Prototypes for network connection utility functions.
 *
 * Copyright 2009, 2010 Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006, 2007, 2008, 2010
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#ifndef UTIL_NETWORK_H
#define UTIL_NETWORK_H 1

#include <config.h>
#include <portable/macros.h>
#include <portable/socket.h>
#include <portable/stdbool.h>

#include <sys/types.h>

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/*
 * Create a socket and bind it to the specified address and port (either IPv4
 * or IPv6), returning the resulting file descriptor or -1 on error.  Errors
 * are reported using warn/syswarn.  To bind to all interfaces, use "any" or
 * "all" for address.
 */
socket_type network_bind_ipv4(const char *address, unsigned short port)
    __attribute__((__nonnull__));
socket_type network_bind_ipv6(const char *address, unsigned short port)
    __attribute__((__nonnull__));

/*
 * Create and bind sockets for every local address (normally two, one for IPv4
 * and one for IPv6, if IPv6 support is enabled).  If IPv6 is not enabled,
 * just one socket will be created and bound to the IPv4 wildcard address.
 * fds will be set to an array containing the resulting file descriptors, with
 * count holding the count returned.
 */
void network_bind_all(unsigned short port, socket_type **fds, int *count)
    __attribute__((__nonnull__));

/*
 * Create a socket and connect it to the remote service given by the linked
 * list of addrinfo structs.  Returns the new file descriptor on success and
 * -1 on failure, with the error left in errno.  Takes an optional source
 * address.
 */
socket_type network_connect(struct addrinfo *, const char *source)
    __attribute__((__nonnull__(1)));

/*
 * Like network_connect but takes a host and port instead.  If host lookup
 * fails, errno may not be set to anything useful.
 */
socket_type network_connect_host(const char *host, unsigned short port,
                                 const char *source)
    __attribute__((__nonnull__(1)));

/*
 * Creates a socket of the specified domain and type and binds it to the
 * appropriate source address, either the one supplied or all addresses if the
 * source address is NULL or "all".  Returns the newly created file descriptor
 * or -1 on error.
 *
 * This is a lower-level function intended primarily for the use of clients
 * that will then go on to do a non-blocking connect.
 */
socket_type network_client_create(int domain, int type, const char *source);

/*
 * Put an ASCII representation of the address in a sockaddr into the provided
 * buffer, which should hold at least INET6_ADDRSTRLEN characters.
 */
bool network_sockaddr_sprint(char *, size_t, const struct sockaddr *)
    __attribute__((__nonnull__));

/*
 * Returns if the addresses from the two sockaddrs are equal.  The ports are
 * ignored, and only AF_INET or AF_INET6 sockaddrs are supported (all others
 * will return false).
 */
bool network_sockaddr_equal(const struct sockaddr *, const struct sockaddr *)
    __attribute__((__nonnull__));

/* Returns the port number from a sockaddr. */
unsigned short network_sockaddr_port(const struct sockaddr *)
    __attribute__((__nonnull__));

/*
 * Compare two addresses relative to an optional mask.  Returns true if
 * they're equal, false otherwise or on a parse error.
 */
bool network_addr_match(const char *, const char *, const char *mask)
    __attribute__((__nonnull__(1, 2)));

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_NETWORK_H */
