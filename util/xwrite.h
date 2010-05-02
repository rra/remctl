/*
 * Prototypes for write and writev replacements to handle partial writes.
 *
 * Copyright 2008, 2010 Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#ifndef UTIL_XWRITE_H
#define UTIL_XWRITE_H 1

#include <config.h>
#include <portable/macros.h>

#include <sys/types.h>

/* Forward declaration to avoid an include. */
struct iovec;

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/*
 * Like the non-x versions of the same function, but keep writing until either
 * the write is not making progress or there's a real error.  Handle partial
 * writes and EINTR/EAGAIN errors.
 */
ssize_t xpwrite(int fd, const void *buffer, size_t size, off_t offset)
    __attribute__((__nonnull__));
ssize_t xwrite(int fd, const void *buffer, size_t size)
    __attribute__((__nonnull__));
ssize_t xwritev(int fd, const struct iovec *iov, int iovcnt)
    __attribute__((__nonnull__));

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_XWRITE_H */
