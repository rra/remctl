/*
 * Prototypes for setting or clearing file descriptor flags.
 *
 * Copyright 2008, 2010 Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#ifndef UTIL_FDFLAG_H
#define UTIL_FDFLAG_H 1

#include <config.h>
#include <portable/macros.h>
#include <portable/stdbool.h>

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/* Set a file descriptor close-on-exec or nonblocking. */
bool fdflag_close_exec(int fd, bool flag);
bool fdflag_nonblocking(int fd, bool flag);

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_FDFLAG_H */
